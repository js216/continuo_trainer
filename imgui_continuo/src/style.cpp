// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file style.cpp
 * @brief App styling.
 * @author Jakob Kastelic
 */

#include "style.h"
#include "imgui.h"
#include "state.h"
#include "util.h"
#include <array>
#include <span>

void set_font(struct state *state)
{
   ImGuiIO &io = ImGui::GetIO();

   // Load base font
   io.Fonts->AddFontFromFileTTF("fonts/Roboto-Regular.ttf", 18.0F);

   // Merge music font with full Unicode range
   ImFontConfig config;
   config.MergeMode        = true;
   config.GlyphMinAdvanceX = 18.0F; // Make music symbols monospace-ish

   static const std::array<ImWchar, 3> ranges = {0x0020, 0xFFFF, 0};

   state->music_font = io.Fonts->AddFontFromFileTTF("fonts/Bravura.otf", 90.0F,
                                                    &config, ranges.data());

   if (!state->music_font)
      ERROR("Failed to load music font Bravura.otf\n");
}

void set_style(void)
{
   ImGuiStyle &style = ImGui::GetStyle();

   // padding
   style.WindowPadding          = ImVec2(STYLE_PAD_X, STYLE_PAD_Y);
   style.FramePadding           = ImVec2(STYLE_PAD_X, STYLE_PAD_Y);
   style.TouchExtraPadding      = ImVec2(0.0F, 0.0F);
   style.SeparatorTextPadding   = ImVec2(0.0F, 0.0F);
   style.CellPadding            = ImVec2(0.0F, 0.0F);
   style.DisplayWindowPadding   = ImVec2(0.0F, 0.0F);
   style.DisplaySafeAreaPadding = ImVec2(0.0F, 0.0F);

   // spacing
   style.ItemSpacing       = ImVec2(STYLE_PAD_X, STYLE_PAD_Y);
   style.ItemInnerSpacing  = ImVec2(0.0F, 0.0F);
   style.IndentSpacing     = 0.0F;
   style.ColumnsMinSpacing = 0.0F;

   // rounding
   style.WindowRounding    = 5.0F;
   style.ChildRounding     = 5.0F;
   style.PopupRounding     = 5.0F;
   style.FrameRounding     = 5.0F;
   style.TabRounding       = 5.0F;
   style.ScrollbarRounding = 5.0F;
   style.GrabRounding      = 5.0F;

   // border size
   style.WindowBorderSize        = STYLE_PAD_BORDER;
   style.ChildBorderSize         = STYLE_PAD_BORDER;
   style.PopupBorderSize         = STYLE_PAD_BORDER;
   style.FrameBorderSize         = STYLE_PAD_BORDER;
   style.TabBorderSize           = STYLE_PAD_BORDER;
   style.TabBarBorderSize        = STYLE_PAD_BORDER;
   style.SeparatorTextBorderSize = STYLE_PAD_BORDER;

   // window & child windows
   style.WindowMinSize            = ImVec2(2.0F, 32.0F);
   style.WindowTitleAlign         = ImVec2(0.0F, 0.0F);
   style.WindowMenuButtonPosition = ImGuiDir_Left;

   // misc
   style.TabBarOverlineSize = 0.0F;
   style.ScrollbarSize      = 0.0F;
   style.GrabMinSize        = 0.0F;
   style.LogSliderDeadzone  = 0.0F;
   style.MouseCursorScale   = 0.0F;
   style.AntiAliasedLines   = true;
   style.AntiAliasedFill    = true;
}

