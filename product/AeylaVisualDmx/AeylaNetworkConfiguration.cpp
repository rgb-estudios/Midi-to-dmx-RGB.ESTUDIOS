#include "AeylaNetworkConfiguration.h"

#include "network/ipv4_configuration.h"
#include "network/network_change_protocol.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <optional>
#include <random>
#include <sstream>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

#ifdef _WIN32

std::string make_nonce() {
  std::array<std::uint8_t, 16> bytes{};
  std::random_device random;
  for(auto& byte : bytes)
    byte = static_cast<std::uint8_t>(random());
  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for(const auto byte : bytes) stream << std::setw(2) << unsigned(byte);
  return stream.str();
}

std::filesystem::path result_path_for(const std::filesystem::path& request) {
  auto result = request;
  result.replace_extension(".aeylanetresult");
  return result;
}

bool write_request(const std::filesystem::path& path,
                   const aeyla::network::NetworkChangeRequest& request,
                   std::string& error_message) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if(error) {
    error_message = "No se pudo preparar la carpeta temporal de red";
    return false;
  }
  const auto encoded = aeyla::network::encode_network_change_request(request);
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if(!output) {
    error_message = "No se pudo crear la solicitud temporal de red";
    return false;
  }
  output.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
  output.flush();
  if(!output) {
    error_message = "No se pudo completar la solicitud temporal de red";
    return false;
  }
  return true;
}

std::optional<std::string> read_small_file(const std::filesystem::path& path,
                                           std::string& error_message) {
  std::error_code error;
  if(!std::filesystem::is_regular_file(path, error) || error) {
    error_message = "El helper de red no generó un resultado verificable";
    return std::nullopt;
  }
  const auto size = std::filesystem::file_size(path, error);
  if(error || size == 0U || size > 4096U) {
    error_message = "El resultado del helper de red tiene un tamaño inválido";
    return std::nullopt;
  }
  std::ifstream input(path, std::ios::binary);
  std::string text(static_cast<std::size_t>(size), '\0');
  input.read(text.data(), static_cast<std::streamsize>(text.size()));
  if(!input) {
    error_message = "No se pudo leer el resultado completo del helper de red";
    return std::nullopt;
  }
  return text;
}

void module_anchor() {}

std::optional<std::filesystem::path> module_path() {
  HMODULE module = nullptr;
  if(!GetModuleHandleExW(
         GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
         reinterpret_cast<LPCWSTR>(&module_anchor), &module))
    return std::nullopt;

  std::vector<wchar_t> buffer(32768U, L'\0');
  const DWORD length = GetModuleFileNameW(
      module, buffer.data(), static_cast<DWORD>(buffer.size()));
  if(length == 0U || length >= buffer.size()) return std::nullopt;
  return std::filesystem::path(std::wstring(buffer.data(), length));
}

std::optional<std::filesystem::path> find_helper() {
  const auto module = module_path();
  if(!module.has_value()) return std::nullopt;
  const auto module_directory = module->parent_path();
  std::array<std::filesystem::path, 8> candidates{};
  candidates[0] = module_directory / L"AeylaNetworkHelper.exe";
  candidates[1] = module_directory.parent_path() / L"Resources" /
                  L"AeylaNetworkHelper.exe";
  auto cursor = module_directory;
  for(std::size_t index = 2U; index < candidates.size(); ++index) {
    candidates[index] = cursor / L"AeylaNetworkHelper.exe";
    if(cursor.has_parent_path()) cursor = cursor.parent_path();
  }
  std::error_code error;
  for(const auto& candidate : candidates) {
    if(!candidate.empty() && std::filesystem::is_regular_file(candidate, error) &&
       !error)
      return candidate;
    error.clear();
  }
  return std::nullopt;
}

