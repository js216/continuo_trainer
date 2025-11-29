// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file main.cpp
 * @brief Reliable "C-style" Modern C++ integration.
 * @details No classes, no RAII, manual memory management, strict bounds
 * checking.
 */

#include "app.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include "logic.h"
#include "midi.h"
#include "state.h"
#include "theory.h"
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

struct PlatformState {
   // 0-255 safe ASCII storage.
   // We explicitly do not store high-value X11 keys (like Arrows) here
   // to prevent stack corruption/segfaults.
   bool keys[256];
   int mouse_x;
   int mouse_y;
   bool mouse_buttons[3];
};

struct X11Context {
   Display *display;
   Window window;
   GLXContext gl_context;
   Atom wm_delete_window;
   bool valid;
};

constexpr int SCREEN_W      = 800;
constexpr int SCREEN_H      = 650;
constexpr double TARGET_FPS = 60.0;
constexpr std::chrono::duration<double> TARGET_FRAME_TIME(1.0 / TARGET_FPS);

static ImGuiKey key_map_x11_to_imgui(KeySym ks)
{
   switch (ks) {
      case XK_Tab: return ImGuiKey_Tab;
      case XK_Left: return ImGuiKey_LeftArrow;
      case XK_Right: return ImGuiKey_RightArrow;
      case XK_Up: return ImGuiKey_UpArrow;
      case XK_Down: return ImGuiKey_DownArrow;
      case XK_Page_Up: return ImGuiKey_PageUp;
      case XK_Page_Down: return ImGuiKey_PageDown;
      case XK_Home: return ImGuiKey_Home;
      case XK_End: return ImGuiKey_End;
      case XK_Insert: return ImGuiKey_Insert;
      case XK_Delete: return ImGuiKey_Delete;
      case XK_BackSpace: return ImGuiKey_Backspace;
      case XK_space: return ImGuiKey_Space;
      case XK_Return: return ImGuiKey_Enter;
      case XK_Escape: return ImGuiKey_Escape;

      // Modifiers - Map physical keys
      case XK_Shift_L: return ImGuiKey_LeftShift;
      case XK_Shift_R: return ImGuiKey_RightShift;
      case XK_Control_L: return ImGuiKey_LeftCtrl;
      case XK_Control_R: return ImGuiKey_RightCtrl;
      case XK_Alt_L: return ImGuiKey_LeftAlt;
      case XK_Alt_R: return ImGuiKey_RightAlt;
      case XK_Super_L: return ImGuiKey_LeftSuper;
      case XK_Super_R: return ImGuiKey_RightSuper;

      default:
         if (ks >= XK_a && ks <= XK_z)
            return static_cast<ImGuiKey>(ImGuiKey_A + (ks - XK_a));
         if (ks >= XK_A && ks <= XK_Z)
            return static_cast<ImGuiKey>(ImGuiKey_A + (ks - XK_A));
         if (ks >= XK_0 && ks <= XK_9)
            return static_cast<ImGuiKey>(ImGuiKey_0 + (ks - XK_0));
         return ImGuiKey_None;
   }
}

static bool init_x11_opengl(X11Context *ctx, int width, int height,
                            const char *title)
{
   *ctx = X11Context{};

   ctx->display = XOpenDisplay(nullptr);
   if (!ctx->display) {
      fprintf(stderr, "Fatal: Failed to open X display.\n");
      return false;
   }

   int screen  = DefaultScreen(ctx->display);
   Window root = RootWindow(ctx->display, screen);

   static int visual_attribs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24,
                                  GLX_DOUBLEBUFFER, None};
   XVisualInfo *vi = glXChooseVisual(ctx->display, screen, visual_attribs);
   if (!vi) {
      fprintf(stderr, "Fatal: Failed to choose visual.\n");
      XCloseDisplay(ctx->display);
      ctx->display = nullptr;
      return false;
   }

   Colormap cmap = XCreateColormap(ctx->display, root, vi->visual, AllocNone);
   XSetWindowAttributes swa = {};
   swa.colormap             = cmap;
   swa.event_mask           = ExposureMask | KeyPressMask | KeyReleaseMask |
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                    StructureNotifyMask;

   ctx->window =
       XCreateWindow(ctx->display, root, 0, 0, width, height, 0, vi->depth,
                     InputOutput, vi->visual, CWColormap | CWEventMask, &swa);

   XFree(vi);

   if (!ctx->window) {
      fprintf(stderr, "Fatal: Failed to create window.\n");
      XCloseDisplay(ctx->display);
      return false;
   }

   ctx->wm_delete_window = XInternAtom(ctx->display, "WM_DELETE_WINDOW", False);
   XSetWMProtocols(ctx->display, ctx->window, &ctx->wm_delete_window, 1);
   XStoreName(ctx->display, ctx->window, title);
   XMapWindow(ctx->display, ctx->window);

   ctx->gl_context = glXCreateContext(ctx->display, vi, nullptr, GL_TRUE);
   if (!ctx->gl_context) {
      fprintf(stderr, "Fatal: Failed to create GL context.\n");
      XDestroyWindow(ctx->display, ctx->window);
      XCloseDisplay(ctx->display);
      return false;
   }

   glXMakeCurrent(ctx->display, ctx->window, ctx->gl_context);
   ctx->valid = true;
   return true;
}

static void shutdown_x11_opengl(X11Context *ctx)
{
   if (!ctx->valid)
      return;

   if (ctx->display) {
      glXMakeCurrent(ctx->display, None, nullptr);
      if (ctx->gl_context)
         glXDestroyContext(ctx->display, ctx->gl_context);
      if (ctx->window)
         XDestroyWindow(ctx->display, ctx->window);
      XCloseDisplay(ctx->display);
   }

   ctx->valid = false;
}

