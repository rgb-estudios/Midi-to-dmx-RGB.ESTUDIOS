#include "runtime/host_event.h"
#include "runtime/host_event_ingress.h"
#include "runtime/plugin_state.h"
#include "runtime/runtime_safety_state.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace {

using aeyla::runtime::HostEvent;
using aeyla::runtime::HostEventIngress;
using aeyla::runtime::HostEventType;
using aeyla::runtime::PluginComponentState;
using aeyla::runtime::RuntimeSafetyReason;
using aeyla::runtime::RuntimeSafetyState;

constexpr const char* kProductName = "AEYLA Visual DMX Standalone Alpha";
constexpr const char* kVersion = "0.1.0-alpha.2";

const char* reason_name(RuntimeSafetyReason reason) noexcept {
  switch (reason) {
    case RuntimeSafetyReason::none:
      return "none";
    case RuntimeSafetyReason::operator_disarm:
      return "operator_disarm";
    case RuntimeSafetyReason::project_invalid:
      return "project_invalid";
    case RuntimeSafetyReason::backend_unavailable:
      return "backend_unavailable";
    case RuntimeSafetyReason::show_not_ready:
      return "show_not_ready";
    case RuntimeSafetyReason::project_reload:
      return "project_reload";
    case RuntimeSafetyReason::event_overflow:
      return "event_overflow";
    case RuntimeSafetyReason::host_deactivation:
      return "host_deactivation";
    case RuntimeSafetyReason::shutdown:
      return "shutdown";
  }
  return "unknown";
}

bool run_self_test() {
  HostEventIngress<8> ingress;
  RuntimeSafetyState safety;
  PluginComponentState state;

  if (safety.output_armed() || !safety.blackout()) {
    std::cerr << "SELF TEST FAIL: unsafe initial state\n";
    return false;
  }

  safety.set_project_valid(true);
  safety.set_backend_ready(true);
  if (!safety.request_arm()) {
    std::cerr << "SELF TEST FAIL: diagnostic backend did not arm\n";
    return false;
  }

  const HostEvent note_on{
      HostEventType::note_on, 0, 60, 0, 100.0F / 127.0F, 0, -1};
  const HostEvent note_off{
      HostEventType::note_off, 0, 60, 0, 0.0F, 0, -1};

  if (!ingress.try_submit(note_on) || !ingress.try_submit(note_off)) {
    std::cerr << "SELF TEST FAIL: event ingress rejected valid events\n";
    return false;
  }

  HostEvent consumed{};
  if (!ingress.try_consume(consumed) || consumed.type != HostEventType::note_on ||
      consumed.note != 60) {
    std::cerr << "SELF TEST FAIL: note-on mismatch\n";
    return false;
  }
  if (!ingress.try_consume(consumed) || consumed.type != HostEventType::note_off ||
      consumed.note != 60) {
    std::cerr << "SELF TEST FAIL: note-off mismatch\n";
    return false;
  }

  state.grand_master = 0.42F;
  state.blackout = false;
  const auto encoded = aeyla::runtime::encode_plugin_component_state(state);
  if (!encoded.ok()) {
    std::cerr << "SELF TEST FAIL: state encode failed\n";
    return false;
  }
  const auto decoded = aeyla::runtime::decode_plugin_component_state(encoded.bytes);
  if (!decoded.ok() || decoded.state != state) {
    std::cerr << "SELF TEST FAIL: state round trip failed\n";
    return false;
  }

  safety.on_shutdown();
  if (safety.output_armed()) {
    std::cerr << "SELF TEST FAIL: shutdown left output armed\n";
    return false;
  }

  const auto actions = safety.consume_pending_actions();
  if (!actions.release_transients || !actions.force_haze_zero ||
      !actions.publish_safe_frame) {
    std::cerr << "SELF TEST FAIL: shutdown safe actions missing\n";
    return false;
  }

  std::cout << "SELF TEST PASS: runtime, event ingress, state and shutdown safety\n";
  return true;
}

class DiagnosticStandalone {
 public:
  DiagnosticStandalone() {
    // This first standalone uses a null/diagnostic backend. It proves the shared
    // runtime and operator flow without transmitting Art-Net or USB-DMX.
    safety_.set_project_valid(true);
    safety_.set_backend_ready(true);
    worker_ = std::thread([this] { consume_events(); });
  }

  ~DiagnosticStandalone() {
    running_.store(false, std::memory_order_release);
    if (worker_.joinable()) {
      worker_.join();
    }
    safety_.on_shutdown();
  }

  int run() {
    print_header();
    print_help();
    print_status();

    std::string line;
    while (running_.load(std::memory_order_acquire) &&
           std::cout << "\naeyla> " && std::getline(std::cin, line)) {
      service_safety();
      if (!handle_command(line)) {
        break;
      }
    }

    running_.store(false, std::memory_order_release);
    return 0;
  }

 private:
  void print_header() const {
    std::cout << "\n" << kProductName << "  " << kVersion << "\n"
              << "RGB Estudios / AEYLA\n"
              << "Diagnostic null backend: NO DMX IS TRANSMITTED in this alpha.\n"
              << "The executable shares the same event ingress, state encoding and\n"
              << "safety model used by the VST3 adapter.\n\n";
  }

  void print_help() const {
    std::cout << "Commands:\n"
              << "  a                 toggle Output Arm\n"
              << "  b                 toggle Blackout\n"
              << "  g <0-100>         set Grand Master percent\n"
              << "  n <0-127> <0-127> submit MIDI Note On + velocity\n"
              << "  f <0-127>         submit MIDI Note Off\n"
              << "  p                 run a short MIDI diagnostic pattern\n"
              << "  s                 show current status\n"
              << "  h                 show commands\n"
              << "  q                 safe shutdown\n";
  }

