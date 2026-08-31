#pragma once

#include "AeylaVisualDmx.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>

class AeylaRuntimeStatusControl final : public IControl
{
public:
  AeylaRuntimeStatusControl(const IRECT& bounds, AeylaVisualDmx& plug)
  : IControl(bounds)
  , mPlug(plug)
  {
  }

  void Draw(IGraphics& g) override
  {
    mLiveOpen = mPlug.UiWorkspace() == 1;
    if(mLiveOpen)
      DrawLive(g);
    else
      DrawNormal(g);
    DrawOperatorFrame(g);
  }

  bool IsHit(float x, float y) const override
  {
    if(Contains(Header(), x, y) || Contains(Footer(), x, y)) return true;
    // A modal file menu owns the whole editor surface while open. Otherwise a
    // click outside the panel can fall through to MainControl and trigger a
    // transport/timeline/system action while the menu remains visible.
    if(mFileMenuOpen) return Contains(mRECT, x, y);
    if(mPlug.UiWorkspace() == 1) return Contains(mRECT, x, y);
    return false;
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void)mod;

    // ARCHIVO is modal for navigation/content, while the two physical safety
    // controls remain immediately reachable. A first click on a tab therefore
    // closes ARCHIVO and is consumed instead of changing workspace underneath.
    if(mFileMenuOpen && Contains(Header(), x, y) &&
       !Contains(HeaderArmButton(), x, y) &&
       !Contains(HeaderBlackoutButton(), x, y))
    {
      mFileMenuOpen = false;
      SetDirty(false);
      return;
    }

    // The shell is always first. Workspace navigation is presentation-only and
    // can never touch physical authority. ARM and APAGÓN are explicit actions.
    for(std::size_t index = 0U; index < 4U; ++index)
    {
      if(!Contains(NavTab(index), x, y)) continue;
      mPlug.SetUiWorkspaceFromUI(static_cast<int>(index));
      mLiveOpen = index == 1U;
      mFileMenuOpen = false;
      mLiveConfigIndex = -1;
      mDraggingMemory = -1;
      if(mLiveOpen)
        mLiveMessage = "EN VIVO · operación limpia; EDITAR abre DMX, MIDI, modo y fade sólo para una memoria.";
      SetDirty(false);
      return;
    }

    if(Contains(HeaderArmButton(), x, y))
    {
      const bool takeArmed = mPlug.TakeOutputArmed();
      const bool modelArmed = mPlug.OutputArmed();

      // The header is the single global authority control. If either legacy
      // model authority or Take authority is active, one DESARMAR gesture must
      // remove every voluntary authority represented by this button. Never
      // show DESARMAR and then route the click into an incompatible ARM path.
      if(takeArmed || modelArmed)
      {
        if(takeArmed)
          ReportLive(mPlug.ToggleTakeOutputArmFromUI());
        if(modelArmed)
          mPlug.ForceDisarmFromUI();
        if(modelArmed && !takeArmed)
        {
          mLiveMessageError = false;
          mLiveMessage = "SALIDA DESARMADA · autoridad física retirada";
        }
      }
      else
        ReportLive(mPlug.ToggleTakeOutputArmFromUI());

      SetDirty(false);
      return;
    }

    if(Contains(HeaderBlackoutButton(), x, y))
    {
      const bool enable = !mPlug.GlobalBlackout();
      mPlug.SetBlackoutFromUI(enable);
      mLiveMessageError = false;
      mLiveMessage = enable
          ? "APAGÓN TOTAL · DMX 0 continuo · ARM y carrier conservados"
          : "APAGÓN LIBERADO · vuelve el estado subyacente sin rearmar";
      SetDirty(false);
      return;
    }

    // File operations remain reachable from every workspace, including
    // EN VIVO. The old ordering let the live surface swallow ARCHIVO clicks.
    if(Contains(ArchiveButton(), x, y))
    {
      mFileMenuOpen = !mFileMenuOpen;
      SetDirty(false);
      return;
    }

    if(mFileMenuOpen)
    {
      BuildFileMenuButtons();
      for(std::size_t index = 0U; index < mFileButtons.size(); ++index)
      {
        if(!Contains(mFileButtons[index], x, y)) continue;
        mFileMenuOpen = false;
        HandleFileAction(index);
        return;
      }
      if(!Contains(FileMenuPanel(), x, y))
      {
        // Safety: dismissing the menu never clicks through into a live control.
        mFileMenuOpen = false;
        SetDirty(false);
        return;
      }
    }

    if(mPlug.UiWorkspace() == 1)
    {
      HandleLiveMouseDown(x, y);
      return;
    }
  }

  void OnMouseDrag(float x, float y, float dX, float dY,
                   const IMouseMod& mod) override
  {
    (void)y;
    (void)dX;
    (void)dY;
    (void)mod;
    if(!mLiveOpen || mDraggingMemory < 0)
      return;
    const std::size_t index = static_cast<std::size_t>(mDraggingMemory);
    if(mLiveConfigIndex == static_cast<int>(index))
      return;
    ApplyFaderFromX(index, x);
    SetDirty(false);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    (void)x;
    (void)y;
    (void)mod;
    mDraggingMemory = -1;
  }

