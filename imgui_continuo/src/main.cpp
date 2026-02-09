// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file main.cpp
 * @brief Application entry point for X11.
 */

#include "app.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include "logic.h"
#include "midi.h"
#include "state.h"
#include "util.h"
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <chrono>
#include <string>
#include <thread>
#include <utility>

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

constexpr int screen_w      = 800;
constexpr int screen_h      = 650;
constexpr double target_fps = 60.0;
constexpr std::chrono::duration<double> target_frame_time(1.0 / target_fps);

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
      error("Fatal: Failed to open X display");
      return false;
   }

   const int screen  = DefaultScreen(ctx->display);
   const Window root = RootWindow(ctx->display, screen);

   static int visual_attribs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24,
                                  GLX_DOUBLEBUFFER, None};
   XVisualInfo *vi = glXChooseVisual(ctx->display, screen, visual_attribs);
   if (!vi) {
      error("Fatal: Failed to choose visual");
      XCloseDisplay(ctx->display);
      ctx->display = nullptr;
      return false;
   }

   const Colormap cmap =
       XCreateColormap(ctx->display, root, vi->visual, AllocNone);
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
      error("Fatal: Failed to create window");
      XCloseDisplay(ctx->display);
      return false;
   }

   ctx->wm_delete_window = XInternAtom(ctx->display, "WM_DELETE_WINDOW", False);
   XSetWMProtocols(ctx->display, ctx->window, &ctx->wm_delete_window, 1);
   XStoreName(ctx->display, ctx->window, title);
   XMapWindow(ctx->display, ctx->window);

   ctx->gl_context = glXCreateContext(ctx->display, vi, nullptr, GL_TRUE);
   if (!ctx->gl_context) {
      error("Fatal: Failed to create GL context");
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

static void handle_client_message(const XEvent *xev, bool *running,
                                  X11Context *ctx)
{
   const auto value = xev->xclient.data.l[0];

   if (std::cmp_equal(value, ctx->wm_delete_window)) {
      *running = false;
   }
}

static void handle_key_event(const XEvent *xev, PlatformState *pstate,
                             ImGuiIO &io)
{
   const bool pressed = (xev->type == KeyPress);

   // Create a local mutable copy for Xlib functions
   XKeyEvent key_event = xev->xkey;

   // Safe: XLookupKeysym does not modify the fields we care about
   KeySym ks = XLookupKeysym(&key_event, 0);

   if (ks < 256) {
      pstate->keys[ks] = pressed;
   }

   const ImGuiKey imgui_key = key_map_x11_to_imgui(ks);
   if (imgui_key != ImGuiKey_None) {
      io.AddKeyEvent(imgui_key, pressed);
   }

   io.AddKeyEvent(ImGuiKey_LeftShift, (key_event.state & ShiftMask));
   io.AddKeyEvent(ImGuiKey_LeftCtrl, (key_event.state & ControlMask));
   io.AddKeyEvent(ImGuiKey_LeftAlt, (key_event.state & Mod1Mask));

   if (pressed) {
      char text[32];
      // key_event is already mutable and safe to pass
      const int count =
          XLookupString(&key_event, text, sizeof(text), &ks, nullptr);
      for (int i = 0; i < count; ++i) {
         io.AddInputCharacter(text[i]);
      }
   }
}

static void handle_button_event(const XEvent *xev, PlatformState *pstate,
                                ImGuiIO &io)
{
   const int btn = static_cast<int>(xev->xbutton.button) - 1;
   if (btn >= 0 && btn < 3) {
      pstate->mouse_buttons[btn] = (xev->type == ButtonPress);
   }

   if (xev->type == ButtonPress) {
      if (xev->xbutton.button == 4)
         io.AddMouseWheelEvent(0.0F, 1.0F);
      if (xev->xbutton.button == 5)
         io.AddMouseWheelEvent(0.0F, -1.0F);
   }
}

static void handle_motion_event(const XEvent *xev, PlatformState *pstate)
{
   pstate->mouse_x = xev->xmotion.x;
   pstate->mouse_y = xev->xmotion.y;
}

static void handle_configure_event(const XEvent *xev, ImGuiIO &io)
{
   io.DisplaySize =
       ImVec2((float)xev->xconfigure.width, (float)xev->xconfigure.height);
}

static void process_platform_events(X11Context *ctx, PlatformState *pstate,
                                    bool *running)
{
   ImGuiIO &io = ImGui::GetIO();
   XEvent xev;

   while (XPending(ctx->display)) {
      XNextEvent(ctx->display, &xev);

      switch (xev.type) {
         case ClientMessage: handle_client_message(&xev, running, ctx); break;
         case KeyPress:
         case KeyRelease: handle_key_event(&xev, pstate, io); break;
         case ButtonPress:
         case ButtonRelease: handle_button_event(&xev, pstate, io); break;
         case MotionNotify: handle_motion_event(&xev, pstate); break;
         case ConfigureNotify: handle_configure_event(&xev, io); break;
         default: // ignored
            break;
      }
   }
}

int main()
{
   // 1. Initialization
   X11Context x11_ctx = {};
   if (!init_x11_opengl(&x11_ctx, screen_w, screen_h, "ImGui Procedural")) {
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
      const std::chrono::duration<float> elapsed = frame_start - last_time;
      last_time                                  = frame_start;
      float dt                                   = elapsed.count();
      if (dt <= 0.0F)
         dt = 0.0001F;

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
      auto frame_end                                = clock::now();
      const std::chrono::duration<double> work_time = frame_end - frame_start;
      if (work_time < target_frame_time) {
         std::this_thread::sleep_for(target_frame_time - work_time);
      }
   }

   // 3. Cleanup
   state_save_settings(app_state.settings);
   shutdown_imgui_system();
   shutdown_x11_opengl(&x11_ctx);

   return 0;
}