static void init_imgui_system()
{
   IMGUI_CHECKVERSION();
   ImGui::CreateContext();
   ImGuiIO &io    = ImGui::GetIO();
   io.IniFilename = "imgui.ini";
   ImGui_ImplOpenGL3_Init("#version 130");
}

static void shutdown_imgui_system()
{
   ImGui_ImplOpenGL3_Shutdown();
   ImGui::DestroyContext();
}

static void process_platform_events(X11Context *ctx, PlatformState *pstate,
                                    bool *running)
{
   XEvent xev;
   ImGuiIO &io = ImGui::GetIO();

   while (XPending(ctx->display)) {
      XNextEvent(ctx->display, &xev);

      switch (xev.type) {
         case ClientMessage:
            if ((Atom)xev.xclient.data.l[0] == ctx->wm_delete_window) {
               *running = false;
            }
            break;

         case KeyPress:
         case KeyRelease: {
            bool pressed = (xev.type == KeyPress);
            KeySym ks    = XLookupKeysym(&xev.xkey, 0);

            // FIXED: Removed redundant 'ks >= 0' check (KeySym is unsigned)
            if (ks < 256) {
               pstate->keys[ks] = pressed;
            }

            // Pass to ImGui
            ImGuiKey imgui_key = key_map_x11_to_imgui(ks);
            if (imgui_key != ImGuiKey_None) {
               io.AddKeyEvent(imgui_key, pressed);
            }

            // FIXED: Use explicit Left keys instead of Mod keys (more
            // compatible) This syncs the ImGui modifier state with the X11
            // mask.
            io.AddKeyEvent(ImGuiKey_LeftShift, (xev.xkey.state & ShiftMask));
            io.AddKeyEvent(ImGuiKey_LeftCtrl, (xev.xkey.state & ControlMask));
            io.AddKeyEvent(ImGuiKey_LeftAlt, (xev.xkey.state & Mod1Mask));
            // Optional: Super/Meta key usually on Mod4Mask
            // io.AddKeyEvent(ImGuiKey_LeftSuper, (xev.xkey.state & Mod4Mask));

            // Character Input
            if (pressed) {
               char text[32];
               int count =
                   XLookupString(&xev.xkey, text, sizeof(text), &ks, nullptr);
               for (int i = 0; i < count; ++i) {
                  io.AddInputCharacter(text[i]);
               }
            }
            break;
         }

         case ButtonPress:
         case ButtonRelease: {
            int btn = xev.xbutton.button - 1;
            // Buttons 1, 2, 3
            if (btn >= 0 && btn < 3) {
               pstate->mouse_buttons[btn] = (xev.type == ButtonPress);
            }
            // Wheel (Buttons 4, 5)
            if (xev.type == ButtonPress) {
               if (xev.xbutton.button == 4)
                  io.AddMouseWheelEvent(0.0f, 1.0f);
               if (xev.xbutton.button == 5)
                  io.AddMouseWheelEvent(0.0f, -1.0f);
            }
            break;
         }

         case MotionNotify:
            pstate->mouse_x = xev.xmotion.x;
            pstate->mouse_y = xev.xmotion.y;
            break;

         case ConfigureNotify:
            io.DisplaySize = ImVec2((float)xev.xconfigure.width,
                                    (float)xev.xconfigure.height);
            break;
      }
   }
}

int main()
{
   // 1. Initialization
   X11Context x11_ctx = {};
   if (!init_x11_opengl(&x11_ctx, SCREEN_W, SCREEN_H, "ImGui Procedural")) {
      return 1;
   }

   init_imgui_system();

   struct state app_state = {};
   app_init(&app_state);

   PlatformState pstate = {};
   bool running         = true;

   // Timekeeping
   using clock    = std::chrono::steady_clock;
   auto last_time = clock::now();

   // 2. Loop
   while (running) {
      auto frame_start = clock::now();

      // Calculate DT
      std::chrono::duration<float> elapsed = frame_start - last_time;
      last_time                            = frame_start;
      float dt                             = elapsed.count();
      if (dt <= 0.0f)
         dt = 0.0001f;

      // Logic
      process_platform_events(&x11_ctx, &pstate, &running);
      poll_midi(&app_state);
      logic_receive(&app_state);

      // Update ImGui IO
      ImGuiIO &io     = ImGui::GetIO();
      io.DeltaTime    = dt;
      io.MousePos     = ImVec2((float)pstate.mouse_x, (float)pstate.mouse_y);
      io.MouseDown[0] = pstate.mouse_buttons[0];
      io.MouseDown[1] = pstate.mouse_buttons[1];
      io.MouseDown[2] = pstate.mouse_buttons[2];

      // Render
      ImGui_ImplOpenGL3_NewFrame();
      ImGui::NewFrame();

      app_render(&app_state);

      ImGui::Render();
      glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
      glClear(GL_COLOR_BUFFER_BIT);
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      glXSwapBuffers(x11_ctx.display, x11_ctx.window);

      // Frame Limiter
      auto frame_end                          = clock::now();
      std::chrono::duration<double> work_time = frame_end - frame_start;
      if (work_time < TARGET_FRAME_TIME) {
         std::this_thread::sleep_for(TARGET_FRAME_TIME - work_time);
      }
   }

   // 3. Cleanup
   shutdown_imgui_system();
   shutdown_x11_opengl(&x11_ctx);

   return 0;
}
