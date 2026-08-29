from pathlib import Path

visual = Path("product/AeylaVisualDmx/AeylaVisualDmx.cpp")
text = visual.read_text(encoding="utf-8")

old_restore = '''  if(uuidMismatch || schemaMismatch || checksumMismatch)
  {
    // The Set identifies a project that is not the currently loaded package.
    // Until locator-based asynchronous loading is implemented, expose an
    // invalid project and publish only the safe frame.
    mModel.set_project_valid(false);
    mModel.set_blackout(true);
    mParamBlackout.store(true, std::memory_order_release);
    mHostStateRestoreErrors.fetch_add(1U, std::memory_order_relaxed);
    const std::scoped_lock stateLock(mHostStateMutex);
    mHostStateCache.song_bindings.clear();
  }

  // UnserializeParams already restored host-visible preferences. Applying them
'''
new_restore = '''  if(uuidMismatch || schemaMismatch || checksumMismatch)
  {
    // The Set identifies a project that is not the currently loaded package.
    // Until locator-based asynchronous loading is implemented, expose an
    // invalid project and publish only the safe frame. Never carry Take
    // bindings from a different project identity into the current session.
    mModel.set_project_valid(false);
    mModel.set_blackout(true);
    mParamBlackout.store(true, std::memory_order_release);
    mHostStateRestoreErrors.fetch_add(1U, std::memory_order_relaxed);
    {
      const std::scoped_lock stateLock(mHostStateMutex);
      mHostStateCache.song_bindings.clear();
      mHostStateCache.take_library_locator.clear();
      mHostStateCache.take_bindings.clear();
    }
    aeyla::take_library_session::clear(this);
  }
  else
  {
    // State 1.3 restores only non-destructive Take/library state. This never
    // arms Art-Net and never starts playback. A missing cross-platform path
    // remains pending until the operator selects the destination library once.
    aeyla::take_library_session::stage_persisted_state(
        this, mModel.project_document().project_id,
        pending->take_library_locator, pending->take_bindings);
  }

  // UnserializeParams already restored host-visible preferences. Applying them
'''
if text.count(old_restore) != 1:
    raise SystemExit("restore anchor mismatch")
text = text.replace(old_restore, new_restore)

old_refresh = '''void AeylaVisualDmx::RefreshHostStateCacheLocked()
{
  const auto& snapshot = mModel.snapshot();
  if(!snapshot.project_valid)
    return;

  std::array<std::uint8_t, 16> uuid{};
  if(!DecodeCanonicalUuid(snapshot.project_id, uuid))
    return;

  const std::scoped_lock lock(mHostStateMutex);
  if(mHostStateCache.project_uuid != uuid)
  {
    mHostStateCache.project_checksum.fill(0U);
    mHostStateCache.locator_mode = aeyla::runtime::ProjectLocatorMode::none;
    mHostStateCache.project_locator.clear();
    mHostStateCache.song_bindings.clear();
  }
  mHostStateCache.project_uuid = uuid;
  mHostStateCache.project_schema_major =
      mModel.project_document().schema_version.major;
  mHostStateCache.project_schema_minor =
      mModel.project_document().schema_version.minor;
  mHostStateCache.grand_master = snapshot.grand_master;
  // Persist the global operator/safety latch, never a transient artistic
  // blackout caused by a missing/out-of-range Cue.
  mHostStateCache.blackout = snapshot.global_blackout;
}
'''
new_refresh = '''void AeylaVisualDmx::RefreshHostStateCacheLocked()
{
  const auto& snapshot = mModel.snapshot();
  if(!snapshot.project_valid)
    return;

  std::array<std::uint8_t, 16> uuid{};
  if(!DecodeCanonicalUuid(snapshot.project_id, uuid))
    return;

  // Snapshot Take state before taking mHostStateMutex to keep lock order
  // deterministic. Only the 15 show Songs can enter host component state.
  aeyla::take_library_session::ensure_scope(this, snapshot.project_id);
  std::vector<std::string> songIds;
  const auto& show = mModel.show_program();
  songIds.reserve(std::min(
      show.songs.size(), aeyla::runtime::kMaxSessionTakeBindings));
  for(const auto& song : show.songs)
  {
    if(songIds.size() >= aeyla::runtime::kMaxSessionTakeBindings)
      break;
    songIds.push_back(song.song_id);
  }
  auto hostTakeState =
      aeyla::take_library_session::snapshot_for_host(this, songIds);

  const std::scoped_lock lock(mHostStateMutex);
  if(mHostStateCache.project_uuid != uuid)
  {
    mHostStateCache.project_checksum.fill(0U);
    mHostStateCache.locator_mode = aeyla::runtime::ProjectLocatorMode::none;
    mHostStateCache.project_locator.clear();
    mHostStateCache.song_bindings.clear();
    mHostStateCache.take_library_locator.clear();
    mHostStateCache.take_bindings.clear();
  }
  mHostStateCache.project_uuid = uuid;
  mHostStateCache.project_schema_major =
      mModel.project_document().schema_version.major;
  mHostStateCache.project_schema_minor =
      mModel.project_document().schema_version.minor;
  mHostStateCache.grand_master = snapshot.grand_master;
  // Persist the global operator/safety latch, never a transient artistic
  // blackout caused by a missing/out-of-range Cue.
  mHostStateCache.blackout = snapshot.global_blackout;
  mHostStateCache.take_library_locator =
      std::move(hostTakeState.library_locator);
  mHostStateCache.take_bindings = std::move(hostTakeState.bindings);
}
'''
if text.count(old_refresh) != 1:
    raise SystemExit("refresh anchor mismatch")
visual.write_text(text.replace(old_refresh, new_refresh), encoding="utf-8")

session = Path("product/AeylaVisualDmx/AeylaTakeLibrarySession.cpp")
session_text = session.read_text(encoding="utf-8")
if "#include <iterator>\n" not in session_text:
    if session_text.count("#include <algorithm>\n") != 1:
        raise SystemExit("include anchor mismatch")
    session_text = session_text.replace(
        "#include <algorithm>\n",
        "#include <algorithm>\n#include <iterator>\n",
        1,
    )

old_u8 = "      candidate = std::filesystem::u8path(savedLocator).lexically_normal();\n"
new_u8 = '''      std::u8string locatorUtf8;
      locatorUtf8.reserve(savedLocator.size());
      for(const unsigned char byte : savedLocator)
        locatorUtf8.push_back(static_cast<char8_t>(byte));
      candidate = std::filesystem::path(locatorUtf8).lexically_normal();
'''
if session_text.count(old_u8) != 1:
    raise SystemExit("u8 locator anchor mismatch")
session.write_text(session_text.replace(old_u8, new_u8), encoding="utf-8")
