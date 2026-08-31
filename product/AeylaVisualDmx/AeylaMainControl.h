#pragma once

#include "AeylaVisualDmx.h"
#include "network/ipv4_configuration.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

class AeylaMainControl final : public IControl
{
public:
  AeylaMainControl(const IRECT& bounds, AeylaVisualDmx& plug)
  : IControl(bounds, {kParamBlackout})
  , mPlug(plug)
  {
    (void)mPlug.RefreshNetworkInterfacesFromUI();
    RestoreNetworkFieldFromSelectedTx();
  }

  void Draw(IGraphics& g) override
  {
    SyncWorkspaceFromProduct();
    BuildLayout();
    g.FillRect(kBackground, mRECT);
    // R10.6: the canonical shell/header is owned exclusively by
    // AeylaRuntimeStatusControl. MainControl renders workspace content only.
    DrawSetlist(g);
    if(mWorkspaceView == WorkspaceView::take_editor)
      DrawTakeEditor(g);
    else if(mWorkspaceView == WorkspaceView::midi_show)
      DrawMidiShow(g);
    else
      DrawRouting(g);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    BuildLayout();

    // R10.6: never leave hidden duplicate controls below the canonical shell.
    // RuntimeStatusControl owns every header hit-zone (navigation, ARM, APAGÓN).
    if(Contains(mHeader, x, y))
      return;

    const std::size_t songCount = mPlug.SongCount();
    for(std::size_t index = 0; index < songCount && index < mSongRows.size(); ++index)
    {
      if(!Contains(mSongRows[index], x, y))
        continue;
      if(!mPlug.SelectSongFromUI(index))
        mMessage = "No se puede cambiar de canción mientras GRABAR está activo.";
      SetDirty(false);
      return;
    }

    if(Contains(mNewSongButton, x, y))
    {
      Report(mPlug.CreateSongFromUI());
      SetDirty(false);
      return;
    }

    if(mWorkspaceView == WorkspaceView::take_editor)
    {
      if(Contains(mRecordButton, x, y))
      {
        Report(mPlug.ToggleTakeCaptureFromUI());
        SetDirty(false);
        return;
      }
      if(Contains(mPlayButton, x, y))
      {
        Report(mPlug.ToggleActiveTakePlaybackFromUI());
        SetDirty(false);
        return;
      }
      if(Contains(mStopButton, x, y))
      {
        mPlug.StopActiveTakePlaybackFromUI();
        mMessage = "DETENER · se mantiene el cuadro DMX actual.";
        SetDirty(false);
        return;
      }

      if(Contains(mVersionPrevious, x, y)) { Report(mPlug.CycleActiveTakeVersionFromUI(-1)); SetDirty(false); return; }
      if(Contains(mVersionNext, x, y)) { Report(mPlug.CycleActiveTakeVersionFromUI(1)); SetDirty(false); return; }
      if(Contains(mReturnRawButton, x, y)) { Report(mPlug.ReturnToRawTakeFromUI()); SetDirty(false); return; }
      if(Contains(mZoomOutButton, x, y)) { ZoomAt(2.0, 0.5F); SetDirty(false); return; }
      if(Contains(mZoomResetButton, x, y)) { mViewStart = 0.0; mViewEnd = 1.0; SetDirty(false); return; }
      if(Contains(mZoomInButton, x, y)) { ZoomAt(0.5, 0.5F); SetDirty(false); return; }

      if(Contains(mTimeline, x, y))
      {
        const auto snapshot = mPlug.ActiveTakeEditorSnapshot();
        if(!snapshot.available || snapshot.frame_count == 0U) return;
        if(mod.S)
        {
          mDragKind = DragKind::pan;
          SetDirty(false);
          return;
        }
        const float inX = XAtBoundary(snapshot.start_frame, snapshot.frame_count);
        const float outX = XAtBoundary(snapshot.end_frame_exclusive,
                                       snapshot.frame_count);
        if(x >= inX - 6.0F && x <= inX + 64.0F)
          mDragKind = DragKind::in_handle;
        else if(x >= outX - 68.0F && x <= outX + 6.0F)
          mDragKind = DragKind::out_handle;
        else
        {
          mDragKind = DragKind::playhead;
          ReportInline(mPlug.SeekActiveTakeFrameFromUI(
              FrameAtX(x, snapshot.frame_count)));
        }
        SetDirty(false);
        return;
      }

      const auto editor = mPlug.ActiveTakeEditorSnapshot();
      if(Contains(mInTimeField, x, y) && editor.available)
      {
        BeginTextEdit(EditKind::take_in_time, mInTimeField,
                      FormatTime(static_cast<double>(editor.start_frame) /
                                 editor.frames_per_second));
        return;
      }
      if(Contains(mOutTimeField, x, y) && editor.available)
      {
        BeginTextEdit(EditKind::take_out_time, mOutTimeField,
                      FormatTime(static_cast<double>(editor.end_frame_exclusive) /
                                 editor.frames_per_second));
        return;
      }
      if(Contains(mMarkInButton, x, y) && editor.available) { Report(mPlug.SetActiveTakeInFrameFromUI(editor.current_frame)); SetDirty(false); return; }
      if(Contains(mMarkOutButton, x, y) && editor.available) { Report(mPlug.SetActiveTakeOutFrameFromUI(std::min(editor.current_frame + 1U, editor.frame_count))); SetDirty(false); return; }
      if(Contains(mGoInButton, x, y) && editor.available) { ReportInline(mPlug.SeekActiveTakeFrameFromUI(editor.start_frame)); SetDirty(false); return; }
      if(Contains(mGoOutButton, x, y) && editor.available) { ReportInline(mPlug.SeekActiveTakeFrameFromUI(editor.end_frame_exclusive - 1U)); SetDirty(false); return; }
      if(Contains(mInFrameMinus, x, y) && editor.available) { ReportInline(mPlug.SetActiveTakeInFrameFromUI(editor.start_frame == 0U ? 0U : editor.start_frame - 1U)); SetDirty(false); return; }
      if(Contains(mInFramePlus, x, y) && editor.available) { ReportInline(mPlug.SetActiveTakeInFrameFromUI(editor.start_frame + 1U)); SetDirty(false); return; }
      if(Contains(mOutFrameMinus, x, y) && editor.available) { ReportInline(mPlug.SetActiveTakeOutFrameFromUI(editor.end_frame_exclusive - 1U)); SetDirty(false); return; }
      if(Contains(mOutFramePlus, x, y) && editor.available) { ReportInline(mPlug.SetActiveTakeOutFrameFromUI(std::min(editor.end_frame_exclusive + 1U, editor.frame_count))); SetDirty(false); return; }
      if(Contains(mResetTrimButton, x, y)) { Report(mPlug.ResetActiveTakeTrimFromUI()); SetDirty(false); return; }
      if(Contains(mConsolidateButton, x, y)) { Report(mPlug.ConsolidateActiveTakeFromUI()); SetDirty(false); return; }
      return;
    }

    if(mWorkspaceView == WorkspaceView::midi_show)
    {
      if(Contains(mMidiEnableButton, x, y))
      {
        Report(mPlug.ToggleShowMidiFromUI());
        SetDirty(false);
        return;
      }
      if(Contains(mMidiChannelPrevious, x, y))
      {
        ReportInline(mPlug.CycleShowMidiChannelFromUI(-1));
        SetDirty(false);
        return;
      }
      if(Contains(mMidiChannelNext, x, y))
      {
        ReportInline(mPlug.CycleShowMidiChannelFromUI(1));
        SetDirty(false);
        return;
      }
      constexpr std::array<aeyla::runtime::ShowMidiLearnTarget, 8U> targets{
          aeyla::runtime::ShowMidiLearnTarget::previous_song,
          aeyla::runtime::ShowMidiLearnTarget::next_song,
          aeyla::runtime::ShowMidiLearnTarget::play_retrigger,
          aeyla::runtime::ShowMidiLearnTarget::pause_resume,
          aeyla::runtime::ShowMidiLearnTarget::stop_reset,
          aeyla::runtime::ShowMidiLearnTarget::capture_start,
          aeyla::runtime::ShowMidiLearnTarget::capture_stop,
          aeyla::runtime::ShowMidiLearnTarget::launch_song_base};
      for(std::size_t index = 0U; index < mMidiLearnButtons.size(); ++index)
      {
        if(!Contains(mMidiLearnButtons[index], x, y)) continue;
        ReportInline(mPlug.BeginShowMidiLearnFromUI(targets[index]));
        SetDirty(false);
        return;
      }
      return;
    }

    if(Contains(mRxPrevious, x, y))
    {
      if(!mPlug.CycleRxInterfaceFromUI(-1))
        mMessage = mPlug.NetworkConfigurationBusy()
            ? "Espera a que termine el cambio de red actual."
            : (mPlug.TakeRecording()
                ? "No se puede cambiar el adaptador RX mientras se graba."
                : ((mPlug.TakeOutputArmed() || mPlug.OutputArmed())
                    ? "Desarma la salida antes de cambiar el adaptador RX."
                    : "No se detectaron adaptadores RX seleccionables."));
      SetDirty(false);
      return;
    }
    if(Contains(mRxNext, x, y))
    {
      if(!mPlug.CycleRxInterfaceFromUI(1))
        mMessage = mPlug.NetworkConfigurationBusy()
            ? "Espera a que termine el cambio de red actual."
            : (mPlug.TakeRecording()
                ? "No se puede cambiar el adaptador RX mientras se graba."
                : ((mPlug.TakeOutputArmed() || mPlug.OutputArmed())
                    ? "Desarma la salida antes de cambiar el adaptador RX."
                    : "No se detectaron adaptadores RX seleccionables."));
      SetDirty(false);
      return;
    }
    if(Contains(mTxPrevious, x, y))
    {
      if(mPlug.CycleTxInterfaceFromUI(-1))
      {
        RestoreNetworkFieldFromSelectedTx();
        mMessage = "Adaptador TX cambiado · verifica IPv4/máscara y luego APLICAR.";
      }
      else
        mMessage = mPlug.NetworkConfigurationBusy()
            ? "Espera a que termine el cambio de red actual."
            : (mPlug.TakeRecording()
                ? "Detén y guarda la toma antes de cambiar el adaptador TX."
                : ((mPlug.TakeOutputArmed() || mPlug.OutputArmed())
                    ? "Desarma la salida antes de cambiar el adaptador TX."
                    : "No se detectaron adaptadores TX seleccionables."));
      SetDirty(false);
      return;
    }
    if(Contains(mTxNext, x, y))
    {
      if(mPlug.CycleTxInterfaceFromUI(1))
      {
        RestoreNetworkFieldFromSelectedTx();
        mMessage = "Adaptador TX cambiado · verifica IPv4/máscara y luego APLICAR.";
      }
      else
        mMessage = mPlug.NetworkConfigurationBusy()
            ? "Espera a que termine el cambio de red actual."
            : (mPlug.TakeRecording()
                ? "Detén y guarda la toma antes de cambiar el adaptador TX."
                : ((mPlug.TakeOutputArmed() || mPlug.OutputArmed())
                    ? "Desarma la salida antes de cambiar el adaptador TX."
                    : "No se detectaron adaptadores TX seleccionables."));
      SetDirty(false);
      return;
    }

    if(Contains(mLocalNetworkField, x, y))
    {
      BeginTextEdit(EditKind::local_network, mLocalNetworkField,
                    mLocalNetworkText);
      return;
    }
    if(Contains(mApplyNetworkButton, x, y))
    {
      ApplySimpleNetwork();
      SetDirty(false);
      return;
    }
    if(Contains(mRefreshNetworkButton, x, y))
    {
      if(mPlug.NetworkConfigurationBusy())
        mMessage = "Espera a que termine el cambio de red actual.";
      else if(mPlug.TakeRecording())
        mMessage = "Detén y guarda la toma antes de actualizar adaptadores.";
      else if(mPlug.TakeOutputArmed() || mPlug.OutputArmed())
        mMessage = "Desarma la salida antes de actualizar adaptadores.";
      else if(mPlug.RefreshNetworkInterfacesFromUI())
      {
        RestoreNetworkFieldFromSelectedTx();
        mMessage = "Adaptadores de red actualizados.";
      }
      else
        mMessage = "No se detectaron adaptadores IPv4 activos.";
      SetDirty(false);
      return;
    }
  }

