#pragma once

#include "AeylaVisualDmx.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

class AeylaMainControl final : public IControl
{
public:
  AeylaMainControl(const IRECT& bounds, AeylaVisualDmx& plug)
  : IControl(bounds, {kParamBlackout})
  , mPlug(plug)
  {
    (void)mPlug.RefreshNetworkInterfacesFromUI();
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
      SetValueFromUserInput(GetValue(0) > 0.5 ? 0.0 : 1.0, 0);
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
      mMessage = "TX route changed · physical output requires re-arm.";
      SetDirty(false);
      return;
    }
    if(Contains(mTxNext, x, y))
    {
      (void)mPlug.CycleTxInterfaceFromUI(1);
      mMessage = "TX route changed · physical output requires re-arm.";
      SetDirty(false);
      return;
    }
    if(Contains(mRefreshNetworkButton, x, y))
    {
      if(mPlug.RefreshNetworkInterfacesFromUI())
        mMessage = "Network adapters refreshed.";
      else
        mMessage = "No active IPv4 adapters detected.";
      SetDirty(false);
      return;
    }
  }

private:
  static constexpr IColor kBackground{255, 7, 8, 11};
  static constexpr IColor kPanel{255, 14, 16, 21};
  static constexpr IColor kPanelRaised{255, 21, 24, 31};
  static constexpr IColor kPanelSelected{255, 30, 25, 30};
  static constexpr IColor kLine{255, 43, 48, 59};
  static constexpr IColor kLineStrong{255, 73, 80, 95};
  static constexpr IColor kText{255, 235, 238, 242};
  static constexpr IColor kMuted{255, 135, 143, 157};
  static constexpr IColor kFaint{255, 88, 95, 108};
  static constexpr IColor kAccent{255, 229, 48, 61};
  static constexpr IColor kAccentDark{255, 84, 25, 33};
  static constexpr IColor kGood{255, 70, 205, 137};
  static constexpr IColor kWarn{255, 238, 159, 64};

  static bool Contains(const IRECT& rect, float x, float y) noexcept
  {
    return x >= rect.L && x <= rect.R && y >= rect.T && y <= rect.B;
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

  void BuildLayout()
  {
    constexpr float margin = 14.0F;
    constexpr float headerHeight = 70.0F;
    constexpr float footerReserve = 54.0F;
    constexpr float gap = 10.0F;
    const float leftWidth = std::clamp(mRECT.W() * 0.22F, 218.0F, 286.0F);
    const float rightWidth = std::clamp(mRECT.W() * 0.27F, 286.0F, 356.0F);

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
    const float cardTop = route.T + 48.0F;
    mRxCard = IRECT(route.L, cardTop, route.R, cardTop + 112.0F);
    mTxCard = IRECT(route.L, mRxCard.B + 10.0F,
                    route.R, mRxCard.B + 122.0F);
    mRxPrevious = IRECT(mRxCard.L + 10.0F, mRxCard.B - 37.0F,
                        mRxCard.L + 46.0F, mRxCard.B - 9.0F);
    mRxNext = IRECT(mRxCard.R - 46.0F, mRxCard.B - 37.0F,
                    mRxCard.R - 10.0F, mRxCard.B - 9.0F);
    mTxPrevious = IRECT(mTxCard.L + 10.0F, mTxCard.B - 37.0F,
                        mTxCard.L + 46.0F, mTxCard.B - 9.0F);
    mTxNext = IRECT(mTxCard.R - 46.0F, mTxCard.B - 37.0F,
                    mTxCard.R - 10.0F, mTxCard.B - 9.0F);
    mRefreshNetworkButton = IRECT(route.L, mTxCard.B + 12.0F,
                                  route.R, mTxCard.B + 42.0F);
  }

  void DrawHeader(IGraphics& g)
  {
    g.DrawText(IText(20.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "AEYLA  /  SHOW PLAYER",
               IRECT(mHeader.L, mHeader.T, mHeader.L + 360.0F, mHeader.B - 20.0F));
    const std::string project = mPlug.ProjectName();
    g.DrawText(IText(9.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle),
               ("RGB ESTUDIOS · " + project +
                " · HOST-NATIVE CAPTURE/REPLAY PRETEST").c_str(),
               IRECT(mHeader.L, mHeader.B - 27.0F,
                     mHeader.L + 620.0F, mHeader.B));

    const bool blackout = GetValue(0) > 0.5;
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
    g.DrawText(IText(12.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "SETLIST",
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
               "AUTHORING SOURCE  AVOLITES TITAN  →  ART-NET U1  →  TAKE",
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
               "CAPTURE CONTRACT",
               IRECT(work.L, infoTop, work.R, infoTop + 24.0F));
    g.DrawText(IText(9.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Top),
               "· 512 DMX slots / one Art-Net universe\n"
               "· normalized recording at 44 Hz\n"
               "· first Art-Net source locks during each Take\n"
               "· STOP holds the current frame; DISARM is explicit\n"
               "· host heartbeat / offline render can auto-disarm output\n"
               "· Takes are RAM-only in this PRETEST build",
               IRECT(work.L, infoTop + 27.0F, work.R,
                     std::min(work.B - 58.0F, infoTop + 140.0F)));

    const IRECT messageRect(work.L, work.B - 48.0F, work.R, work.B);
    g.FillRoundRect(IColor(255, 11, 13, 18), messageRect, 6.0F);
    const std::string message = mMessage.empty()
        ? "Ready. Select a Song, confirm RX LIVE, record a Take, then test replay."
        : mMessage;
    g.DrawText(IText(9.0F, mMessage.empty() ? kFaint : kWarn,
                     "AeylaUI", EAlign::Near, EVAlign::Middle),
               message.c_str(), messageRect.GetPadded(-10.0F));
  }

  void DrawRouting(IGraphics& g)
  {
    Card(g, mRouting);
    g.DrawText(IText(12.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "ROUTING",
               IRECT(mRouting.L + 12.0F, mRouting.T + 8.0F,
                     mRouting.R - 12.0F, mRouting.T + 36.0F));

    DrawRouteCard(g, mRxCard, "ART-NET INPUT / RX",
                  mPlug.RxInterfaceStatus(), mPlug.CaptureInputStatus(),
                  mRxPrevious, mRxNext,
                  mPlug.CaptureAcceptedPackets() > 0U ? kGood : kWarn);
    DrawRouteCard(g, mTxCard, "ART-NET OUTPUT / TX",
                  mPlug.TxInterfaceStatus(), mPlug.OutputBackendStatus(),
                  mTxPrevious, mTxNext,
                  mPlug.BackendReady() ? kGood : kWarn);

    Button(g, mRefreshNetworkButton, "REFRESH NETWORK ADAPTERS",
           kPanelRaised, kLineStrong);

    const float infoTop = mRefreshNetworkButton.B + 14.0F;
    char diagnostics[160];
    std::snprintf(diagnostics, sizeof(diagnostics),
                  "NICs %llu  ·  RX PKT %llu  ·  SEQ GAP %llu  ·  TX PKT %llu  ·  ERR %llu",
                  static_cast<unsigned long long>(mPlug.NetworkInterfaceCount()),
                  static_cast<unsigned long long>(mPlug.CaptureAcceptedPackets()),
                  static_cast<unsigned long long>(mPlug.CaptureSequenceGaps()),
                  static_cast<unsigned long long>(mPlug.ArtNetSentPackets()),
                  static_cast<unsigned long long>(mPlug.ArtNetSendErrors()));
    g.DrawText(IText(8.5F, kFaint, "AeylaUI", EAlign::Near, EVAlign::Top),
               diagnostics,
               IRECT(mRouting.L + 14.0F, infoTop,
                     mRouting.R - 14.0F, infoTop + 52.0F));
  }

  void DrawRouteCard(IGraphics& g, const IRECT& rect,
                     const char* title, const std::string& adapter,
                     const std::string& signal, const IRECT& previous,
                     const IRECT& next, const IColor& statusColor)
  {
    g.FillRoundRect(kPanelRaised, rect, 7.0F);
    g.DrawRoundRect(kLine, rect, 7.0F, nullptr, 1.0F);
    g.DrawText(IText(9.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle),
               title, IRECT(rect.L + 10.0F, rect.T + 7.0F,
                            rect.R - 10.0F, rect.T + 27.0F));
    g.DrawText(IText(9.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               adapter.c_str(), IRECT(rect.L + 10.0F, rect.T + 29.0F,
                                      rect.R - 10.0F, rect.T + 54.0F));
    g.DrawText(IText(8.5F, statusColor, "AeylaUI", EAlign::Near, EVAlign::Middle),
               signal.c_str(), IRECT(rect.L + 10.0F, rect.T + 55.0F,
                                     rect.R - 10.0F, rect.T + 79.0F));
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
  IRECT mRefreshNetworkButton{};
};
