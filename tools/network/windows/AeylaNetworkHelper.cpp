#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "network/ipv4_configuration.h"
#include "network/network_change_protocol.h"
#include "network/network_interfaces.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using aeyla::network::NetworkChangeRequest;
using aeyla::network::NetworkChangeResult;
using aeyla::network::NetworkChangeResultState;

constexpr std::uintmax_t kMaximumRequestBytes = 4096U;

std::optional<std::filesystem::path> scoped_request_path(
    const std::filesystem::path& requested) {
  std::error_code error;
  const auto root = std::filesystem::weakly_canonical(
      std::filesystem::temp_directory_path() / L"RGB-Estudios" /
          L"AEYLA" / L"network",
      error);
  if(error) return std::nullopt;
  const auto normalized = std::filesystem::weakly_canonical(requested, error);
  if(error || normalized.extension() != L".aeylanetreq")
    return std::nullopt;
  const auto relative = std::filesystem::relative(normalized, root, error);
  if(error || relative.empty() || relative.is_absolute())
    return std::nullopt;
  for(const auto& component : relative) {
    if(component == L"..") return std::nullopt;
  }
  return normalized;
}

std::filesystem::path result_path_for(const std::filesystem::path& request) {
  auto result = request;
  result.replace_extension(".aeylanetresult");
  return result;
}

bool write_result(const std::filesystem::path& destination,
                  const NetworkChangeResult& result) {
  std::error_code error;
  std::filesystem::create_directories(destination.parent_path(), error);
  if(error) return false;
  auto temporary = destination;
  temporary += L".tmp";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if(!output) return false;
    const auto encoded = aeyla::network::encode_network_change_result(result);
    output.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
    output.flush();
    if(!output) return false;
  }
  std::filesystem::remove(destination, error);
  error.clear();
  std::filesystem::rename(temporary, destination, error);
  return !error;
}

std::optional<std::string> read_request(const std::filesystem::path& path,
                                        std::string& error_message) {
  std::error_code error;
  if(!std::filesystem::is_regular_file(path, error) || error) {
    error_message = "No se encontró la solicitud de cambio de red";
    return std::nullopt;
  }
  const auto size = std::filesystem::file_size(path, error);
  if(error || size == 0U || size > kMaximumRequestBytes) {
    error_message = "La solicitud de cambio de red tiene un tamaño inválido";
    return std::nullopt;
  }
  std::ifstream input(path, std::ios::binary);
  if(!input) {
    error_message = "No se pudo leer la solicitud de cambio de red";
    return std::nullopt;
  }
  std::string text(static_cast<std::size_t>(size), '\0');
  input.read(text.data(), static_cast<std::streamsize>(text.size()));
  if(!input) {
    error_message = "La solicitud de cambio de red quedó incompleta";
    return std::nullopt;
  }
  return text;
}

bool process_is_elevated() noexcept {
  HANDLE token = nullptr;
  if(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
  TOKEN_ELEVATION elevation{};
  DWORD returned = 0U;
  const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation,
                                      sizeof(elevation), &returned);
  CloseHandle(token);
  return ok && elevation.TokenIsElevated != 0U;
}

std::wstring system_netsh() {
  std::vector<wchar_t> buffer(MAX_PATH, L'\0');
  const UINT length = GetSystemDirectoryW(buffer.data(),
                                          static_cast<UINT>(buffer.size()));
  if(length == 0U || length >= buffer.size()) return L"netsh.exe";
  std::filesystem::path path(std::wstring(buffer.data(), length));
  return (path / L"netsh.exe").wstring();
}