  void OnMouseDrag(float x, float y, float dX, float dY,
                   const IMouseMod& mod) override
  {
    (void)y;
    (void)dY;
    (void)mod;
    if(mWorkspaceView != WorkspaceView::take_editor ||
       mDragKind == DragKind::none) return;
    if(mDragKind == DragKind::pan)
    {
      const double span = mViewEnd - mViewStart;
      const double delta = -static_cast<double>(dX / std::max(1.0F, mTimeline.W())) * span;
      mViewStart = std::clamp(mViewStart + delta, 0.0, 1.0 - span);
      mViewEnd = mViewStart + span;
      SetDirty(false);
      return;
    }
    const auto snapshot = mPlug.ActiveTakeEditorSnapshot();
    if(!snapshot.available || snapshot.frame_count == 0U) return;
    const auto frame = FrameAtX(x, snapshot.frame_count);
    if(mDragKind == DragKind::in_handle)
      ReportInline(mPlug.SetActiveTakeInFrameFromUI(frame));
    else if(mDragKind == DragKind::out_handle)
      ReportInline(mPlug.SetActiveTakeOutFrameFromUI(
          std::min(frame + 1U, snapshot.frame_count)));
    else if(mDragKind == DragKind::playhead)
      ReportInline(mPlug.SeekActiveTakeFrameFromUI(frame));
    SetDirty(false);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    (void)x;
    (void)y;
    (void)mod;
    mDragKind = DragKind::none;
  }

  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override
  {
    (void)mod;
    if(mWorkspaceView != WorkspaceView::take_editor ||
       !Contains(mTimeline, x, y)) return;
    const float anchor = std::clamp((x - mTimeline.L) /
                                    std::max(1.0F, mTimeline.W()), 0.0F, 1.0F);
    ZoomAt(d > 0.0F ? 0.8 : 1.25, anchor);
    SetDirty(false);
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    BuildLayout();
    const std::size_t songCount = mPlug.SongCount();
    for(std::size_t index = 0; index < songCount && index < mSongRows.size(); ++index)
    {
      if(!Contains(mSongRows[index], x, y))
        continue;
      if(mPlug.TakeRecording() || mPlug.TakePlaying() ||
         mPlug.TakeOutputArmed())
      {
        mMessage = "Detén GRABAR/REPRODUCIR y desarma antes de renombrar una canción.";
        SetDirty(false);
        return;
      }
      mEditingSongIndex = index;
      BeginTextEdit(EditKind::song_name, mSongRows[index], mPlug.SongName(index));
      return;
    }
    IControl::OnMouseDblClick(x, y, mod);
  }

  void OnTextEntryCompletion(const char* str, int valIdx) override
  {
    (void)valIdx;
    const std::string value = str == nullptr ? std::string{} : Trim(str);
    if(mEditKind == EditKind::song_name)
      Report(mPlug.RenameSongFromUI(mEditingSongIndex, value));
    else if(mEditKind == EditKind::local_network)
    {
      mLocalNetworkText = value;
      mMessage = "Red editada · presiona APLICAR IP Y PREPARAR ART-NET.";
    }
    else if(mEditKind == EditKind::take_in_time ||
            mEditKind == EditKind::take_out_time)
    {
      double seconds = 0.0;
      const auto editor = mPlug.ActiveTakeEditorSnapshot();
      if(!editor.available || editor.frames_per_second == 0U)
        mMessage = "No hay una toma DMX disponible para editar.";
      else if(!ParseTime(value, seconds))
        mMessage = "Tiempo inválido · usa MM:SS.mmm o segundos.";
      else
      {
        const auto frame = static_cast<std::uint64_t>(std::llround(
            seconds * static_cast<double>(editor.frames_per_second)));
        if(mEditKind == EditKind::take_in_time)
          Report(mPlug.SetActiveTakeInFrameFromUI(frame));
        else
          Report(mPlug.SetActiveTakeOutFrameFromUI(frame));
      }
    }
    mEditKind = EditKind::none;
    SetDirty(false);
  }

private:
  enum class WorkspaceView { take_editor, midi_show, network_output };
  enum class EditKind {
    none,
    song_name,
    local_network,
    take_in_time,
    take_out_time
  };
  enum class DragKind { none, in_handle, out_handle, playhead, pan };

  inline static const IColor kBackground{255, 7, 8, 11};
  inline static const IColor kPanel{255, 14, 16, 21};
  inline static const IColor kPanelRaised{255, 21, 24, 31};
  inline static const IColor kPanelSelected{255, 27, 22, 34};
  inline static const IColor kLine{255, 43, 48, 59};
  inline static const IColor kLineStrong{255, 73, 80, 95};
  inline static const IColor kText{255, 235, 238, 242};
  inline static const IColor kMuted{255, 135, 143, 157};
  inline static const IColor kFaint{255, 88, 95, 108};
  inline static const IColor kAccent{255, 202, 145, 255};
  inline static const IColor kAccentDark{255, 53, 35, 67};
  inline static const IColor kCyan{255, 68, 214, 255};
  inline static const IColor kDanger{255, 231, 45, 55};
  inline static const IColor kDangerDark{255, 66, 18, 25};
  inline static const IColor kGood{255, 70, 205, 137};
  inline static const IColor kWarn{255, 238, 159, 64};

  void SyncWorkspaceFromProduct() noexcept
  {
    switch(mPlug.UiWorkspace())
    {
      case 2: mWorkspaceView = WorkspaceView::midi_show; break;
      case 3: mWorkspaceView = WorkspaceView::network_output; break;
      default: mWorkspaceView = WorkspaceView::take_editor; break;
    }
  }

  static bool Contains(const IRECT& rect, float x, float y) noexcept
  {
    return x >= rect.L && x <= rect.R && y >= rect.T && y <= rect.B;
  }

  float XAtBoundary(std::uint64_t boundary, std::uint64_t frameCount) const
  {
    if(frameCount == 0U) return mTimeline.L;
    const double normalized = std::clamp(
        static_cast<double>(boundary) / static_cast<double>(frameCount),
        mViewStart, mViewEnd);
    return mTimeline.L + static_cast<float>(
        (normalized - mViewStart) / (mViewEnd - mViewStart)) * mTimeline.W();
  }

  std::uint64_t FrameAtX(float x, std::uint64_t frameCount) const
  {
    if(frameCount == 0U) return 0U;
    const double local = std::clamp(
        static_cast<double>((x - mTimeline.L) / std::max(1.0F, mTimeline.W())),
        0.0, 1.0);
    const double normalized = mViewStart + local * (mViewEnd - mViewStart);
    return std::min<std::uint64_t>(
        static_cast<std::uint64_t>(std::floor(normalized * frameCount)),
        frameCount - 1U);
  }

