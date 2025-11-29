// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file main.cpp
 * @brief Minimal Linux X11 + OpenGL + ImGui integration.
 * @author Jakob Kastelic
 */

#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cstring>
#include <ctime>
#include <unistd.h>

#include "app.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include "logic.h"
#include "midi.h"
#include "state.h"
#include "theory.h"

struct PlatformState {
   bool keys[256] = {};
   int mouse_x = 0, mouse_y = 0;
   bool mouse_buttons[3] = {};
};

struct X11Context {
   Display *display;
   Window window;
   GLXContext gl_context;
};

static X11Context init_x11(int width, int height, const char *title)
{
   X11Context ctx{};
   ctx.display = XOpenDisplay(nullptr);
   if (!ctx.display)
      throw std::runtime_error("Failed to open X display");

   int screen  = DefaultScreen(ctx.display);
   Window root = RootWindow(ctx.display, screen);

   static int visual_attribs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24,
                                  GLX_DOUBLEBUFFER, None};
   XVisualInfo *vi = glXChooseVisual(ctx.display, screen, visual_attribs);
   if (!vi)
      throw std::runtime_error("Failed to choose visual");

   Colormap cmap = XCreateColormap(ctx.display, root, vi->visual, AllocNone);
   XSetWindowAttributes swa{};
   swa.colormap   = cmap;
   swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

   ctx.window =
       XCreateWindow(ctx.display, root, 0, 0, width, height, 0, vi->depth,
                     InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
   XMapWindow(ctx.display, ctx.window);

   Atom wmDelete = XInternAtom(ctx.display, "WM_DELETE_WINDOW", False);
   XSetWMProtocols(ctx.display, ctx.window, &wmDelete, 1);
   XStoreName(ctx.display, ctx.window, title);

   ctx.gl_context = glXCreateContext(ctx.display, vi, nullptr, GL_TRUE);
   glXMakeCurrent(ctx.display, ctx.window, ctx.gl_context);

   return ctx;
}

// ---------------------- ImGui Initialization ----------------------
static void init_imgui()
{
   IMGUI_CHECKVERSION();
   ImGui::CreateContext();
   ImGuiIO &io    = ImGui::GetIO();
   io.IniFilename = "imgui.ini";
   ImGui_ImplOpenGL3_Init("#version 130");
}

// ---------------------- Key Translation ----------------------
static ImGuiKey keysym_to_imgui(KeySym ks)
{
   switch (ks) {
      case XK_Left: return ImGuiKey_LeftArrow;
      case XK_Right: return ImGuiKey_RightArrow;
      case XK_Up: return ImGuiKey_UpArrow;
      case XK_Down: return ImGuiKey_DownArrow;
      case XK_Return: return ImGuiKey_Enter;
      case XK_Escape: return ImGuiKey_Escape;
      case XK_BackSpace: return ImGuiKey_Backspace;
      case XK_Tab: return ImGuiKey_Tab;
      case XK_space: return ImGuiKey_Space;
      default:
         if (ks >= XK_a && ks <= XK_z)
            return static_cast<ImGuiKey>(ImGuiKey_A + (ks - XK_a));
         if (ks >= XK_0 && ks <= XK_9)
            return static_cast<ImGuiKey>(ImGuiKey_0 + (ks - XK_0));
         return ImGuiKey_None;
   }
}

static void process_input(X11Context &x11, PlatformState &pstate, bool &running)
{
   XEvent xev;
   ImGuiIO &io = ImGui::GetIO();

   while (XPending(x11.display)) {
      XNextEvent(x11.display, &xev);

      switch (xev.type) {
         case KeyPress:
         case KeyRelease: {
            bool pressed = xev.type == KeyPress;
            KeySym ks    = XLookupKeysym(&xev.xkey, 0);

            // Update pstate.keys safely only for 0–255
            if (ks < 256)
               pstate.keys[ks] = pressed;

            // Translate to ImGuiKey and send event
            ImGuiKey imgui_key = keysym_to_imgui(ks);
            if (imgui_key != ImGuiKey_None)
               io.AddKeyEvent(imgui_key, pressed);

            // Only ASCII characters for InputText
            if (pressed && ks >= XK_space && ks <= XK_asciitilde)
               io.AddInputCharacter(static_cast<unsigned int>(ks));

            break;
         }

         case ButtonPress:
         case ButtonRelease: {
            int btn = xev.xbutton.button - 1;
            if (btn >= 0 && btn < 3)
               pstate.mouse_buttons[btn] = (xev.type == ButtonPress);
            break;
         }

         case MotionNotify: {
            pstate.mouse_x = xev.xmotion.x;
            pstate.mouse_y = xev.xmotion.y;
            break;
         }

         case ClientMessage: running = false; break;
      }
   }
}

static void render_frame(X11Context &x11, PlatformState &pstate,
                         struct state &app_state)
{
   ImGuiIO &io = ImGui::GetIO();

   // Update display size
   Window r;
   int wx, wy;
   unsigned int w, h, bw, depth;
   XGetGeometry(x11.display, x11.window, &r, &wx, &wy, &w, &h, &bw, &depth);
   io.DisplaySize.x = static_cast<float>(w);
   io.DisplaySize.y = static_cast<float>(h);

   // Delta time
   static double last_time = 0.0;
   double current_time     = static_cast<double>(clock()) / CLOCKS_PER_SEC;
   io.DeltaTime = last_time > 0.0 ? static_cast<float>(current_time - last_time)
                                  : 1.0f / 60.0f;
   last_time    = current_time;

   // Mouse
   io.MousePos     = ImVec2(static_cast<float>(pstate.mouse_x),
                            static_cast<float>(pstate.mouse_y));
   io.MouseDown[0] = pstate.mouse_buttons[0];
   io.MouseDown[1] = pstate.mouse_buttons[1];
   io.MouseDown[2] = pstate.mouse_buttons[2];

   // Start frame
   ImGui_ImplOpenGL3_NewFrame();
   ImGui::NewFrame();

   app_render(&app_state);

   ImGui::Render();
   glClear(GL_COLOR_BUFFER_BIT);
   ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

   glXSwapBuffers(x11.display, x11.window);
}

int main()
{
   try {
      X11Context x11 = init_x11(800, 650, "ImGui Example");
      init_imgui();

      struct state app_state{};
      app_init(&app_state);

      PlatformState pstate{};
      bool running = true;

      while (running) {
         process_input(x11, pstate, running);

         poll_midi(&app_state);
         logic_receive(&app_state);

         render_frame(x11, pstate, app_state);

         // ~60 FPS
         usleep(16000);
      }

      ImGui_ImplOpenGL3_Shutdown();
      ImGui::DestroyContext();

      glXMakeCurrent(x11.display, None, nullptr);
      glXDestroyContext(x11.display, x11.gl_context);
      XDestroyWindow(x11.display, x11.window);
      XCloseDisplay(x11.display);
   } catch (const std::exception &e) {
      fprintf(stderr, "Error: %s\n", e.what());
      return 1;
   }

   return 0;
}