private:
  inline static const IColor kBackground{255, 7, 8, 11};
  inline static const IColor kPanel{255, 13, 15, 19};
  inline static const IColor kRaised{255, 21, 23, 29};
  inline static const IColor kLine{255, 47, 51, 62};
  inline static const IColor kText{255, 232, 234, 238};
  inline static const IColor kMuted{255, 132, 141, 154};
  inline static const IColor kFaint{255, 84, 91, 103};
  inline static const IColor kGood{255, 57, 211, 132};
  inline static const IColor kWarn{255, 245, 154, 43};
  inline static const IColor kDanger{255, 231, 45, 55};
  inline static const IColor kBrand{255, 202, 145, 255};
  inline static const IColor kCyan{255, 68, 214, 255};

  [[nodiscard]] IRECT Header() const noexcept
  {
    return IRECT(mRECT.L, mRECT.T, mRECT.R, mRECT.T + 82.0F);
  }

  [[nodiscard]] IRECT NavTab(std::size_t index) const noexcept
  {
    const float left = mRECT.L + 14.0F;
    const float width = std::clamp((mRECT.W() - 28.0F) * 0.105F, 88.0F, 112.0F);
    const float gap = 5.0F;
    const float x = left + static_cast<float>(index) * (width + gap);
    return IRECT(x, mRECT.T + 49.0F, x + width, mRECT.T + 78.0F);
  }

  [[nodiscard]] IRECT HeaderBlackoutButton() const noexcept
  {
    return IRECT(mRECT.R - 150.0F, mRECT.T + 10.0F,
                 mRECT.R - 12.0F, mRECT.T + 40.0F);
  }

  [[nodiscard]] IRECT HeaderArmButton() const noexcept
  {
    return IRECT(mRECT.R - 260.0F, mRECT.T + 10.0F,
                 mRECT.R - 158.0F, mRECT.T + 40.0F);
  }

  [[nodiscard]] IRECT Footer() const noexcept
  {
    return IRECT(mRECT.L, mRECT.B - 46.0F, mRECT.R, mRECT.B);
  }

  [[nodiscard]] IRECT ArchiveButton() const noexcept
  {
    const auto footer = Footer();
    return IRECT(footer.R - 118.0F, footer.T + 7.0F,
                 footer.R - 12.0F, footer.B - 7.0F);
  }

  [[nodiscard]] IRECT CompactLiveButton() const noexcept { return {}; }
  [[nodiscard]] IRECT TopLiveTab() const noexcept { return {}; }

  [[nodiscard]] IRECT FileMenuPanel() const noexcept
  {
    const auto archive = ArchiveButton();
    return IRECT(archive.R - 310.0F, archive.T - 92.0F,
                 archive.R, archive.T - 8.0F);
  }

  static bool Contains(const IRECT& rectangle, float x, float y) noexcept
  {
    return x >= rectangle.L && x <= rectangle.R &&
           y >= rectangle.T && y <= rectangle.B;
  }

  static bool Empty(const WDL_String& value) noexcept
  {
    const char* text = value.Get();
    return text == nullptr || text[0] == '\0';
  }

  static std::filesystem::path PathFromUtf8(const char* text)
  {
    if(text == nullptr || text[0] == '\0') return {};
    const std::string bytes(text);
    std::u8string utf8;
    utf8.reserve(bytes.size());
    for(const unsigned char value : bytes)
      utf8.push_back(static_cast<char8_t>(value));
    return std::filesystem::path(utf8);
  }

  static std::filesystem::path DialogPath(const WDL_String& fileName,
                                          const WDL_String& path)
  {
    const auto file = PathFromUtf8(fileName.Get());
    if(file.is_absolute()) return file;
    return PathFromUtf8(path.Get()) / file;
  }

  static void Button(IGraphics& g, const IRECT& rect,
                     const std::string& label,
                     const IColor& fill,
                     const IColor& border,
                     const IColor& text = kText,
                     float textSize = 10.5F)
  {
    g.FillRoundRect(fill, rect, 5.0F);
    g.DrawRoundRect(border, rect, 5.0F, nullptr, 1.0F);
    g.DrawText(IText(textSize, text, "AeylaUI",
                     EAlign::Center, EVAlign::Middle),
               label.c_str(), rect.GetPadded(-4.0F));
  }

  static void Pill(IGraphics& g, const IRECT& rect,
                   const std::string& label, const IColor& accent)
  {
    g.FillRoundRect(IColor(255, 11, 13, 17), rect, 5.0F);
    g.DrawRoundRect(accent, rect, 5.0F, nullptr, 1.0F);
    g.DrawText(IText(9.4F, accent, "AeylaUI",
                     EAlign::Center, EVAlign::Middle),
               label.c_str(), rect.GetPadded(-4.0F));
  }

  void DrawNormal(IGraphics& g)
  {
    DrawUnifiedHeader(g);
    DrawFooter(g);
    if(mFileMenuOpen)
      DrawFileMenu(g);
  }

  void DrawUnifiedHeader(IGraphics& g)
  {
    const auto header = Header();
    g.FillRect(kBackground, header);
    g.DrawLine(kLine, header.L, header.B - 1.0F, header.R, header.B - 1.0F,
               nullptr, 1.0F);

    g.DrawText(IText(18.5F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "RGB LIVE CONTROL",
               IRECT(header.L + 14.0F, header.T + 4.0F,
                     header.L + 245.0F, header.T + 31.0F));
    g.DrawText(IText(9.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "RGB ESTUDIOS · SHOW / AEYLA · R10.6 PRETEST",
               IRECT(header.L + 14.0F, header.T + 28.0F,
                     header.L + 330.0F, header.T + 45.0F));

    // Sparse Campo Vivo signature: identity cue, not decoration.
    g.DrawLine(IColor(255, 232, 166, 201), header.L + 14.0F, header.T + 43.0F,
               header.L + 104.0F, header.T + 43.0F, nullptr, 1.0F);
    g.DrawLine(kBrand, header.L + 23.0F, header.T + 46.0F,
               header.L + 123.0F, header.T + 46.0F, nullptr, 1.0F);
    g.DrawLine(kCyan, header.L + 14.0F, header.T + 47.5F,
               header.L + 94.0F, header.T + 47.5F, nullptr, 1.0F);

    static constexpr std::array<const char*, 4U> tabs{
        "TOMA", "EN VIVO", "MIDI", "SISTEMA"};
    const int active = mPlug.UiWorkspace();
    for(std::size_t index = 0U; index < tabs.size(); ++index)
    {
      const auto tab = NavTab(index);
      const bool selected = active == static_cast<int>(index);
      if(selected)
      {
        g.FillRoundRect(IColor(255, 31, 23, 39), tab, 6.0F);
        g.DrawRoundRect(kBrand, tab, 6.0F, nullptr, 1.0F);
      }
      else
        g.DrawRoundRect(IColor(150, 47, 51, 62), tab, 6.0F, nullptr, 1.0F);
      g.DrawText(IText(10.4F, selected ? kText : kMuted, "AeylaUI",
                       EAlign::Center, EVAlign::Middle), tabs[index], tab);
      if(selected)
        g.FillRoundRect(kBrand, IRECT(tab.L + 18.0F, tab.B - 2.5F,
                                     tab.R - 18.0F, tab.B), 1.2F);
    }

    const auto tx = mPlug.ArtNetOutputStatus();
    const bool physicalAuthority = tx.enabled || tx.override_enabled;
    const float statusRight = HeaderArmButton().L - 8.0F;
    const float statusLeft = std::max(header.L + 470.0F, statusRight - 168.0F);
    if(statusRight - statusLeft > 70.0F)
    {
      std::string net = "ART-NET · SIN SALIDA";
      IColor netColor = kWarn;
      if(tx.fail_closed)
      {
        net = "ART-NET · FAIL-CLOSED";
        netColor = kDanger;
      }
      else if(physicalAuthority && tx.blackout_latched)
      {
        net = "APAGÓN · " + std::to_string(tx.configured_fps) + " Hz";
        netColor = kDanger;
      }
      else if(physicalAuthority)
      {
        net = "ART-NET TX · " + std::to_string(tx.configured_fps) + " Hz";
        netColor = kGood;
      }
      else if(mPlug.BackendReady())
      {
        net = "ART-NET · LISTA / SIN CARRIER";
        netColor = kCyan;
      }
      Pill(g, IRECT(statusLeft, header.T + 10.0F, statusRight, header.T + 40.0F),
           net, netColor);
    }

    const bool armed = mPlug.TakeOutputArmed() || mPlug.OutputArmed();
    Button(g, HeaderArmButton(), armed ? "DESARMAR" : "ARMAR",
           armed ? IColor(255, 18, 31, 24) : kRaised,
           armed ? kGood : kLine, armed ? kGood : kText, 9.6F);

    const bool blackout = mPlug.GlobalBlackout();
    Button(g, HeaderBlackoutButton(),
           blackout ? "APAGÓN TOTAL ACTIVO" : "APAGÓN TOTAL",
           blackout ? kDanger : IColor(255, 26, 13, 17), kDanger,
           blackout ? kText : kDanger, 8.9F);
  }

  void DrawFooter(IGraphics& g)
  {
    const auto footer = Footer();
    g.FillRect(kBackground, footer);
    g.DrawLine(kLine, footer.L, footer.T, footer.R, footer.T, nullptr, 1.0F);

    const auto tx = mPlug.ArtNetOutputStatus();
    const bool physicalAuthority = tx.enabled || tx.override_enabled;
    IColor rail = kBrand;
    if(mPlug.TakeRecording() || mPlug.TakeOutputLive() ||
       (physicalAuthority && tx.blackout_latched))
      rail = kDanger;
    else if(!mPlug.RuntimeHealthy() || mPlug.RenderingOffline() || tx.fail_closed)
      rail = kDanger;
    else if(mPlug.TakePlaying()) rail = kGood;
    else if(physicalAuthority) rail = kWarn;
    g.FillRect(rail, IRECT(footer.L, footer.T, footer.R, footer.T + 2.0F));

    const auto compactLive = CompactLiveButton();
    if(compactLive.W() > 0.0F)
      Button(g, compactLive, "EN VIVO", IColor(255, 24, 17, 31),
             kBrand, kBrand, 10.0F);

    const auto archive = ArchiveButton();
    Button(g, archive, mFileMenuOpen ? "CERRAR ARCHIVO" : "ARCHIVO",
           kRaised, mFileMenuOpen ? kBrand : kLine,
           mFileMenuOpen ? kBrand : kText, 9.8F);

    const float rightLimit = compactLive.W() > 0.0F
        ? compactLive.L - 10.0F : archive.L - 10.0F;
    const float split = footer.L + (rightLimit - footer.L) * 0.48F;

    std::string project = mPlug.ProjectDirty() ? "SIN GUARDAR · " : "GUARDADO · ";
    project += mPlug.CurrentProjectPath().empty()
        ? mPlug.ProjectName()
        : mPlug.CurrentProjectPath().filename().string();
    g.DrawText(IText(10.2F, mPlug.ProjectDirty() ? kWarn : kGood,
                     "AeylaUI", EAlign::Near, EVAlign::Middle),
               project.c_str(),
               IRECT(footer.L + 14.0F, footer.T + 3.0F,
                     split - 8.0F, footer.B));

    std::string operation;
    IColor operationColor = kMuted;
    if(tx.fail_closed) {
      operation = "FAIL-CLOSED · REARME MANUAL";
      operationColor = kDanger;
    }
    else if(physicalAuthority && tx.blackout_latched) {
      operation = "APAGÓN · DMX 0 · CARRIER " +
          std::to_string(tx.configured_fps) + " Hz";
      operationColor = kDanger;
    }
    else if(mPlug.TakeRecording()) {
      operation = "REC · CAPTURANDO";
      operationColor = kDanger;
    }
    else if(mPlug.TakeOutputLive()) {
      operation = "AL AIRE · CARRIER ACTIVO";
      operationColor = kDanger;
    }
    else if(mPlug.TakePlaying()) {
      operation = "PLAY · REPRODUCIENDO";
      operationColor = kGood;
    }
    else if(physicalAuthority) {
      operation = "ART-NET ARMADA · CARRIER " +
          std::to_string(tx.configured_fps) + " Hz";
      operationColor = kWarn;
    }
    else if(mPlug.BackendReady()) {
      operation = "ART-NET LISTA · SIN CARRIER";
      operationColor = kCyan;
    }
    else {
      operation = "SALIDA NO PREPARADA";
      operationColor = kWarn;
    }
    g.DrawText(IText(9.7F, operationColor, "AeylaUI",
                     EAlign::Far, EVAlign::Middle),
               operation.c_str(),
               IRECT(split, footer.T + 3.0F, rightLimit, footer.B));
  }

  void BuildFileMenuButtons()
  {
    const auto panel = FileMenuPanel();
    const float gap = 6.0F;
    const float innerL = panel.L + 8.0F;
    const float innerR = panel.R - 8.0F;
    const float innerT = panel.T + 8.0F;
    const float innerB = panel.B - 8.0F;
    const float w = (innerR - innerL - gap) * 0.5F;
    const float h = (innerB - innerT - gap) * 0.5F;
    mFileButtons[0] = IRECT(innerL, innerT, innerL + w, innerT + h);
    mFileButtons[1] = IRECT(innerL + w + gap, innerT, innerR, innerT + h);
    mFileButtons[2] = IRECT(innerL, innerT + h + gap,
                            innerL + w, innerB);
    mFileButtons[3] = IRECT(innerL + w + gap, innerT + h + gap,
                            innerR, innerB);
  }

  void DrawFileMenu(IGraphics& g)
  {
    BuildFileMenuButtons();
    const auto panel = FileMenuPanel();
    g.FillRoundRect(IColor(255, 10, 12, 16), panel, 7.0F);
    g.DrawRoundRect(kBrand, panel, 7.0F, nullptr, 1.0F);
    static constexpr std::array<const char*, 4U> labels{
        "NUEVO", "ABRIR", "GUARDAR", "GUARDAR COMO"};
    for(std::size_t index = 0U; index < mFileButtons.size(); ++index)
    {
      const bool blocked = mPlug.TakeRecording() && index < 2U;
      Button(g, mFileButtons[index], labels[index],
             blocked ? IColor(255, 35, 31, 25) : kRaised,
             blocked ? kWarn : kLine,
             blocked ? kWarn : kText, 9.3F);
    }
  }

  void HandleFileAction(std::size_t index)
  {
    if(index == 0U)
    {
      if(mPlug.TakeRecording())
      {
        ReportFileStatus(mPlug.NewProjectFromUI());
        return;
      }
      ConfirmDiscardThen([this]() {
        ReportFileStatus(mPlug.NewProjectFromUI());
        SetDirty(false);
      });
      return;
    }
    if(index == 1U)
    {
      if(mPlug.TakeRecording())
      {
        ReportFileStatus(mPlug.OpenProjectFromUI(std::filesystem::path{}));
        return;
      }
      ConfirmDiscardThen([this]() { PromptOpen(); });
      return;
    }
    if(index == 2U)
    {
      if(mPlug.CurrentProjectPath().empty()) PromptSaveAs();
      else ReportFileStatus(mPlug.SaveProjectFromUI());
      SetDirty(false);
      return;
    }
    if(index == 3U)
      PromptSaveAs();
  }

  void OpenLiveWorkspace()
  {
    mPlug.SetUiWorkspaceFromUI(1);
    mLiveOpen = true;
    mFileMenuOpen = false;
    mLiveConfigIndex = -1;
    mDraggingMemory = -1;
    mLiveMessageError = false;
    mLiveMessage = "EN VIVO · selecciona una memoria para operar; EDITAR abre DMX/MIDI/modo/fade sólo en esa memoria.";
    SetDirty(false);
  }

  void BuildLiveLayout()
  {
    const float left = mRECT.L + 16.0F;
    const float right = mRECT.R - 16.0F;
    const float top = Header().B + 8.0F;

    const float transportTop = top;
    const float transportGap = 8.0F;
    const float transportW = std::clamp(
        ((right - left) - transportGap * 3.0F) / 4.0F, 118.0F, 176.0F);
    const float transportTotal = transportW * 4.0F + transportGap * 3.0F;
    const float transportLeft = left + std::max(0.0F,
        ((right - left) - transportTotal) * 0.5F);
    for(std::size_t index = 0U; index < mLiveTransport.size(); ++index)
    {
      const float x = transportLeft +
          static_cast<float>(index) * (transportW + transportGap);
      mLiveTransport[index] = IRECT(x, transportTop, x + transportW,
                                   transportTop + 40.0F);
    }

    const float contentTop = transportTop + 52.0F;
    const float contentBottom = Footer().T - 38.0F;
    const float split = left + (right - left) * 0.31F;
    mLiveSetlistPanel = IRECT(left, contentTop, split - 9.0F, contentBottom);
    mLiveMemoryPanel = IRECT(split + 9.0F, contentTop, right, contentBottom);
    mLiveMessageRect = IRECT(left, contentBottom + 6.0F,
                             right, Footer().T - 5.0F);

    const std::size_t songCount = std::min<std::size_t>(
        mPlug.SongCount(), mLiveSongRows.size());
    const bool compactLiveSetlist = mLiveSetlistPanel.H() < 470.0F;
    const float rowTop = mLiveSetlistPanel.T +
        (compactLiveSetlist ? 64.0F : 104.0F);
    const float available = std::max(1.0F, mLiveSetlistPanel.B - rowTop - 8.0F);
    const float rowH = std::clamp(
        available / std::max<std::size_t>(songCount, 1U), 22.0F, 31.0F);
    for(std::size_t index = 0U; index < mLiveSongRows.size(); ++index)
    {
      if(index >= songCount) {
        mLiveSongRows[index] = {};
        continue;
      }
      const float y = rowTop + static_cast<float>(index) * rowH;
      mLiveSongRows[index] = IRECT(mLiveSetlistPanel.L + 8.0F, y,
                                   mLiveSetlistPanel.R - 8.0F,
                                   y + rowH - 2.0F);
    }

    const float gridL = mLiveMemoryPanel.L + 8.0F;
    const float gridR = mLiveMemoryPanel.R - 8.0F;
    const float gridT = mLiveMemoryPanel.T + 42.0F;
    const float gridB = mLiveMemoryPanel.B - 8.0F;
    const float gap = 9.0F;
    const float cardW = (gridR - gridL - gap) * 0.5F;
    const float cardH = (gridB - gridT - gap) * 0.5F;

    for(std::size_t index = 0U; index < mLiveMemoryCards.size(); ++index)
    {
      const std::size_t col = index % 2U;
      const std::size_t row = index / 2U;
      const float x = gridL + static_cast<float>(col) * (cardW + gap);
      const float y = gridT + static_cast<float>(row) * (cardH + gap);
      const IRECT card(x, y, x + cardW, y + cardH);
      mLiveMemoryCards[index] = card;
      mLiveConfigButtons[index] = IRECT(card.R - 94.0F, card.T + 8.0F,
                                        card.R - 8.0F, card.T + 31.0F);
      mLiveMainControls[index] = IRECT(card.L + 10.0F, card.T + 58.0F,
                                       card.R - 10.0F, card.B - 36.0F);
      const auto control = mLiveMainControls[index];
      const float trackY = control.T + control.H() * 0.52F;
      mLiveFaders[index] = IRECT(control.L + 18.0F, trackY - 4.5F,
                                 control.R - 18.0F, trackY + 4.5F);

      const float cfgTop = card.T + 61.0F;
      const float cfgGap = 7.0F;
      const float cfgW = (card.W() - 27.0F) * 0.5F;
      const float cfgH = 29.0F;
      mLiveDmxButtons[index] = IRECT(card.L + 10.0F, cfgTop,
                                     card.L + 10.0F + cfgW, cfgTop + cfgH);
      mLiveMidiButtons[index] = IRECT(mLiveDmxButtons[index].R + cfgGap,
                                      cfgTop, card.R - 10.0F, cfgTop + cfgH);
      const float secondTop = cfgTop + cfgH + cfgGap;
      mLiveModeButtons[index] = IRECT(card.L + 10.0F, secondTop,
                                      card.L + 10.0F + cfgW, secondTop + cfgH);
      mLiveFadeButtons[index] = IRECT(mLiveModeButtons[index].R + cfgGap,
                                      secondTop, card.R - 10.0F,
                                      secondTop + cfgH);
      mLiveBackButtons[index] = IRECT(card.L + 10.0F, card.B - 36.0F,
                                      card.R - 10.0F, card.B - 10.0F);
    }
  }

  void DrawLive(IGraphics& g)
  {
    BuildLiveLayout();
    g.FillRect(kBackground, mRECT);

    static constexpr std::array<const char*, 4U> transport{
        "PREV", "PLAY / GO", "HOLD", "NEXT"};
    for(std::size_t index = 0U; index < mLiveTransport.size(); ++index)
    {
      IColor fill = kRaised;
      IColor border = kLine;
      IColor text = kText;
      if(index == 1U)
      {
        fill = mPlug.TakePlaying() ? IColor(255, 10, 44, 25)
                                   : IColor(255, 13, 28, 20);
        border = kGood;
        text = kGood;
      }
      else if(index == 2U)
      {
        fill = IColor(255, 38, 28, 12);
        border = kWarn;
        text = kWarn;
      }
      Button(g, mLiveTransport[index], transport[index],
             fill, border, text, 9.7F);
    }

    DrawLiveSetlist(g);
    DrawLiveMemories(g);

    g.DrawText(IText(9.8F, mLiveMessageError ? kDanger : kMuted,
                     "AeylaUI", EAlign::Near, EVAlign::Middle),
               mLiveMessage.c_str(), mLiveMessageRect);
    DrawUnifiedHeader(g);
    DrawFooter(g);
    if(mFileMenuOpen)
      DrawFileMenu(g);
  }

  void DrawLiveSetlist(IGraphics& g)
  {
    g.FillRoundRect(kPanel, mLiveSetlistPanel, 7.0F);
    g.DrawRoundRect(kLine, mLiveSetlistPanel, 7.0F, nullptr, 1.0F);
    g.DrawText(IText(11.5F, kText, "AeylaUI",
                     EAlign::Near, EVAlign::Middle),
               "SETLIST",
               IRECT(mLiveSetlistPanel.L + 11.0F, mLiveSetlistPanel.T + 5.0F,
                     mLiveSetlistPanel.R - 10.0F, mLiveSetlistPanel.T + 26.0F));

    const std::size_t prepared = mPlug.ActiveSongIndex();
    const int live = mPlug.ActiveTakeSongIndex();
    const bool compactStatus = mLiveSetlistPanel.H() < 470.0F;

    std::string airName = "—";
    if(live >= 0 && static_cast<std::size_t>(live) < mPlug.SongCount())
      airName = std::to_string(live + 1) + " · " +
                mPlug.SongName(static_cast<std::size_t>(live));
    std::string preparedName = "—";
    if(prepared < mPlug.SongCount())
      preparedName = std::to_string(prepared + 1U) + " · " +
                     mPlug.SongName(prepared);

    if(compactStatus)
    {
      const IRECT status(mLiveSetlistPanel.L + 9.0F,
                         mLiveSetlistPanel.T + 29.0F,
                         mLiveSetlistPanel.R - 9.0F,
                         mLiveSetlistPanel.T + 57.0F);
      g.FillRoundRect(IColor(255, 12, 14, 19), status, 5.0F);
      g.DrawRoundRect(kLine, status, 5.0F, nullptr, 1.0F);
      const std::string compact = "AIRE " + airName +
          "   ·   PREP " + preparedName;
      g.DrawText(IText(8.2F, kMuted, "AeylaUI",
                       EAlign::Near, EVAlign::Middle),
                 compact.c_str(), status.GetPadded(-7.0F));
    }
    else
    {
      const IRECT air(mLiveSetlistPanel.L + 9.0F,
                      mLiveSetlistPanel.T + 29.0F,
                      mLiveSetlistPanel.R - 9.0F,
                      mLiveSetlistPanel.T + 61.0F);
      const IRECT prep(mLiveSetlistPanel.L + 9.0F,
                       mLiveSetlistPanel.T + 66.0F,
                       mLiveSetlistPanel.R - 9.0F,
                       mLiveSetlistPanel.T + 98.0F);
      g.FillRoundRect(IColor(255, 43, 13, 20), air, 5.0F);
      g.DrawRoundRect(kDanger, air, 5.0F, nullptr, 1.0F);
      g.DrawText(IText(7.8F, kDanger, "AeylaUI",
                       EAlign::Near, EVAlign::Top),
                 "AL AIRE", air.GetPadded(-7.0F));
      g.DrawText(IText(10.6F, kText, "AeylaUI",
                       EAlign::Near, EVAlign::Bottom),
                 airName.c_str(), air.GetPadded(-7.0F));

      g.FillRoundRect(IColor(255, 8, 23, 31), prep, 5.0F);
      g.DrawRoundRect(kCyan, prep, 5.0F, nullptr, 1.0F);
      g.DrawText(IText(7.8F, kCyan, "AeylaUI",
                       EAlign::Near, EVAlign::Top),
                 "PREPARADA", prep.GetPadded(-7.0F));
      g.DrawText(IText(10.6F, kText, "AeylaUI",
                       EAlign::Near, EVAlign::Bottom),
                 preparedName.c_str(), prep.GetPadded(-7.0F));
    }

    const std::size_t count = std::min<std::size_t>(
        mPlug.SongCount(), mLiveSongRows.size());
    for(std::size_t index = 0U; index < count; ++index)
    {
      const bool onAir = live >= 0 && static_cast<std::size_t>(live) == index;
      const bool isPrepared = prepared == index;
      IColor fill = kRaised;
      IColor border = kLine;
      IColor color = kText;
      std::string badge;
      if(onAir) {
        fill = IColor(255, 49, 14, 22);
        border = kDanger;
        color = kDanger;
        badge = "AL AIRE";
      }
      else if(isPrepared) {
        fill = IColor(255, 8, 18, 24);
        border = kCyan;
        color = kCyan;
        badge = "PREPARADA";
      }
      g.FillRoundRect(fill, mLiveSongRows[index], 4.0F);
      g.DrawRoundRect(border, mLiveSongRows[index], 4.0F, nullptr, 1.0F);
      const std::string label = (index + 1U < 10U ? "0" : "") +
          std::to_string(index + 1U) + "  " + mPlug.SongName(index);
      g.DrawText(IText(9.5F, color, "AeylaUI",
                       EAlign::Near, EVAlign::Middle),
                 label.c_str(), mLiveSongRows[index].GetPadded(-7.0F));
      if(!badge.empty())
        g.DrawText(IText(7.8F, color, "AeylaUI",
                         EAlign::Far, EVAlign::Middle),
                   badge.c_str(), mLiveSongRows[index].GetPadded(-7.0F));
    }
  }

  void DrawLiveMemories(IGraphics& g)
  {
    g.FillRoundRect(kPanel, mLiveMemoryPanel, 7.0F);
    g.DrawRoundRect(kLine, mLiveMemoryPanel, 7.0F, nullptr, 1.0F);
    g.DrawText(IText(11.5F, kText, "AeylaUI",
                     EAlign::Near, EVAlign::Middle),
               "MEMORIAS EN VIVO",
               IRECT(mLiveMemoryPanel.L + 11.0F, mLiveMemoryPanel.T + 5.0F,
                     mLiveMemoryPanel.R - 10.0F, mLiveMemoryPanel.T + 26.0F));
    g.DrawText(IText(8.3F, kMuted, "AeylaUI",
                     EAlign::Far, EVAlign::Middle),
               "4 ACCESOS · DMX / MIDI DENTRO DE EDITAR",
               IRECT(mLiveMemoryPanel.L + 11.0F, mLiveMemoryPanel.T + 5.0F,
                     mLiveMemoryPanel.R - 11.0F, mLiveMemoryPanel.T + 26.0F));

    for(std::size_t index = 0U; index < mLiveMemoryCards.size(); ++index)
      DrawMemoryCard(g, index);
  }

  void DrawMemoryCard(IGraphics& g, std::size_t index)
  {
    const auto view = mPlug.LiveMemoryViewFromUI(index);
    const bool configuring = mLiveConfigIndex == static_cast<int>(index);
    const bool active = view.level > 0.005F || view.target_level > 0.005F;
    const auto& card = mLiveMemoryCards[index];

    IColor border = configuring ? kBrand :
        (view.learning || view.midi_learning ? kBrand :
            (active ? kGood : (view.configured ? kLine : kWarn)));
    g.FillRoundRect(active ? IColor(255, 10, 25, 19) : IColor(255, 17, 19, 24),
                    card, 6.0F);
    g.DrawRoundRect(border, card, 6.0F, nullptr,
                    configuring || active ? 1.4F : 1.0F);

    g.DrawText(IText(11.4F, active ? kGood : kText, "AeylaUI",
                     EAlign::Near, EVAlign::Middle),
               view.name.c_str(),
               IRECT(card.L + 10.0F, card.T + 4.0F,
                     card.R - 106.0F, card.T + 27.0F));

    const std::string dmx = view.configured
        ? "DMX " + std::to_string(view.channel_count) + " CH"
        : (view.learning ? "DMX PASO 1/2" : "DMX SIN CONFIGURAR");
    const std::string midi = MidiLabel(view);
    g.DrawText(IText(8.4F, view.configured ? kGood : kWarn, "AeylaUI",
                     EAlign::Near, EVAlign::Middle),
               dmx.c_str(),
               IRECT(card.L + 10.0F, card.T + 27.0F,
                     card.L + card.W() * 0.52F, card.T + 48.0F));
    g.DrawText(IText(8.4F,
                     view.midi_kind == aeyla::live_memory_session::MidiBindingKind::none
                         ? (view.midi_learning ? kBrand : kMuted) : kBrand,
                     "AeylaUI", EAlign::Far, EVAlign::Middle),
               midi.c_str(),
               IRECT(card.L + card.W() * 0.48F, card.T + 27.0F,
                     card.R - 10.0F, card.T + 48.0F));

    Button(g, mLiveConfigButtons[index],
           configuring ? "EDITANDO" : "EDITAR",
           configuring ? IColor(255, 30, 20, 37) : kRaised,
           configuring ? kBrand : kLine,
           configuring ? kBrand : kMuted, 8.2F);

    if(configuring)
      DrawMemoryConfig(g, index, view);
    else if(view.mode == aeyla::output::LiveMemoryControlMode::toggle)
      DrawToggleOperation(g, index, view);
    else
      DrawFaderOperation(g, index, view);
  }

  void DrawToggleOperation(
      IGraphics& g, std::size_t index,
      const aeyla::live_memory_session::MemoryView& view)
  {
    const auto& control = mLiveMainControls[index];
    const bool on = view.target_level > 0.5F;
    const bool ready = view.configured;
    g.FillRoundRect(on ? IColor(255, 12, 46, 31) : IColor(255, 10, 11, 15),
                    control, 6.0F);
    g.DrawRoundRect(on ? kGood : (ready ? kLine : kWarn),
                    control, 6.0F, nullptr, on ? 1.6F : 1.0F);
    g.DrawText(IText(19.0F, on ? kGood : (ready ? kText : kWarn),
                     "AeylaUI", EAlign::Center, EVAlign::Middle),
               ready ? (on ? "ON" : "OFF") : "CONFIGURA DMX",
               control.GetPadded(-6.0F));
    g.DrawText(IText(8.2F, kMuted, "AeylaUI",
                     EAlign::Center, EVAlign::Bottom),
               ready ? "BOTÓN / TOGGLE" : "el MIDI puede mapearse antes",
               control.GetPadded(-7.0F));
  }

  void DrawFaderOperation(
      IGraphics& g, std::size_t index,
      const aeyla::live_memory_session::MemoryView& view)
  {
    const auto& control = mLiveMainControls[index];
    const auto& track = mLiveFaders[index];
    const float normalized = std::clamp(view.level, 0.0F, 1.0F);
    const float handleX = track.L + track.W() * normalized;
    const bool ready = view.configured;

    g.FillRoundRect(IColor(255, 10, 11, 15), control, 6.0F);
    g.DrawRoundRect(ready ? kLine : kWarn, control, 6.0F, nullptr, 1.0F);
    if(!ready)
    {
      g.DrawText(IText(14.0F, kWarn, "AeylaUI",
                       EAlign::Center, EVAlign::Middle),
                 "CONFIGURA DMX", control);
      g.DrawText(IText(8.2F, kMuted, "AeylaUI",
                       EAlign::Center, EVAlign::Bottom),
                 "el MIDI CC puede mapearse antes", control.GetPadded(-7.0F));
      return;
    }

    g.FillRoundRect(IColor(255, 5, 7, 9), track, 5.0F);
    if(normalized > 0.0F)
      g.FillRoundRect(kBrand, IRECT(track.L, track.T, handleX, track.B), 5.0F);
    g.DrawRoundRect(kLine, track, 5.0F, nullptr, 1.0F);
    const IRECT handle(handleX - 8.0F, track.T - 12.0F,
                       handleX + 8.0F, track.B + 12.0F);
    g.FillRoundRect(kText, handle, 4.0F);
    g.DrawRoundRect(kBrand, handle, 4.0F, nullptr, 1.1F);
    const std::string percentage =
        std::to_string(static_cast<int>(std::lround(normalized * 100.0F))) + "%";
    g.DrawText(IText(15.0F, normalized > 0.0F ? kBrand : kText,
                     "AeylaUI", EAlign::Center, EVAlign::Bottom),
               percentage.c_str(), control.GetPadded(-7.0F));
  }

  void DrawMemoryConfig(
      IGraphics& g, std::size_t index,
      const aeyla::live_memory_session::MemoryView& view)
  {
    const std::string dmxLabel = view.learning
        ? "2/2 CAPTURAR ON"
        : "1/2 CAPTURAR OFF";
    const std::string midiLabel = view.midi_learning
        ? (view.mode == aeyla::output::LiveMemoryControlMode::toggle
               ? "ESPERANDO NOTE…" : "MUEVE CC…")
        : (view.mode == aeyla::output::LiveMemoryControlMode::toggle
               ? "APRENDER NOTE" : "APRENDER CC");
    const std::string modeLabel =
        view.mode == aeyla::output::LiveMemoryControlMode::toggle
            ? "MODO · BOTÓN" : "MODO · FADER";
    const std::string fadeLabel = view.fade_ms == 100U ? "FADE · 0.1 s"
        : (view.fade_ms == 1500U ? "FADE · 1.5 s" : "FADE · 1.0 s");

    Button(g, mLiveDmxButtons[index], dmxLabel,
           view.learning ? IColor(255, 30, 20, 37) : kRaised,
           view.learning ? kBrand : kWarn,
           view.learning ? kBrand : kWarn, 8.2F);
    Button(g, mLiveMidiButtons[index], midiLabel,
           view.midi_learning ? IColor(255, 30, 20, 37) : kRaised,
           view.midi_learning ? kBrand : kLine,
           view.midi_learning ? kBrand : kText, 8.2F);
    Button(g, mLiveModeButtons[index], modeLabel, kRaised, kLine,
           view.mode == aeyla::output::LiveMemoryControlMode::fader
               ? kBrand : kText, 8.2F);
    Button(g, mLiveFadeButtons[index], fadeLabel, kRaised, kLine,
           kText, 8.2F);

    const auto& card = mLiveMemoryCards[index];
    std::string instruction;
    IColor instructionColor = kMuted;
    if(view.learning) {
      instruction = "Avolites: enciende SÓLO esta memoria y captura ON.";
      instructionColor = kBrand;
    }
    else if(view.midi_learning) {
      instruction = view.mode == aeyla::output::LiveMemoryControlMode::toggle
          ? "MIDI: presiona la Note. Ese primer toque sólo asigna."
          : "MIDI: mueve el CC. Ese primer movimiento sólo asigna.";
      instructionColor = kBrand;
    }
    else {
      instruction = "DMX: deja esta memoria OFF en Avolites para el paso 1. MIDI puede aprenderse antes o después.";
    }
    g.DrawText(IText(8.1F, instructionColor, "AeylaUI",
                     EAlign::Near, EVAlign::Middle),
               instruction.c_str(),
               IRECT(card.L + 11.0F, mLiveFadeButtons[index].B + 6.0F,
                     card.R - 11.0F, mLiveBackButtons[index].T - 4.0F));

    Button(g, mLiveBackButtons[index], "VOLVER A OPERAR",
           IColor(255, 24, 17, 31), kBrand, kBrand, 8.4F);
  }

  static std::string MidiLabel(
      const aeyla::live_memory_session::MemoryView& view)
  {
    if(view.midi_learning)
      return view.mode == aeyla::output::LiveMemoryControlMode::toggle
          ? "MIDI · ESPERANDO NOTE" : "MIDI · ESPERANDO CC";
    if(view.midi_kind == aeyla::live_memory_session::MidiBindingKind::note)
      return "NOTE " + std::to_string(view.midi_number) +
             " · CH " + std::to_string(view.midi_channel);
    if(view.midi_kind ==
       aeyla::live_memory_session::MidiBindingKind::control_change)
      return "CC " + std::to_string(view.midi_number) +
             " · CH " + std::to_string(view.midi_channel);
    return "MIDI SIN ASIGNAR";
  }

  void HandleLiveMouseDown(float x, float y)
  {
    BuildLiveLayout();

    if(Contains(mLiveTransport[0], x, y))
    {
      if(mPlug.SelectAdjacentSongFromUI(-1))
        SetLiveMessage(true, "PREV · canción anterior PREPARADA; AL AIRE no cambia.");
      else
        SetLiveMessage(false, "PREV no pudo cambiar la canción preparada.");
      SetDirty(false);
      return;
    }
    if(Contains(mLiveTransport[1], x, y))
    {
      ReportLive(mPlug.ToggleActiveTakePlaybackFromUI());
      SetDirty(false);
      return;
    }
    if(Contains(mLiveTransport[2], x, y))
    {
      mPlug.StopActiveTakePlaybackFromUI();
      SetLiveMessage(true, "HOLD · cuadro DMX retenido; carrier Art-Net continúa si está armado.");
      SetDirty(false);
      return;
    }
    if(Contains(mLiveTransport[3], x, y))
    {
      if(mPlug.SelectAdjacentSongFromUI(1))
        SetLiveMessage(true, "NEXT · canción siguiente PREPARADA; AL AIRE no cambia.");
      else
        SetLiveMessage(false, "NEXT no pudo cambiar la canción preparada.");
      SetDirty(false);
      return;
    }

    const std::size_t songCount = std::min<std::size_t>(
        mPlug.SongCount(), mLiveSongRows.size());
    for(std::size_t index = 0U; index < songCount; ++index)
    {
      if(!Contains(mLiveSongRows[index], x, y)) continue;
      if(mPlug.SelectSongFromUI(index))
        SetLiveMessage(true, "PREPARADA · " + mPlug.SongName(index) +
            " · AL AIRE conserva autoridad hasta PLAY / GO.");
      else
        SetLiveMessage(false, "No fue posible preparar esa canción.");
      SetDirty(false);
      return;
    }

    for(std::size_t index = 0U; index < mLiveMemoryCards.size(); ++index)
    {
      const auto view = mPlug.LiveMemoryViewFromUI(index);
      if(Contains(mLiveConfigButtons[index], x, y))
      {
        const bool activeOrFading = view.transitioning ||
            view.level > 0.005F || view.target_level > 0.005F;
        if(activeOrFading && mLiveConfigIndex != static_cast<int>(index))
        {
          SetLiveMessage(false, view.name +
              " · llévala a OFF / 0% antes de editar DMX, MIDI, modo o fade.");
          SetDirty(false);
          return;
        }
        mLiveConfigIndex = mLiveConfigIndex == static_cast<int>(index)
            ? -1 : static_cast<int>(index);
        mDraggingMemory = -1;
        SetLiveMessage(true, mLiveConfigIndex == static_cast<int>(index)
            ? "EDITAR · DMX / MIDI / modo / fade de esta memoria."
            : "OPERACIÓN · controles de autoría ocultos.");
        SetDirty(false);
        return;
      }

      if(mLiveConfigIndex == static_cast<int>(index))
      {
        if(Contains(mLiveDmxButtons[index], x, y))
        {
          ReportLive(mPlug.LearnLiveMemoryFromAvolitesFromUI(index));
          SetDirty(false);
          return;
        }
        if(Contains(mLiveMidiButtons[index], x, y))
        {
          (void)mPlug.LiveMemoryViewFromUI(index);
          ReportLiveSession(
              aeyla::live_memory_session::arm_midi_learn(&mPlug, index));
          SetDirty(false);
          return;
        }
        if(Contains(mLiveModeButtons[index], x, y))
        {
          ReportLive(mPlug.ToggleLiveMemoryModeFromUI(index));
          SetDirty(false);
          return;
        }
        if(Contains(mLiveFadeButtons[index], x, y))
        {
          ReportLive(mPlug.CycleLiveMemoryFadeFromUI(index, 1));
          SetDirty(false);
          return;
        }
        if(Contains(mLiveBackButtons[index], x, y))
        {
          mLiveConfigIndex = -1;
          SetLiveMessage(true, "OPERACIÓN · configuración cerrada.");
          SetDirty(false);
          return;
        }
        continue;
      }

      if(view.mode == aeyla::output::LiveMemoryControlMode::toggle &&
         Contains(mLiveMainControls[index], x, y))
      {
        if(!view.configured)
        {
          mLiveConfigIndex = static_cast<int>(index);
          mDraggingMemory = -1;
          SetLiveMessage(false, view.name +
              " · primero aprende DMX: captura OFF y luego ON.");
        }
        else
          ReportLive(mPlug.ToggleLiveMemoryFromUI(index));
        SetDirty(false);
        return;
      }
      if(view.mode == aeyla::output::LiveMemoryControlMode::fader &&
         Contains(mLiveMainControls[index], x, y))
      {
        if(!view.configured)
        {
          mLiveConfigIndex = static_cast<int>(index);
          mDraggingMemory = -1;
          SetLiveMessage(false, view.name +
              " · primero aprende DMX: captura OFF y luego ON.");
          SetDirty(false);
          return;
        }
        mDraggingMemory = static_cast<int>(index);
        ApplyFaderFromX(index, x);
        SetDirty(false);
        return;
      }
    }
  }

  void ApplyFaderFromX(std::size_t index, float x)
  {
    if(index >= mLiveFaders.size()) return;
    const auto& rect = mLiveFaders[index];
    const float normalized = std::clamp(
        (x - rect.L) / std::max(1.0F, rect.W()), 0.0F, 1.0F);
    const auto result = mPlug.SetLiveMemoryLevelFromUI(index, normalized);
    if(!result.succeeded)
      ReportLive(result);
    else
      mLiveMessageError = false;
  }

  void DrawOperatorFrame(IGraphics& g)
  {
    const bool recording = mPlug.TakeRecording();
    const bool playing = !recording && mPlug.TakePlaying();
    if(!recording && !playing) return;

    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const double ms = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    const float pulse = static_cast<float>(0.5 + 0.5 * std::sin(ms * 0.008));
    const int alphaOuter = static_cast<int>(75.0F + pulse * 95.0F);
    const int alphaInner = static_cast<int>(175.0F + pulse * 75.0F);
    const IColor base = recording ? kDanger : kGood;
    const IColor outer(alphaOuter, base.R, base.G, base.B);
    const IColor inner(alphaInner, base.R, base.G, base.B);
    const IRECT frame = mRECT.GetPadded(-5.0F);
    g.DrawRoundRect(outer, frame, 8.0F, nullptr, 4.5F);
    g.DrawRoundRect(inner, frame.GetPadded(-2.0F), 7.0F, nullptr, 1.5F);

    // Header/footer already communicate REC/PLAY. Keep only the peripheral
    // pulse so operator state is visible without covering navigation or status.
  }

  void SetLiveMessage(bool ok, std::string message)
  {
    mLiveMessageError = !ok;
    mLiveMessage = std::move(message);
  }

  void ReportLive(const aeyla::product::AuthoringResult& result)
  {
    SetLiveMessage(result.succeeded, result.message);
  }

  void ReportLiveSession(const aeyla::live_memory_session::ActionResult& result)
  {
    SetLiveMessage(result.succeeded, result.message);
  }

  void ReportFileStatus(const aeyla::product::ProjectFileStatus& status)
  {
    if(status.succeeded) return;
    std::string message = status.message;
    if(!status.diagnostics.empty())
      message += "\n\n" + status.diagnostics.front();
    GetUI()->ShowMessageBox(message.c_str(),
                            "RGB LIVE CONTROL · ERROR DE PROYECTO", kMB_OK);
  }

  void ConfirmDiscardThen(std::function<void()> action)
  {
    if(!mPlug.ProjectDirty())
    {
      action();
      return;
    }
    GetUI()->ShowMessageBox(
        "El proyecto tiene cambios sin guardar. ¿Continuar y descartarlos?",
        "RGB LIVE CONTROL · CAMBIOS SIN GUARDAR", kMB_YESNO,
        [action = std::move(action)](EMsgBoxResult result) {
          if(result == kYES) action();
        });
  }

  void PromptOpen()
  {
    mDialogFileName.Set("");
    mDialogPath.Set(mPlug.CurrentProjectPath().empty()
                        ? ""
                        : mPlug.CurrentProjectPath().parent_path().string().c_str());
    GetUI()->PromptForFile(
        mDialogFileName, mDialogPath, EFileAction::Open, ".aeylashow",
        [this](const WDL_String& fileName, const WDL_String& path) {
          if(Empty(fileName)) return;
          ReportFileStatus(mPlug.OpenProjectFromUI(DialogPath(fileName, path)));
          (void)mPlug.RefreshNetworkInterfacesFromUI();
          SetDirty(false);
        });
  }

  void PromptSaveAs()
  {
    mDialogFileName.Set(mPlug.CurrentProjectPath().empty()
                            ? "RGB-Live-Control-Show.aeylashow"
                            : mPlug.CurrentProjectPath().filename().string().c_str());
    mDialogPath.Set(mPlug.CurrentProjectPath().empty()
                        ? ""
                        : mPlug.CurrentProjectPath().parent_path().string().c_str());
    GetUI()->PromptForFile(
        mDialogFileName, mDialogPath, EFileAction::Save, ".aeylashow",
        [this](const WDL_String& fileName, const WDL_String& path) {
          if(Empty(fileName)) return;
          auto target = DialogPath(fileName, path);
          if(target.extension() != ".aeylashow") target += ".aeylashow";
          ReportFileStatus(mPlug.SaveProjectAsFromUI(target));
          SetDirty(false);
        });
  }

  AeylaVisualDmx& mPlug;

  bool mFileMenuOpen{false};
  std::array<IRECT, 4U> mFileButtons{};

  bool mLiveOpen{false};
  bool mLiveMessageError{false};
  int mLiveConfigIndex{-1};
  int mDraggingMemory{-1};
  std::string mLiveMessage{
      "EN VIVO · memorias en OFF hasta configuración y ARM explícito."};

  std::array<IRECT, 4U> mLiveTransport{};
  IRECT mLiveSetlistPanel{};
  IRECT mLiveMemoryPanel{};
  IRECT mLiveMessageRect{};
  std::array<IRECT, 15U> mLiveSongRows{};
  std::array<IRECT, 4U> mLiveMemoryCards{};
  std::array<IRECT, 4U> mLiveConfigButtons{};
  std::array<IRECT, 4U> mLiveMainControls{};
  std::array<IRECT, 4U> mLiveFaders{};
  std::array<IRECT, 4U> mLiveDmxButtons{};
  std::array<IRECT, 4U> mLiveMidiButtons{};
  std::array<IRECT, 4U> mLiveModeButtons{};
  std::array<IRECT, 4U> mLiveFadeButtons{};
  std::array<IRECT, 4U> mLiveBackButtons{};

  WDL_String mDialogFileName;
  WDL_String mDialogPath;
};