bool launch_elevated_helper(const std::filesystem::path& helper,
                            const std::filesystem::path& request,
                            DWORD& exit_code,
                            std::string& error_message) {
  const std::wstring parameters = L"--request \"" + request.wstring() + L"\"";
  SHELLEXECUTEINFOW execution{};
  execution.cbSize = sizeof(execution);
  execution.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
  execution.lpVerb = L"runas";
  execution.lpFile = helper.c_str();
  execution.lpParameters = parameters.c_str();
  execution.nShow = SW_HIDE;
  if(!ShellExecuteExW(&execution)) {
    const DWORD code = GetLastError();
    error_message = code == ERROR_CANCELLED
        ? "Windows canceló la autorización UAC; la red no cambió"
        : "No se pudo iniciar el helper de red · código " +
              std::to_string(code);
    return false;
  }
  const DWORD wait = WaitForSingleObject(execution.hProcess, 60000U);
  if(wait != WAIT_OBJECT_0) {
    (void)TerminateProcess(execution.hProcess, ERROR_TIMEOUT);
    CloseHandle(execution.hProcess);
    error_message =
        "El helper de red agotó el tiempo; verifica manualmente el adaptador";
    return false;
  }
  exit_code = ERROR_GEN_FAILURE;
  (void)GetExitCodeProcess(execution.hProcess, &exit_code);
  CloseHandle(execution.hProcess);
  return true;
}

#endif

}  // namespace

AeylaNetworkConfiguration::~AeylaNetworkConfiguration() { Shutdown(); }

bool AeylaNetworkConfiguration::Start(
    const aeyla::network::NetworkInterface& adapter,
    std::string ipv4,
    std::uint8_t prefix_length,
    std::string& error_message) {
  error_message.clear();
  if(mBusy.exchange(true, std::memory_order_acq_rel)) {
    error_message = "Ya existe un cambio de red AEYLA en curso";
    return false;
  }
  if(adapter.id.empty() || adapter.interface_index == 0U) {
    mBusy.store(false, std::memory_order_release);
    error_message = "El adaptador TX no posee una identidad física válida";
    return false;
  }
  std::string network_error;
  if(!aeyla::network::make_ipv4_network(ipv4, prefix_length,
                                         network_error).has_value()) {
    mBusy.store(false, std::memory_order_release);
    error_message = std::move(network_error);
    return false;
  }

#ifndef _WIN32
  // A plug-in hosted by Ableton must not launch a privileged network mutation.
  // Configure the show NIC before Live starts; AEYLA then validates and binds
  // to that already-configured address without elevated privileges.
#ifdef __APPLE__
  error_message =
      "macOS: configura la IPv4/submáscara del Ethernet en Ajustes del Sistema > Red, "
      "luego vuelve a AEYLA y pulsa REESCANEAR. El plugin no solicita privilegios dentro de Ableton.";
#else
  error_message =
      "La configuración automática de IP/submáscara no está disponible en esta plataforma";
#endif
  mBusy.store(false, std::memory_order_release);
  Publish(AeylaNetworkConfigurationState::failed, ipv4, prefix_length,
          error_message);
  return false;
#else
  if(mWorker.joinable()) mWorker.join();
  Publish(AeylaNetworkConfigurationState::awaiting_permission, ipv4,
          prefix_length,
          "Esperando autorización UAC de Windows · REAPER continúa sin privilegios");
  try {
    mWorker = std::thread(&AeylaNetworkConfiguration::Run, this, adapter,
                          std::move(ipv4), prefix_length);
  } catch(...) {
    mBusy.store(false, std::memory_order_release);
    error_message = "No se pudo iniciar el cambio de red en segundo plano";
    Publish(AeylaNetworkConfigurationState::failed, {}, 0U, error_message);
    return false;
  }
  return true;
#endif
}

AeylaNetworkConfigurationSnapshot AeylaNetworkConfiguration::Snapshot() const {
  const std::scoped_lock lock(mMutex);
  return mSnapshot;
}

void AeylaNetworkConfiguration::Shutdown() noexcept {
  if(mWorker.joinable()) mWorker.join();
  mBusy.store(false, std::memory_order_release);
}