  void ZoomAt(double factor, float anchor)
  {
    const double oldSpan = mViewEnd - mViewStart;
    const double nextSpan = std::clamp(oldSpan * factor, 0.05, 1.0);
    const double local = std::clamp(static_cast<double>(anchor), 0.0, 1.0);
    const double fixed = mViewStart + local * oldSpan;
    mViewStart = std::clamp(fixed - local * nextSpan, 0.0, 1.0 - nextSpan);
    mViewEnd = mViewStart + nextSpan;
  }

  static std::string Trim(std::string_view value)
  {
    while(!value.empty() && (value.front() == ' ' || value.front() == '\t'))
      value.remove_prefix(1U);
    while(!value.empty() && (value.back() == ' ' || value.back() == '\t'))
      value.remove_suffix(1U);
    return std::string(value);
  }

  static std::string FormatTime(double seconds)
  {
    if(!std::isfinite(seconds) || seconds < 0.0) seconds = 0.0;
    const auto totalMs = static_cast<std::uint64_t>(std::llround(seconds * 1000.0));
    const auto minutes = totalMs / 60000U;
    const auto secondsPart = (totalMs / 1000U) % 60U;
    const auto millis = totalMs % 1000U;
    std::ostringstream stream;
    stream << minutes << ':' << std::setw(2) << std::setfill('0') << secondsPart
           << '.' << std::setw(3) << millis;
    return stream.str();
  }

  static bool ParseTime(std::string_view text, double& seconds)
  {
    std::string normalized = Trim(text);
    std::replace(normalized.begin(), normalized.end(), ',', '.');
    if(normalized.empty()) return false;
    const auto colon = normalized.find(':');
    if(colon != std::string::npos &&
       normalized.find(':', colon + 1U) != std::string::npos)
      return false;

    double minutes = 0.0;
    std::string secondsText = normalized;
    if(colon != std::string::npos)
    {
      const std::string minutesText = normalized.substr(0U, colon);
      if(minutesText.empty()) return false;
      char* minutesEnd = nullptr;
      minutes = std::strtod(minutesText.c_str(), &minutesEnd);
      if(minutesEnd == nullptr || *minutesEnd != '\0' || minutes < 0.0 ||
         std::floor(minutes) != minutes)
        return false;
      secondsText = normalized.substr(colon + 1U);
    }
    if(secondsText.empty()) return false;
    char* secondsEnd = nullptr;
    const double parsedSeconds = std::strtod(secondsText.c_str(), &secondsEnd);
    if(secondsEnd == nullptr || *secondsEnd != '\0' ||
       !std::isfinite(parsedSeconds) || parsedSeconds < 0.0 ||
       (colon != std::string::npos && parsedSeconds >= 60.0))
      return false;
    seconds = minutes * 60.0 + parsedSeconds;
    return std::isfinite(seconds);
  }

  static void Card(IGraphics& g, const IRECT& rect)
  {
    g.FillRoundRect(kPanel, rect, 9.0F);
    g.DrawRoundRect(kLine, rect, 9.0F, nullptr, 1.0F);
  }

  static void Button(IGraphics& g, const IRECT& rect, const char* label,
                     const IColor& fill, const IColor& border,
                     const IColor& text = kText)
  {
    g.FillRoundRect(fill, rect, 6.0F);
    g.DrawRoundRect(border, rect, 6.0F, nullptr, 1.0F);
    g.DrawText(IText(12.0F, text, "AeylaUI", EAlign::Center, EVAlign::Middle),
               label, rect.GetPadded(-4.0F));
  }

  static void Field(IGraphics& g, const IRECT& rect, const char* label,
                    const std::string& value, const IColor& valueColor = kText)
  {
    g.DrawText(IText(12.0F, kFaint, "AeylaUI", EAlign::Near, EVAlign::Middle),
               label, IRECT(rect.L, rect.T - 16.0F, rect.R, rect.T - 2.0F));
    g.FillRoundRect(IColor(255, 10, 12, 17), rect, 5.0F);
    g.DrawRoundRect(kLineStrong, rect, 5.0F, nullptr, 1.0F);
    g.DrawText(IText(12.0F, valueColor, "AeylaUI", EAlign::Near, EVAlign::Middle),
               value.c_str(), rect.GetPadded(-8.0F));
  }

  static void StatusRow(IGraphics& g, const IRECT& rect, const char* label,
                        const std::string& value, const IColor& statusColor)
  {
    g.FillRoundRect(IColor(255, 11, 13, 18), rect, 5.0F);
    g.DrawRoundRect(kLine, rect, 5.0F, nullptr, 1.0F);
    g.FillRoundRect(statusColor,
                    IRECT(rect.L + 8.0F, rect.T + 8.0F,
                          rect.L + 16.0F, rect.B - 8.0F), 3.0F);
    g.DrawText(IText(12.0F, kFaint, "AeylaUI", EAlign::Near, EVAlign::Middle),
               label, IRECT(rect.L + 24.0F, rect.T,
                            rect.L + 126.0F, rect.B));
    g.DrawText(IText(12.0F, statusColor, "AeylaUI", EAlign::Near,
                     EVAlign::Middle),
               value.c_str(), IRECT(rect.L + 128.0F, rect.T,
                                    rect.R - 8.0F, rect.B));
  }

  void BeginTextEdit(EditKind kind, const IRECT& rect, const std::string& current)
  {
    auto* ui = GetUI();
    if(ui == nullptr) return;
    mEditKind = kind;
    ui->CreateTextEntry(*this,
                        IText(12.0F, kText, "AeylaUI", EAlign::Near,
                              EVAlign::Middle),
                        rect, current.c_str(), kNoValIdx);
  }

  void RestoreNetworkFieldFromSelectedTx()
  {
    mLocalNetworkText.clear();
    const auto adapter = mPlug.SelectedTxInterface();
    if(!adapter.has_value() || adapter->ipv4.empty() ||
       adapter->prefix_length == 0U || adapter->prefix_length > 32U)
      return;
    mLocalNetworkText = adapter->ipv4 + " / " +
        aeyla::network::format_ipv4(
            aeyla::network::mask_from_prefix(adapter->prefix_length));
  }

  void ApplySimpleNetwork()
  {
    if(mPlug.NetworkConfigurationBusy())
    {
      mMessage = mPlug.NetworkConfigurationStatus();
      return;
    }
    if(mPlug.TakeOutputArmed() || mPlug.OutputArmed() ||
       mPlug.ArtNetOutputStatus().enabled ||
       mPlug.ArtNetOutputStatus().override_enabled)
    {
      mMessage = "Desarma la salida antes de aplicar cambios de red.";
      return;
    }
    const auto separator = mLocalNetworkText.find('/');
    if(separator == std::string::npos)
    {
      mMessage = "Usa IPv4 / máscara, por ejemplo 2.0.0.20 / 255.0.0.0";
      return;
    }
    const std::string ipv4 = Trim(
        std::string_view(mLocalNetworkText).substr(0U, separator));
    const std::string mask = Trim(
        std::string_view(mLocalNetworkText).substr(separator + 1U));
    Report(mPlug.ApplyTxNetworkFromUI(ipv4, mask));
  }

