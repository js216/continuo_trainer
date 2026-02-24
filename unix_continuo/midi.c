/* midi.c --- read MIDI input from device and output timed events
 *
 * Reads incoming MIDI messages from connected devices. Outputs text
 * events with timestamp and channel info suitable for downstream
 * programs. Accepts commands via stdin to select input/output device
 * and optionally forward input to output (hardware synth). On startup
 * reports available MIDI devices.
 *
 * Settings (connected devices, forwarding) are saved to midi.log on
 * every change and restored automatically on startup.
 *
 * Commands (stdin):
 *   MIDI IN <n>      -- open MIDI input device number n
 *   MIDI OUT <n>     -- open MIDI output device number n
 *   MIDI FORWARD ON  -- enable forwarding of input to output
 *   MIDI FORWARD OFF -- disable forwarding
 *   MIDI DEVICES     -- re-enumerate and print available devices
 *   MIDI TEST        -- send a test note (C#4) to the output
 *
 * Events (stdout):
 *   DEVICE_AVAIL <n> <devicename>
 *   NOTE_ON <lily> VELOCITY:<v> TIME:<ms>
 *   NOTE_OFF <lily>
 *   STATUS <message>
 *
 * LilyPond absolute pitch examples: c' = middle C, fis'' = F#5, bes = Bb3
 */

/* Required for: pipe, clock_gettime, struct timespec, poll */
#define _POSIX_C_SOURCE 200809L

#include <rtmidi/rtmidi_c.h>

#include <inttypes.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

#define MAX_DEVICES 64
#define MAX_NAME_LEN 256
#define MAX_PRESSED 128
#define CMD_BUF_SZ 512
#define LOG_PATH "midi.log"

/* LilyPond absolute pitch note names (chromatic scale, no flats) */
static const char *const NOTE_NAMES[12] = {
    "c", "cis", "d", "dis", "e", "f", "fis", "g", "gis", "a", "ais", "b"};

/* C#4 = MIDI note 61, matching NOTES_Cs4 in theory.h */
#define NOTE_Cs4 61

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

typedef struct {
	/* Device registry */
	char dev_names[MAX_DEVICES][MAX_NAME_LEN];
	int n_devices;

	/* Currently open port indices (-1 = none) */
	int in_idx;
	int out_idx;
	RtMidiInPtr midi_in;
	RtMidiOutPtr midi_out;

	/* Pressed-note list, insertion-ordered */
	unsigned char pressed[MAX_PRESSED];
	int n_pressed;

	/* MIDI-forwarding flag */
	int forward;

	/* Saved device names from log (used to reopen on startup) */
	char saved_in_name[MAX_NAME_LEN];
	char saved_out_name[MAX_NAME_LEN];

	/* Self-pipe: MIDI callback writes a byte to wake up poll() */
	int pipe_r; /* read end  – watched by poll() */
	int pipe_w; /* write end – written by MIDI callback */

	/* Set to 0 to exit the main loop */
	int running;
} State;

/* ------------------------------------------------------------------ */
/* Utility                                                             */
/* ------------------------------------------------------------------ */

int64_t now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);

	return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
}

/* Convert MIDI note to LilyPond absolute pitch, e.g. 60 -> "c'", 74 -> "d''" */
static void note_to_lily(unsigned char note, char *buf, size_t len)
{
	/* MIDI octave: note/12 - 1, so octave 4 (middle C = 60) gives 4.
	   LilyPond: c' = middle C (octave 4), c'' = octave 5,
		     c  = octave 3, c, = octave 2, c,, = octave 1 */
	int octave = (int)(note / 12) - 1;
	const char *name = NOTE_NAMES[note % 12];
	size_t pos = (size_t)snprintf(buf, len, "%s", name);

	if (octave >= 4) {
		int ticks = octave - 3; /* c' at octave 4 -> 1 tick */
		for (int i = 0; i < ticks && pos < len - 1; i++)
			buf[pos++] = '\'';
	} else {
		int commas =
		    3 - octave; /* c at octave 3 -> 0 commas; c, -> octave 2 */
		for (int i = 0; i < commas && pos < len - 1; i++)
			buf[pos++] = ',';
	}
	buf[pos] = '\0';
}

/* ------------------------------------------------------------------ */
/* Output helpers                                                      */
/* ------------------------------------------------------------------ */

