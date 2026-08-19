#pragma once

#include "AeylaVisualDmx.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
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
    RestoreRouteFieldsFromBackend();
  }

  void Draw(IGraphics& g) override
  {
    BuildLayout();
    g.FillRect(kBackground, mRECT);
    DrawHeader(g);
    DrawSetlist(g);
    DrawSongWorkspace(g);
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
          ? "BLACKOUT latched · outputs disarmed once. Disable BLACKOUT, then ARM manually."
          : "BLACKOUT released · output remains DISARMED until you ARM it.";
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
      if(Contains(mSongRows[index], x, y))
      {
        if(!mPlug.SelectSongFromUI(index))
          mMessage = "Song switch blocked while capture is active.";
        SetDirty(false);
        return;
      }
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
      mMessage = "Playback stopped · HOLD current DMX frame.";
      SetDirty(false);
      return;
    }

    if(Contains(mRxPrevious, x, y))
    {
      if(!mPlug.CycleRxInterfaceFromUI(-1))
        mMessage = "RX adapter change blocked while recording.";
      SetDirty(false);
      return;
    }
    if(Contains(mRxNext, x, y))
    {
      if(!mPlug.CycleRxInterfaceFromUI(1))
        mMessage = "RX adapter change blocked while recording.";
      SetDirty(false);
      return;
    }
    if(Contains(mTxPrevious, x, y))
    {
      (void)mPlug.CycleTxInterfaceFromUI(-1);
      mMessage = "TX NIC changed · route restarted DISARMED.";
      SetDirty(false);
      return;
    }
    if(Contains(mTxNext, x, y))
    {
      (void)mPlug.CycleTxInterfaceFromUI(1);
      mMessage = "TX NIC changed · route restarted DISARMED.";
      SetDirty(false);
      return;
    }

    if(Contains(mRouteNameField, x, y))
    {
      BeginTextEdit(EditKind::route_name, mRouteNameField,
                    mRouteName.empty() ? "MAIN ARTNET" : mRouteName);
      return;
    }
    if(Contains(mTargetIpField, x, y))
    {
      BeginTextEdit(EditKind::target_ip, mTargetIpField, mTargetIp);
      return;
    }
    if(Contains(mUniverseField, x, y))
    {
      BeginTextEdit(EditKind::universe, mUniverseField,
                    std::to_string(mUniverseDisplay));
      return;
    }
    if(Contains(mApplyOutputButton, x, y))
    {
      ApplyOutputRoute();
      SetDirty(false);
      return;
    }

    if(Contains(mRefreshNetworkButton, x, y))
    {
      if(mPlug.RefreshNetworkInterfacesFromUI())
        mMessage = "Network adapters refreshed · verify RX/TX NIC before ARM.";
      else
        mMessage = "No active IPv4 adapters detected.";
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
        mMessage = "Stop REC/PLAY before renaming a Song.";
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
    const std::string value = str == nullptr ? std::string{} : std::string(str);
    switch(mEditKind)
    {
      case EditKind::song_name:
        Report(mPlug.RenameSongFromUI(mEditingSongIndex, value));
        break;
      case EditKind::route_name:
        mRouteName = Trim(value);
        if(mRouteName.empty()) mRouteName = "MAIN ARTNET";
        mMessage = "Route label updated · configuration unchanged.";
        break;
      case EditKind::target_ip:
        mTargetIp = Trim(value);
        mMessage = "Target edited · press APPLY / VALIDATE OUTPUT.";
        break;
      case EditKind::universe:
      {
        const std::string normalized = Trim(value);
        unsigned parsed = 0U;
        const auto result = std::from_chars(
            normalized.data(), normalized.data() + normalized.size(), parsed);
        if(result.ec != std::errc{} ||
           result.ptr != normalized.data() + normalized.size() ||
           parsed < 1U || parsed > 32768U)
          mMessage = "Universe must be 1..32768. AEYLA uses Universe 1 for this show.";
        else
        {
          mUniverseDisplay = parsed;
          mMessage = "Universe edited · press APPLY / VALIDATE OUTPUT.";
        }
        break;
      }
      case EditKind::none:
        break;
    }
    mEditKind = EditKind::none;
    SetDirty(false);
  }