  void BuildLayout()
  {
    constexpr float margin = 14.0F;
    constexpr float headerHeight = 82.0F;
    constexpr float footerReserve = 62.0F;
    constexpr float gap = 10.0F;
    const float leftWidth = std::clamp(mRECT.W() * 0.20F, 218.0F, 252.0F);

    mHeader = IRECT(mRECT.L + margin, mRECT.T + 8.0F,
                    mRECT.R - margin, mRECT.T + headerHeight);
    const float contentTop = mHeader.B + 6.0F;
    const float contentBottom = mRECT.B - footerReserve;
    mSetlist = IRECT(mRECT.L + margin, contentTop,
                     mRECT.L + margin + leftWidth, contentBottom);
    mWorkspace = IRECT(mSetlist.R + gap, contentTop,
                       mRECT.R - margin, contentBottom);
    mRouting = mWorkspace;

    mBlackoutButton = IRECT(mHeader.R - 146.0F, mHeader.T + 18.0F,
                            mHeader.R, mHeader.B - 18.0F);
    mTakeArmButton = IRECT(mHeader.R - 302.0F, mHeader.T + 18.0F,
                           mHeader.R - 154.0F, mHeader.B - 18.0F);
    mTakeEditorTab = IRECT(mHeader.L + 300.0F, mHeader.T + 18.0F,
                           mHeader.L + 394.0F, mHeader.B - 18.0F);
    mMidiShowTab = IRECT(mTakeEditorTab.R + 6.0F, mTakeEditorTab.T,
                         mTakeEditorTab.R + 110.0F, mTakeEditorTab.B);
    mNetworkOutputTab = IRECT(mMidiShowTab.R + 6.0F, mTakeEditorTab.T,
                              mMidiShowTab.R + 102.0F, mTakeEditorTab.B);

    const IRECT listBody = mSetlist.GetPadded(-10.0F);
    mNewSongButton = IRECT(listBody.L, mSetlist.B - 42.0F,
                           listBody.R, mSetlist.B - 12.0F);
    float top = listBody.T + 42.0F;
    const float availableRows = std::max(0.0F, mNewSongButton.T - 8.0F - top);
    const float rowHeight = std::clamp(
        availableRows / static_cast<float>(mSongRows.size()), 21.0F, 28.0F);
    for(std::size_t index = 0; index < mSongRows.size(); ++index)
    {
      mSongRows[index] = IRECT(listBody.L, top,
                               listBody.R, top + rowHeight - 2.0F);
      top += rowHeight;
    }

    const IRECT work = mWorkspace.GetPadded(-16.0F);
    const bool compactEditor = work.H() < 560.0F;
    const float versionTop = work.T + (compactEditor ? 58.0F : 70.0F);
    mReturnRawButton = IRECT(work.L, versionTop, work.L + 154.0F,
                             versionTop + 26.0F);
    mVersionPrevious = IRECT(mReturnRawButton.R + 8.0F, versionTop,
                             mReturnRawButton.R + 42.0F, versionTop + 26.0F);
    mVersionNext = IRECT(mVersionPrevious.R + 5.0F, versionTop,
                         mVersionPrevious.R + 39.0F, versionTop + 26.0F);
    mZoomInButton = IRECT(work.R - 34.0F, versionTop,
                          work.R, versionTop + 26.0F);
    mZoomResetButton = IRECT(mZoomInButton.L - 54.0F, versionTop,
                             mZoomInButton.L - 5.0F, versionTop + 26.0F);
    mZoomOutButton = IRECT(mZoomResetButton.L - 39.0F, versionTop,
                           mZoomResetButton.L - 5.0F, versionTop + 26.0F);
    const float timelineTop = work.T + (compactEditor ? 92.0F : 104.0F);
    // The waveform is the primary editing surface, so it absorbs spare height
    // in both compact and nominal hosts. The compact branch reserves 244 px
    // below it for transport, trim rows and the summary, while retaining the
    // proven 80 px minimum.
    const float timelineBottom = compactEditor
        ? std::max(timelineTop + 80.0F, work.B - 244.0F)
        : std::max(timelineTop + 94.0F, work.B - 357.0F);
    mTimeline = IRECT(work.L, timelineTop, work.R, timelineBottom);
    const float transportTop = mTimeline.B + (compactEditor ? 8.0F : 14.0F);
    const float transportGap = 8.0F;
    const float transportWidth = (work.W() - transportGap * 2.0F) / 3.0F;
    mRecordButton = IRECT(work.L, transportTop,
                          work.L + transportWidth,
                          transportTop + (compactEditor ? 36.0F : 40.0F));
    mPlayButton = IRECT(mRecordButton.R + transportGap, transportTop,
                        mRecordButton.R + transportGap + transportWidth,
                        transportTop + (compactEditor ? 36.0F : 40.0F));
    mStopButton = IRECT(mPlayButton.R + transportGap, transportTop,
                        work.R, transportTop + (compactEditor ? 36.0F : 40.0F));

    const float editorTop = mStopButton.B + (compactEditor ? 12.0F : 36.0F);
    const float smallGap = 6.0F;
    const float timeWidth = std::clamp(work.W() * 0.20F, 112.0F, 150.0F);
    const float frameWidth = 36.0F;
    const float goWidth = std::clamp(work.W() * 0.13F, 72.0F, 94.0F);
    const float labelWidth = 66.0F;
    const float markWidth = work.W() - labelWidth - timeWidth -
                            frameWidth * 2.0F - goWidth - smallGap * 5.0F;
    float left = work.L + labelWidth;
    mInTimeField = IRECT(left, editorTop, left + timeWidth, editorTop + 32.0F);
    left = mInTimeField.R + smallGap;
    mInFrameMinus = IRECT(left, editorTop, left + frameWidth, editorTop + 32.0F);
    left = mInFrameMinus.R + smallGap;
    mInFramePlus = IRECT(left, editorTop, left + frameWidth, editorTop + 32.0F);
    left = mInFramePlus.R + smallGap;
    mMarkInButton = IRECT(left, editorTop, left + markWidth, editorTop + 32.0F);
    left = mMarkInButton.R + smallGap;
    mGoInButton = IRECT(left, editorTop, work.R, editorTop + 32.0F);

    const float outTop = editorTop + (compactEditor ? 40.0F : 48.0F);
    left = work.L + labelWidth;
    mOutTimeField = IRECT(left, outTop, left + timeWidth, outTop + 32.0F);
    left = mOutTimeField.R + smallGap;
    mOutFrameMinus = IRECT(left, outTop, left + frameWidth, outTop + 32.0F);
    left = mOutFrameMinus.R + smallGap;
    mOutFramePlus = IRECT(left, outTop, left + frameWidth, outTop + 32.0F);
    left = mOutFramePlus.R + smallGap;
    mMarkOutButton = IRECT(left, outTop, left + markWidth, outTop + 32.0F);
    left = mMarkOutButton.R + smallGap;
    mGoOutButton = IRECT(left, outTop, work.R, outTop + 32.0F);
    mResetTrimButton = IRECT(work.L,
                             outTop + (compactEditor ? 38.0F : 45.0F),
                             work.R,
                             outTop + (compactEditor ? 66.0F : 76.0F));
    mConsolidateButton = IRECT(work.L, mResetTrimButton.B + 8.0F,
                               work.R,
                               mResetTrimButton.B +
                                   (compactEditor ? 38.0F : 43.0F));

    const IRECT route = mRouting.GetPadded(-12.0F);
    const bool compactRouting = route.H() < 520.0F;
    const float cardTop = route.T + (compactRouting ? 42.0F : 46.0F);
    const float routeGap = 12.0F;
    const float cardWidth = (route.W() - routeGap) * 0.5F;
    const float routeCardHeight = compactRouting ? 128.0F : 150.0F;
    mRxCard = IRECT(route.L, cardTop, route.L + cardWidth,
                    cardTop + routeCardHeight);
    mTxCard = IRECT(mRxCard.R + routeGap, cardTop, route.R,
                    cardTop + routeCardHeight);
    mRxPrevious = IRECT(mRxCard.L + 10.0F, mRxCard.B - 34.0F,
                        mRxCard.L + 46.0F, mRxCard.B - 8.0F);
    mRxNext = IRECT(mRxCard.R - 46.0F, mRxCard.B - 34.0F,
                    mRxCard.R - 10.0F, mRxCard.B - 8.0F);
    mTxPrevious = IRECT(mTxCard.L + 10.0F, mTxCard.B - 34.0F,
                        mTxCard.L + 46.0F, mTxCard.B - 8.0F);
    mTxNext = IRECT(mTxCard.R - 46.0F, mTxCard.B - 34.0F,
                    mTxCard.R - 10.0F, mTxCard.B - 8.0F);

    const float networkTop = mTxCard.B + (compactRouting ? 32.0F : 44.0F);
    const float networkFieldWidth = route.W() * 0.56F;
    mLocalNetworkField = IRECT(route.L, networkTop,
                               route.L + networkFieldWidth, networkTop + 40.0F);
    mApplyNetworkButton = IRECT(mLocalNetworkField.R + 12.0F, networkTop,
                                route.R, networkTop + 40.0F);
    mRefreshNetworkButton = IRECT(
        mApplyNetworkButton.L,
        mApplyNetworkButton.B + (compactRouting ? 8.0F : 10.0F),
        route.R,
        mApplyNetworkButton.B + (compactRouting ? 40.0F : 46.0F));

    const IRECT midi = mWorkspace.GetPadded(-16.0F);
    mCompactMidi = midi.H() < 580.0F;
    const float midiTop = midi.T + (mCompactMidi ? 42.0F : 50.0F);
    const float midiControlHeight = mCompactMidi ? 34.0F : 38.0F;
    mMidiEnableButton = IRECT(
        midi.L, midiTop, midi.L + (mCompactMidi ? 220.0F : 250.0F),
        midiTop + midiControlHeight);
    mMidiChannelPrevious = IRECT(mMidiEnableButton.R + 18.0F, midiTop,
                                 mMidiEnableButton.R + 56.0F,
                                 midiTop + midiControlHeight);
    mMidiChannelField = IRECT(mMidiChannelPrevious.R + 6.0F, midiTop,
                              mMidiChannelPrevious.R + 126.0F,
                              midiTop + midiControlHeight);
    mMidiChannelNext = IRECT(mMidiChannelField.R + 6.0F, midiTop,
                             mMidiChannelField.R + 44.0F,
                             midiTop + midiControlHeight);
    const float rowTop = midiTop + (mCompactMidi ? 44.0F : 62.0F);
    const float rowGap = mCompactMidi ? 4.0F : 7.0F;
    // Reserve every inter-row gap, both state rows, the message and all three
    // safety/sync footer lines. At the nominal 1280x800 canvas this keeps the
    // final warning inside the card. The compact 960x620 branch combines its
    // capture/safety footer into two lines while retaining all eight controls.
    const float midiFixedBelowRows = mCompactMidi ? 154.0F : 224.0F;
    const float rowSpace = midi.B - rowTop - midiFixedBelowRows -
        rowGap * static_cast<float>(mMidiRows.size() - 1U);
    const float midiRowHeight = std::clamp(
        rowSpace / static_cast<float>(mMidiRows.size()),
        mCompactMidi ? 24.0F : 28.0F,
        mCompactMidi ? 31.0F : 36.0F);
    for(std::size_t index = 0U; index < mMidiRows.size(); ++index)
    {
      const float topRow = rowTop + static_cast<float>(index) *
          (midiRowHeight + rowGap);
      mMidiRows[index] = IRECT(
          midi.L, topRow, midi.R, topRow + midiRowHeight);
      mMidiLearnButtons[index] = IRECT(midi.R - 132.0F, topRow + 4.0F,
                                       midi.R - 6.0F,
                                       topRow + midiRowHeight - 4.0F);
    }
    const float statusTop = mMidiRows.back().B +
        (mCompactMidi ? 6.0F : 12.0F);
    const float statusHeight = mCompactMidi ? 24.0F : 30.0F;
    mMidiPreparedStatus = IRECT(midi.L, statusTop, midi.R,
                                statusTop + statusHeight);
    const float activeTop = mMidiPreparedStatus.B +
        (mCompactMidi ? 3.0F : 5.0F);
    mMidiActiveStatus = IRECT(midi.L, activeTop, midi.R,
                              activeTop + statusHeight);
    const float messageTop = mMidiActiveStatus.B +
        (mCompactMidi ? 4.0F : 8.0F);
    mMidiMessageStatus = IRECT(
        midi.L, messageTop, midi.R,
        std::min(midi.B, messageTop + (mCompactMidi ? 36.0F : 58.0F)));
  }