static void out_status(const char *fmt, ...)
{
	va_list ap;
	printf("STATUS ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
	fflush(stdout);
}

static void out_devices(const State *s)
{
	for (int i = 0; i < s->n_devices; i++) {
		printf("DEVICE_AVAIL %d %s\n", i, s->dev_names[i]);
	}
	fflush(stdout);
}

static void out_note_on(unsigned char note, unsigned char velocity)
{
	char name[16];
	note_to_lily(note, name, sizeof(name));
	printf("NOTE_ON %s VELOCITY:%u TIME:%" PRId64 "\n", name,
	       (unsigned)velocity, now_ms());
	fflush(stdout);
}

static void out_note_off(unsigned char note)
{
	char name[16];
	note_to_lily(note, name, sizeof(name));
	printf("NOTE_OFF %s TIME:%" PRId64 "\n", name, now_ms());
	fflush(stdout);
}

/* ------------------------------------------------------------------ */
/* Note tracking                                                       */
/* ------------------------------------------------------------------ */

static void add_pressed_note(State *s, unsigned char note)
{
	for (int i = 0; i < s->n_pressed; i++)
		if (s->pressed[i] == note)
			return;
	if (s->n_pressed < MAX_PRESSED)
		s->pressed[s->n_pressed++] = note;
}

static void remove_pressed_note(State *s, unsigned char note)
{
	for (int i = 0; i < s->n_pressed; i++) {
		if (s->pressed[i] == note) {
			memmove(&s->pressed[i], &s->pressed[i + 1],
				(size_t)(s->n_pressed - i - 1));
			s->n_pressed--;
			return;
		}
	}
}

/* ------------------------------------------------------------------ */
/* MIDI callback – runs in RtMidi's background thread.                */
/*                                                                     */
/* All message handling is done here. You cannot mix the callback API  */
/* with rtmidi_in_get_message() – calling getMessage() when a callback */
/* is set produces the "a user callback is currently set" warning and  */
/* returns nothing. The self-pipe byte just wakes poll() on the main   */
/* thread so it can react to stdin commands without busy-waiting; the  */
/* actual MIDI work is already done before that byte is written.       */
/* ------------------------------------------------------------------ */

static void midi_callback(double stamp, const unsigned char *msg, size_t size,
			  void *userdata)
{
	(void)stamp;
	State *s = (State *)userdata;

	if (size < 3)
		goto wake;

	{
		unsigned char status = msg[0] & 0xF0U;
		unsigned char note = msg[1];
		unsigned char velocity = msg[2];
		int changed = 0;

		if (status == 0x90 && velocity > 0) {
			add_pressed_note(s, note);
			out_note_on(note, velocity);
			changed = 1;
		} else if (status == 0x80 ||
			   (status == 0x90 && velocity == 0)) {
			remove_pressed_note(s, note);
			out_note_off(note);
			changed = 1;
		}

		(void)changed;

		/* Forward raw bytes to output if enabled */
		if (s->forward && s->midi_out)
			rtmidi_out_send_message(s->midi_out, msg, (int)size);
	}

wake:
	/* Wake the main poll() so it can handle any pending stdin too */
	(void)write(s->pipe_w, "!", 1);
}

/* Forward declarations (open_midi_in/out are needed by restore_from_log) */
static void open_midi_in(State *s, int idx);
static void open_midi_out(State *s, int idx);

/* ------------------------------------------------------------------ */
/* Settings log                                                        */
/*                                                                     */
/* midi.log stores device names (not indices, which can shift when     */
/* devices are plugged/unplugged) and the forward flag. Format:        */
/*   IN <device name>                                                  */
/*   OUT <device name>                                                 */
/*   FORWARD 1                                                         */
/* ------------------------------------------------------------------ */

static void save_log(const State *s)
{
	FILE *f = fopen(LOG_PATH, "w");
	if (!f) {
		out_status("Warning: could not write %s", LOG_PATH);
		return;
	}
	if (s->in_idx >= 0)
		fprintf(f, "IN %s\n", s->dev_names[s->in_idx]);
	if (s->out_idx >= 0)
		fprintf(f, "OUT %s\n", s->dev_names[s->out_idx]);
	fprintf(f, "FORWARD %d\n", s->forward);
	fclose(f);
}

/* Find device index by exact name match; returns -1 if not found */
static int find_device_by_name(const State *s, const char *name)
{
	for (int i = 0; i < s->n_devices; i++)
		if (strcmp(s->dev_names[i], name) == 0)
			return i;
	return -1;
}

static void load_log(State *s)
{
	FILE *f = fopen(LOG_PATH, "r");
	if (!f)
		return; /* no log yet, that's fine */

	char line[CMD_BUF_SZ];
	while (fgets(line, sizeof(line), f)) {
		/* strip newline */
		char *nl = strpbrk(line, "\r\n");
		if (nl)
			*nl = '\0';

		char name[MAX_NAME_LEN];
		int fwd;

		if (sscanf(line, "IN %255[^\n]", name) == 1)
			strncpy(s->saved_in_name, name, MAX_NAME_LEN - 1);
		else if (sscanf(line, "OUT %255[^\n]", name) == 1)
			strncpy(s->saved_out_name, name, MAX_NAME_LEN - 1);
		else if (sscanf(line, "FORWARD %d", &fwd) == 1)
			s->forward = fwd;
	}
	fclose(f);
}

/* Called after refresh_devices() to reopen ports saved in the log */
static void restore_from_log(State *s)
{
	if (s->saved_in_name[0]) {
		int idx = find_device_by_name(s, s->saved_in_name);
		if (idx >= 0)
			open_midi_in(s, idx);
		else
			out_status("Saved IN device not found: %s",
				   s->saved_in_name);
	}
	if (s->saved_out_name[0]) {
		int idx = find_device_by_name(s, s->saved_out_name);
		if (idx >= 0)
			open_midi_out(s, idx);
		else
			out_status("Saved OUT device not found: %s",
				   s->saved_out_name);
	}
	if (s->forward)
		out_status("Forwarding restored");
}

/* ------------------------------------------------------------------ */
/* Device management                                                   */
/* ------------------------------------------------------------------ */

static void refresh_devices(State *s)
{
	s->n_devices = 0;

	RtMidiInPtr probe = rtmidi_in_create_default();
	if (!probe || !probe->ok) {
		out_status("RtMidi probe failed");
		if (probe)
			rtmidi_in_free(probe);
		return;
	}

	unsigned int n = rtmidi_get_port_count(probe);
	for (unsigned int i = 0; i < n && i < MAX_DEVICES; i++) {
		int len = MAX_NAME_LEN;
		rtmidi_get_port_name(probe, i, s->dev_names[i], &len);
		s->n_devices++;
	}
	rtmidi_in_free(probe);

	if (s->n_devices == 0) {
		strncpy(s->dev_names[0], "(no MIDI devices)", MAX_NAME_LEN - 1);
		s->dev_names[0][MAX_NAME_LEN - 1] = '\0';
		s->n_devices = 1;
	}

	out_devices(s);
}

static void open_midi_in(State *s, int idx)
{
	if (idx < 0 || idx >= s->n_devices) {
		out_status("Invalid IN device index");
		return;
	}

	if (s->midi_in) {
		rtmidi_close_port(s->midi_in);
		rtmidi_in_free(s->midi_in);
		s->midi_in = NULL;
		s->in_idx = -1;
	}

	RtMidiInPtr h = rtmidi_in_create_default();
	if (!h || !h->ok) {
		out_status("Failed to create MIDI input");
		return;
	}

	/* Register callback before opening so no messages are missed.
	   All message handling happens inside the callback; we never call
	   rtmidi_in_get_message() since that conflicts with callback mode. */
	rtmidi_in_set_callback(h, midi_callback, s);

	rtmidi_open_port(h, (unsigned int)idx, "midi_c_in");
	rtmidi_in_ignore_types(h, 0, 0,
			       0); /* don't ignore sysex/timing/sense */

	if (!h->ok) {
		out_status("Failed to open MIDI input port");
		rtmidi_in_free(h);
		return;
	}

	s->midi_in = h;
	s->in_idx = idx;
	out_status("MIDI input opened: %s", s->dev_names[idx]);
}

static void open_midi_out(State *s, int idx)
{
	if (idx < 0 || idx >= s->n_devices) {
		out_status("Invalid OUT device index");
		return;
	}

	if (s->midi_out) {
		rtmidi_close_port(s->midi_out);
		rtmidi_out_free(s->midi_out);
		s->midi_out = NULL;
		s->out_idx = -1;
	}

	RtMidiOutPtr h = rtmidi_out_create_default();
	if (!h || !h->ok) {
		out_status("Failed to create MIDI output");
		return;
	}

	rtmidi_open_port(h, (unsigned int)idx, "midi_c_out");

	if (!h->ok) {
		out_status("Failed to open MIDI output port");
		rtmidi_out_free(h);
		return;
	}

	s->midi_out = h;
	s->out_idx = idx;
	out_status("MIDI output opened: %s", s->dev_names[idx]);
}

static void close_midi_in(State *s)
{
	if (s->midi_in) {
		rtmidi_close_port(s->midi_in);
		rtmidi_in_free(s->midi_in);
		s->midi_in = NULL;
		s->in_idx = -1;
		out_status("MIDI input disconnected");
	} else {
		out_status("No MIDI input connected");
	}
}

static void close_midi_out(State *s)
{
	if (s->midi_out) {
		rtmidi_close_port(s->midi_out);
		rtmidi_out_free(s->midi_out);
		s->midi_out = NULL;
		s->out_idx = -1;
		out_status("MIDI output disconnected");
	} else {
		out_status("No MIDI output connected");
	}
}

static void test_midi_out(State *s)
{
	if (!s->midi_out) {
		out_status("No MIDI output connected");
		return;
	}

	unsigned char msg_on[3] = {0x90, NOTE_Cs4, 100};
	unsigned char msg_off[3] = {0x80, NOTE_Cs4, 0};

	if (rtmidi_out_send_message(s->midi_out, msg_on, 3) < 0) {
		out_status("MIDI test error (Note On)");
		return;
	}

	struct timespec ts_wait = {0, 250 * 1000 * 1000};
	nanosleep(&ts_wait, NULL);

	if (rtmidi_out_send_message(s->midi_out, msg_off, 3) < 0) {
		out_status("MIDI test error (Note Off)");
		return;
	}

	out_status("MIDI test sent: C#4");
}

/* ------------------------------------------------------------------ */
/* Command parsing                                                     */
/* ------------------------------------------------------------------ */

static void handle_command(State *s, const char *line)
{
	char cmd[CMD_BUF_SZ];
	strncpy(cmd, line, CMD_BUF_SZ - 1);
	cmd[CMD_BUF_SZ - 1] = '\0';

	char *nl = strpbrk(cmd, "\r\n");
	if (nl)
		*nl = '\0';

	int n;

	if (sscanf(cmd, "MIDI IN %d", &n) == 1) {
		open_midi_in(s, n);
		save_log(s);
	} else if (sscanf(cmd, "MIDI OUT %d", &n) == 1) {
		open_midi_out(s, n);
		save_log(s);
	} else if (strcmp(cmd, "MIDI FORWARD ON") == 0) {
		s->forward = 1;
		out_status("Forwarding enabled");
		save_log(s);
	} else if (strcmp(cmd, "MIDI FORWARD OFF") == 0) {
		s->forward = 0;
		out_status("Forwarding disabled");
		save_log(s);
	} else if (strcmp(cmd, "MIDI DEVICES") == 0) {
		refresh_devices(s);
	} else if (strcmp(cmd, "MIDI TEST") == 0) {
		test_midi_out(s);
	} else if (strncmp(cmd, "MIDI", 4) == 0) {
		out_status("Unknown MIDI command: %s", cmd);
	}
	/* Lines not starting with MIDI are silently ignored */
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int main(void)
{
	State s;
	memset(&s, 0, sizeof(s));
	s.in_idx = -1;
	s.out_idx = -1;
	s.running = 1;

	setbuf(stdout, NULL);

	/* Self-pipe: midi_callback (background thread) writes here to wake
	   the main poll() without busy-waiting */
	int pipefd[2];
	if (pipe(pipefd) != 0) {
		perror("pipe");
		return 1;
	}
	s.pipe_r = pipefd[0];
	s.pipe_w = pipefd[1];

	refresh_devices(&s);
	load_log(&s);
	restore_from_log(&s);

	struct pollfd fds[2];
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	fds[1].fd = s.pipe_r;
	fds[1].events = POLLIN;

	char line[CMD_BUF_SZ];

	while (s.running) {
		int ret =
		    poll(fds, 2, -1); /* block until stdin or MIDI arrives */
		if (ret < 0)
			break;

		if (fds[0].revents & POLLIN) {
			if (!fgets(line, sizeof(line), stdin)) {
				s.running = 0; /* EOF */
				break;
			}
			handle_command(&s, line);
		}

		/* Drain the wake bytes written by midi_callback; the actual
		   MIDI processing already happened inside the callback itself.
		 */
		if (fds[1].revents & POLLIN) {
			char discard[64];
			(void)read(s.pipe_r, discard, sizeof(discard));
		}
	}

	close_midi_in(&s);
	close_midi_out(&s);
	close(s.pipe_r);
	close(s.pipe_w);
	return 0;
}
