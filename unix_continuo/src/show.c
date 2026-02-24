// SPDX-License-Identifier: MIT
// show.c --- display the primitive stream from draw in an X11 window
// Copyright (c) 2026 Jakob Kastelic

#define _POSIX_C_SOURCE 200809L
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Tunables                                                             */
/* ------------------------------------------------------------------ */

#define FONT_NAME   "-*-fixed-medium-r-*-*-14-*-*-*-*-*-iso8859-*"
#define FONT_SMALL  "-*-fixed-medium-r-*-*-11-*-*-*-*-*-iso8859-*"
#define BEZIER_SEGS 24      /* line segments used to tessellate one bezier */
#define MAX_LINE    4096    /* maximum input line length                   */
#define MAX_CMDS    65536   /* maximum number of stored draw commands      */

/* ------------------------------------------------------------------ */
/* Command storage                                                      */
/* ------------------------------------------------------------------ */

typedef enum {
    CMD_STROKE_LINE,
    CMD_FILL_CIRCLE,
    CMD_STROKE_CIRCLE,
    CMD_STROKE_ARC,
    CMD_FILL_ARC,
    CMD_FILL_TRIANGLE,
    CMD_STROKE_CURVE,
    CMD_FILL_RECT,
    CMD_DRAW_TEXT
} CmdKind;

typedef struct {
    CmdKind kind;
    int     a[12];  /* integer arguments: coords, sizes, thickness, r g b */
    char   *text;   /* heap string for CMD_DRAW_TEXT, NULL otherwise       */
} Cmd;

static Cmd  cmds[MAX_CMDS];
static int  ncmds    = 0;
static int  canvas_w = 800;   /* score pixel width  (from "# canvas W H") */
static int  canvas_h = 600;   /* score pixel height (from "# canvas W H") */

/* ------------------------------------------------------------------ */
/* Colour                                                               */
/* ------------------------------------------------------------------ */

static unsigned long
make_color(Display *dpy, int r, int g, int b)
{
    XColor c;
    c.red   = (unsigned short)(r * 257);
    c.green = (unsigned short)(g * 257);
    c.blue  = (unsigned short)(b * 257);
    c.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), &c);
    return c.pixel;
}

/* ------------------------------------------------------------------ */
/* Bezier curve tessellation                                            */
/* Matches the algorithm in nuklear_xlib nk_xsurf_stroke_curve.        */
/* ------------------------------------------------------------------ */

static void
draw_bezier(Display *dpy, Drawable d, GC gc,
            int x0, int y0, int cx0, int cy0,
            int cx1, int cy1, int x1, int y1)
{
    float ts = 1.0f / BEZIER_SEGS;
    int   lx = x0, ly = y0;
    int   i;

    for (i = 1; i <= BEZIER_SEGS; i++) {
        float t  = ts * (float)i;
        float u  = 1.0f - t;
        float w1 = u*u*u;
        float w2 = 3.0f*u*u*t;
        float w3 = 3.0f*u*t*t;
        float w4 = t*t*t;
        int   nx = (int)(w1*(float)x0 + w2*(float)cx0
                       + w3*(float)cx1 + w4*(float)x1);
        int   ny = (int)(w1*(float)y0 + w2*(float)cy0
                       + w3*(float)cy1 + w4*(float)y1);
        XDrawLine(dpy, d, gc, lx, ly, nx, ny);
        lx = nx;
        ly = ny;
    }
}

/* ------------------------------------------------------------------ */
/* Render all stored commands onto drawable d                           */
/* ------------------------------------------------------------------ */

