# Continuo Trainer

An interactive basso continuo trainer that analyzes your performance on a MIDI
keyboard against a displayed bassline, giving feedback on harmony, rhythm, and
style while adapting exercises through spaced repetition and gamified learning.

![Screenshot](screenshot.png)

### Getting Started

Prerequisites: SDL2.

To build, just clone project and call make, and then run the program:

    $ git clone git@github.com:js216/continuo_trainer.git
    $ cd continuo_trainer/imgui_continuo
    $ make -j4
    $ ./continuo_trainer

For static code analysis, before committing new changes, we also need bear,
clang-tidy and clang-format (from llvmorg-21.1.6), cppcheck, graphviz,
include-what-you-use. Or run the checks one by one as desired:

    $ make check # runs all of them; or:
    $ make format
    $ make cppcheck
    $ make tidy
    $ make inclusions
    $ make iwyu

### The Story

I wondered how hard it would be to create a little AI system for practicing
continuo. The program would show a bassline and you have to realize it, and then
it would take the MIDI data to analyze if the realization was correct,
appropriate, and done with a good sense of rhythm. If not, it would present you
with more of similar examples, until the concept is learned. It could have some
spaced repetition scheme to ensure permanence. It could also play the melody
line to practice playing with other instruments. It would incorporate all the
latest findings about gamification, retention, hacking the brain rewards
pathways to make the process either fun or at least addictive enough to ensure
seamless learning. I imagine a proof of concept could be hacked together as a
simple Python script running a loop: show a note on a staff with a bass clef,
wait for MIDI input, check if it's a correct chord given the note (and maybe
some continuo figures), and then show the next note. This could be done in
levels, or stages, going through chapters similar to the Handel continuo
exercises. The chapters could be mixed up a little bit, adding the old material
into the mix with the new one, keeping scores of which figures, or which levels,
are the most difficult, just like Anki keeps scores for each flashcard. Then,
when one learns the basic figurations, the one note at a time approach would be
modified in showing a couple notes, or a couple bars, at a time, assuming the
user (or should we say player!) is able to realize individual chords fast
enough. Then, with the several notes or bars displayed, the program could also
teach some basic voice leading and judge the fluency, style, grace, or even
appropriateness to a particular national or temporal style. Finally, if the
software is proven to work and be fun enough to "play", the thing could be
packaged into a simple all-in-one plastic toy, with maybe one or two octaves of
a musical keyboard (doesn't need to be full scale), a simple display of the
bassline (could be just fixed LEDs arranged on a staff printed on Lexan), a
small speaker, and some input/output connections (maybe MIDI in/out, sound
in/out, and/or USB for either). Then, the whole thing would be placed on the
market only to realize there is no market for continuo-training toys ...

### The App

This is a highly engaging web-based musical game designed to teach how to
realize basso continuo from traditional notation. It transforms real-time
musical skill execution into a fast, rewarding loop to redirect the attention
that would otherwise go to passive media consumption towards mastering a musical
skill.

To make skill-building as compelling as social media, we use dynamic
audio-visual feedback, surprise, anticipation, streaks, progression cues,
variable rewards, social comparison, and fantasy identity framing. Over time,
challenges escalate from single chords to extended continuo lines, simple fugal
improvisation, and ultimately full orchestral accompaniment.

In short: it's a compulsively playable musical "skill machine" that turns the
addictive mechanics of modern media into rapid, focused, and creative music
practice.

### Todo

- streak comes from score goal
- time is just a number, not a progress bar
- automatically select the next lesson (via next lesson button)
- generate lots of intro lessons in Python, nd targeted ones
- add voice leading to scoring
- add time pressure via "Challenge" mode, where notes fly in from the right and
  you have to hit to chord in the central green region (or yellow around it, &c)
- add melody instruments that play in "Karaoke" mode
- use Emscripten to make a web app
- Adaptive difficulty scaling for skill retention and challenge

#### Gamification

- more rewards and complexity
    - Unlock slightly more complex figures for jackpots
    - Introduce rare collectible items (ornaments, timbres, historical figures)
    - Unlock new rewards based on streaks and performance
    - Full progression system visible: points, streaks, ranks, collectibles
    - Audio/visual reward variety expansion
    - More complex audio/visual flourishes for jackpot hits
    - Phase-based progression to keep “one more challenge” loop compelling
- social and competitive features
    - Add leaderboard placeholders (local for MVP)
    - Implement leaderboards (global/friends)
    - Show streaks, ranks, and relative performance notifications
    - Optional social comparison features (avatars, status badges)
    - Limited-time weekly/daily challenges
    - Unlock rare musical content for top performers
- Identity & Engagement Optimization
    - Experiment with fantasy identity framing (Virtuoso, Scholar, Assassin)
    - Adjust audio/visual reward metaphors per identity
    - Collect engagement data for identity A/B testing
    - Tune reward probabilities, jackpot intensity, and pacing based on metrics
    - Implement hybrid rewards (music + social + collectibles)

### Author

Jakob Kastelic

## License

This project is licensed under the GNU General Public License v2.0 or later.
See the [LICENSE](LICENSE) file for details.
