#include "network/network_interfaces.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

namespace aeyla::network {
namespace {

#ifdef _WIN32
std::string wide_to_utf8(const wchar_t* text) {
  if(text == nullptr || *text == L'\0') return {};
  const int required = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0,
                                           nullptr, nullptr);
  if(required <= 1) return {};
  std::string result(static_cast<std::size_t>(required), '\0');
  const int converted = WideCharToMultiByte(CP_UTF8, 0, text, -1,
                                            result.data(), required,
                                            nullptr, nullptr);
  if(converted <= 1) return {};
  result.resize(static_cast<std::size_t>(converted - 1));
  return result;
}
#endif

void normalize(std::vector<NetworkInterface>& interfaces) {
  // Loopback is useful in automated socket tests but is never a valid
  // operator-facing Art-Net show route. Exposing 127.0.0.1 in the same selector
  // as physical NICs allowed the R03 UI to enter a perfectly valid local socket
  // state that could never reach external lighting hardware.
  interfaces.erase(
      std::remove_if(interfaces.begin(), interfaces.end(),
                     [](const NetworkInterface& item) {
                       return item.loopback || item.ipv4.rfind("127.", 0U) == 0U;
                     }),
      interfaces.end());

  std::sort(interfaces.begin(), interfaces.end(),
            [](const NetworkInterface& a, const NetworkInterface& b) {
              if(a.name != b.name) return a.name < b.name;
              return a.ipv4 < b.ipv4;
            });
  interfaces.erase(
      std::unique(interfaces.begin(), interfaces.end(),
                  [](const NetworkInterface& a, const NetworkInterface& b) {
                    return a.id == b.id && a.ipv4 == b.ipv4;
                  }),
      interfaces.end());
}

#ifndef _WIN32
std::uint8_t prefix_from_mask(const sockaddr_in* mask) noexcept {
  if(mask == nullptr) return 0U;
  std::uint32_t bits = ntohl(mask->sin_addr.s_addr);
  std::uint8_t prefix = 0U;
  while((bits & 0x80000000U) != 0U) {
    ++prefix;
    bits <<= 1U;
  }
  return prefix;
}
#endif

}  // namespace

std::vector<NetworkInterface> enumerate_ipv4_interfaces() {
  std::vector<NetworkInterface> result;

#ifdef _WIN32
  WSADATA winsock{};
  if(WSAStartup(MAKEWORD(2, 2), &winsock) != 0)
    return result;

  ULONG buffer_size = 16U * 1024U;
  std::vector<unsigned char> storage(buffer_size);
  auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
  ULONG status = GetAdaptersAddresses(
      AF_INET, GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST |
                   GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
      nullptr, adapters, &buffer_size);
  if(status == ERROR_BUFFER_OVERFLOW) {
    storage.resize(buffer_size);
    adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
    status = GetAdaptersAddresses(
        AF_INET, GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST |
                     GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        nullptr, adapters, &buffer_size);
  }
  if(status == NO_ERROR) {
    for(auto* adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
      if(adapter->OperStatus != IfOperStatusUp) continue;
      const bool loopback = adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK;
      std::string name = wide_to_utf8(adapter->FriendlyName);
      if(name.empty() && adapter->AdapterName != nullptr)
        name = adapter->AdapterName;
      const std::string id = adapter->AdapterName == nullptr
                                 ? name
                                 : std::string(adapter->AdapterName);

      for(auto* address = adapter->FirstUnicastAddress;
          address != nullptr; address = address->Next) {
        if(address->Address.lpSockaddr == nullptr ||
           address->Address.lpSockaddr->sa_family != AF_INET)
          continue;
        const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(
            address->Address.lpSockaddr);
        std::array<char, INET_ADDRSTRLEN> text{};
        if(inet_ntop(AF_INET, &ipv4->sin_addr, text.data(), text.size()) == nullptr)
          continue;
        result.push_back({id, name, text.data(),
                          static_cast<std::uint8_t>(
                              std::min<ULONG>(address->OnLinkPrefixLength, 32U)),
                          loopback});
      }
    }
  }
  (void)WSACleanup();
#else
  ifaddrs* addresses = nullptr;
  if(getifaddrs(&addresses) != 0 || addresses == nullptr)
    return result;

  for(const ifaddrs* item = addresses; item != nullptr; item = item->ifa_next) {
    if(item->ifa_addr == nullptr || item->ifa_addr->sa_family != AF_INET)
      continue;
    if((item->ifa_flags & IFF_UP) == 0) continue;

    const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(item->ifa_addr);
    std::array<char, INET_ADDRSTRLEN> text{};
    if(inet_ntop(AF_INET, &ipv4->sin_addr, text.data(), text.size()) == nullptr)
      continue;

    const auto* mask = item->ifa_netmask == nullptr
                           ? nullptr
                           : reinterpret_cast<const sockaddr_in*>(item->ifa_netmask);
    const bool loopback = (item->ifa_flags & IFF_LOOPBACK) != 0;
    const std::string name = item->ifa_name == nullptr ? "IPv4" : item->ifa_name;
    result.push_back({name, name, text.data(), prefix_from_mask(mask), loopback});
  }
  freeifaddrs(addresses);
#endif

  normalize(result);
  return result;
}

}  // namespace aeyla::network