void dark_mode(void)
{
   std::span<ImVec4> colors(ImGui::GetStyle().Colors, ImGuiCol_COUNT);

   // background colors
   colors[ImGuiCol_WindowBg] = ImVec4(0.11F, 0.11F, 0.13F, 1.00F);
   colors[ImGuiCol_ChildBg]  = ImVec4(0.13F, 0.13F, 0.15F, 1.00F);
   colors[ImGuiCol_PopupBg]  = ImVec4(0.10F, 0.10F, 0.12F, 0.98F);

   // text
   colors[ImGuiCol_Text]           = ImVec4(0.95F, 0.96F, 0.98F, 1.00F);
   colors[ImGuiCol_TextDisabled]   = ImVec4(0.50F, 0.50F, 0.55F, 1.00F);
   colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26F, 0.59F, 0.98F, 0.35F);

   // borders and separators
   colors[ImGuiCol_Border]           = ImVec4(0.35F, 0.35F, 0.40F, 0.60F);
   colors[ImGuiCol_BorderShadow]     = ImVec4(0.00F, 0.00F, 0.00F, 0.00F);
   colors[ImGuiCol_Separator]        = ImVec4(0.30F, 0.30F, 0.33F, 0.60F);
   colors[ImGuiCol_SeparatorHovered] = ImVec4(0.40F, 0.40F, 0.45F, 0.78F);
   colors[ImGuiCol_SeparatorActive]  = ImVec4(0.45F, 0.45F, 0.50F, 1.00F);

   // frames, buttons, etc.
   colors[ImGuiCol_FrameBg]        = ImVec4(0.20F, 0.21F, 0.24F, 1.00F);
   colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25F, 0.26F, 0.29F, 1.00F);
   colors[ImGuiCol_FrameBgActive]  = ImVec4(0.30F, 0.31F, 0.35F, 1.00F);

   // scrollbars & grabs
   colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.15F, 0.15F, 0.18F, 1.00F);
   colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.35F, 0.35F, 0.40F, 0.70F);
   colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40F, 0.40F, 0.45F, 0.80F);
   colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.45F, 0.45F, 0.50F, 0.90F);

   // tabs
   colors[ImGuiCol_Tab]                = ImVec4(0.16F, 0.16F, 0.19F, 1.00F);
   colors[ImGuiCol_TabHovered]         = ImVec4(0.26F, 0.59F, 0.98F, 0.80F);
   colors[ImGuiCol_TabActive]          = ImVec4(0.20F, 0.20F, 0.25F, 1.00F);
   colors[ImGuiCol_TabUnfocused]       = ImVec4(0.13F, 0.13F, 0.16F, 1.00F);
   colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.16F, 0.16F, 0.20F, 1.00F);

   // buttons
   colors[ImGuiCol_Button]        = ImVec4(0.2F, 0.2F, 0.3F, 1.0F);
   colors[ImGuiCol_ButtonHovered] = ImVec4(0.3F, 0.8F, 0.4F, 1.0F);
   colors[ImGuiCol_ButtonActive]  = ImVec4(0.1F, 0.6F, 0.2F, 1.0F);
}

static ImVec2 anchor_offset(enum anchor anch, const ImVec2 &text_size)
{
   switch (anch) {
      case ANCHOR_TOP_LEFT: return {0.0F, 0.0F};
      case ANCHOR_TOP_CENTER: return {-text_size.x / 2, 0.0F};
      case ANCHOR_TOP_RIGHT: return {-text_size.x, 0.0F};
      case ANCHOR_CENTER_LEFT: return {0.0F, -text_size.y / 2};
      case ANCHOR_CENTER: return {-text_size.x / 2, -text_size.y / 2};
      case ANCHOR_CENTER_RIGHT: return {-text_size.x, -text_size.y / 2};
      case ANCHOR_BOTTOM_LEFT: return {0.0F, -text_size.y};
      case ANCHOR_BOTTOM_CENTER: return {-text_size.x / 2, -text_size.y};
      case ANCHOR_BOTTOM_RIGHT: return {-text_size.x, -text_size.y};
      default: return {0.0F, 0.0F};
   }
}

void style_text(const char *text, float x, float y, const font_config *cfg)
{
   static const font_config default_cfg = {
       .fontsize     = 18.0F,
       .anch         = ANCHOR_TOP_LEFT,
       .color        = STYLE_WHITE,
       .border_size  = 0.0F,
       .border_color = STYLE_WHITE,
       .anchor_size  = 0.0F,
       .anchor_color = STYLE_WHITE,
   };

   if (!cfg)
      cfg = &default_cfg;

   ImFont *font    = ImGui::GetFont();
   float font_size = cfg->fontsize;
   uint32_t color  = cfg->color;

   ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, text);
   ImVec2 offset    = anchor_offset(cfg->anch, text_size);
   ImVec2 pos       = ImVec2(x + offset.x, y + offset.y);

   ImDrawList *draw = ImGui::GetWindowDrawList();
   draw->AddText(font, font_size, pos, color, text);

   if (cfg->border_size > 0.0F) {
      draw->AddRect(pos, ImVec2(pos.x + text_size.x, pos.y + text_size.y),
                    cfg->border_color, 0.0F, 0, cfg->border_size);
   }

   if (cfg->anchor_size > 0.0F) {
      draw->AddCircleFilled(ImVec2(x, y), cfg->anchor_size, cfg->anchor_color);
   }
}
