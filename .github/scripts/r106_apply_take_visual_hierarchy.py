from pathlib import Path

path = Path('product/AeylaVisualDmx/AeylaMainControl.h')
text = path.read_text(encoding='utf-8')

replacements = [
    (
        '''    g.DrawText(IText(12.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               mPlug.TakeRecording()
                   ? "CANCIONES · BLOQUEADAS (GRABAR)"
                   : "CANCIONES · DOBLE CLIC RENOMBRA",
               IRECT(mSetlist.L + 12.0F, mSetlist.T + 8.0F,
                     mSetlist.R - 12.0F, mSetlist.T + 36.0F));
''',
        '''    g.DrawText(IText(12.5F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "SETLIST",
               IRECT(mSetlist.L + 12.0F, mSetlist.T + 6.0F,
                     mSetlist.R - 12.0F, mSetlist.T + 27.0F));
    g.DrawText(IText(8.4F, mPlug.TakeRecording() ? kWarn : kFaint,
                     "AeylaUI", EAlign::Near, EVAlign::Middle),
               mPlug.TakeRecording()
                   ? "BLOQUEADA · GRABANDO"
                   : "DOBLE CLIC PARA RENOMBRAR",
               IRECT(mSetlist.L + 12.0F, mSetlist.T + 25.0F,
                     mSetlist.R - 12.0F, mSetlist.T + 41.0F));
'''
    ),
    (
        '''      const bool selected = index == prepared;
      const bool onAir = static_cast<int>(index) == activeTake;
      if(selected) g.FillRoundRect(kPanelSelected, row, 5.0F);
      if(selected) g.DrawRoundRect(kAccent, row, 5.0F, nullptr, 1.0F);
      if(onAir)
        g.FillRoundRect(kDanger,
                        IRECT(row.R - 12.0F, row.T + 7.0F,
                              row.R - 6.0F, row.B - 7.0F), 3.0F);
''',
        '''      const bool selected = index == prepared;
      const bool onAir = static_cast<int>(index) == activeTake;
      if(onAir)
      {
        g.FillRoundRect(IColor(255, 48, 14, 22), row, 5.0F);
        g.DrawRoundRect(kDanger, row, 5.0F, nullptr, 1.0F);
      }
      else if(selected)
      {
        g.FillRoundRect(IColor(255, 8, 23, 31), row, 5.0F);
        g.DrawRoundRect(kCyan, row, 5.0F, nullptr, 1.0F);
      }
'''
    ),
    (
        '''      g.DrawText(IText(12.0F, selected ? kAccent : kFaint,
                       "AeylaUI", EAlign::Near, EVAlign::Middle),
                 number, IRECT(row.L + 8.0F, row.T, row.L + 34.0F, row.B));
      std::string name = mPlug.SongName(index);
      if(name.empty()) name = "Canción";
      g.DrawText(IText(12.0F, selected ? kText : kMuted,
                       "AeylaUI", EAlign::Near, EVAlign::Middle),
                 name.c_str(), IRECT(row.L + 38.0F, row.T,
                                     row.R - (onAir ? 18.0F : 8.0F), row.B));
''',
        '''      const IColor rowAccent = onAir ? kDanger : (selected ? kCyan : kFaint);
      g.DrawText(IText(12.0F, rowAccent,
                       "AeylaUI", EAlign::Near, EVAlign::Middle),
                 number, IRECT(row.L + 8.0F, row.T, row.L + 34.0F, row.B));
      std::string name = mPlug.SongName(index);
      if(name.empty()) name = "Canción";
      g.DrawText(IText(12.0F, (onAir || selected) ? kText : kMuted,
                       "AeylaUI", EAlign::Near, EVAlign::Middle),
                 name.c_str(), IRECT(row.L + 38.0F, row.T,
                                     row.R - 64.0F, row.B));
      if(onAir)
        g.DrawText(IText(7.8F, kDanger, "AeylaUI", EAlign::Far,
                         EVAlign::Middle),
                   "AL AIRE", row.GetPadded(-7.0F));
      else if(selected)
        g.DrawText(IText(7.8F, kCyan, "AeylaUI", EAlign::Far,
                         EVAlign::Middle),
                   "PREP", row.GetPadded(-7.0F));
'''
    ),
    (
        '''    g.DrawText(IText(18.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               title.c_str(),
               IRECT(work.L, work.T, work.R,
                     work.T + (compact ? 28.0F : 34.0F)));
    const std::string editorStatus = compact && !mMessage.empty()
        ? mMessage
        : mPlug.ActiveTakeStatus();
    g.DrawText(IText(12.0F,
                     mPlug.TakeRecording() ? kDanger :
                         (compact && !mMessage.empty() ? kWarn : kMuted),
                     "AeylaUI", EAlign::Near, EVAlign::Middle),
               editorStatus.c_str(),
               IRECT(work.L, work.T + (compact ? 29.0F : 38.0F),
                     work.R, work.T + (compact ? 53.0F : 66.0F)));
''',
        '''    g.DrawText(IText(18.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               title.c_str(),
               IRECT(work.L, work.T, work.R - 140.0F,
                     work.T + (compact ? 28.0F : 34.0F)));
    const std::string editorStatus = compact && !mMessage.empty()
        ? mMessage
        : mPlug.ActiveTakeStatus();
    const bool recordingNow = mPlug.TakeRecording();
    const bool playingNow = !recordingNow && mPlug.TakePlaying();
    const IColor stateColor = recordingNow ? kDanger :
        (playingNow ? kGood : (compact && !mMessage.empty() ? kWarn : kMuted));
    const char* stateLabel = recordingNow ? "REC" :
        (playingNow ? "PLAY" : "TOMA");
    const IRECT statePill(work.R - 112.0F, work.T + 2.0F,
                          work.R, work.T + 27.0F);
    g.FillRoundRect(IColor(255, 11, 13, 18), statePill, 5.0F);
    g.DrawRoundRect(stateColor, statePill, 5.0F, nullptr, 1.0F);
    g.DrawText(IText(9.5F, stateColor, "AeylaUI", EAlign::Center,
                     EVAlign::Middle), stateLabel, statePill);
    g.DrawText(IText(11.0F, stateColor,
                     "AeylaUI", EAlign::Near, EVAlign::Middle),
               editorStatus.c_str(),
               IRECT(work.L, work.T + (compact ? 29.0F : 38.0F),
                     work.R, work.T + (compact ? 53.0F : 66.0F)));
'''
    ),
    (
        '''    g.FillRoundRect(IColor(255, 9, 11, 15), mTimeline, 5.0F);
    g.DrawRoundRect(kLine, mTimeline, 5.0F, nullptr, 1.0F);
''',
        '''    g.FillRoundRect(IColor(255, 9, 11, 15), mTimeline, 6.0F);
    g.DrawRoundRect(editor.available ? kCyan : kLine,
                    mTimeline, 6.0F, nullptr, editor.available ? 1.2F : 1.0F);
'''
    ),
    (
        '''    Button(g, mRecordButton,
           mPlug.TakeRecording() ? "DETENER + GUARDAR TOMA" :
               (recordBlocked ? "GRABACIÓN BLOQUEADA" : "GRABAR NUEVA TOMA"),
''',
        '''    Button(g, mRecordButton,
           mPlug.TakeRecording() ? "REC · DETENER + GUARDAR" :
               (recordBlocked ? "REC · BLOQUEADO" : "REC · NUEVA TOMA"),
'''
    ),
    (
        '''    Button(g, mPlayButton,
           mPlug.TakePlaying() ? "REPRODUCIENDO" :
               (playBlocked ? "REPRODUCCIÓN BLOQUEADA" : "REPRODUCIR TOMA ACTIVA"),
''',
        '''    Button(g, mPlayButton,
           mPlug.TakePlaying() ? "PLAY · REPRODUCIENDO" :
               (playBlocked ? "PLAY · BLOQUEADO" : "PLAY / PAUSA"),
'''
    ),
    (
        '''    Button(g, mStopButton, "DETENER / MANTENER", kPanelRaised, kLineStrong);
''',
        '''    Button(g, mStopButton, "HOLD / STOP", kPanelRaised, kWarn, kWarn);
'''
    ),
    (
        '''               "RED Y SALIDA ART-NET · UNIVERSO 1",
''',
        '''               "SISTEMA · RED / SALIDA ART-NET · UNIVERSO 1",
'''
    ),
]

for old, new in replacements:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f'Expected exactly one match, found {count}: {old[:80]!r}')
    text = text.replace(old, new, 1)

path.write_text(text, encoding='utf-8')