  void DrawHeader(IGraphics& g)
  {
    g.DrawText(IText(21.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "AEYLA",
               IRECT(mHeader.L, mHeader.T, mHeader.L + 282.0F, mHeader.B - 20.0F));
    g.DrawText(IText(12.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "RGB ESTUDIOS · R09 PRETEST · MIDI REC DIRECT",
               IRECT(mHeader.L, mHeader.B - 27.0F,
                     mHeader.L + 290.0F, mHeader.B));

    const bool editorView = mWorkspaceView == WorkspaceView::take_editor;
    const bool midiView = mWorkspaceView == WorkspaceView::midi_show;
    const bool networkView = mWorkspaceView == WorkspaceView::network_output;
    Button(g, mTakeEditorTab, "TOMA",
           editorView ? kPanelSelected : kPanelRaised,
           editorView ? kAccent : kLineStrong,
           editorView ? kText : kMuted);
    Button(g, mMidiShowTab, "MIDI / SHOW",
           midiView ? kPanelSelected : kPanelRaised,
           midiView ? kAccent : kLineStrong,
           midiView ? kText : kMuted);
    Button(g, mNetworkOutputTab, "RED / SALIDA",
           networkView ? kPanelSelected : kPanelRaised,
           networkView ? kAccent : kLineStrong,
           networkView ? kText : kMuted);

    const bool blackout = mPlug.GlobalBlackout();
    Button(g, mBlackoutButton, blackout ? "APAGÓN ACTIVO" : "APAGÓN DESACTIVADO",
           blackout ? kAccentDark : kPanelRaised,
           blackout ? kAccent : kLineStrong,
           blackout ? kText : kGood);

    const bool armed = mPlug.TakeOutputArmed();
    std::string armLabel = "ARMAR SALIDA DE TOMA";
    bool armBlocked = false;
    if(armed)
      armLabel = "SALIDA DE TOMA ARMADA";
    else if(mPlug.TakeRecording())
      armLabel = "BLOQUEADA · GRABANDO";
    else if(mPlug.GlobalBlackout())
      armLabel = "BLOQUEADA · APAGÓN";
    else if(mPlug.ShowMidiMapping().enabled && mPlug.ShowMidiPreflightBusy())
      armLabel = "BLOQUEADA · PRECARGA MIDI";
    else if(!mPlug.BackendReady())
      armLabel = "BLOQUEADA · RED";
    else if(!mPlug.RuntimeHealthy() || mPlug.RenderingOffline())
      armLabel = "BLOQUEADA · MOTOR";
    armBlocked = !armed && armLabel.rfind("BLOQUEADA", 0U) == 0U;
    Button(g, mTakeArmButton, armLabel.c_str(),
           armed ? kGood : (armBlocked ? IColor(255, 54, 42, 22) : kPanelRaised),
           armed ? kGood : (armBlocked ? kWarn : kLineStrong),
           armed ? IColor(255, 8, 30, 20) : (armBlocked ? kWarn : kText));
  }

  void DrawSetlist(IGraphics& g)
  {
    Card(g, mSetlist);
    g.DrawText(IText(12.5F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
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

    const std::size_t count = mPlug.SongCount();
    const std::size_t prepared = mPlug.ActiveSongIndex();
    const int activeTake = mPlug.ActiveTakeSongIndex();
    for(std::size_t index = 0; index < mSongRows.size(); ++index)
    {
      const IRECT& row = mSongRows[index];
      if(index >= count)
      {
        g.DrawLine(kLine, row.L, row.B, row.R, row.B, nullptr, 1.0F);
        continue;
      }
      const bool selected = index == prepared;
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
      char number[8];
      std::snprintf(number, sizeof(number), "%02d", static_cast<int>(index + 1U));
      const IColor rowAccent = onAir ? kDanger : (selected ? kCyan : kFaint);
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
    }

    Button(g, mNewSongButton,
           count >= 15U ? "LÍMITE: 15 CANCIONES" :
               (mPlug.TakeRecording() ? "BLOQUEADA · GRABANDO" : "+ NUEVA CANCIÓN"),
           mPlug.TakeRecording() ? IColor(255, 54, 42, 22) : kPanelRaised,
           mPlug.TakeRecording() ? kWarn : kLineStrong,
           count >= 15U ? kFaint :
               (mPlug.TakeRecording() ? kWarn : kText));
  }

  void DrawTakeEditor(IGraphics& g)
  {
    Card(g, mWorkspace);
    const IRECT work = mWorkspace.GetPadded(-16.0F);
    const bool compact = work.H() < 560.0F;
    const std::size_t count = mPlug.SongCount();
    const std::size_t active = mPlug.ActiveSongIndex();
    std::string title = "SIN CANCIÓN SELECCIONADA";
    if(count > 0U)
    {
      char prefix[24];
      std::snprintf(prefix, sizeof(prefix), "%02d / %02d · ",
                    static_cast<int>(active + 1U), static_cast<int>(count));
      title = prefix + mPlug.SongName(active);
    }
    g.DrawText(IText(18.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
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

    const auto editor = mPlug.ActiveTakeEditorSnapshot();
    if(editor.path != mLastEditorPath)
    {
      mLastEditorPath = editor.path;
      mViewStart = 0.0;
      mViewEnd = 1.0;
    }
    const double fps = editor.frames_per_second == 0U
        ? 1.0 : static_cast<double>(editor.frames_per_second);
    const double original = static_cast<double>(editor.frame_count) / fps;
    const double inTime = static_cast<double>(editor.start_frame) / fps;
    const double outTime = static_cast<double>(editor.end_frame_exclusive) / fps;
    const double effective = static_cast<double>(
        editor.end_frame_exclusive >= editor.start_frame
            ? editor.end_frame_exclusive - editor.start_frame : 0U) / fps;

    Button(g, mReturnRawButton,
           editor.raw_source ? "TOMA BRUTA" : "VOLVER A TOMA BRUTA",
           editor.raw_source ? IColor(255, 18, 51, 38) : kPanelRaised,
           editor.raw_source ? kGood : kLineStrong,
           editor.raw_source ? kGood : kText);
    Button(g, mVersionPrevious, "<", kPanelRaised, kLineStrong);
    Button(g, mVersionNext, ">", kPanelRaised, kLineStrong);
    Button(g, mZoomOutButton, "-", kPanelRaised, kLineStrong);
    Button(g, mZoomResetButton, "1:1", kPanelRaised, kLineStrong);
    Button(g, mZoomInButton, "+", kPanelRaised, kLineStrong);
    const std::string version = editor.available
        ? "VERSIÓN " + std::to_string(editor.version_index + 1U) + " / " +
              std::to_string(editor.version_count) + " · " + editor.take_name
        : "SIN TOMA DMX";
    g.DrawText(IText(12.0F, editor.raw_source ? kMuted : kGood,
                     "AeylaUI", EAlign::Near, EVAlign::Middle),
               version.c_str(),
               IRECT(mVersionNext.R + 8.0F, mVersionNext.T,
                     mZoomOutButton.L - 8.0F, mVersionNext.B));

    g.FillRoundRect(IColor(255, 9, 11, 15), mTimeline, 6.0F);
    g.DrawRoundRect(editor.available ? kCyan : kLine,
                    mTimeline, 6.0F, nullptr, editor.available ? 1.2F : 1.0F);
    if(editor.available && editor.frame_count > 0U)
    {
      const float graphTop = mTimeline.T + 14.0F;
      const float graphBottom = mTimeline.B - 17.0F;
      for(std::size_t index = 0U; index < editor.activity_count; ++index)
      {
        const double bucketStart = static_cast<double>(index) /
                                   static_cast<double>(editor.activity_count);
        const double bucketEnd = static_cast<double>(index + 1U) /
                                 static_cast<double>(editor.activity_count);
        if(bucketEnd < mViewStart || bucketStart > mViewEnd) continue;
        const float left = mTimeline.L + static_cast<float>(
            (std::max(bucketStart, mViewStart) - mViewStart) /
            (mViewEnd - mViewStart)) * mTimeline.W();
        const float right = mTimeline.L + static_cast<float>(
            (std::min(bucketEnd, mViewEnd) - mViewStart) /
            (mViewEnd - mViewStart)) * mTimeline.W();
        const float level = static_cast<float>(editor.activity_level[index]) / 255.0F;
        const float motion = static_cast<float>(editor.activity_motion[index]) / 255.0F;
        const float levelTop = graphBottom - level * (graphBottom - graphTop);
        g.FillRect(IColor(255, 60, 72, 89),
                   IRECT(left, levelTop, std::max(left + 1.0F, right), graphBottom));
        if(motion > 0.0F)
        {
          const float motionTop = graphBottom - motion * (graphBottom - graphTop);
          g.FillRect(IColor(220, 68, 214, 255),
                     IRECT(left, motionTop, std::max(left + 1.0F, right),
                           std::min(graphBottom, motionTop + 2.0F)));
        }
      }

      const float inX = XAtBoundary(editor.start_frame, editor.frame_count);
      const float outX = XAtBoundary(editor.end_frame_exclusive,
                                     editor.frame_count);
      if(inX > mTimeline.L)
        g.FillRect(IColor(190, 8, 9, 12), IRECT(mTimeline.L, mTimeline.T, inX, mTimeline.B));
      if(outX < mTimeline.R)
        g.FillRect(IColor(190, 8, 9, 12), IRECT(outX, mTimeline.T, mTimeline.R, mTimeline.B));
      if(outX > inX)
        g.DrawRect(kCyan, IRECT(inX, mTimeline.T + 1.0F, outX,
                                mTimeline.B - 1.0F), nullptr, 1.0F);
      g.DrawLine(kGood, inX, mTimeline.T, inX, mTimeline.B, nullptr, 3.0F);
      g.DrawLine(kGood, outX, mTimeline.T, outX, mTimeline.B, nullptr, 3.0F);
      const IRECT inGrip(inX, mTimeline.T + 3.0F,
                         std::min(inX + 64.0F, mTimeline.R),
                         mTimeline.T + 23.0F);
      const IRECT outGrip(std::max(outX - 68.0F, mTimeline.L),
                          mTimeline.T + 3.0F, outX, mTimeline.T + 23.0F);
      g.FillRoundRect(kGood, inGrip, 4.0F);
      g.FillRoundRect(kGood, outGrip, 4.0F);
      g.DrawText(IText(12.0F, IColor(255, 7, 24, 16), "AeylaUI",
                       EAlign::Center, EVAlign::Middle),
                 "ENTRADA", inGrip);
      g.DrawText(IText(12.0F, IColor(255, 7, 24, 16), "AeylaUI",
                       EAlign::Center, EVAlign::Middle),
                 "SALIDA", outGrip);

      const float playheadX = XAtBoundary(
          std::min(editor.current_frame, editor.frame_count), editor.frame_count);
      if(playheadX >= mTimeline.L && playheadX <= mTimeline.R)
      {
        g.DrawLine(kAccent, playheadX, mTimeline.T, playheadX,
                   mTimeline.B, nullptr, 2.0F);
        g.FillTriangle(kAccent,
                       playheadX - 6.0F, mTimeline.T,
                       playheadX + 6.0F, mTimeline.T,
                       playheadX, mTimeline.T + 8.0F);
      }

      const double viewStartTime = mViewStart * original;
      const double viewEndTime = mViewEnd * original;
      g.DrawText(IText(12.0F, kFaint, "AeylaUI", EAlign::Near, EVAlign::Middle),
                 FormatTime(viewStartTime).c_str(),
                 IRECT(mTimeline.L + 5.0F, mTimeline.B - 17.0F,
                       mTimeline.L + 82.0F, mTimeline.B));
      g.DrawText(IText(12.0F, kFaint, "AeylaUI", EAlign::Far, EVAlign::Middle),
                 FormatTime(viewEndTime).c_str(),
                 IRECT(mTimeline.R - 82.0F, mTimeline.B - 17.0F,
                       mTimeline.R - 5.0F, mTimeline.B));
    }

    const bool recordBlocked = !mPlug.TakeRecording() &&
        (mPlug.TakePlaying() || mPlug.TakeOutputArmed());
    Button(g, mRecordButton,
           mPlug.TakeRecording() ? "REC · DETENER + GUARDAR" :
               (recordBlocked ? "REC · BLOQUEADO" : "REC · NUEVA TOMA"),
           mPlug.TakeRecording() ? kDangerDark :
               (recordBlocked ? IColor(255, 54, 42, 22) : kPanelRaised),
           mPlug.TakeRecording() ? kDanger :
               (recordBlocked ? kWarn : kLineStrong),
           mPlug.TakeRecording() ? kDanger :
               (recordBlocked ? kWarn : kText));
    const bool playBlocked = !mPlug.TakePlaying() &&
        (mPlug.TakeRecording() || !editor.available);
    Button(g, mPlayButton,
           mPlug.TakePlaying() ? "PLAY · REPRODUCIENDO" :
               (playBlocked ? "PLAY · BLOQUEADO" : "PLAY / PAUSA"),
           mPlug.TakePlaying() ? kGood :
               (playBlocked ? IColor(255, 54, 42, 22) : kPanelRaised),
           mPlug.TakePlaying() ? kGood :
               (playBlocked ? kWarn : kLineStrong),
           mPlug.TakePlaying() ? IColor(255, 8, 30, 20) :
               (playBlocked ? kWarn : kText));
    Button(g, mStopButton, "HOLD / STOP", kPanelRaised, kWarn, kWarn);

    const float editorTop = mInTimeField.T;
    g.DrawText(IText(12.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "ENTRADA", IRECT(work.L, editorTop, work.L + 60.0F, editorTop + 32.0F));
    g.FillRoundRect(IColor(255, 10, 12, 17), mInTimeField, 5.0F);
    g.DrawRoundRect(kGood, mInTimeField, 5.0F, nullptr, 1.0F);
    g.DrawText(IText(12.0F, kText, "AeylaUI", EAlign::Center, EVAlign::Middle),
               FormatTime(inTime).c_str(), mInTimeField.GetPadded(-5.0F));
    Button(g, mInFrameMinus, "-1f", kPanelRaised, kLineStrong);
    Button(g, mInFramePlus, "+1f", kPanelRaised, kLineStrong);
    Button(g, mMarkInButton, "MARCAR ENTRADA EN CABEZAL",
           IColor(255, 18, 51, 38), kGood, kGood);
    Button(g, mGoInButton, "IR A ENTRADA", kPanelRaised, kLineStrong);

    const float outTop = mOutTimeField.T;
    g.DrawText(IText(12.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "SALIDA", IRECT(work.L, outTop, work.L + 60.0F, outTop + 32.0F));
    g.FillRoundRect(IColor(255, 10, 12, 17), mOutTimeField, 5.0F);
    g.DrawRoundRect(kGood, mOutTimeField, 5.0F, nullptr, 1.0F);
    g.DrawText(IText(12.0F, kText, "AeylaUI", EAlign::Center, EVAlign::Middle),
               FormatTime(outTime).c_str(), mOutTimeField.GetPadded(-5.0F));
    Button(g, mOutFrameMinus, "-1f", kPanelRaised, kLineStrong);
    Button(g, mOutFramePlus, "+1f", kPanelRaised, kLineStrong);
    Button(g, mMarkOutButton, "MARCAR SALIDA EN CABEZAL",
           IColor(255, 18, 51, 38), kGood, kGood);
    Button(g, mGoOutButton, "IR A SALIDA", kPanelRaised, kLineStrong);
    Button(g, mResetTrimButton, "RESTAURAR ENTRADA / SALIDA", kPanelRaised, kLineStrong);
    Button(g, mConsolidateButton, "CONSOLIDAR MUESTRA DMX",
           IColor(255, 18, 51, 38), kGood, kGood);

    const float infoTop = mConsolidateButton.B + 8.0F;
    const std::string times = "ENTRADA " + FormatTime(inTime) +
        "   ·   SALIDA " + FormatTime(outTime) +
        "   ·   DURACIÓN " + FormatTime(effective) +
        "   ·   TOMA BRUTA " + FormatTime(original);
    g.DrawText(IText(12.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle),
               times.c_str(), IRECT(work.L, infoTop, work.R, infoTop + 28.0F));

    if(!compact)
    {
      const IRECT messageRect(work.L, work.B - 48.0F, work.R, work.B);
      g.FillRoundRect(IColor(255, 11, 13, 18), messageRect, 6.0F);
      const std::string message = mMessage.empty()
          ? "CLIC/ARRASTRE = CABEZAL · ARRASTRA ENTRADA/SALIDA · RUEDA = AMPLIAR · MAYÚS+ARRASTRE = DESPLAZAR."
          : mMessage;
      g.DrawText(IText(12.0F, mMessage.empty() ? kFaint : kWarn,
                       "AeylaUI", EAlign::Near, EVAlign::Middle),
                 message.c_str(), messageRect.GetPadded(-10.0F));
    }
  }

  void DrawMidiShow(IGraphics& g)
  {
    Card(g, mWorkspace);
    const auto mapping = mPlug.ShowMidiMapping();
    g.DrawText(IText(18.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "AUTOMATIZACIÓN MIDI DE SHOW",
               IRECT(mWorkspace.L + 16.0F, mWorkspace.T + 10.0F,
                     mWorkspace.R - 16.0F, mWorkspace.T + 42.0F));

    Button(g, mMidiEnableButton,
           mapping.enabled ? "MIDI SHOW ACTIVO" : "ACTIVAR MIDI SHOW",
           mapping.enabled ? IColor(255, 18, 51, 38) : kPanelRaised,
           mapping.enabled ? kGood : kLineStrong,
           mapping.enabled ? kGood : kText);
    Button(g, mMidiChannelPrevious, "<", kPanelRaised, kLineStrong);
    const std::string channel = "CANAL " + std::to_string(mapping.channel);
    Button(g, mMidiChannelField, channel.c_str(), kPanelRaised, kLineStrong,
           kText);
    Button(g, mMidiChannelNext, ">", kPanelRaised, kLineStrong);

    constexpr std::array<const char*, 8U> labels{
        "CANCIÓN ANTERIOR",
        "SIGUIENTE CANCIÓN",
        "PLAY / REINICIAR DESDE CERO",
        "PAUSA / REANUDAR",
        "STOP / RESET A CERO",
        "REC START · INICIO CAPTURA",
        "REC STOP · FIN CAPTURA",
        "LANZAR CANCIONES 01–15"};
    const std::array<std::uint8_t, 8U> notes{
        mapping.previous_note, mapping.next_note, mapping.play_note,
        mapping.pause_note, mapping.stop_note, mapping.capture_start_note,
        mapping.capture_stop_note, mapping.launch_base_note};
    constexpr std::array<aeyla::runtime::ShowMidiLearnTarget, 8U> targets{
        aeyla::runtime::ShowMidiLearnTarget::previous_song,
        aeyla::runtime::ShowMidiLearnTarget::next_song,
        aeyla::runtime::ShowMidiLearnTarget::play_retrigger,
        aeyla::runtime::ShowMidiLearnTarget::pause_resume,
        aeyla::runtime::ShowMidiLearnTarget::stop_reset,
        aeyla::runtime::ShowMidiLearnTarget::capture_start,
        aeyla::runtime::ShowMidiLearnTarget::capture_stop,
        aeyla::runtime::ShowMidiLearnTarget::launch_song_base};
    const auto learning = mPlug.ShowMidiLearnTarget();
    for(std::size_t index = 0U; index < mMidiRows.size(); ++index)
    {
      const auto& row = mMidiRows[index];
      const bool waiting = learning == targets[index];
      g.FillRoundRect(waiting ? IColor(255, 54, 42, 22) :
                                IColor(255, 11, 13, 18), row, 6.0F);
      g.DrawRoundRect(waiting ? kWarn : kLine, row, 6.0F, nullptr, 1.0F);
      g.DrawText(IText(12.0F, kText, "AeylaUI", EAlign::Near,
                       EVAlign::Middle),
                 labels[index],
                 IRECT(row.L + 12.0F, row.T, row.L + row.W() * 0.52F, row.B));
      std::string note = "NOTA " + std::to_string(notes[index]);
      if(index == 7U)
        note += "–" + std::to_string(
            static_cast<unsigned>(notes[index]) + 14U);
      g.DrawText(IText(12.0F, waiting ? kWarn : kGood, "AeylaUI",
                       EAlign::Center, EVAlign::Middle),
                 note.c_str(),
                 IRECT(row.L + row.W() * 0.52F, row.T,
                       mMidiLearnButtons[index].L - 8.0F, row.B));
      Button(g, mMidiLearnButtons[index],
             waiting ? "ESPERANDO NOTA…" : "APRENDER MIDI",
             waiting ? IColor(255, 54, 42, 22) : kPanelRaised,
             waiting ? kWarn : kLineStrong,
             waiting ? kWarn : kText);
    }

    const std::size_t prepared = mPlug.ActiveSongIndex();
    const int active = mPlug.ActiveTakeSongIndex();
    const std::string preparedName = prepared < mPlug.SongCount()
        ? mPlug.SongName(prepared) : "SIN CANCIÓN";
    const std::string activeName = active >= 0 &&
            static_cast<std::size_t>(active) < mPlug.SongCount()
        ? mPlug.SongName(static_cast<std::size_t>(active))
        : "NINGUNA";
    const std::string preparedStatus = mPlug.SongCount() == 0U
        ? preparedName
        : std::to_string(prepared + 1U) + " · " + preparedName;
    StatusRow(g, mMidiPreparedStatus, "PREPARADA", preparedStatus, kCyan);
    StatusRow(g, mMidiActiveStatus, "AL AIRE",
              activeName, active >= 0 ? kDanger : kFaint);
    g.FillRoundRect(IColor(255, 11, 13, 18), mMidiMessageStatus, 6.0F);
    g.DrawRoundRect(kLine, mMidiMessageStatus, 6.0F, nullptr, 1.0F);
    const std::string message = mPlug.ShowMidiStatus();
    g.DrawText(IText(12.0F,
                     learning == aeyla::runtime::ShowMidiLearnTarget::none
                         ? kMuted : kWarn,
                     "AeylaUI", EAlign::Near, EVAlign::Middle),
               message.c_str(), mMidiMessageStatus.GetPadded(-10.0F));

    const float footerTop = mMidiMessageStatus.B +
        (mCompactMidi ? 4.0F : 8.0F);
    g.DrawText(IText(12.0F, kGood, "AeylaUI", EAlign::Near, EVAlign::Top),
               mCompactMidi
                   ? "SINCRONÍA: MUESTRAS DEL DAW · sin deriva acumulativa"
                   : "SINCRONÍA: MUESTRAS DEL DAW · sin reloj global ni deriva acumulativa",
               IRECT(mWorkspace.L + 18.0F, footerTop,
                     mWorkspace.R - 18.0F,
                     footerTop + (mCompactMidi ? 18.0F : 22.0F)));
    if(mCompactMidi)
    {
      g.DrawText(IText(12.0F, kWarn, "AeylaUI", EAlign::Near, EVAlign::Top),
                 ("CAPTURA: N" + std::to_string(mapping.capture_start_note) +
                  " REC START · N" + std::to_string(mapping.capture_stop_note) +
                  " REC STOP · CERO = REC START").c_str(),
                 IRECT(mWorkspace.L + 18.0F, footerTop + 19.0F,
                       mWorkspace.R - 18.0F, footerTop + 42.0F));
    }
    else
    {
      g.DrawText(IText(12.0F, kFaint, "AeylaUI", EAlign::Near, EVAlign::Top),
                 ("CAPTURA DMX: N" + std::to_string(mapping.capture_start_note) +
                  " REC START fija CERO · N" +
                  std::to_string(mapping.capture_stop_note) +
                  " REC STOP finaliza · AEYLA no usa MTC.").c_str(),
                 IRECT(mWorkspace.L + 18.0F, footerTop + 23.0F,
                       mWorkspace.R - 18.0F, footerTop + 52.0F));
      g.DrawText(IText(12.0F, kWarn, "AeylaUI", EAlign::Near, EVAlign::Top),
                 "MIDI nunca arma Art-Net ni desactiva APAGÓN. Prepara la salida manualmente antes del show.",
                 IRECT(mWorkspace.L + 18.0F, footerTop + 52.0F,
                       mWorkspace.R - 18.0F, footerTop + 78.0F));
    }
  }

  void DrawRouting(IGraphics& g)
  {
    Card(g, mRouting);
    const bool compact = mRouting.GetPadded(-12.0F).H() < 520.0F;
    g.DrawText(IText(16.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "SISTEMA · RED / SALIDA ART-NET · UNIVERSO 1",
               IRECT(mRouting.L + 12.0F, mRouting.T + 8.0F,
                     mRouting.R - 12.0F, mRouting.T + 34.0F));

    const auto capture = mPlug.ArtNetCaptureStatus();
    const auto output = mPlug.ArtNetOutputStatus();
    const bool networkBusy = mPlug.NetworkConfigurationBusy();
    const bool physicalAuthority = output.enabled || output.override_enabled;
    const bool routeSelectionBlocked =
        networkBusy || mPlug.TakeRecording() ||
        mPlug.TakeOutputArmed() || mPlug.OutputArmed() || physicalAuthority;

    DrawRouteCard(g, mRxCard, "ENTRADA / ADAPTADOR RX",
                  mPlug.RxInterfaceStatus(), mPlug.CaptureInputStatus(),
                  mRxPrevious, mRxNext,
                  capture.storage_failed ? kDanger :
                      (capture.signal_present ? kGood : kWarn),
                  routeSelectionBlocked);
    DrawRouteCard(g, mTxCard, "SALIDA / ADAPTADOR TX",
                  mPlug.TxInterfaceStatus(), mPlug.OutputBackendStatus(),
                  mTxPrevious, mTxNext,
                  output.fail_closed ? kDanger :
                      (mPlug.BackendReady() ? kGood : kWarn),
                  routeSelectionBlocked);

    Field(g, mLocalNetworkField, "IPv4 AEYLA / MÁSCARA DE SUBRED",
          mLocalNetworkText.empty() ? "clic para configurar" : mLocalNetworkText,
          mLocalNetworkText.empty() ? kWarn : kText);
    const bool networkApplyBlocked =
        networkBusy || mPlug.TakeRecording() ||
        mPlug.TakeOutputArmed() || mPlug.OutputArmed() || physicalAuthority;
    Button(g, mApplyNetworkButton,
           networkBusy ? "APLICANDO RED · ESPERA" :
               (mPlug.TakeRecording() ? "BLOQUEADA · GRABANDO" :
                  (physicalAuthority || mPlug.TakeOutputArmed() || mPlug.OutputArmed()
                      ? "BLOQUEADA · SALIDA ARMADA"
                      : "APLICAR IP Y PREPARAR ART-NET")),
           networkApplyBlocked ? IColor(255, 54, 42, 22) :
               (mPlug.BackendReady() ? IColor(255, 18, 51, 38) : kPanelRaised),
           networkApplyBlocked ? kWarn :
               (mPlug.BackendReady() ? kGood : kLineStrong),
           networkApplyBlocked ? kWarn :
               (mPlug.BackendReady() ? kGood : kText));
    const bool refreshBlocked = routeSelectionBlocked ||
        mPlug.TakeOutputArmed() || mPlug.OutputArmed();
    Button(g, mRefreshNetworkButton,
           refreshBlocked ? "ACTUALIZACIÓN BLOQUEADA" :
                            "ACTUALIZAR ADAPTADORES",
           refreshBlocked ? IColor(255, 35, 31, 25) : kPanelRaised,
           refreshBlocked ? kWarn : kLineStrong,
           refreshBlocked ? kWarn : kText);

    const float infoTop = mRefreshNetworkButton.B + 12.0F;
    char reception[220];
    if(capture.signal_present)
      std::snprintf(reception, sizeof(reception),
                    "SEÑAL PRESENTE · %.0f ms · %llu paquetes · %llu saltos",
                    capture.last_packet_age_ms,
                    static_cast<unsigned long long>(capture.packets_accepted),
                    static_cast<unsigned long long>(capture.sequence_gaps));
    else
      std::snprintf(reception, sizeof(reception),
                    "%s · U%u",
                    capture.running ? "ESPERANDO ART-NET" : "RECEPTOR DETENIDO",
                    static_cast<unsigned>(capture.port_address + 1U));

    char transmission[220];
    if(physicalAuthority && output.blackout_latched)
      std::snprintf(transmission, sizeof(transmission),
                    "APAGÓN · %u Hz · %llu paquetes negros · %llu errores",
                    static_cast<unsigned>(output.configured_fps),
                    static_cast<unsigned long long>(output.blackout_packets),
                    static_cast<unsigned long long>(output.send_errors));
    else if(physicalAuthority)
      std::snprintf(transmission, sizeof(transmission),
                    "CARRIER %u Hz · %llu paquetes · %llu errores · %llu retrasos",
                    static_cast<unsigned>(output.configured_fps),
                    static_cast<unsigned long long>(output.sent_packets),
                    static_cast<unsigned long long>(output.send_errors),
                    static_cast<unsigned long long>(output.timing_misses));
    else if(output.running)
      std::snprintf(transmission, sizeof(transmission),
                    "MOTOR LISTO · SIN CARRIER · %llu errores acumulados",
                    static_cast<unsigned long long>(output.send_errors));
    else
      std::snprintf(transmission, sizeof(transmission),
                    "MOTOR DETENIDO · %llu errores acumulados",
                    static_cast<unsigned long long>(output.send_errors));

    const std::string authority = output.fail_closed
        ? "FAIL-CLOSED · REARME MANUAL"
        : (physicalAuthority && output.blackout_latched
            ? "APAGÓN TOTAL · ARM CONSERVADO"
            : (mPlug.TakeOutputLive() ? "TOMA AL AIRE" :
                (physicalAuthority ? "ARMADA · CARRIER ACTIVO" : "DESARMADA")));
    constexpr float rowHeight = 26.0F;
    const IRECT rowBounds(mRouting.L + 14.0F, infoTop,
                          mRouting.R - 14.0F, infoTop + rowHeight);
    const IRECT transmissionRow(rowBounds.L, rowBounds.B + 3.0F,
                                rowBounds.R, rowBounds.B + 3.0F + rowHeight);
    const IRECT authorityRow(transmissionRow.L, transmissionRow.B + 3.0F,
                             transmissionRow.R,
                             transmissionRow.B + 3.0F + rowHeight);
    StatusRow(g, rowBounds, "RECEPCIÓN", reception,
              capture.signal_present ? kGood : kWarn);
    StatusRow(g, transmissionRow, "TRANSMISIÓN", transmission,
              output.fail_closed || output.consecutive_send_errors > 0U
                  ? kDanger
                  : (physicalAuthority
                      ? (output.blackout_latched ? kDanger : kGood)
                      : (output.running ? kCyan : kFaint)));
    StatusRow(g, authorityRow, "AUTORIDAD", authority,
              output.fail_closed || (physicalAuthority && output.blackout_latched)
                  ? kDanger
                  : (mPlug.TakeOutputLive() ? kDanger :
                      (physicalAuthority ? kWarn : kCyan)));

    const std::string networkStatus = mPlug.NetworkConfigurationStatus();
    g.DrawText(IText(12.0F,
                     networkBusy ? kWarn :
                         (mPlug.BackendReady() ? kGood : kFaint),
                     "AeylaUI", EAlign::Near, EVAlign::Top),
               networkStatus.c_str(),
               IRECT(mRouting.L + 14.0F, infoTop + 90.0F,
                     mRouting.R - 14.0F, infoTop + 118.0F));

    const std::string backendError = mPlug.OutputBackendError();
    if(!backendError.empty())
      g.DrawText(IText(12.0F, kWarn, "AeylaUI", EAlign::Near, EVAlign::Top),
                 backendError.c_str(),
                 IRECT(mRouting.L + 14.0F, infoTop + 118.0F,
                       mRouting.R - 14.0F, infoTop + 146.0F));

    if(compact)
    {
      if(backendError.empty() && !mMessage.empty())
        g.DrawText(IText(12.0F, kWarn, "AeylaUI", EAlign::Near, EVAlign::Top),
                   mMessage.c_str(),
                   IRECT(mRouting.L + 14.0F, infoTop + 118.0F,
                         mRouting.R - 14.0F, infoTop + 146.0F));
    }
    else
    {
      // Between roughly 694 and 710 px of host height, the routing view has
      // just enough room to leave compact mode but not enough to place this
      // panel at its old fixed offset without covering the backend error.
      const float messageTop = std::max(infoTop + 150.0F,
                                        mRouting.B - 64.0F);
      const IRECT messageRect(mRouting.L + 14.0F, messageTop,
                              mRouting.R - 14.0F, mRouting.B - 14.0F);
      g.FillRoundRect(IColor(255, 11, 13, 18), messageRect, 6.0F);
      const std::string message = mMessage.empty()
          ? "Selecciona Ethernet TX, define IPv4/máscara y aplica. La salida permanece desarmada."
          : mMessage;
      g.DrawText(IText(12.0F, mMessage.empty() ? kFaint : kWarn,
                       "AeylaUI", EAlign::Near, EVAlign::Middle),
                 message.c_str(), messageRect.GetPadded(-10.0F));
    }
  }

  void DrawRouteCard(IGraphics& g, const IRECT& rect,
                     const char* title, const std::string& adapter,
                     const std::string& signal, const IRECT& previous,
                     const IRECT& next, const IColor& statusColor,
                     bool selectionBlocked)
  {
    g.FillRoundRect(kPanelRaised, rect, 7.0F);
    g.DrawRoundRect(kLine, rect, 7.0F, nullptr, 1.0F);
    g.DrawText(IText(12.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle),
               title, IRECT(rect.L + 10.0F, rect.T + 5.0F,
                            rect.R - 10.0F, rect.T + 23.0F));
    g.DrawText(IText(12.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               adapter.c_str(), IRECT(rect.L + 10.0F, rect.T + 24.0F,
                                      rect.R - 10.0F, rect.T + 48.0F));
    g.DrawText(IText(12.0F, statusColor, "AeylaUI", EAlign::Near, EVAlign::Middle),
               signal.c_str(), IRECT(rect.L + 52.0F, rect.B - 34.0F,
                                     rect.R - 52.0F, rect.B - 7.0F));
    Button(g, previous, "<",
           selectionBlocked ? IColor(255, 35, 31, 25) : kPanel,
           selectionBlocked ? kWarn : kLineStrong,
           selectionBlocked ? kWarn : kText);
    Button(g, next, ">",
           selectionBlocked ? IColor(255, 35, 31, 25) : kPanel,
           selectionBlocked ? kWarn : kLineStrong,
           selectionBlocked ? kWarn : kText);
  }

  void Report(const aeyla::product::AuthoringResult& result)
  {
    mMessage = result.message;
    if(result.succeeded) return;
    if(auto* ui = GetUI())
      ui->ShowMessageBox(result.message.c_str(), "AEYLA · OPERACIÓN BLOQUEADA", kMB_OK);
  }

  void ReportInline(const aeyla::product::AuthoringResult& result)
  {
    mMessage = result.message;
  }

  AeylaVisualDmx& mPlug;
  std::string mMessage;
  std::string mLocalNetworkText;
  EditKind mEditKind{EditKind::none};
  DragKind mDragKind{DragKind::none};
  WorkspaceView mWorkspaceView{WorkspaceView::take_editor};
  std::size_t mEditingSongIndex{0U};
  std::filesystem::path mLastEditorPath;
  double mViewStart{0.0};
  double mViewEnd{1.0};

  IRECT mHeader{};
  IRECT mSetlist{};
  IRECT mWorkspace{};
  IRECT mRouting{};
  IRECT mBlackoutButton{};
  IRECT mTakeArmButton{};
  IRECT mTakeEditorTab{};
  IRECT mMidiShowTab{};
  IRECT mNetworkOutputTab{};
  std::array<IRECT, 15> mSongRows{};
  IRECT mNewSongButton{};
  IRECT mReturnRawButton{};
  IRECT mVersionPrevious{};
  IRECT mVersionNext{};
  IRECT mZoomOutButton{};
  IRECT mZoomResetButton{};
  IRECT mZoomInButton{};
  IRECT mTimeline{};
  IRECT mRecordButton{};
  IRECT mPlayButton{};
  IRECT mStopButton{};
  IRECT mInTimeField{};
  IRECT mInFrameMinus{};
  IRECT mInFramePlus{};
  IRECT mMarkInButton{};
  IRECT mGoInButton{};
  IRECT mOutTimeField{};
  IRECT mOutFrameMinus{};
  IRECT mOutFramePlus{};
  IRECT mMarkOutButton{};
  IRECT mGoOutButton{};
  IRECT mResetTrimButton{};
  IRECT mConsolidateButton{};
  IRECT mMidiEnableButton{};
  IRECT mMidiChannelPrevious{};
  IRECT mMidiChannelField{};
  IRECT mMidiChannelNext{};
  std::array<IRECT, 8U> mMidiRows{};
  std::array<IRECT, 8U> mMidiLearnButtons{};
  IRECT mMidiPreparedStatus{};
  IRECT mMidiActiveStatus{};
  IRECT mMidiMessageStatus{};
  bool mCompactMidi{false};
  IRECT mRxCard{};
  IRECT mTxCard{};
  IRECT mRxPrevious{};
  IRECT mRxNext{};
  IRECT mTxPrevious{};
  IRECT mTxNext{};
  IRECT mLocalNetworkField{};
  IRECT mApplyNetworkButton{};
  IRECT mRefreshNetworkButton{};
};