static void
render_all(Display *dpy, Drawable d, GC gc,
           XFontStruct *font, XFontStruct *font_small)
{
    unsigned long col;
    int i;

    /* White background fills the full score canvas. */
    XSetForeground(dpy, gc, make_color(dpy, 255, 255, 255));
    XFillRectangle(dpy, d, gc, 0, 0,
                   (unsigned)canvas_w, (unsigned)canvas_h);

    for (i = 0; i < ncmds; i++) {
        Cmd *cmd = &cmds[i];
        int *a   = cmd->a;

        switch (cmd->kind) {

        /* stroke_line x0 y0 x1 y1 thickness r g b */
        case CMD_STROKE_LINE:
            col = make_color(dpy, a[5], a[6], a[7]);
            XSetForeground(dpy, gc, col);
            XSetLineAttributes(dpy, gc, (unsigned)a[4],
                               LineSolid, CapRound, JoinRound);
            XDrawLine(dpy, d, gc, a[0], a[1], a[2], a[3]);
            XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
            break;

        /* fill_circle x y w h r g b  (bounding-box top-left) */
        case CMD_FILL_CIRCLE:
            col = make_color(dpy, a[4], a[5], a[6]);
            XSetForeground(dpy, gc, col);
            XFillArc(dpy, d, gc, a[0], a[1],
                     (unsigned)a[2], (unsigned)a[3], 0, 360*64);
            break;

        /* stroke_circle x y w h thickness r g b */
        case CMD_STROKE_CIRCLE:
            col = make_color(dpy, a[5], a[6], a[7]);
            XSetForeground(dpy, gc, col);
            XSetLineAttributes(dpy, gc, (unsigned)a[4],
                               LineSolid, CapButt, JoinMiter);
            XDrawArc(dpy, d, gc, a[0], a[1],
                     (unsigned)a[2], (unsigned)a[3], 0, 360*64);
            XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
            break;

        /* stroke_arc cx cy radius a_min_deg a_max_deg thickness r g b
         * X11 arc angles are in 64ths of a degree, CCW from 3-o'clock. */
        case CMD_STROKE_ARC: {
            int rad    = a[2];
            int start  = a[3] * 64;
            int extent = (a[4] - a[3]) * 64;
            col = make_color(dpy, a[6], a[7], a[8]);
            XSetForeground(dpy, gc, col);
            XSetLineAttributes(dpy, gc, (unsigned)a[5],
                               LineSolid, CapButt, JoinMiter);
            XDrawArc(dpy, d, gc,
                     a[0]-rad, a[1]-rad,
                     (unsigned)(2*rad), (unsigned)(2*rad),
                     start, extent);
            XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
            break;
        }

        /* fill_arc cx cy radius a_min_deg a_max_deg r g b */
        case CMD_FILL_ARC: {
            int rad    = a[2];
            int start  = a[3] * 64;
            int extent = (a[4] - a[3]) * 64;
            col = make_color(dpy, a[5], a[6], a[7]);
            XSetForeground(dpy, gc, col);
            XFillArc(dpy, d, gc,
                     a[0]-rad, a[1]-rad,
                     (unsigned)(2*rad), (unsigned)(2*rad),
                     start, extent);
            break;
        }

        /* fill_triangle x0 y0 x1 y1 x2 y2 r g b */
        case CMD_FILL_TRIANGLE: {
            XPoint pts[3];
            pts[0].x = (short)a[0]; pts[0].y = (short)a[1];
            pts[1].x = (short)a[2]; pts[1].y = (short)a[3];
            pts[2].x = (short)a[4]; pts[2].y = (short)a[5];
            col = make_color(dpy, a[6], a[7], a[8]);
            XSetForeground(dpy, gc, col);
            XFillPolygon(dpy, d, gc, pts, 3, Convex, CoordModeOrigin);
            break;
        }

        /* stroke_curve x0 y0 cx0 cy0 cx1 cy1 x1 y1 thickness r g b */
        case CMD_STROKE_CURVE:
            col = make_color(dpy, a[9], a[10], a[11]);
            XSetForeground(dpy, gc, col);
            XSetLineAttributes(dpy, gc, (unsigned)a[8],
                               LineSolid, CapRound, JoinRound);
            draw_bezier(dpy, d, gc,
                        a[0], a[1], a[2], a[3],
                        a[4], a[5], a[6], a[7]);
            XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
            break;

        /* fill_rect x y w h r g b */
        case CMD_FILL_RECT:
            col = make_color(dpy, a[4], a[5], a[6]);
            XSetForeground(dpy, gc, col);
            XFillRectangle(dpy, d, gc, a[0], a[1],
                           (unsigned)a[2], (unsigned)a[3]);
            break;

        /* draw_text x y "text" r g b
         * a[2] > 80 means the colour is grey (figured-bass): smaller font. */
        case CMD_DRAW_TEXT:
            if (!cmd->text) break;
            col = make_color(dpy, a[2], a[3], a[4]);
            XSetForeground(dpy, gc, col);
            if (a[2] > 80 && font_small) {
                XSetFont(dpy, gc, font_small->fid);
                XDrawString(dpy, d, gc, a[0], a[1],
                            cmd->text, (int)strlen(cmd->text));
                XSetFont(dpy, gc, font->fid);
            } else {
                XDrawString(dpy, d, gc, a[0], a[1],
                            cmd->text, (int)strlen(cmd->text));
            }
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Blit score pixmap centred in the window                             */
/* Any margin around the score is filled with white.                   */
/* ------------------------------------------------------------------ */

static void
blit_centered(Display *dpy, Window win, GC gc, Pixmap buf,
              int win_w, int win_h)
{
    int off_x  = (win_w - canvas_w) / 2;
    int off_y  = (win_h - canvas_h) / 2;
    int copy_w = canvas_w < win_w ? canvas_w : win_w;
    int copy_h = canvas_h < win_h ? canvas_h : win_h;

    if (off_x < 0) off_x = 0;
    if (off_y < 0) off_y = 0;

    /* Clear the whole window so margins outside the score are white. */
    XSetForeground(dpy, gc, WhitePixel(dpy, DefaultScreen(dpy)));
    XFillRectangle(dpy, win, gc, 0, 0,
                   (unsigned)win_w, (unsigned)win_h);

    XCopyArea(dpy, buf, win, gc,
              0, 0, (unsigned)copy_w, (unsigned)copy_h,
              off_x, off_y);
}

/* ------------------------------------------------------------------ */
/* Input parsing                                                        */
/* ------------------------------------------------------------------ */

static void
parse_stdin(void)
{
    char line[MAX_LINE];
    char tok[MAX_LINE];
    char tbuf[MAX_LINE];

    while (fgets(line, sizeof(line), stdin)) {
        char *p = line;
        int   n;

        /* Skip leading whitespace. */
        while (*p == ' ' || *p == '\t') p++;

        /* Blank line. */
        if (*p == '\0' || *p == '\n') continue;

        /* Comment lines: the only interesting one is "# canvas W H". */
        if (*p == '#') {
            int w, h;
            if (sscanf(p, "# canvas %d %d", &w, &h) == 2) {
                canvas_w = w;
                canvas_h = h;
            }
            continue;
        }

        if (ncmds >= MAX_CMDS) {
            fprintf(stderr, "show: too many commands (max %d)\n", MAX_CMDS);
            break;
        }

        /* Read keyword, advance p past it. */
        n = 0;
        if (sscanf(p, "%s%n", tok, &n) != 1) continue;
        p += n;

        {
            Cmd *cmd = &cmds[ncmds];
            memset(cmd, 0, sizeof(*cmd));

            /* stroke_line x0 y0 x1 y1 t r g b */
            if (strcmp(tok, "stroke_line") == 0) {
                if (sscanf(p, "%d %d %d %d %d %d %d %d",
                           &cmd->a[0], &cmd->a[1], &cmd->a[2], &cmd->a[3],
                           &cmd->a[4], &cmd->a[5], &cmd->a[6],
                           &cmd->a[7]) != 8) continue;
                cmd->kind = CMD_STROKE_LINE; ncmds++; continue;
            }

            /* fill_circle x y w h r g b */
            if (strcmp(tok, "fill_circle") == 0) {
                if (sscanf(p, "%d %d %d %d %d %d %d",
                           &cmd->a[0], &cmd->a[1], &cmd->a[2], &cmd->a[3],
                           &cmd->a[4], &cmd->a[5], &cmd->a[6]) != 7) continue;
                cmd->kind = CMD_FILL_CIRCLE; ncmds++; continue;
            }

            /* stroke_circle x y w h t r g b */
            if (strcmp(tok, "stroke_circle") == 0) {
                if (sscanf(p, "%d %d %d %d %d %d %d %d",
                           &cmd->a[0], &cmd->a[1], &cmd->a[2], &cmd->a[3],
                           &cmd->a[4], &cmd->a[5], &cmd->a[6],
                           &cmd->a[7]) != 8) continue;
                cmd->kind = CMD_STROKE_CIRCLE; ncmds++; continue;
            }

            /* stroke_arc cx cy radius a_min a_max t r g b */
            if (strcmp(tok, "stroke_arc") == 0) {
                if (sscanf(p, "%d %d %d %d %d %d %d %d %d",
                           &cmd->a[0], &cmd->a[1], &cmd->a[2], &cmd->a[3],
                           &cmd->a[4], &cmd->a[5], &cmd->a[6], &cmd->a[7],
                           &cmd->a[8]) != 9) continue;
                cmd->kind = CMD_STROKE_ARC; ncmds++; continue;
            }

            /* fill_arc cx cy radius a_min a_max r g b */
            if (strcmp(tok, "fill_arc") == 0) {
                if (sscanf(p, "%d %d %d %d %d %d %d %d",
                           &cmd->a[0], &cmd->a[1], &cmd->a[2], &cmd->a[3],
                           &cmd->a[4], &cmd->a[5], &cmd->a[6],
                           &cmd->a[7]) != 8) continue;
                cmd->kind = CMD_FILL_ARC; ncmds++; continue;
            }

            /* fill_triangle x0 y0 x1 y1 x2 y2 r g b */
            if (strcmp(tok, "fill_triangle") == 0) {
                if (sscanf(p, "%d %d %d %d %d %d %d %d %d",
                           &cmd->a[0], &cmd->a[1], &cmd->a[2], &cmd->a[3],
                           &cmd->a[4], &cmd->a[5], &cmd->a[6], &cmd->a[7],
                           &cmd->a[8]) != 9) continue;
                cmd->kind = CMD_FILL_TRIANGLE; ncmds++; continue;
            }

            /* stroke_curve x0 y0 cx0 cy0 cx1 cy1 x1 y1 t r g b */
            if (strcmp(tok, "stroke_curve") == 0) {
                if (sscanf(p, "%d %d %d %d %d %d %d %d %d %d %d %d",
                           &cmd->a[0],  &cmd->a[1],  &cmd->a[2],  &cmd->a[3],
                           &cmd->a[4],  &cmd->a[5],  &cmd->a[6],  &cmd->a[7],
                           &cmd->a[8],  &cmd->a[9],  &cmd->a[10],
                           &cmd->a[11]) != 12) continue;
                cmd->kind = CMD_STROKE_CURVE; ncmds++; continue;
            }

            /* fill_rect x y w h r g b */
            if (strcmp(tok, "fill_rect") == 0) {
                if (sscanf(p, "%d %d %d %d %d %d %d",
                           &cmd->a[0], &cmd->a[1], &cmd->a[2], &cmd->a[3],
                           &cmd->a[4], &cmd->a[5], &cmd->a[6]) != 7) continue;
                cmd->kind = CMD_FILL_RECT; ncmds++; continue;
            }

            /* draw_text x y "text" r g b
             * The text field may contain spaces; after scanning x and y we
             * walk the remainder of the line manually to extract the quoted
             * token and the trailing r g b. */
            if (strcmp(tok, "draw_text") == 0) {
                int   x, y, r, g, b;
                char *q;
                int   fields, sn;

                if (sscanf(p, "%d %d", &x, &y) != 2) continue;

                /* Walk past the two number fields. */
                q      = p;
                fields = 0;
                while (*q && fields < 2) {
                    while (*q == ' ' || *q == '\t') q++;
                    while (*q && *q != ' ' && *q != '\t') q++;
                    fields++;
                }
                while (*q == ' ' || *q == '\t') q++;

                /* Extract the (possibly multi-word) quoted string. */
                if (*q == '"') {
                    char *end = strchr(q + 1, '"');
                    int   tlen;
                    if (!end) continue;
                    tlen = (int)(end - q - 1);
                    if (tlen >= (int)sizeof(tbuf))
                        tlen = (int)sizeof(tbuf) - 1;
                    memcpy(tbuf, q + 1, (size_t)tlen);
                    tbuf[tlen] = '\0';
                    q = end + 1;
                } else {
                    sn = 0;
                    if (sscanf(q, "%s%n", tbuf, &sn) != 1) continue;
                    q += sn;
                }

                if (sscanf(q, "%d %d %d", &r, &g, &b) != 3) continue;

                cmd->kind = CMD_DRAW_TEXT;
                cmd->a[0] = x;
                cmd->a[1] = y + 12; /* X11 draws text from baseline; shift down */
                cmd->a[2] = r;
                cmd->a[3] = g;
                cmd->a[4] = b;
                cmd->text = strdup(tbuf);
                if (!cmd->text) { perror("strdup"); exit(1); }
                ncmds++;
                continue;
            }

            /* Unknown keyword — silently ignore. */
        }
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                                 */
/* ------------------------------------------------------------------ */

int
main(void)
{
    Display     *dpy;
    int          screen;
    Window       root, win;
    GC           gc;
    Pixmap       buf;
    XFontStruct *font, *font_small;
    XEvent       ev;
    Atom         wm_delete;
    int          win_w, win_h;
    int          running;

    /* Read and parse the entire stream before opening the window so that
     * canvas_w / canvas_h are known before XCreateSimpleWindow is called. */
    parse_stdin();

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "show: cannot open display\n");
        return 1;
    }
    screen = DefaultScreen(dpy);
    root   = RootWindow(dpy, screen);

    /* Load fonts; fall back to the guaranteed "fixed" if necessary. */
    font = XLoadQueryFont(dpy, FONT_NAME);
    if (!font) font = XLoadQueryFont(dpy, "fixed");
    if (!font) { fprintf(stderr, "show: cannot load font\n"); return 1; }

    font_small = XLoadQueryFont(dpy, FONT_SMALL);
    if (!font_small) font_small = font;

    /* Open the window at the score's natural pixel size. */
    win_w = canvas_w;
    win_h = canvas_h;

    win = XCreateSimpleWindow(dpy, root, 0, 0,
                              (unsigned)win_w, (unsigned)win_h, 0,
                              BlackPixel(dpy, screen),
                              WhitePixel(dpy, screen));
    XStoreName(dpy, win, "Score");
    XSelectInput(dpy, win,
                 ExposureMask | KeyPressMask | StructureNotifyMask);

    /* Advertise the score's natural size as preferred; impose no maximum
     * so the window manager allows the window to be freely resized. */
    {
        XSizeHints hints;
        memset(&hints, 0, sizeof(hints));
        hints.flags      = PSize | PMinSize;
        hints.width      = canvas_w;
        hints.height     = canvas_h;
        hints.min_width  = 64;
        hints.min_height = 64;
        XSetWMNormalHints(dpy, win, &hints);
    }

    /* Handle close-button clicks from the window manager. */
    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);

    gc = XCreateGC(dpy, win, 0, NULL);
    XSetFont(dpy, gc, font->fid);
    XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);

    /* Render the score once into a fixed-size off-screen pixmap.
     * On resize the pixmap is unchanged; only the blit position moves. */
    buf = XCreatePixmap(dpy, win,
                        (unsigned)canvas_w, (unsigned)canvas_h,
                        (unsigned)DefaultDepth(dpy, screen));
    render_all(dpy, buf, gc, font, font_small);

    XMapWindow(dpy, win);
    XFlush(dpy);

    running = 1;
    while (running) {
        XNextEvent(dpy, &ev);

        switch (ev.type) {

        case Expose:
            /* count == 0: last expose in a batch — safe to paint now. */
            if (ev.xexpose.count == 0)
                blit_centered(dpy, win, gc, buf, win_w, win_h);
            break;

        case ConfigureNotify:
            /* Track every resize the WM reports. */
            win_w = ev.xconfigure.width;
            win_h = ev.xconfigure.height;
            blit_centered(dpy, win, gc, buf, win_w, win_h);
            break;

        case KeyPress: {
            KeySym key = XLookupKeysym(&ev.xkey, 0);
            if (key == XK_q || key == XK_Q || key == XK_Escape)
                running = 0;
            break;
        }

        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == wm_delete)
                running = 0;
            break;

        default:
            break;
        }
    }

    /* Free all resources in reverse allocation order. */
    {
        int i;
        for (i = 0; i < ncmds; i++)
            free(cmds[i].text);
    }
    XFreePixmap(dpy, buf);
    XFreeGC(dpy, gc);
    if (font_small != font) XFreeFont(dpy, font_small);
    XFreeFont(dpy, font);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
