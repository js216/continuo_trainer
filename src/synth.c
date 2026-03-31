// SPDX-License-Identifier: MIT
// synth.c --- polyphonic ADSR software synthesizer with stdin control
// Copyright (c) 2026 Jakob Kastelic

/*
 * DESCRIPTION
 * Reads LilyPond pitch events and configuration commands from stdin.
 * Implements a dual-oscillator subtractive synth with per-voice
 * ADSR envelopes and low-pass filtering.
 *
 * INPUT (stdin)
 *   NOTE_ON <pitch>     Trigger a note.
 *   NOTE_OFF <pitch>    Release a note (triggers ADSR release).
 *   SET <param> <val>   Adjust synth parameters.
 *
 * COMMANDS (stdin)
 *   SET ATTACK <val>       Envelope attack time (seconds)
 *   SET DECAY <val>        Envelope decay time (seconds)
 *   SET SUSTAIN <val>      Sustain level (0.0 to 1.0)
 *   SET RELEASE <val>      Envelope release time (seconds)
 *   SET OSC1_GAIN <val>    Oscillator 1 volume
 *   SET OSC2_GAIN <val>    Oscillator 2 volume
 *   SET OSC1_OCT <val>     Oscillator 1 octave offset (integer)
 *   SET OSC2_OCT <val>     Oscillator 2 octave offset (integer)
 *   SET OSC1_DETUNE <val>  Oscillator 1 fine tune (1.0 is neutral)
 *   SET OSC2_DETUNE <val>  Oscillator 2 fine tune (1.0 is neutral)
 *   SET OSC1_CUTOFF <val>  Oscillator 1 LPF alpha (0.0 to 1.0)
 *   SET OSC2_CUTOFF <val>  Oscillator 2 LPF alpha (0.0 to 1.0)
 *   SET MASTER_GAIN <val>  Global output volume
 */

#define MINIAUDIO_IMPLEMENTATION
#include <math.h>
#include <miniaudio.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_RATE 48000
#define BUFFER_SIZE 64
#define MAX_POLYPHONY 16
#define HEADROOM 4.0f

typedef enum {
	ENV_IDLE,
	ENV_ATTACK,
	ENV_DECAY,
	ENV_SUSTAIN,
	ENV_RELEASE
} EnvState;

typedef struct {
	float osc1_gain, osc2_gain;
	float osc1_cutoff, osc2_cutoff;
	float osc1_detune, osc2_detune;
	int osc1_oct, osc2_oct;
	float attack, decay, sustain, release;
	float master_gain;
} Params;

typedef struct {
	float freq;
	float target_f1, target_f2;
	double phase1, phase2;
	float lpf1, lpf2;
	EnvState state;
	float env_vol;
	bool active;
} Voice;

typedef struct {
	Voice voices[MAX_POLYPHONY];
	Params p;
} Synth;

/* Helper to update voice target frequencies when params change */
void update_voice_frequencies(Synth *synth)
{
	for (int i = 0; i < MAX_POLYPHONY; i++) {
		if (synth->voices[i].active) {
			float f = synth->voices[i].freq;
			synth->voices[i].target_f1 = f *
			    powf(2.0f, synth->p.osc1_oct) *
			    synth->p.osc1_detune;
			synth->voices[i].target_f2 = f *
			    powf(2.0f, synth->p.osc2_oct) *
			    synth->p.osc2_detune;
		}
	}
}

float update_env(Voice *v, Params *p)
{
	float a_step = 1.0f / (p->attack * SAMPLE_RATE);
	float d_step = (1.0f - p->sustain) / (p->decay * SAMPLE_RATE);
	float r_step = p->sustain / (p->release * SAMPLE_RATE);

	switch (v->state) {
	case ENV_ATTACK:
		v->env_vol += a_step;
		if (v->env_vol >= 1.0f) {
			v->env_vol = 1.0f;
			v->state = ENV_DECAY;
		}
		break;
	case ENV_DECAY:
		v->env_vol -= d_step;
		if (v->env_vol <= p->sustain) {
			v->env_vol = p->sustain;
			v->state = ENV_SUSTAIN;
		}
		break;
	case ENV_RELEASE:
		v->env_vol -= r_step;
		if (v->env_vol <= 0.0f) {
			v->env_vol = 0.0f;
			v->state = ENV_IDLE;
			v->active = false;
		}
		break;
	default:
		break;
	}
	return v->env_vol;
}

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput,
		   ma_uint32 frameCount)
{
	(void)pInput;
	Synth *synth = (Synth *)pDevice->pUserData;
	float *out = (float *)pOutput;

	if (synth->p.master_gain == 0.0f) {
		memset(pOutput, 0, frameCount * 2 * sizeof(float));
		return;
	}

	for (ma_uint32 i = 0; i < frameCount; i++) {
		float mixed = 0;
		for (int v = 0; v < MAX_POLYPHONY; v++) {
			if (synth->voices[v].active) {
				float env =
				    update_env(&synth->voices[v], &synth->p);

				synth->voices[v].phase1 +=
				    synth->voices[v].target_f1 / SAMPLE_RATE;
				if (synth->voices[v].phase1 > 1.0)
					synth->voices[v].phase1 -= 1.0;
				float s1 = (synth->voices[v].phase1 < 0.5)
				    ? synth->p.osc1_gain
				    : -synth->p.osc1_gain;
				synth->voices[v].lpf1 += synth->p.osc1_cutoff *
				    (s1 - synth->voices[v].lpf1);

				synth->voices[v].phase2 +=
				    synth->voices[v].target_f2 / SAMPLE_RATE;
				if (synth->voices[v].phase2 > 1.0)
					synth->voices[v].phase2 -= 1.0;
				float s2 = (synth->voices[v].phase2 < 0.5)
				    ? synth->p.osc2_gain
				    : -synth->p.osc2_gain;
				synth->voices[v].lpf2 += synth->p.osc2_cutoff *
				    (s2 - synth->voices[v].lpf2);

				mixed += (synth->voices[v].lpf1 +
					  synth->voices[v].lpf2) *
				    env;
			}
		}
		float sample = (mixed / HEADROOM) * synth->p.master_gain;
		out[i * 2] = sample;
		out[i * 2 + 1] = sample;
	}
}