bool run_netsh(const std::vector<std::wstring>& arguments,
               std::string& error_message) {
  const std::wstring executable = system_netsh();
  std::wstring command = L"\"" + executable + L"\"";
  for(const auto& argument : arguments) {
    command.push_back(L' ');
    command += argument;
  }
  std::vector<wchar_t> mutable_command(command.begin(), command.end());
  mutable_command.push_back(L'\0');

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if(!CreateProcessW(executable.c_str(), mutable_command.data(), nullptr,
                     nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                     &startup, &process)) {
    error_message = "No se pudo iniciar netsh · código " +
                    std::to_string(GetLastError());
    return false;
  }
  const DWORD wait = WaitForSingleObject(process.hProcess, 15000U);
  DWORD exit_code = ERROR_GEN_FAILURE;
  if(wait == WAIT_OBJECT_0)
    (void)GetExitCodeProcess(process.hProcess, &exit_code);
  else
    (void)TerminateProcess(process.hProcess, ERROR_TIMEOUT);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  if(wait != WAIT_OBJECT_0) {
    error_message = "Windows agotó el tiempo al configurar la red";
    return false;
  }
  if(exit_code != 0U) {
    error_message = "Windows rechazó el cambio IPv4 · netsh " +
                    std::to_string(exit_code);
    return false;
  }
  return true;
}

bool adapter_matches(const aeyla::network::NetworkInterface& adapter,
                     const NetworkChangeRequest& request) noexcept {
  return adapter.interface_index == request.adapter_index &&
         adapter.id == request.adapter_id;
}

bool address_present(const NetworkChangeRequest& request,
                     bool& present_on_other_adapter,
                     bool& wireless) {
  present_on_other_adapter = false;
  wireless = false;
  bool present = false;
  bool identity_seen = false;
  for(const auto& adapter : aeyla::network::enumerate_ipv4_interfaces()) {
    if(adapter_matches(adapter, request)) {
      identity_seen = true;
      wireless = wireless || adapter.wireless;
      if(adapter.ipv4 == request.ipv4 &&
         adapter.prefix_length == request.prefix_length)
        present = true;
    } else if(adapter.ipv4 == request.ipv4) {
      present_on_other_adapter = true;
    }
  }
  return identity_seen && present;
}

bool adapter_identity_exists(const NetworkChangeRequest& request,
                             bool& wireless) {
  wireless = false;
  for(const auto& adapter : aeyla::network::enumerate_ipv4_interfaces()) {
    if(!adapter_matches(adapter, request)) continue;
    wireless = wireless || adapter.wireless;
    return true;
  }
  return false;
}

bool bind_test(std::string_view ipv4, std::string& error_message) {
  WSADATA data{};
  if(WSAStartup(MAKEWORD(2, 2), &data) != 0) {
    error_message = "No se pudo iniciar Winsock para validar la IPv4";
    return false;
  }
  const SOCKET socket_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(socket_handle == INVALID_SOCKET) {
    WSACleanup();
    error_message = "No se pudo crear el socket de validación IPv4";
    return false;
  }
  sockaddr_in local{};
  local.sin_family = AF_INET;
  local.sin_port = 0U;
  const std::string address(ipv4);
  const bool parsed = inet_pton(AF_INET, address.c_str(), &local.sin_addr) == 1;
  const bool bound = parsed &&
      bind(socket_handle, reinterpret_cast<const sockaddr*>(&local),
           sizeof(local)) == 0;
  closesocket(socket_handle);
  WSACleanup();
  if(!bound)
    error_message = "La nueva IPv4 existe, pero Windows no permite ligar Art-Net a ella";
  return bound;
}

