// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file main.cpp
 * @brief Platform-specific entry point and SDL/OpenGL/ImGui integration.
 * @author Jakob Kastelic
 */

#include "app.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"
#include "midi.h"
#include "notes.h"
#include "state.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <stdio.h>

static bool handle_events(void)
{
   SDL_Event event;

   while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT)
         return false;
   }

   SDL_Delay(16);
   return true;
}

int main(void)
{
   SDL_Init(SDL_INIT_VIDEO);
   SDL_Window *window = SDL_CreateWindow(
       "ImGui Example", SDL_WINDOWPOS_CENTERED_DISPLAY(0U),
       SDL_WINDOWPOS_CENTERED_DISPLAY(0U), 800, 480, SDL_WINDOW_OPENGL);
   SDL_GLContext gl_context = SDL_GL_CreateContext(window);

   IMGUI_CHECKVERSION();
   ImGui::CreateContext();
   ImGuiIO &io    = ImGui::GetIO();
   io.IniFilename = nullptr;
   ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
   ImGui_ImplOpenGL3_Init("#version 130");

   struct state state = {};
   init_state(&state);

   while (handle_events()) {
      poll_midi(&state);
      logic_receive(&state);

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplSDL2_NewFrame();
      ImGui::NewFrame();

      render_ui(&state);

      ImGui::Render();
      glClear(GL_COLOR_BUFFER_BIT);
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      SDL_GL_SwapWindow(window);
   }

   ImGui_ImplOpenGL3_Shutdown();
   ImGui_ImplSDL2_Shutdown();
   ImGui::DestroyContext();
   SDL_GL_DeleteContext(gl_context);
   SDL_DestroyWindow(window);
   SDL_Quit();

   return 0;
}