float lily_to_freq(const char *s)
{
	static const struct {
		const char *n;
		int semi;
	} names[] = {{"cis", 1},  {"dis", 3}, {"fis", 6}, {"gis", 8},
		     {"ais", 10}, {"c", 0},   {"d", 2},	  {"e", 4},
		     {"f", 5},	  {"g", 7},   {"a", 9},	  {"b", 11}};
	int semi = -1;
	for (size_t i = 0; i < 12; i++) {
		size_t len = strlen(names[i].n);
		if (strncmp(s, names[i].n, len) == 0) {
			semi = names[i].semi;
			s += len;
			break;
		}
	}
	if (semi < 0)
		return 0.0f;
	int oct = 3;
	while (*s == '\'') {
		oct++;
		s++;
	}
	while (*s == ',') {
		oct--;
		s++;
	}
	return 440.0f * powf(2.0f, ((oct + 1) * 12 + semi - 69.0f) / 12.0f);
}

int main(void)
{
	Synth synth = {.p = {.osc1_gain = 0.8f,
			     .osc2_gain = 0.5f,
			     .osc1_cutoff = 0.15f,
			     .osc2_cutoff = 0.08f,
			     .osc1_detune = 1.0f,
			     .osc2_detune = 1.004f,
			     .osc1_oct = 0,
			     .osc2_oct = -1,
			     .attack = 0.01f,
			     .decay = 0.1f,
			     .sustain = 0.7f,
			     .release = 0.05f,
			     .master_gain = 0.3f}};

	ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
	cfg.playback.format = ma_format_f32;
	cfg.playback.channels = 2;
	cfg.sampleRate = SAMPLE_RATE;
	cfg.periodSizeInFrames = BUFFER_SIZE;
	cfg.dataCallback = data_callback;
	cfg.pUserData = &synth;

	ma_device dev;
	if (ma_device_init(NULL, &cfg, &dev) != MA_SUCCESS)
		return 1;
	ma_device_start(&dev);

	char line[256];
	while (fgets(line, sizeof(line), stdin)) {
		char cmd[32], arg1[32], arg2[32];
		int count = sscanf(line, "%31s %31s %31s", cmd, arg1, arg2);
		if (count < 2)
			continue;

		if (strcmp(cmd, "NOTE_ON") == 0) {
			float f = lily_to_freq(arg1);
			for (int i = 0; i < MAX_POLYPHONY; i++) {
				if (synth.voices[i].state == ENV_IDLE) {
					synth.voices[i].freq = f;
					synth.voices[i].target_f1 = f *
					    powf(2.0f, synth.p.osc1_oct) *
					    synth.p.osc1_detune;
					synth.voices[i].target_f2 = f *
					    powf(2.0f, synth.p.osc2_oct) *
					    synth.p.osc2_detune;
					synth.voices[i].state = ENV_ATTACK;
					synth.voices[i].active = true;
					synth.voices[i].env_vol = 0.0f;
					break;
				}
			}
		} else if (strcmp(cmd, "NOTE_OFF") == 0) {
			float f = lily_to_freq(arg1);
			for (int i = 0; i < MAX_POLYPHONY; i++) {
				if (synth.voices[i].active &&
				    fabs(synth.voices[i].freq - f) < 0.1)
					synth.voices[i].state = ENV_RELEASE;
			}
		} else if (strcmp(cmd, "SET") == 0 && count == 3) {
			float val = atof(arg2);
			bool freq_changed = false;

			if (strcmp(arg1, "ATTACK") == 0)
				synth.p.attack = val;
			else if (strcmp(arg1, "DECAY") == 0)
				synth.p.decay = val;
			else if (strcmp(arg1, "SUSTAIN") == 0)
				synth.p.sustain = val;
			else if (strcmp(arg1, "RELEASE") == 0)
				synth.p.release = val;
			else if (strcmp(arg1, "OSC1_GAIN") == 0)
				synth.p.osc1_gain = val;
			else if (strcmp(arg1, "OSC2_GAIN") == 0)
				synth.p.osc2_gain = val;
			else if (strcmp(arg1, "OSC1_OCT") == 0) {
				synth.p.osc1_oct = (int)val;
				freq_changed = true;
			} else if (strcmp(arg1, "OSC2_OCT") == 0) {
				synth.p.osc2_oct = (int)val;
				freq_changed = true;
			} else if (strcmp(arg1, "OSC1_DETUNE") == 0) {
				synth.p.osc1_detune = val;
				freq_changed = true;
			} else if (strcmp(arg1, "OSC2_DETUNE") == 0) {
				synth.p.osc2_detune = val;
				freq_changed = true;
			} else if (strcmp(arg1, "OSC1_CUTOFF") == 0)
				synth.p.osc1_cutoff = val;
			else if (strcmp(arg1, "OSC2_CUTOFF") == 0)
				synth.p.osc2_cutoff = val;
			else if (strcmp(arg1, "MASTER_GAIN") == 0 ||
				 strcmp(arg1, "VOLUME") == 0)
				synth.p.master_gain = val;

			if (freq_changed)
				update_voice_frequencies(&synth);
		}
	}
	ma_device_uninit(&dev);
	return 0;
}