  void print_status() {
    service_safety();
    std::cout << std::fixed << std::setprecision(0)
              << "STATUS | arm=" << (safety_.output_armed() ? "ON" : "OFF")
              << " | blackout=" << (safety_.blackout() ? "ON" : "OFF")
              << " | grand_master=" << (state_.grand_master * 100.0F) << "%"
              << " | note_on=" << note_on_count_.load(std::memory_order_relaxed)
              << " | note_off=" << note_off_count_.load(std::memory_order_relaxed)
              << " | dropped=" << ingress_.dropped_events()
              << " | reason=" << reason_name(safety_.reason()) << "\n";
  }

  bool handle_command(const std::string& line) {
    std::istringstream input(line);
    std::string command;
    input >> command;
    if (command.empty()) {
      return true;
    }

    if (command == "q" || command == "quit" || command == "exit") {
      safety_.on_shutdown();
      const auto actions = safety_.consume_pending_actions();
      std::cout << "Safe shutdown: transients=" << actions.release_transients
                << ", haze_zero=" << actions.force_haze_zero
                << ", safe_frame=" << actions.publish_safe_frame << "\n";
      return false;
    }

    if (command == "h" || command == "help") {
      print_help();
      return true;
    }

    if (command == "s" || command == "status") {
      print_status();
      return true;
    }

    if (command == "a" || command == "arm") {
      if (safety_.output_armed()) {
        safety_.disarm();
      } else if (!safety_.request_arm()) {
        std::cout << "Arm rejected: project/backend safety gate is not ready.\n";
      }
      print_status();
      return true;
    }

    if (command == "b" || command == "blackout") {
      const bool enabled = !safety_.blackout();
      safety_.set_blackout(enabled);
      state_.blackout = enabled;
      print_status();
      return true;
    }

    if (command == "g" || command == "gm") {
      int percent = -1;
      if (!(input >> percent) || percent < 0 || percent > 100) {
        std::cout << "Usage: g <0-100>\n";
        return true;
      }
      state_.grand_master = static_cast<float>(percent) / 100.0F;
      print_status();
      return true;
    }

    if (command == "n" || command == "note") {
      int note = -1;
      int velocity = -1;
      if (!(input >> note >> velocity) || note < 0 || note > 127 ||
          velocity < 0 || velocity > 127) {
        std::cout << "Usage: n <note 0-127> <velocity 0-127>\n";
        return true;
      }
      submit_event(HostEventType::note_on, note, velocity);
      print_status();
      return true;
    }

    if (command == "f" || command == "off") {
      int note = -1;
      if (!(input >> note) || note < 0 || note > 127) {
        std::cout << "Usage: f <note 0-127>\n";
        return true;
      }
      submit_event(HostEventType::note_off, note, 0);
      print_status();
      return true;
    }

    if (command == "p" || command == "pattern") {
      run_pattern();
      print_status();
      return true;
    }

    std::cout << "Unknown command. Type h for help.\n";
    return true;
  }

  void submit_event(HostEventType type, int note, int velocity) {
    const HostEvent event{
        type,
        0,
        static_cast<std::uint8_t>(std::clamp(note, 0, 127)),
        0,
        static_cast<float>(std::clamp(velocity, 0, 127)) / 127.0F,
        0,
        -1};

    if (!ingress_.try_submit(event)) {
      std::cout << "Event queue overflow: safe release requested.\n";
    }
  }

  void run_pattern() {
    constexpr int notes[] = {48, 52, 55, 60, 64, 67, 72};
    for (const int note : notes) {
      submit_event(HostEventType::note_on, note, 100);
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      submit_event(HostEventType::note_off, note, 0);
    }
    std::cout << "Diagnostic MIDI pattern submitted.\n";
  }

  void consume_events() {
    while (running_.load(std::memory_order_acquire)) {
      HostEvent event{};
      bool consumed_any = false;
      while (ingress_.try_consume(event)) {
        consumed_any = true;
        if (event.type == HostEventType::note_on && event.value > 0.0F) {
          note_on_count_.fetch_add(1, std::memory_order_relaxed);
        } else if (event.type == HostEventType::note_off ||
                   (event.type == HostEventType::note_on && event.value <= 0.0F)) {
          note_off_count_.fetch_add(1, std::memory_order_relaxed);
        }
      }

      if (!consumed_any) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
    }
  }

  void service_safety() {
    if (ingress_.consume_transient_release_request()) {
      safety_.on_event_overflow();
    }
    const auto actions = safety_.consume_pending_actions();
    if (actions.release_transients || actions.force_haze_zero ||
        actions.publish_safe_frame) {
      std::cout << "SAFETY | release_transients=" << actions.release_transients
                << " | haze_zero=" << actions.force_haze_zero
                << " | safe_frame=" << actions.publish_safe_frame << "\n";
    }
  }

  HostEventIngress<256> ingress_{};
  RuntimeSafetyState safety_{};
  PluginComponentState state_{};
  std::atomic<bool> running_{true};
  std::atomic<std::uint64_t> note_on_count_{0};
  std::atomic<std::uint64_t> note_off_count_{0};
  std::thread worker_{};
};

}  // namespace

int main(int argc, char** argv) {
  if (argc > 1) {
    const std::string argument = argv[1];
    if (argument == "--self-test") {
      return run_self_test() ? 0 : 1;
    }
    if (argument == "--version") {
      std::cout << kProductName << " " << kVersion << "\n";
      return 0;
    }
  }

  DiagnosticStandalone application;
  return application.run();
}