bool wait_for_address(const NetworkChangeRequest& request,
                      std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while(std::chrono::steady_clock::now() < deadline) {
    bool elsewhere = false;
    bool wireless = false;
    if(address_present(request, elsewhere, wireless)) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  bool elsewhere = false;
  bool wireless = false;
  return address_present(request, elsewhere, wireless);
}

bool add_address(const NetworkChangeRequest& request,
                 const aeyla::network::Ipv4Network& network,
                 std::string& error_message) {
  return run_netsh(
      {L"interface", L"ipv4", L"add", L"address",
       L"name=" + std::to_wstring(request.adapter_index),
       L"address=" + std::filesystem::path(network.address).wstring(),
       L"mask=" + std::filesystem::path(network.mask).wstring(),
       L"store=persistent"},
      error_message);
}

bool remove_address(const NetworkChangeRequest& request,
                    std::string& error_message) {
  return run_netsh(
      {L"interface", L"ipv4", L"delete", L"address",
       L"name=" + std::to_wstring(request.adapter_index),
       L"address=" + std::filesystem::path(request.ipv4).wstring(),
       L"store=persistent"},
      error_message);
}

int fail(const std::filesystem::path& result_path,
         std::string message,
         const NetworkChangeRequest* request = nullptr,
         NetworkChangeResultState state = NetworkChangeResultState::failed) {
  NetworkChangeResult result;
  result.state = state;
  if(request != nullptr) {
    result.ipv4 = request->ipv4;
    result.prefix_length = request->prefix_length;
    result.nonce = request->nonce;
  }
  result.message = std::move(message);
  (void)write_result(result_path, result);
  return state == NetworkChangeResultState::rolled_back ? 3 : 2;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if(argc != 3 || std::wstring_view(argv[1]) != L"--request") return 64;
  const auto scoped_request = scoped_request_path(argv[2]);
  if(!scoped_request.has_value()) return 65;
  const std::filesystem::path request_path = *scoped_request;
  const auto result_path = result_path_for(request_path);

  if(!process_is_elevated())
    return fail(result_path,
                "El helper requiere la confirmación UAC de Windows");

  std::string error;
  const auto request_text = read_request(request_path, error);
  if(!request_text.has_value()) return fail(result_path, std::move(error));
  const auto request = aeyla::network::decode_network_change_request(
      *request_text, error);
  if(!request.has_value()) return fail(result_path, std::move(error));
  const std::wstring expected_nonce(request->nonce.begin(), request->nonce.end());
  if(request_path.stem().wstring() != expected_nonce ||
     request_path.parent_path().filename().wstring() != expected_nonce)
    return fail(result_path,
                "La identidad de la solicitud no coincide con su ruta temporal",
                &*request);

  const auto network = aeyla::network::make_ipv4_network(
      request->ipv4, request->prefix_length, error);
  if(!network.has_value())
    return fail(result_path, std::move(error), &*request);

  bool wireless = false;
  if(!adapter_identity_exists(*request, wireless))
    return fail(result_path,
                "El adaptador seleccionado cambió o ya no está conectado",
                &*request);
  if(wireless)
    return fail(result_path,
                "AEYLA no aplica una IPv4 de show sobre Wi-Fi; selecciona Ethernet",
                &*request);

  bool elsewhere = false;
  bool ignored_wireless = false;
  const bool already_present =
      address_present(*request, elsewhere, ignored_wireless);
  if(elsewhere)
    return fail(result_path,
                "La IPv4 solicitada ya pertenece a otro adaptador de Windows",
                &*request);

  bool added = false;
  if(!already_present) {
    if(!add_address(*request, *network, error))
      return fail(result_path, std::move(error), &*request);
    added = true;
  }

  if(!wait_for_address(*request, std::chrono::seconds(5)) ||
     !bind_test(request->ipv4, error)) {
    if(added) {
      std::string rollback_error;
      const bool rolled_back = remove_address(*request, rollback_error);
      return fail(result_path,
                  (error.empty() ? "Windows no confirmó la nueva IPv4" : error) +
                      (rolled_back
                           ? " · cambio retirado automáticamente"
                           : " · ADVERTENCIA: no fue posible retirar el cambio: " +
                                 rollback_error),
                  &*request,
                  rolled_back ? NetworkChangeResultState::rolled_back
                              : NetworkChangeResultState::failed);
    }
    return fail(result_path,
                error.empty() ? "La IPv4 existente no superó la validación"
                              : std::move(error),
                &*request);
  }

  NetworkChangeResult result;
  result.state = NetworkChangeResultState::committed;
  result.ipv4 = request->ipv4;
  result.prefix_length = request->prefix_length;
  result.nonce = request->nonce;
  result.message = already_present
      ? "La IPv4 AEYLA ya estaba configurada y superó la validación"
      : "IPv4 AEYLA agregada al adaptador y validada; la red anterior se conservó";
  if(!write_result(result_path, result)) return 5;
  return 0;
}

#else
int main() { return 64; }
#endif