private:
  enum class EditKind { none, song_name, route_name, target_ip, universe };

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
    g.DrawText(IText(10.0F, text, "AeylaUI", EAlign::Center, EVAlign::Middle),
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

  void RestoreRouteFieldsFromBackend()
  {
    const std::string status = mPlug.OutputBackendStatus();
    constexpr std::string_view prefix = "ARTNET ";
    if(status.rfind(prefix.data(), 0U) != 0U)
      return;
    const auto separator = status.rfind('@');
    if(separator == std::string::npos || separator <= prefix.size())
      return;
    mTargetIp = status.substr(prefix.size(), separator - prefix.size());
    const std::string raw = status.substr(separator + 1U);
    unsigned portAddress = 0U;
    const auto parsed = std::from_chars(raw.data(), raw.data() + raw.size(), portAddress);
    if(parsed.ec == std::errc{} && parsed.ptr == raw.data() + raw.size() &&
       portAddress <= 32767U)
      mUniverseDisplay = portAddress + 1U;
  }

  void ApplyOutputRoute()
  {
    if(mTargetIp.empty())
    {
      mMessage = "Set TARGET IP first.";
      return;
    }
    if(mUniverseDisplay < 1U || mUniverseDisplay > 32768U)
    {
      mMessage = "Universe must be 1..32768.";
      return;
    }

    const unsigned rawPortAddress = mUniverseDisplay - 1U;
    const std::string specification =
        mTargetIp + "@" + std::to_string(rawPortAddress);
    const auto result = mPlug.ConfigureArtNetFromUI(specification);
    Report(result);
    if(result.succeeded)
      mMessage = (mRouteName.empty() ? "MAIN ARTNET" : mRouteName) +
          " · VALIDATED · " + mTargetIp + " · U" +
          std::to_string(mUniverseDisplay) + " · DISARMED";
  }

  void BuildLayout()
  {
    constexpr float margin = 14.0F;
    constexpr float headerHeight = 70.0F;
    constexpr float footerReserve = 54.0F;
    constexpr float gap = 10.0F;
    const float leftWidth = std::clamp(mRECT.W() * 0.22F, 218.0F, 286.0F);
    const float rightWidth = std::clamp(mRECT.W() * 0.31F, 324.0F, 410.0F);

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
    mTimeline = IRECT(work.L, work.T + 128.0F, work.R, work.T + 166.0F);
    const float buttonTop = mTimeline.B + 18.0F;
    const float transportGap = 8.0F;
    const float transportWidth = (work.W() - transportGap * 2.0F) / 3.0F;
    mRecordButton = IRECT(work.L, buttonTop,
                          work.L + transportWidth, buttonTop + 42.0F);
    mPlayButton = IRECT(mRecordButton.R + transportGap, buttonTop,
                        mRecordButton.R + transportGap + transportWidth,
                        buttonTop + 42.0F);
    mStopButton = IRECT(mPlayButton.R + transportGap, buttonTop,
                        work.R, buttonTop + 42.0F);

    const IRECT route = mRouting.GetPadded(-12.0F);
    const float cardTop = route.T + 42.0F;
    mRxCard = IRECT(route.L, cardTop, route.R, cardTop + 96.0F);
    mTxCard = IRECT(route.L, mRxCard.B + 8.0F,
                    route.R, mRxCard.B + 104.0F);
    mRxPrevious = IRECT(mRxCard.L + 10.0F, mRxCard.B - 34.0F,
                        mRxCard.L + 45.0F, mRxCard.B - 7.0F);
    mRxNext = IRECT(mRxCard.R - 45.0F, mRxCard.B - 34.0F,
                    mRxCard.R - 10.0F, mRxCard.B - 7.0F);
    mTxPrevious = IRECT(mTxCard.L + 10.0F, mTxCard.B - 34.0F,
                        mTxCard.L + 45.0F, mTxCard.B - 7.0F);
    mTxNext = IRECT(mTxCard.R - 45.0F, mTxCard.B - 34.0F,
                    mTxCard.R - 10.0F, mTxCard.B - 7.0F);

    const float fieldTop = mTxCard.B + 29.0F;
    mRouteNameField = IRECT(route.L, fieldTop, route.R, fieldTop + 31.0F);
    mTargetIpField = IRECT(route.L, mRouteNameField.B + 23.0F,
                           route.R - 82.0F, mRouteNameField.B + 54.0F);
    mUniverseField = IRECT(route.R - 72.0F, mRouteNameField.B + 23.0F,
                           route.R, mRouteNameField.B + 54.0F);
    mApplyOutputButton = IRECT(route.L, mTargetIpField.B + 10.0F,
                               route.R, mTargetIpField.B + 43.0F);
    mRefreshNetworkButton = IRECT(route.L, mApplyOutputButton.B + 8.0F,
                                  route.R, mApplyOutputButton.B + 37.0F);
  }

  void DrawHeader(IGraphics& g)
  {
    g.DrawText(IText(20.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "AEYLA  /  SHOW PLAYER",
               IRECT(mHeader.L, mHeader.T, mHeader.L + 360.0F, mHeader.B - 20.0F));
    const std::string project = mPlug.ProjectName();
    g.DrawText(IText(9.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle),
               ("RGB ESTUDIOS · " + project +
                " · AVOLITES AUTHORING / DAW PLAYBACK").c_str(),
               IRECT(mHeader.L, mHeader.B - 27.0F,
                     mHeader.L + 620.0F, mHeader.B));

    const bool blackout = mPlug.EffectiveBlackout();
    Button(g, mBlackoutButton, blackout ? "BLACKOUT ON" : "BLACKOUT OFF",
           blackout ? kAccentDark : kPanelRaised,
           blackout ? kAccent : kLineStrong,
           blackout ? kText : kGood);

    const bool takeArmed = mPlug.TakeOutputArmed();
    Button(g, mTakeArmButton,
           takeArmed ? "TAKE OUTPUT ARMED" : "ARM TAKE OUTPUT",
           takeArmed ? kGood : kPanelRaised,
           takeArmed ? kGood : kLineStrong,
           takeArmed ? IColor(255, 8, 30, 20) : kText);
  }

  void DrawSetlist(IGraphics& g)
  {
    Card(g, mSetlist);
    g.DrawText(IText(10.5F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "SETLIST · DOUBLE CLICK TO RENAME",
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
      if(selected)
        g.FillRoundRect(kPanelSelected, row, 5.0F);
      if(selected)
        g.DrawRoundRect(kAccent, row, 5.0F, nullptr, 1.0F);

      char number[8];
      std::snprintf(number, sizeof(number), "%02d", static_cast<int>(index + 1U));
      g.DrawText(IText(9.0F, selected ? kAccent : kFaint,
                       "AeylaUI", EAlign::Near, EVAlign::Middle),
                 number,
                 IRECT(row.L + 8.0F, row.T, row.L + 34.0F, row.B));
      std::string name = mPlug.SongName(index);
      if(name.empty()) name = "Song";
      g.DrawText(IText(9.0F, selected ? kText : kMuted,
                       "AeylaUI", EAlign::Near, EVAlign::Middle),
                 name.c_str(),
                 IRECT(row.L + 38.0F, row.T, row.R - 8.0F, row.B));
    }

    Button(g, mNewSongButton,
           count >= 15U ? "15 SONG LIMIT" : "+ NEW SONG",
           kPanelRaised, kLineStrong, count >= 15U ? kFaint : kText);
  }

  void DrawSongWorkspace(IGraphics& g)
  {
    Card(g, mWorkspace);
    const IRECT work = mWorkspace.GetPadded(-16.0F);
    const std::size_t count = mPlug.SongCount();
    const std::size_t active = mPlug.ActiveSongIndex();

    std::string title = "NO SONG SELECTED";
    if(count > 0U)
    {
      char prefix[32];
      std::snprintf(prefix, sizeof(prefix), "%02d / %02d  ·  ",
                    static_cast<int>(active + 1U), static_cast<int>(count));
      title = prefix + mPlug.SongName(active);
    }
    g.DrawText(IText(18.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               title.c_str(), IRECT(work.L, work.T, work.R, work.T + 36.0F));
    g.DrawText(IText(9.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "AVOLITES TITAN  →  ART-NET U1  →  .AEYLATAKE",
               IRECT(work.L, work.T + 39.0F, work.R, work.T + 61.0F));

    const std::string take = mPlug.ActiveTakeStatus();
    g.DrawText(IText(11.0F, mPlug.TakeRecording() ? kAccent : kText,
                     "AeylaUI", EAlign::Near, EVAlign::Middle),
               take.c_str(), IRECT(work.L, work.T + 74.0F,
                                   work.R, work.T + 104.0F));

    g.FillRoundRect(IColor(255, 9, 11, 15), mTimeline, 5.0F);
    g.DrawRoundRect(kLine, mTimeline, 5.0F, nullptr, 1.0F);
    const double progress = mPlug.ActiveTakePlaybackProgress();
    if(progress > 0.0)
    {
      const float right = mTimeline.L +
          static_cast<float>(std::clamp(progress, 0.0, 1.0)) * mTimeline.W();
      g.FillRoundRect(kAccentDark,
                      IRECT(mTimeline.L, mTimeline.T, right, mTimeline.B), 5.0F);
    }
    char percent[32];
    std::snprintf(percent, sizeof(percent), "%03d%%",
                  static_cast<int>(std::clamp(progress, 0.0, 1.0) * 100.0));
    g.DrawText(IText(9.0F, kMuted, "AeylaUI", EAlign::Far, EVAlign::Middle),
               percent, mTimeline.GetPadded(-10.0F));

    Button(g, mRecordButton,
           mPlug.TakeRecording() ? "STOP + SAVE TAKE" : "RECORD NEW TAKE",
           mPlug.TakeRecording() ? kAccentDark : kPanelRaised,
           mPlug.TakeRecording() ? kAccent : kLineStrong);
    Button(g, mPlayButton,
           mPlug.TakePlaying() ? "STOP / HOLD" : "PLAY ACTIVE TAKE",
           mPlug.TakePlaying() ? kGood : kPanelRaised,
           mPlug.TakePlaying() ? kGood : kLineStrong,
           mPlug.TakePlaying() ? IColor(255, 8, 30, 20) : kText);
    Button(g, mStopButton, "STOP / HOLD", kPanelRaised, kLineStrong);

    const float infoTop = mStopButton.B + 28.0F;
    g.DrawText(IText(10.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "TAKE PLAYBACK",
               IRECT(work.L, infoTop, work.R, infoTop + 24.0F));
    g.DrawText(IText(9.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Top),
               "· normalized DMX recording at 44 Hz\n"
               "· Takes persist as portable .aeylatake files\n"
               "· STOP holds the current frame; DISARM removes authority\n"
               "· IN / OUT trim is the next gate after verified physical TX\n"
               "· transition curves will affect only explicitly safe channels",
               IRECT(work.L, infoTop + 27.0F, work.R,
                     std::min(work.B - 58.0F, infoTop + 138.0F)));

    const IRECT messageRect(work.L, work.B - 48.0F, work.R, work.B);
    g.FillRoundRect(IColor(255, 11, 13, 18), messageRect, 6.0F);
    const std::string message = mMessage.empty()
        ? "Configure OUTPUT ROUTE, validate it, release BLACKOUT, ARM, then PLAY."
        : mMessage;
    g.DrawText(IText(9.0F, mMessage.empty() ? kFaint : kWarn,
                     "AeylaUI", EAlign::Near, EVAlign::Middle),
               message.c_str(), messageRect.GetPadded(-10.0F));
  }

  void DrawRouting(IGraphics& g)
  {
    Card(g, mRouting);
    g.DrawText(IText(12.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "ROUTING / OUTPUT",
               IRECT(mRouting.L + 12.0F, mRouting.T + 8.0F,
                     mRouting.R - 12.0F, mRouting.T + 34.0F));

    DrawRouteCard(g, mRxCard, "ART-NET INPUT / RX NIC",
                  mPlug.RxInterfaceStatus(), mPlug.CaptureInputStatus(),
                  mRxPrevious, mRxNext,
                  mPlug.CaptureAcceptedPackets() > 0U ? kGood : kWarn);
    DrawRouteCard(g, mTxCard, "ART-NET OUTPUT / TX NIC",
                  mPlug.TxInterfaceStatus(), mPlug.OutputBackendStatus(),
                  mTxPrevious, mTxNext,
                  mPlug.BackendReady() ? kGood : kWarn);

    Field(g, mRouteNameField, "ROUTE NAME", mRouteName,
          mRouteName.empty() ? kFaint : kText);
    Field(g, mTargetIpField, "TARGET IP / NODE",
          mTargetIp.empty() ? "click to set" : mTargetIp,
          mTargetIp.empty() ? kWarn : kText);
    Field(g, mUniverseField, "UNIVERSE",
          "U" + std::to_string(mUniverseDisplay));

    Button(g, mApplyOutputButton, "APPLY / VALIDATE OUTPUT",
           mPlug.BackendReady() ? IColor(255, 18, 51, 38) : kPanelRaised,
           mPlug.BackendReady() ? kGood : kLineStrong,
           mPlug.BackendReady() ? kGood : kText);
    Button(g, mRefreshNetworkButton, "REFRESH NETWORK ADAPTERS",
           kPanelRaised, kLineStrong);

    const float infoTop = mRefreshNetworkButton.B + 8.0F;
    char diagnostics[192];
    std::snprintf(diagnostics, sizeof(diagnostics),
                  "RX %llu pkt · gap %llu   |   TX %llu pkt · err %llu   |   %s",
                  static_cast<unsigned long long>(mPlug.CaptureAcceptedPackets()),
                  static_cast<unsigned long long>(mPlug.CaptureSequenceGaps()),
                  static_cast<unsigned long long>(mPlug.ArtNetSentPackets()),
                  static_cast<unsigned long long>(mPlug.ArtNetSendErrors()),
                  mPlug.TakeOutputArmed() ? "ON AIR" : "DISARMED");
    g.DrawText(IText(8.2F,
                     mPlug.ArtNetSendErrors() > 0U ? kWarn : kFaint,
                     "AeylaUI", EAlign::Near, EVAlign::Top),
               diagnostics,
               IRECT(mRouting.L + 14.0F, infoTop,
                     mRouting.R - 14.0F, infoTop + 28.0F));

    const std::string backendError = mPlug.OutputBackendError();
    if(!backendError.empty())
      g.DrawText(IText(8.2F, kWarn, "AeylaUI", EAlign::Near, EVAlign::Top),
                 backendError.c_str(),
                 IRECT(mRouting.L + 14.0F, infoTop + 26.0F,
                       mRouting.R - 14.0F, infoTop + 55.0F));
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
    g.DrawText(IText(8.3F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               adapter.c_str(), IRECT(rect.L + 10.0F, rect.T + 24.0F,
                                      rect.R - 10.0F, rect.T + 47.0F));
    g.DrawText(IText(8.0F, statusColor, "AeylaUI", EAlign::Near, EVAlign::Middle),
               signal.c_str(), IRECT(rect.L + 52.0F, rect.B - 34.0F,
                                     rect.R - 52.0F, rect.B - 7.0F));
    Button(g, previous, "<", kPanel, kLineStrong);
    Button(g, next, ">", kPanel, kLineStrong);
  }

  void Report(const aeyla::product::AuthoringResult& result)
  {
    mMessage = result.message;
    if(result.succeeded)
      return;
    if(auto* ui = GetUI())
      ui->ShowMessageBox(result.message.c_str(), "AEYLA · OPERATION BLOCKED", kMB_OK);
  }

  AeylaVisualDmx& mPlug;
  std::string mMessage;
  std::string mRouteName{"MAIN ARTNET"};
  std::string mTargetIp;
  unsigned mUniverseDisplay{1U};
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
  IRECT mRxCard{};
  IRECT mTxCard{};
  IRECT mRxPrevious{};
  IRECT mRxNext{};
  IRECT mTxPrevious{};
  IRECT mTxNext{};
  IRECT mRouteNameField{};
  IRECT mTargetIpField{};
  IRECT mUniverseField{};
  IRECT mApplyOutputButton{};
  IRECT mRefreshNetworkButton{};
};
