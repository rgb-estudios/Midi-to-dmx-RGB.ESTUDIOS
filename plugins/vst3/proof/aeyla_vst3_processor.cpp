#include "aeyla_vst3_processor.h"

#include "aeyla_vst3_cids.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace Steinberg::AeylaVst3 {
namespace {

bool readStateBytes(IBStream* stream, std::vector<std::uint8_t>& bytes) {
  if (!stream) return false;

  bytes.assign(aeyla::runtime::kMaxPluginStateBytes, 0U);
  int32 bytesRead = 0;
  const auto result = stream->read(bytes.data(), static_cast<int32>(bytes.size()), &bytesRead);
  if ((result != kResultOk && result != kResultTrue) || bytesRead <= 0) {
    bytes.clear();
    return false;
  }

  bytes.resize(static_cast<std::size_t>(bytesRead));
  return true;
}

std::int64_t projectSampleFor(const Vst::ProcessData& data) noexcept {
  // In VST3 SDK 3.8 projectTimeSamples is part of the always-valid base
  // ProcessContext fields whenever the host provides a context object.
  if (!data.processContext) return -1;
  return static_cast<std::int64_t>(data.processContext->projectTimeSamples);
}

std::uint8_t clampMidiByte(int32 value) noexcept {
  return static_cast<std::uint8_t>(std::clamp(value, 0, 127));
}

}  // namespace

Processor::Processor() {
  setControllerClass(kControllerUid);
  componentState_.blackout = true;
  componentState_.grand_master = 1.0F;
}

tresult PLUGIN_API Processor::initialize(FUnknown* context) {
  const auto result = AudioEffect::initialize(context);
  if (result != kResultOk) return result;

  addAudioOutput(STR16("Silent Stereo Out"), Vst::SpeakerArr::kStereo);
  addEventInput(STR16("MIDI In"), 1);

  safety_.set_project_valid(true);   // Built-in proof project.
  safety_.set_backend_ready(false);  // No hardware output in this milestone.
  safety_.set_blackout(true);
  return kResultOk;
}

tresult PLUGIN_API Processor::terminate() {
  safety_.on_shutdown();
  return AudioEffect::terminate();
}

tresult PLUGIN_API Processor::setActive(TBool state) {
  if (state) {
    armParameterHigh_ = false;
    armSeenLowSinceActivation_ = false;
    safety_.set_blackout(componentState_.blackout);
  } else {
    safety_.on_host_deactivation();
  }
  return AudioEffect::setActive(state);
}

tresult PLUGIN_API Processor::setupProcessing(Vst::ProcessSetup& setup) {
  return AudioEffect::setupProcessing(setup);
}

tresult PLUGIN_API Processor::canProcessSampleSize(int32 symbolicSampleSize) {
  return (symbolicSampleSize == Vst::kSample32 || symbolicSampleSize == Vst::kSample64)
             ? kResultTrue
             : kResultFalse;
}

void Processor::processParameterChanges(Vst::ProcessData& data) noexcept {
  if (!data.inputParameterChanges) return;

  const int32 count = data.inputParameterChanges->getParameterCount();
  for (int32 index = 0; index < count; ++index) {
    auto* queue = data.inputParameterChanges->getParameterData(index);
    if (!queue || queue->getPointCount() <= 0) continue;

    int32 sampleOffset = 0;
    Vst::ParamValue value = 0.0;
    if (queue->getPoint(queue->getPointCount() - 1, sampleOffset, value) != kResultTrue)
      continue;

    value = std::clamp(value, 0.0, 1.0);
    switch (queue->getParameterId()) {
      case kGrandMasterId:
        componentState_.grand_master = static_cast<float>(value);
        break;
      case kBlackoutId:
        componentState_.blackout = value >= 0.5;
        safety_.set_blackout(componentState_.blackout);
        break;
      case kOutputArmRequestId: {
        const bool high = value >= 0.5;
        if (!high) {
          armSeenLowSinceActivation_ = true;
          if (armParameterHigh_) safety_.disarm();
        } else if (!armParameterHigh_ && armSeenLowSinceActivation_) {
          (void)safety_.request_arm();
        }
        armParameterHigh_ = high;
        break;
      }
      default:
        break;
    }
  }
}