void AeylaNetworkConfiguration::Run(
    aeyla::network::NetworkInterface adapter,
    std::string ipv4,
    std::uint8_t prefix_length) noexcept {
  try {
#ifdef _WIN32
    const auto helper = find_helper();
    if(!helper.has_value()) {
      Publish(AeylaNetworkConfigurationState::failed, ipv4, prefix_length,
              "No se encontró AeylaNetworkHelper.exe dentro del producto");
      mBusy.store(false, std::memory_order_release);
      return;
    }

    const std::string nonce = make_nonce();
    const auto transaction_directory =
        std::filesystem::temp_directory_path() / "RGB-Estudios" /
        "AEYLA" / "network" / nonce;
    const auto request_path = transaction_directory /
                              (nonce + ".aeylanetreq");
    const auto result_path = result_path_for(request_path);
    const aeyla::network::NetworkChangeRequest request{
        adapter.interface_index, adapter.id, ipv4, prefix_length, nonce};
    std::string error;
    if(!write_request(request_path, request, error)) {
      Publish(AeylaNetworkConfigurationState::failed, ipv4, prefix_length,
              std::move(error));
      mBusy.store(false, std::memory_order_release);
      return;
    }

    Publish(AeylaNetworkConfigurationState::applying, ipv4, prefix_length,
            "Aplicando IPv4 AEYLA al adaptador TX · salida física desarmada");
    DWORD exit_code = ERROR_GEN_FAILURE;
    if(!launch_elevated_helper(*helper, request_path, exit_code, error)) {
      Publish(AeylaNetworkConfigurationState::failed, ipv4, prefix_length,
              std::move(error));
      std::error_code cleanup;
      std::filesystem::remove_all(transaction_directory, cleanup);
      mBusy.store(false, std::memory_order_release);
      return;
    }

    const auto result_text = read_small_file(result_path, error);
    if(!result_text.has_value()) {
      Publish(AeylaNetworkConfigurationState::failed, ipv4, prefix_length,
              std::move(error));
    } else {
      const auto result = aeyla::network::decode_network_change_result(
          *result_text, error);
      if(!result.has_value()) {
        Publish(AeylaNetworkConfigurationState::failed, ipv4, prefix_length,
                std::move(error));
      } else if(result->ipv4 != ipv4 ||
                result->prefix_length != prefix_length ||
                result->nonce != nonce) {
        Publish(AeylaNetworkConfigurationState::failed, ipv4, prefix_length,
                "El helper devolvió una transacción distinta de la solicitada");
      } else {
        const auto state = result->state ==
                                   aeyla::network::NetworkChangeResultState::committed
                               ? AeylaNetworkConfigurationState::committed
                               : result->state == aeyla::network::
                                                     NetworkChangeResultState::rolled_back
                                     ? AeylaNetworkConfigurationState::rolled_back
                                     : AeylaNetworkConfigurationState::failed;
        Publish(state, ipv4, prefix_length, result->message);
      }
    }
    std::error_code cleanup;
    std::filesystem::remove_all(transaction_directory, cleanup);
    (void)exit_code;
#else
    (void)adapter;
    Publish(AeylaNetworkConfigurationState::failed, ipv4, prefix_length,
            "La configuración automática de IP/submáscara no está disponible en esta plataforma");
#endif
  } catch(...) {
    Publish(AeylaNetworkConfigurationState::failed, ipv4, prefix_length,
            "Falla inesperada del configurador de red; no se habilitó la salida");
  }
  mBusy.store(false, std::memory_order_release);
}

void AeylaNetworkConfiguration::Publish(
    AeylaNetworkConfigurationState state,
    std::string ipv4,
    std::uint8_t prefix_length,
    std::string message) noexcept {
  try {
    const std::scoped_lock lock(mMutex);
    mSnapshot.state = state;
    ++mSnapshot.revision;
    mSnapshot.ipv4 = std::move(ipv4);
    mSnapshot.prefix_length = prefix_length;
    mSnapshot.message = std::move(message);
  } catch(...) {
  }
}
