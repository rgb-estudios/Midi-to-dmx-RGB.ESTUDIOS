from pathlib import Path

path = Path('product/AeylaVisualDmx/AeylaShowPlayerIntegration.cpp')
text = path.read_text(encoding='utf-8')

old_transport = '''  const auto current = mTakeScheduler.status();
  if(current.playing)
  {
    std::string error;
    if(!mTakeScheduler.pause(error))
      return {false, {}, "PAUSA · se mantiene el último estado DMX"};
    return {true, {}, "PAUSA · se mantiene el último estado DMX"};
  }
  if(current.paused)
  {
    std::string error;
    if(!mTakeScheduler.resume(error))
      return {false, {}, error};
    return {true, {}, "REANUDAR · continúa desde el mismo punto"};
  }
'''
# Correct the exact historical snippet: the failed pause path returns the raw error.
old_transport = old_transport.replace('return {false, {}, "PAUSA · se mantiene el último estado DMX"};\n    return {true, {}, "PAUSA · se mantiene el último estado DMX"};',
                                      'return {false, {}, error};\n    return {true, {}, "PAUSA · se mantiene el último estado DMX"};')
new_transport = '''  const auto current = mTakeScheduler.status();
  const std::size_t preparedSongIndex = ActiveSongIndex();
  const int activeLiveSongIndex = mActiveTakeSongIndex.load(
      std::memory_order_acquire);
  const bool preparedIsCurrentLiveSong =
      activeLiveSongIndex >= 0 &&
      static_cast<std::size_t>(activeLiveSongIndex) == preparedSongIndex;

  // PLAY/PAUSA only toggles transport when the prepared Song is already the
  // Song AL AIRE. If another Song is PREPARADA, this button is a GO and must
  // atomically replace the live reader without withdrawing Art-Net authority.
  if(current.playing && preparedIsCurrentLiveSong)
  {
    std::string error;
    if(!mTakeScheduler.pause(error))
      return {false, {}, error};
    return {true, {}, "PAUSA · se mantiene el último estado DMX"};
  }
  if(current.paused && preparedIsCurrentLiveSong)
  {
    std::string error;
    if(!mTakeScheduler.resume(error))
      return {false, {}, error};
    return {true, {}, "REANUDAR · continúa desde el mismo punto"};
  }
'''
if text.count(old_transport) != 1:
    raise SystemExit(f'transport block mismatch: {text.count(old_transport)}')
text = text.replace(old_transport, new_transport)

old_load = '''  if(!sameValidatedClip)
  {
    // Loading is intentionally destructive to output authority, so it is done
    // only when the selected file really changed. The normal operator order
    // ARM -> PLAY must preserve the arm established by the first action.
    mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
    if(!mTakeScheduler.load_take_file(selected.path, GetSampleRate(), error))
      return {false, {}, "La toma no superó la validación · " + error};
    if(edited.has_value() && edited->path == selected.path &&
       !mTakeScheduler.set_play_range(
           static_cast<std::size_t>(edited->start_frame),
           static_cast<std::size_t>(edited->end_frame_exclusive), error))
      return {false, {}, "El rango ENTRADA / SALIDA no pudo cargarse · " + error};
    aeyla::take_library_session::set_loaded_path(this, songId, selected.path);
    mLoadedTakeSongIndex.store(static_cast<int>(songIndex),
                               std::memory_order_release);
    aeyla::take_library_session::set_storage_message(
        this, "CARGADA DESDE DISCO · " + selected.path.filename().string());
  }

  // El botón manual debe continuar aunque REAPER cierre su dispositivo de
  // audio al perder foco. Los futuros disparos DAW/MIDI conservarán el reloj
  // por muestras del host mediante DmxClipClockSource::host_samples.
  if(!mTakeScheduler.play(
         error, aeyla::capture::DmxClipClockSource::monotonic_realtime))
    return {false, {}, error};
'''
new_load = '''  bool startedByAtomicReplace = false;
  if(!sameValidatedClip)
  {
    mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
    if(scheduler.armed)
    {
      // Validate the prepared file beside the live reader. The old Song keeps
      // its held frame/carrier until replace_armed_take_file() commits one
      // reader swap. A validation/swap failure leaves the previous Song live.
      aeyla::capture::DmxTakeFileReader candidate;
      if(!candidate.open(selected.path, error))
        return {false, {}, "GO cancelado · la canción anterior sigue AL AIRE · " + error};
      const auto info = candidate.info();
      if(!info.open || info.song_id != songId ||
         info.port_address != outputUniverse ||
         expectedStart >= expectedEnd || expectedEnd > info.frame_count)
        return {false, {},
                "GO cancelado · la toma preparada no coincide con canción, universo o ENTRADA/SALIDA · la anterior sigue AL AIRE"};

      if(!mTakeScheduler.replace_armed_take_file(
             candidate, GetSampleRate(),
             static_cast<std::size_t>(expectedStart),
             static_cast<std::size_t>(expectedEnd),
             aeyla::capture::DmxClipClockSource::monotonic_realtime,
             0U, error))
        return {false, {},
                "GO cancelado · la canción anterior sigue AL AIRE · " + error};
      startedByAtomicReplace = true;
    }
    else
    {
      if(!mTakeScheduler.load_take_file(selected.path, GetSampleRate(), error))
        return {false, {}, "La toma no superó la validación · " + error};
      if(edited.has_value() && edited->path == selected.path &&
         !mTakeScheduler.set_play_range(
             static_cast<std::size_t>(edited->start_frame),
             static_cast<std::size_t>(edited->end_frame_exclusive), error))
        return {false, {}, "El rango ENTRADA / SALIDA no pudo cargarse · " + error};
    }
    aeyla::take_library_session::set_loaded_path(this, songId, selected.path);
    mLoadedTakeSongIndex.store(static_cast<int>(songIndex),
                               std::memory_order_release);
    aeyla::take_library_session::set_storage_message(
        this, startedByAtomicReplace
            ? "GO SIN CORTE · " + selected.path.filename().string()
            : "CARGADA DESDE DISCO · " + selected.path.filename().string());
  }

  // El botón manual debe continuar aunque REAPER cierre su dispositivo de
  // audio al perder foco. Los futuros disparos DAW/MIDI conservarán el reloj
  // por muestras del host mediante DmxClipClockSource::host_samples.
  if(!startedByAtomicReplace &&
     !mTakeScheduler.play(
         error, aeyla::capture::DmxClipClockSource::monotonic_realtime))
    return {false, {}, error};
'''
if text.count(old_load) != 1:
    raise SystemExit(f'load block mismatch: {text.count(old_load)}')
text = text.replace(old_load, new_load)

path.write_text(text, encoding='utf-8')
print('manual GO atomic replacement patch applied')
