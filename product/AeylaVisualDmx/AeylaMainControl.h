#pragma once

#include "AeylaVisualDmx.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
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
    BuildLayout();
    g.FillRect(kBackground, mRECT);
    DrawHeader(g);
    DrawSetlist(g);
    DrawTakeEditor(g);
    DrawRouting(g);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void)mod;
    BuildLayout();

    if(Contains(mBlackoutButton, x, y))
    {
      const bool enable = !mPlug.EffectiveBlackout();
      mPlug.SetBlackoutFromUI(enable);
      mMessage = enable
          ? "APAGÓN ACTIVO · salida desarmada."
          : "APAGÓN DESACTIVADO · el armado sigue siendo manual.";
      SetDirty(false);
      return;
    }

    if(Contains(mTakeArmButton, x, y))
    {
      Report(mPlug.ToggleTakeOutputArmFromUI());
      SetDirty(false);
      return;
    }

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

    if(Contains(mInMinus1, x, y)) { Report(mPlug.AdjustActiveTakeInFromUI(-1.0)); SetDirty(false); return; }
    if(Contains(mInMinus01, x, y)) { Report(mPlug.AdjustActiveTakeInFromUI(-0.1)); SetDirty(false); return; }
    if(Contains(mInPlus01, x, y)) { Report(mPlug.AdjustActiveTakeInFromUI(0.1)); SetDirty(false); return; }
    if(Contains(mInPlus1, x, y)) { Report(mPlug.AdjustActiveTakeInFromUI(1.0)); SetDirty(false); return; }
    if(Contains(mOutMinus1, x, y)) { Report(mPlug.AdjustActiveTakeOutFromUI(-1.0)); SetDirty(false); return; }
    if(Contains(mOutMinus01, x, y)) { Report(mPlug.AdjustActiveTakeOutFromUI(-0.1)); SetDirty(false); return; }
    if(Contains(mOutPlus01, x, y)) { Report(mPlug.AdjustActiveTakeOutFromUI(0.1)); SetDirty(false); return; }
    if(Contains(mOutPlus1, x, y)) { Report(mPlug.AdjustActiveTakeOutFromUI(1.0)); SetDirty(false); return; }
    if(Contains(mResetTrimButton, x, y)) { Report(mPlug.ResetActiveTakeTrimFromUI()); SetDirty(false); return; }
    if(Contains(mConsolidateButton, x, y)) { Report(mPlug.ConsolidateActiveTakeFromUI()); SetDirty(false); return; }

    if(Contains(mRxPrevious, x, y))
    {
      if(!mPlug.CycleRxInterfaceFromUI(-1))
        mMessage = "No se puede cambiar el adaptador RX mientras se graba.";
      SetDirty(false);
      return;
    }
    if(Contains(mRxNext, x, y))
    {
      if(!mPlug.CycleRxInterfaceFromUI(1))
        mMessage = "No se puede cambiar el adaptador RX mientras se graba.";
      SetDirty(false);
      return;
    }
    if(Contains(mTxPrevious, x, y))
    {
      (void)mPlug.CycleTxInterfaceFromUI(-1);
      RestoreNetworkFieldFromSelectedTx();
      mMessage = "Adaptador TX cambiado · verifica IPv4/máscara y luego APLICAR.";
      SetDirty(false);
      return;
    }
    if(Contains(mTxNext, x, y))
    {
      (void)mPlug.CycleTxInterfaceFromUI(1);
      RestoreNetworkFieldFromSelectedTx();
      mMessage = "Adaptador TX cambiado · verifica IPv4/máscara y luego APLICAR.";
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
      if(mPlug.RefreshNetworkInterfacesFromUI())
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

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    BuildLayout();
    const std::size_t songCount = mPlug.SongCount();
    for(std::size_t index = 0; index < songCount && index < mSongRows.size(); ++index)
    {
      if(!Contains(mSongRows[index], x, y))
        continue;
      if(mPlug.TakeRecording() || mPlug.TakePlaying())
      {
        mMessage = "Detén GRABAR/REPRODUCIR antes de renombrar una canción.";
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
      mMessage = "Red editada · presiona APLICAR RED.";
    }
    mEditKind = EditKind::none;
    SetDirty(false);
  }

private:
  enum class EditKind { none, song_name, local_network };

  inline static const IColor kBackground{255, 7, 8, 11};
  inline static const IColor kPanel{255, 14, 16, 21};
  inline static const IColor kPanelRaised{255, 21, 24, 31};
  inline static const IColor kPanelSelected{255, 30, 25, 30};
  inline static const IColor kLine{255, 43, 48, 59};
  inline static const IColor kLineStrong{255, 73, 80, 95};
  inline static const IColor kText{255, 235, 238, 242};
  inline static const IColor kMuted{255, 135, 143, 157};
  inline static const IColor kFaint{255, 88, 95, 108};
  inline static const IColor kAccent{255, 229, 48, 61};
  inline static const IColor kAccentDark{255, 84, 25, 33};
  inline static const IColor kGood{255, 70, 205, 137};
  inline static const IColor kWarn{255, 238, 159, 64};

  static bool Contains(const IRECT& rect, float x, float y) noexcept
  {
    return x >= rect.L && x <= rect.R && y >= rect.T && y <= rect.B;
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

  static bool ParseIpv4(std::string_view text, std::uint32_t& result)
  {
    result = 0U;
    std::size_t begin = 0U;
    for(int index = 0; index < 4; ++index)
    {
      const std::size_t end = index == 3 ? text.size() : text.find('.', begin);
      if(end == std::string_view::npos || end == begin) return false;
      unsigned value = 0U;
      const auto parsed = std::from_chars(text.data() + begin,
                                          text.data() + end, value);
      if(parsed.ec != std::errc{} || parsed.ptr != text.data() + end || value > 255U)
        return false;
      result = (result << 8U) | value;
      begin = end + 1U;
    }
    return begin == text.size() + 1U;
  }

  static std::string FormatIpv4(std::uint32_t value)
  {
    return std::to_string((value >> 24U) & 0xFFU) + "." +
           std::to_string((value >> 16U) & 0xFFU) + "." +
           std::to_string((value >> 8U) & 0xFFU) + "." +
           std::to_string(value & 0xFFU);
  }

  static std::string MaskFromPrefix(unsigned prefix)
  {
    if(prefix > 32U) prefix = 32U;
    const std::uint32_t mask = prefix == 0U
        ? 0U
        : static_cast<std::uint32_t>(0xFFFFFFFFULL << (32U - prefix));
    return FormatIpv4(mask);
  }

  static bool IsContiguousMask(std::uint32_t mask)
  {
    const std::uint32_t inverted = ~mask;
    return (inverted & (inverted + 1U)) == 0U;
  }

  static bool SplitNetwork(const std::string& text,
                           std::string& ipText,
                           std::string& maskText,
                           std::uint32_t& ip,
                           std::uint32_t& mask)
  {
    const auto separator = text.find('/');
    if(separator == std::string::npos) return false;
    ipText = Trim(std::string_view(text).substr(0U, separator));
    maskText = Trim(std::string_view(text).substr(separator + 1U));
    if(!ParseIpv4(ipText, ip) || !ParseIpv4(maskText, mask)) return false;
    if(mask == 0U || !IsContiguousMask(mask)) return false;
    return true;
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
    g.DrawText(IText(9.0F, text, "AeylaUI", EAlign::Center, EVAlign::Middle),
               label, rect.GetPadded(-4.0F));
  }

  static void Field(IGraphics& g, const IRECT& rect, const char* label,
                    const std::string& value, const IColor& valueColor = kText)
  {
    g.DrawText(IText(8.0F, kFaint, "AeylaUI", EAlign::Near, EVAlign::Middle),
               label, IRECT(rect.L, rect.T - 16.0F, rect.R, rect.T - 2.0F));
    g.FillRoundRect(IColor(255, 10, 12, 17), rect, 5.0F);
    g.DrawRoundRect(kLineStrong, rect, 5.0F, nullptr, 1.0F);
    g.DrawText(IText(9.0F, valueColor, "AeylaUI", EAlign::Near, EVAlign::Middle),
               value.c_str(), rect.GetPadded(-8.0F));
  }

  void BeginTextEdit(EditKind kind, const IRECT& rect, const std::string& current)
  {
    auto* ui = GetUI();
    if(ui == nullptr) return;
    mEditKind = kind;
    ui->CreateTextEntry(*this,
                        IText(10.0F, kText, "AeylaUI", EAlign::Near,
                              EVAlign::Middle),
                        rect, current.c_str(), kNoValIdx);
  }

  std::string SelectedTxIpFromStatus() const
  {
    const std::string status = mPlug.TxInterfaceStatus();
    const auto slash = status.rfind('/');
    if(slash == std::string::npos) return {};
    const auto delimiter = status.rfind(" · ", slash);
    if(delimiter == std::string::npos) return {};
    return status.substr(delimiter + 3U, slash - (delimiter + 3U));
  }

  void RestoreNetworkFieldFromSelectedTx()
  {
    const std::string status = mPlug.TxInterfaceStatus();
    const auto slash = status.rfind('/');
    if(slash == std::string::npos) return;
    const auto delimiter = status.rfind(" · ", slash);
    if(delimiter == std::string::npos) return;
    const std::string ip = status.substr(delimiter + 3U, slash - (delimiter + 3U));
    const std::string prefixText = status.substr(slash + 1U);
    unsigned prefix = 0U;
    const auto parsed = std::from_chars(prefixText.data(),
                                        prefixText.data() + prefixText.size(), prefix);
    if(parsed.ec != std::errc{} || parsed.ptr != prefixText.data() + prefixText.size())
      return;
    mLocalNetworkText = ip + " / " + MaskFromPrefix(prefix);
  }

  void ApplySimpleNetwork()
  {
    if(mPlug.TakeRecording())
    {
      mMessage = "Detén GRABAR antes de cambiar la red TX.";
      return;
    }

    // Network configuration is never hot-swapped while a Take owns output.
    mPlug.StopActiveTakePlaybackFromUI();
    if(mPlug.TakeOutputArmed())
      (void)mPlug.ToggleTakeOutputArmFromUI();
    mPlug.ForceDisarmFromUI();

    std::string ipText;
    std::string maskText;
    std::uint32_t ip = 0U;
    std::uint32_t mask = 0U;
    if(!SplitNetwork(mLocalNetworkText, ipText, maskText, ip, mask))
    {
      mMessage = "Use: 2.0.0.20 / 255.0.0.0";
      return;
    }

    const std::string selectedIp = SelectedTxIpFromStatus();
    if(selectedIp.empty())
    {
      mMessage = "Selecciona primero un adaptador TX válido.";
      return;
    }
    if(selectedIp != ipText)
    {
      mMessage = "La IPv4 local debe coincidir con el adaptador TX: " + selectedIp;
      return;
    }

    const std::uint32_t broadcast = (ip & mask) | (~mask);
    if(broadcast == ip || broadcast == 0xFFFFFFFFU)
    {
      mMessage = "La máscara no produce un broadcast dirigido utilizable.";
      return;
    }

    // AEYLA show contract: one universe only. User-facing U1 maps to Art-Net
    // Port-Address 0. The directed broadcast is derived, never typed manually.
    const std::string destination = FormatIpv4(broadcast);
    const auto result = mPlug.ConfigureArtNetFromUI(destination + "@0");
    Report(result);
    if(result.succeeded)
      mMessage = "RED LISTA · " + ipText + " / " + maskText +
                 " · U1 · SALIDA DESARMADA";
  }

  void BuildLayout()
  {
    constexpr float margin = 14.0F;
    constexpr float headerHeight = 70.0F;
    constexpr float footerReserve = 54.0F;
    constexpr float gap = 10.0F;
    const float leftWidth = std::clamp(mRECT.W() * 0.21F, 210.0F, 270.0F);
    const float rightWidth = std::clamp(mRECT.W() * 0.28F, 300.0F, 360.0F);

    mHeader = IRECT(mRECT.L + margin, mRECT.T + 8.0F,
                    mRECT.R - margin, mRECT.T + headerHeight);
    const float contentTop = mHeader.B + 6.0F;
    const float contentBottom = mRECT.B - footerReserve;
    mSetlist = IRECT(mRECT.L + margin, contentTop,
                     mRECT.L + margin + leftWidth, contentBottom);
    mRouting = IRECT(mRECT.R - margin - rightWidth, contentTop,
                     mRECT.R - margin, contentBottom);
    mWorkspace = IRECT(mSetlist.R + gap, contentTop,
                       mRouting.L - gap, contentBottom);

    mBlackoutButton = IRECT(mHeader.R - 146.0F, mHeader.T + 14.0F,
                            mHeader.R, mHeader.B - 14.0F);
    mTakeArmButton = IRECT(mHeader.R - 302.0F, mHeader.T + 14.0F,
                           mHeader.R - 154.0F, mHeader.B - 14.0F);

    const IRECT listBody = mSetlist.GetPadded(-10.0F);
    float top = listBody.T + 42.0F;
    const float rowHeight = 28.0F;
    for(std::size_t index = 0; index < mSongRows.size(); ++index)
    {
      mSongRows[index] = IRECT(listBody.L, top,
                               listBody.R, top + rowHeight - 3.0F);
      top += rowHeight;
    }
    mNewSongButton = IRECT(listBody.L, mSetlist.B - 42.0F,
                           listBody.R, mSetlist.B - 12.0F);

    const IRECT work = mWorkspace.GetPadded(-16.0F);
    mTimeline = IRECT(work.L, work.T + 102.0F, work.R, work.T + 144.0F);
    const float transportTop = mTimeline.B + 14.0F;
    const float transportGap = 8.0F;
    const float transportWidth = (work.W() - transportGap * 2.0F) / 3.0F;
    mRecordButton = IRECT(work.L, transportTop,
                          work.L + transportWidth, transportTop + 40.0F);
    mPlayButton = IRECT(mRecordButton.R + transportGap, transportTop,
                        mRecordButton.R + transportGap + transportWidth,
                        transportTop + 40.0F);
    mStopButton = IRECT(mPlayButton.R + transportGap, transportTop,
                        work.R, transportTop + 40.0F);

    const float editorTop = mStopButton.B + 36.0F;
    const float smallGap = 6.0F;
    const float labelWidth = 48.0F;
    const float buttonWidth = (work.W() - labelWidth - smallGap * 4.0F) / 4.0F;
    float left = work.L + labelWidth;
    mInMinus1 = IRECT(left, editorTop, left + buttonWidth, editorTop + 30.0F); left += buttonWidth + smallGap;
    mInMinus01 = IRECT(left, editorTop, left + buttonWidth, editorTop + 30.0F); left += buttonWidth + smallGap;
    mInPlus01 = IRECT(left, editorTop, left + buttonWidth, editorTop + 30.0F); left += buttonWidth + smallGap;
    mInPlus1 = IRECT(left, editorTop, work.R, editorTop + 30.0F);

    left = work.L + labelWidth;
    const float outTop = editorTop + 48.0F;
    mOutMinus1 = IRECT(left, outTop, left + buttonWidth, outTop + 30.0F); left += buttonWidth + smallGap;
    mOutMinus01 = IRECT(left, outTop, left + buttonWidth, outTop + 30.0F); left += buttonWidth + smallGap;
    mOutPlus01 = IRECT(left, outTop, left + buttonWidth, outTop + 30.0F); left += buttonWidth + smallGap;
    mOutPlus1 = IRECT(left, outTop, work.R, outTop + 30.0F);
    mResetTrimButton = IRECT(work.L, outTop + 45.0F, work.R, outTop + 76.0F);
    mConsolidateButton = IRECT(work.L, mResetTrimButton.B + 8.0F,
                               work.R, mResetTrimButton.B + 43.0F);

    const IRECT route = mRouting.GetPadded(-12.0F);
    const float cardTop = route.T + 46.0F;
    mRxCard = IRECT(route.L, cardTop, route.R, cardTop + 108.0F);
    mTxCard = IRECT(route.L, mRxCard.B + 10.0F, route.R, mRxCard.B + 118.0F);
    mRxPrevious = IRECT(mRxCard.L + 10.0F, mRxCard.B - 34.0F,
                        mRxCard.L + 46.0F, mRxCard.B - 8.0F);
    mRxNext = IRECT(mRxCard.R - 46.0F, mRxCard.B - 34.0F,
                    mRxCard.R - 10.0F, mRxCard.B - 8.0F);
    mTxPrevious = IRECT(mTxCard.L + 10.0F, mTxCard.B - 34.0F,
                        mTxCard.L + 46.0F, mTxCard.B - 8.0F);
    mTxNext = IRECT(mTxCard.R - 46.0F, mTxCard.B - 34.0F,
                    mTxCard.R - 10.0F, mTxCard.B - 8.0F);

    mLocalNetworkField = IRECT(route.L, mTxCard.B + 36.0F,
                               route.R, mTxCard.B + 70.0F);
    mApplyNetworkButton = IRECT(route.L, mLocalNetworkField.B + 12.0F,
                                route.R, mLocalNetworkField.B + 46.0F);
    mRefreshNetworkButton = IRECT(route.L, mApplyNetworkButton.B + 8.0F,
                                  route.R, mApplyNetworkButton.B + 38.0F);
  }

  void DrawHeader(IGraphics& g)
  {
    g.DrawText(IText(20.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "AEYLA  /  REPRODUCTOR DE SHOW",
               IRECT(mHeader.L, mHeader.T, mHeader.L + 360.0F, mHeader.B - 20.0F));
    g.DrawText(IText(9.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle),
               ("RGB ESTUDIOS · " + mPlug.ProjectName() + " · R07 PRETEST").c_str(),
               IRECT(mHeader.L, mHeader.B - 27.0F,
                     mHeader.L + 620.0F, mHeader.B));

    const bool blackout = mPlug.EffectiveBlackout();
    Button(g, mBlackoutButton, blackout ? "APAGÓN ACTIVO" : "APAGÓN DESACTIVADO",
           blackout ? kAccentDark : kPanelRaised,
           blackout ? kAccent : kLineStrong,
           blackout ? kText : kGood);

    const bool armed = mPlug.TakeOutputArmed();
    Button(g, mTakeArmButton,
           armed ? "SALIDA DE TOMA ARMADA" : "ARMAR SALIDA DE TOMA",
           armed ? kGood : kPanelRaised,
           armed ? kGood : kLineStrong,
           armed ? IColor(255, 8, 30, 20) : kText);
  }

  void DrawSetlist(IGraphics& g)
  {
    Card(g, mSetlist);
    g.DrawText(IText(10.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "LISTA DE CANCIONES · DOBLE CLIC = RENOMBRAR",
               IRECT(mSetlist.L + 12.0F, mSetlist.T + 8.0F,
                     mSetlist.R - 12.0F, mSetlist.T + 36.0F));

    const std::size_t count = mPlug.SongCount();
    const std::size_t active = mPlug.ActiveSongIndex();
    for(std::size_t index = 0; index < mSongRows.size(); ++index)
    {
      const IRECT& row = mSongRows[index];
      if(index >= count)
      {
        g.DrawLine(kLine, row.L, row.B, row.R, row.B, nullptr, 1.0F);
        continue;
      }
      const bool selected = index == active;
      if(selected) g.FillRoundRect(kPanelSelected, row, 5.0F);
      if(selected) g.DrawRoundRect(kAccent, row, 5.0F, nullptr, 1.0F);
      char number[8];
      std::snprintf(number, sizeof(number), "%02d", static_cast<int>(index + 1U));
      g.DrawText(IText(9.0F, selected ? kAccent : kFaint,
                       "AeylaUI", EAlign::Near, EVAlign::Middle),
                 number, IRECT(row.L + 8.0F, row.T, row.L + 34.0F, row.B));
      std::string name = mPlug.SongName(index);
      if(name.empty()) name = "Canción";
      g.DrawText(IText(9.0F, selected ? kText : kMuted,
                       "AeylaUI", EAlign::Near, EVAlign::Middle),
                 name.c_str(), IRECT(row.L + 38.0F, row.T, row.R - 8.0F, row.B));
    }

    Button(g, mNewSongButton,
           count >= 15U ? "LÍMITE: 15 CANCIONES" : "+ NUEVA CANCIÓN",
           kPanelRaised, kLineStrong, count >= 15U ? kFaint : kText);
  }

  void DrawTakeEditor(IGraphics& g)
  {
    Card(g, mWorkspace);
    const IRECT work = mWorkspace.GetPadded(-16.0F);
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
               title.c_str(), IRECT(work.L, work.T, work.R, work.T + 34.0F));
    g.DrawText(IText(10.0F, mPlug.TakeRecording() ? kAccent : kMuted,
                     "AeylaUI", EAlign::Near, EVAlign::Middle),
               mPlug.ActiveTakeStatus().c_str(),
               IRECT(work.L, work.T + 38.0F, work.R, work.T + 66.0F));

    const double original = mPlug.ActiveTakeOriginalDurationSeconds();
    const double inTime = mPlug.ActiveTakeInSeconds();
    const double outTime = mPlug.ActiveTakeOutSeconds();
    const double effective = mPlug.ActiveTakeEffectiveDurationSeconds();

    g.FillRoundRect(IColor(255, 9, 11, 15), mTimeline, 5.0F);
    g.DrawRoundRect(kLine, mTimeline, 5.0F, nullptr, 1.0F);
    if(original > 0.0)
    {
      const float inX = mTimeline.L + static_cast<float>(std::clamp(inTime / original, 0.0, 1.0)) * mTimeline.W();
      const float outX = mTimeline.L + static_cast<float>(std::clamp(outTime / original, 0.0, 1.0)) * mTimeline.W();
      if(inX > mTimeline.L)
        g.FillRect(IColor(255, 24, 25, 29), IRECT(mTimeline.L, mTimeline.T, inX, mTimeline.B));
      if(outX < mTimeline.R)
        g.FillRect(IColor(255, 24, 25, 29), IRECT(outX, mTimeline.T, mTimeline.R, mTimeline.B));
      if(outX > inX)
        g.DrawRect(kGood, IRECT(inX, mTimeline.T + 2.0F, outX, mTimeline.B - 2.0F), nullptr, 1.0F);
    }
    const double progress = mPlug.ActiveTakePlaybackProgress();
    if(progress > 0.0)
    {
      const float x = mTimeline.L + static_cast<float>(std::clamp(progress, 0.0, 1.0)) * mTimeline.W();
      g.DrawLine(kAccent, x, mTimeline.T, x, mTimeline.B, nullptr, 2.0F);
    }

    Button(g, mRecordButton,
           mPlug.TakeRecording() ? "DETENER + GUARDAR TOMA" : "GRABAR NUEVA TOMA",
           mPlug.TakeRecording() ? kAccentDark : kPanelRaised,
           mPlug.TakeRecording() ? kAccent : kLineStrong);
    Button(g, mPlayButton,
           mPlug.TakePlaying() ? "REPRODUCIENDO" : "REPRODUCIR TOMA ACTIVA",
           mPlug.TakePlaying() ? kGood : kPanelRaised,
           mPlug.TakePlaying() ? kGood : kLineStrong,
           mPlug.TakePlaying() ? IColor(255, 8, 30, 20) : kText);
    Button(g, mStopButton, "DETENER / MANTENER", kPanelRaised, kLineStrong);

    const float editorTop = mInMinus1.T;
    g.DrawText(IText(10.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "ENT.", IRECT(work.L, editorTop, work.L + 42.0F, editorTop + 30.0F));
    Button(g, mInMinus1, "-1.0s", kPanelRaised, kLineStrong);
    Button(g, mInMinus01, "-0.1s", kPanelRaised, kLineStrong);
    Button(g, mInPlus01, "+0.1s", kPanelRaised, kLineStrong);
    Button(g, mInPlus1, "+1.0s", kPanelRaised, kLineStrong);

    const float outTop = mOutMinus1.T;
    g.DrawText(IText(10.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "SAL.", IRECT(work.L, outTop, work.L + 42.0F, outTop + 30.0F));
    Button(g, mOutMinus1, "-1.0s", kPanelRaised, kLineStrong);
    Button(g, mOutMinus01, "-0.1s", kPanelRaised, kLineStrong);
    Button(g, mOutPlus01, "+0.1s", kPanelRaised, kLineStrong);
    Button(g, mOutPlus1, "+1.0s", kPanelRaised, kLineStrong);
    Button(g, mResetTrimButton, "RESTAURAR ENTRADA / SALIDA", kPanelRaised, kLineStrong);
    Button(g, mConsolidateButton, "CONSOLIDAR CLIP",
           IColor(255, 18, 51, 38), kGood, kGood);

    const float infoTop = mConsolidateButton.B + 8.0F;
    const std::string times = "ENTRADA " + FormatTime(inTime) +
        "   ·   SALIDA " + FormatTime(outTime) +
        "   ·   DURACIÓN " + FormatTime(effective) +
        "   ·   ORIGINAL " + FormatTime(original);
    g.DrawText(IText(9.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle),
               times.c_str(), IRECT(work.L, infoTop, work.R, infoTop + 28.0F));

    const IRECT messageRect(work.L, work.B - 48.0F, work.R, work.B);
    g.FillRoundRect(IColor(255, 11, 13, 18), messageRect, 6.0F);
    const std::string message = mMessage.empty()
        ? "GRABA desde Avolites · ajusta ENTRADA/SALIDA · CONSOLIDA · ARMA · REPRODUCE."
        : mMessage;
    g.DrawText(IText(8.8F, mMessage.empty() ? kFaint : kWarn,
                     "AeylaUI", EAlign::Near, EVAlign::Middle),
               message.c_str(), messageRect.GetPadded(-10.0F));
  }

  void DrawRouting(IGraphics& g)
  {
    Card(g, mRouting);
    g.DrawText(IText(12.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "RED ART-NET · U1",
               IRECT(mRouting.L + 12.0F, mRouting.T + 8.0F,
                     mRouting.R - 12.0F, mRouting.T + 34.0F));

    DrawRouteCard(g, mRxCard, "ENTRADA / ADAPTADOR RX",
                  mPlug.RxInterfaceStatus(), mPlug.CaptureInputStatus(),
                  mRxPrevious, mRxNext,
                  mPlug.CaptureAcceptedPackets() > 0U ? kGood : kWarn);
    DrawRouteCard(g, mTxCard, "SALIDA / ADAPTADOR TX",
                  mPlug.TxInterfaceStatus(), mPlug.OutputBackendStatus(),
                  mTxPrevious, mTxNext,
                  mPlug.BackendReady() ? kGood : kWarn);

    Field(g, mLocalNetworkField, "IPv4 LOCAL / MÁSCARA DE SUBRED",
          mLocalNetworkText.empty() ? "clic para configurar" : mLocalNetworkText,
          mLocalNetworkText.empty() ? kWarn : kText);
    Button(g, mApplyNetworkButton, "APLICAR RED",
           mPlug.BackendReady() ? IColor(255, 18, 51, 38) : kPanelRaised,
           mPlug.BackendReady() ? kGood : kLineStrong,
           mPlug.BackendReady() ? kGood : kText);
    Button(g, mRefreshNetworkButton, "ACTUALIZAR ADAPTADORES",
           kPanelRaised, kLineStrong);

    const float infoTop = mRefreshNetworkButton.B + 12.0F;
    char diagnostics[180];
    std::snprintf(diagnostics, sizeof(diagnostics),
                  "RX %llu · saltos %llu   |   TX %llu · errores %llu   |   %s",
                  static_cast<unsigned long long>(mPlug.CaptureAcceptedPackets()),
                  static_cast<unsigned long long>(mPlug.CaptureSequenceGaps()),
                  static_cast<unsigned long long>(mPlug.ArtNetSentPackets()),
                  static_cast<unsigned long long>(mPlug.ArtNetSendErrors()),
                  mPlug.TakeOutputArmed() ? "AL AIRE" : "DESARMADA");
    g.DrawText(IText(8.2F,
                     mPlug.ArtNetSendErrors() > 0U ? kWarn : kFaint,
                     "AeylaUI", EAlign::Near, EVAlign::Top),
               diagnostics,
               IRECT(mRouting.L + 14.0F, infoTop,
                     mRouting.R - 14.0F, infoTop + 30.0F));

    const std::string backendError = mPlug.OutputBackendError();
    if(!backendError.empty())
      g.DrawText(IText(8.2F, kWarn, "AeylaUI", EAlign::Near, EVAlign::Top),
                 backendError.c_str(),
                 IRECT(mRouting.L + 14.0F, infoTop + 28.0F,
                       mRouting.R - 14.0F, infoTop + 72.0F));
  }

  void DrawRouteCard(IGraphics& g, const IRECT& rect,
                     const char* title, const std::string& adapter,
                     const std::string& signal, const IRECT& previous,
                     const IRECT& next, const IColor& statusColor)
  {
    g.FillRoundRect(kPanelRaised, rect, 7.0F);
    g.DrawRoundRect(kLine, rect, 7.0F, nullptr, 1.0F);
    g.DrawText(IText(8.5F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle),
               title, IRECT(rect.L + 10.0F, rect.T + 5.0F,
                            rect.R - 10.0F, rect.T + 23.0F));
    g.DrawText(IText(8.2F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               adapter.c_str(), IRECT(rect.L + 10.0F, rect.T + 24.0F,
                                      rect.R - 10.0F, rect.T + 48.0F));
    g.DrawText(IText(8.0F, statusColor, "AeylaUI", EAlign::Near, EVAlign::Middle),
               signal.c_str(), IRECT(rect.L + 52.0F, rect.B - 34.0F,
                                     rect.R - 52.0F, rect.B - 7.0F));
    Button(g, previous, "<", kPanel, kLineStrong);
    Button(g, next, ">", kPanel, kLineStrong);
  }

  void Report(const aeyla::product::AuthoringResult& result)
  {
    mMessage = result.message;
    if(result.succeeded) return;
    if(auto* ui = GetUI())
      ui->ShowMessageBox(result.message.c_str(), "AEYLA · OPERACIÓN BLOQUEADA", kMB_OK);
  }

  AeylaVisualDmx& mPlug;
  std::string mMessage;
  std::string mLocalNetworkText;
  EditKind mEditKind{EditKind::none};
  std::size_t mEditingSongIndex{0U};

  IRECT mHeader{};
  IRECT mSetlist{};
  IRECT mWorkspace{};
  IRECT mRouting{};
  IRECT mBlackoutButton{};
  IRECT mTakeArmButton{};
  std::array<IRECT, 15> mSongRows{};
  IRECT mNewSongButton{};
  IRECT mTimeline{};
  IRECT mRecordButton{};
  IRECT mPlayButton{};
  IRECT mStopButton{};
  IRECT mInMinus1{};
  IRECT mInMinus01{};
  IRECT mInPlus01{};
  IRECT mInPlus1{};
  IRECT mOutMinus1{};
  IRECT mOutMinus01{};
  IRECT mOutPlus01{};
  IRECT mOutPlus1{};
  IRECT mResetTrimButton{};
  IRECT mConsolidateButton{};
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
