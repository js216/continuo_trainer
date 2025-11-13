/**
 * @file style.cpp
 * @brief App styling.
 * @author Jakob Kastelic
 */

#include "style.h"
#include "imgui.h"

void set_style(void)
{
   ImGuiStyle &style = ImGui::GetStyle();

   // padding
   style.WindowPadding          = ImVec2(10.0F, 10.0F);
   style.FramePadding           = ImVec2(10.0F, 10.0F);
   style.TouchExtraPadding      = ImVec2(0.0F, 0.0F);
   style.SeparatorTextPadding   = ImVec2(0.0F, 0.0F);
   style.CellPadding            = ImVec2(0.0F, 0.0F);
   style.DisplayWindowPadding   = ImVec2(0.0F, 0.0F);
   style.DisplaySafeAreaPadding = ImVec2(0.0F, 0.0F);

   // spacing
   style.ItemSpacing       = ImVec2(0.0F, 10.0F);
   style.ItemInnerSpacing  = ImVec2(0.0F, 0.0F);
   style.IndentSpacing     = 0.0F;
   style.ColumnsMinSpacing = 0.0F;

   // rounding
   style.WindowRounding    = 0.0F;
   style.ChildRounding     = 0.0F;
   style.PopupRounding     = 0.0F;
   style.FrameRounding     = 0.0F;
   style.TabRounding       = 0.0F;
   style.ScrollbarRounding = 0.0F;
   style.GrabRounding      = 0.0F;

   // border size
   style.WindowBorderSize        = 1.0F;
   style.ChildBorderSize         = 1.0F;
   style.PopupBorderSize         = 1.0F;
   style.FrameBorderSize         = 1.0F;
   style.TabBorderSize           = 1.0F;
   style.TabBarBorderSize        = 1.0F;
   style.SeparatorTextBorderSize = 1.0F;

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
   ImVec4 *colors = ImGui::GetStyle().Colors;

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
}
