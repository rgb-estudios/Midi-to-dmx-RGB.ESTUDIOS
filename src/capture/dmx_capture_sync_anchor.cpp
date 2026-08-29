#include "capture/dmx_capture_sync_anchor.h"

#include <algorithm>

namespace aeyla::capture {

void DmxCaptureSyncAnchor::begin(
    const runtime::HostTransportSnapshot& initial_host) noexcept {
  const std::scoped_lock lock(mutex_);
  anchor_frame_ = 0U;
  source_ = DmxCaptureSyncSource::none;
  state_ = initial_host.revision != 0U && !initial_host.rendering_offline &&
                   !initial_host.running
               ? DmxCaptureSyncState::waiting_for_transport
               : DmxCaptureSyncState::unavailable;
}

bool DmxCaptureSyncAnchor::observe(
    const runtime::HostTransportSnapshot& host,
    std::uint64_t recorded_frames) noexcept {
  const std::scoped_lock lock(mutex_);
  if(state_ != DmxCaptureSyncState::waiting_for_transport ||
     host.revision == 0U || host.rendering_offline || !host.running)
    return false;

  anchor_frame_ = recorded_frames;
  source_ = DmxCaptureSyncSource::transport_start;
  state_ = DmxCaptureSyncState::anchored;
  return true;
}

bool DmxCaptureSyncAnchor::anchor_transport_snapshot(
    std::uint64_t recorded_frames) noexcept {
  const std::scoped_lock lock(mutex_);
  if(state_ == DmxCaptureSyncState::idle ||
     source_ == DmxCaptureSyncSource::show_midi_marker ||
     source_ == DmxCaptureSyncSource::transport_start)
    return false;

  anchor_frame_ = recorded_frames;
  source_ = DmxCaptureSyncSource::transport_start;
  state_ = DmxCaptureSyncState::anchored;
  return true;
}

bool DmxCaptureSyncAnchor::anchor_explicit(
    std::uint64_t recorded_frames) noexcept {
  const std::scoped_lock lock(mutex_);
  if(state_ == DmxCaptureSyncState::idle ||
     source_ == DmxCaptureSyncSource::show_midi_marker)
    return false;

  anchor_frame_ = recorded_frames;
  source_ = DmxCaptureSyncSource::show_midi_marker;
  state_ = DmxCaptureSyncState::anchored;
  return true;
}

void DmxCaptureSyncAnchor::reset() noexcept {
  const std::scoped_lock lock(mutex_);
  state_ = DmxCaptureSyncState::idle;
  anchor_frame_ = 0U;
  source_ = DmxCaptureSyncSource::none;
}

DmxCaptureSyncStatus DmxCaptureSyncAnchor::status() const noexcept {
  const std::scoped_lock lock(mutex_);
  return {state_, anchor_frame_, source_};
}

std::optional<std::uint64_t> DmxCaptureSyncAnchor::resolved_anchor(
    std::uint64_t total_frames) const noexcept {
  const std::scoped_lock lock(mutex_);
  if(state_ != DmxCaptureSyncState::anchored || total_frames < 2U)
    return std::nullopt;
  return std::min(anchor_frame_, total_frames - 2U);
}

}  // namespace aeyla::capture