void Processor::processEvents(Vst::ProcessData& data) noexcept {
  bool activityChanged = false;
  const auto projectSample = projectSampleFor(data);

  if (data.inputEvents) {
    Vst::Event event{};
    const int32 eventCount = data.inputEvents->getEventCount();
    for (int32 index = 0; index < eventCount; ++index) {
      if (data.inputEvents->getEvent(index, event) != kResultOk) continue;

      aeyla::runtime::HostEvent hostEvent{};
      hostEvent.sample_offset = event.sampleOffset;
      hostEvent.project_sample = projectSample;

      if (event.type == Vst::Event::kNoteOnEvent) {
        hostEvent.type = aeyla::runtime::HostEventType::note_on;
        hostEvent.channel = clampMidiByte(event.noteOn.channel);
        hostEvent.note = clampMidiByte(event.noteOn.pitch);
        hostEvent.value = std::clamp(event.noteOn.velocity, 0.0F, 1.0F);
        midiActivity_ = static_cast<Vst::ParamValue>(hostEvent.note + 1U) / 128.0;
        activityChanged = true;
        (void)eventIngress_.try_submit(hostEvent);
      } else if (event.type == Vst::Event::kNoteOffEvent) {
        hostEvent.type = aeyla::runtime::HostEventType::note_off;
        hostEvent.channel = clampMidiByte(event.noteOff.channel);
        hostEvent.note = clampMidiByte(event.noteOff.pitch);
        hostEvent.value = std::clamp(event.noteOff.velocity, 0.0F, 1.0F);
        midiActivity_ = 0.0;
        activityChanged = true;
        (void)eventIngress_.try_submit(hostEvent);
      }
    }
  }

  // The full runtime consumer will move to its own worker. The proof drains the
  // bounded ingress here without allocations, locks, network, file or media I/O.
  aeyla::runtime::HostEvent consumed{};
  while (eventIngress_.try_consume(consumed)) {
  }

  if (eventIngress_.consume_transient_release_request()) {
    safety_.on_event_overflow();
  }

  if (activityChanged) publishParameter(data, kMidiActivityId, midiActivity_);

  const auto dropped = eventIngress_.dropped_events();
  if (dropped != lastPublishedDroppedEvents_) {
    lastPublishedDroppedEvents_ = dropped;
    const auto normalized = std::min<Vst::ParamValue>(1.0, static_cast<double>(dropped) / 100.0);
    publishParameter(data, kDroppedEventsId, normalized);
  }
}

void Processor::publishParameter(Vst::ProcessData& data, Vst::ParamID id,
                                 Vst::ParamValue value, int32 sampleOffset) noexcept {
  if (!data.outputParameterChanges) return;
  int32 queueIndex = 0;
  auto* queue = data.outputParameterChanges->addParameterData(id, queueIndex);
  if (!queue) return;
  int32 pointIndex = 0;
  (void)queue->addPoint(sampleOffset, std::clamp(value, 0.0, 1.0), pointIndex);
}

void Processor::clearOutputs(Vst::ProcessData& data) noexcept {
  for (int32 busIndex = 0; busIndex < data.numOutputs; ++busIndex) {
    auto& bus = data.outputs[busIndex];
    for (int32 channel = 0; channel < bus.numChannels; ++channel) {
      if (data.symbolicSampleSize == Vst::kSample32 && bus.channelBuffers32 &&
          bus.channelBuffers32[channel]) {
        std::memset(bus.channelBuffers32[channel], 0,
                    static_cast<std::size_t>(data.numSamples) * sizeof(Vst::Sample32));
      } else if (data.symbolicSampleSize == Vst::kSample64 && bus.channelBuffers64 &&
                 bus.channelBuffers64[channel]) {
        std::memset(bus.channelBuffers64[channel], 0,
                    static_cast<std::size_t>(data.numSamples) * sizeof(Vst::Sample64));
      }
    }

    if (bus.numChannels >= 64)
      bus.silenceFlags = std::numeric_limits<std::uint64_t>::max();
    else if (bus.numChannels > 0)
      bus.silenceFlags = (std::uint64_t{1} << static_cast<unsigned>(bus.numChannels)) - 1U;
    else
      bus.silenceFlags = 0;
  }
}

tresult PLUGIN_API Processor::process(Vst::ProcessData& data) {
  processParameterChanges(data);
  processEvents(data);
  clearOutputs(data);
  (void)safety_.consume_pending_actions();
  return kResultOk;
}

tresult PLUGIN_API Processor::setState(IBStream* state) {
  std::vector<std::uint8_t> bytes;
  if (!readStateBytes(state, bytes)) return kResultFalse;

  const auto decoded = aeyla::runtime::decode_plugin_component_state(bytes);
  if (!decoded.ok()) return kResultFalse;

  safety_.begin_project_reload();
  componentState_ = decoded.state;
  safety_.complete_project_reload(true);
  safety_.set_blackout(componentState_.blackout);
  armParameterHigh_ = false;
  armSeenLowSinceActivation_ = false;
  return kResultOk;
}

tresult PLUGIN_API Processor::getState(IBStream* state) {
  if (!state) return kResultFalse;

  componentState_.blackout = safety_.blackout();
  auto encoded = aeyla::runtime::encode_plugin_component_state(componentState_);
  if (!encoded.ok() || encoded.bytes.size() > static_cast<std::size_t>(std::numeric_limits<int32>::max()))
    return kResultFalse;

  int32 bytesWritten = 0;
  // IBStream::write predates const-correct buffers. The mutable vector is kept
  // local and the stream contract treats it as read-only input.
  const auto result = state->write(encoded.bytes.data(), static_cast<int32>(encoded.bytes.size()),
                                   &bytesWritten);
  if ((result != kResultOk && result != kResultTrue) ||
      bytesWritten != static_cast<int32>(encoded.bytes.size()))
    return kResultFalse;
  return kResultOk;
}

}  // namespace Steinberg::AeylaVst3
