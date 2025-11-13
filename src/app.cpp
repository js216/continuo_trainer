/**
 * @file app.cpp
 * @brief Portable application logic.
 * @author Jakob Kastelic
 */

#include "app.h"
#include "imgui.h"
#include "logic.h"
#include "midi.h"
#include "notes.h"
#include "state.h"
#include "style.h"
#include "util.h"
#include <stdarg.h>
#include <stdio.h>

static void set_status(struct state *state, const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   vsnprintf(state->status, sizeof(state->status), fmt, args);
   va_end(args);
}

void init_state(struct state *state)
{
   set_style();
   set_font("fonts/Roboto-Regular.ttf", 18.0F);
   dark_mode();
   set_status(state, "Ready");
}

static void app_buttons(struct state *state)
{
    if (ImGui::Button("Good")) {
        logic_good(state);
        set_status(state, "One good placed");
    }

    ImGui::SameLine();
    if (ImGui::Button("Bad")) {
        logic_bad(state);
        set_status(state, "One bad placed");
    }

    ImGui::SameLine();
    if (ImGui::Button("Populate")) {
        logic_pop(state);
        set_status(state, "All populated");
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        logic_clear(state);
        set_status(state, "Cleared");
    }

    ImGui::SameLine();
    if (ImGui::Button("MIDI Refresh")) {
        refresh_midi_devices(state);
        set_status(state, "MIDI devices refreshed");
    }

    ImGui::SameLine();
    if (ImGui::Button("Connect")) {
        init_midi(state);
    }

}

static void app_midi(struct state *state)
{
    ImGui::Text("MIDI Devices:");
    if (ImGui::BeginListBox("##midi_list",
                            ImVec2(-FLT_MIN,
                                   5 * ImGui::GetTextLineHeightWithSpacing())))
    {
        for (int i = 0; i < (int)state->midi_devices.size(); i++) {
            bool selected = (state->selected_device == i);
            if (ImGui::Selectable(state->midi_devices[i].c_str(), selected)) {
                state->selected_device = i;

                // Set status message when user clicks on a device
                char msg[128];
                snprintf(msg, sizeof(msg), "Selected MIDI device: %s",
                         state->midi_devices[i].c_str());
                set_status(state, msg);
            }
        }
        ImGui::EndListBox();
    }
}


static void app_controls(struct state *state)
{
    float controls_height = ImGui::GetFrameHeightWithSpacing() * 6.0F;
    ImGui::BeginChild("Controls", ImVec2(0, controls_height), true);

    app_buttons(state);
    app_midi(state);

    ImGui::Separator();
    ImGui::Text("Status: %s", state->status);

    ImGui::EndChild();
}

void render_ui(struct state *state)
{
   const ImGuiIO &io = ImGui::GetIO();

   ImGui::SetNextWindowPos(ImVec2(0, 0));
   ImGui::SetNextWindowSize(io.DisplaySize);
   ImGui::Begin("Main", NULL,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

   app_controls(state);

   notes_staff();
   notes_dots(state->bassline, MAX_CHORDS, STYLE_WHITE);
   notes_chords(state->chords_ok, STYLE_GREEN);
   notes_chords(state->chords_bad, STYLE_RED);

   ImGui::End();
}
