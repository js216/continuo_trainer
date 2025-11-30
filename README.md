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

### calc update

We need to make the stats calculation MUCH cleaner. Right now, most public
functions take vector<attempt_record> arguments, so each time we want stats, we
need to recompute using all the past performance data, which is both ugly and
inefficient. Moreover, I want to implement Anki-style spaced repetition, which
would be difficult in the current system.

To this end, remove lesson_streak, lesson_speed, difficulty from struct stats.
Add speed and streak to struct lesson_meta. Remove difficulty and
difficulty_init from lesson_meta. To support Anki-style spaced repetition,
lesson_meta needs to have ease, interval (seconds), and due_on (unix timestamp)
added.

I propose the following new "streaming" API:

calc_speed(struct stats &stats, struct attempt_record &r); // updates the new
stats.lesson_cache[r.lesson_id].speed field using the moving average algorithm
as already implemented, but be very careful: "speed" is defined in terms of
*average maximum dt*, where dt is time difference between two successive
attempt_record.time, maximum is maximum over a single lesson-attempt (which
starts with r.col_id==0 and ends before the next ==0), and average is an
exponential moving average (EMA)

calc_lesson_streak(struct stats &stats, struct attempt_record &r); // updates
the new stats.lesson_cache[r.lesson_id].streak field (if r.bad_count!=0, resets
streak to 0, otherwise if
r.col_id==stats.lesson_cache[r.lesson_id].total_columns-1 it increments the
streak)

calc_duration(struct stats &stats, struct attempt_record &r); // updates
stats.duration_today, and for that must probably keep a vector or queue or
something of all timestamps today, and when the new day comes, kick out all the
old timestamps (or some more efficient/clean scheme, you decide)

calc_score(struct stats &stats, struct attempt_record &r); // updates
stats.score_today (renamed from stats.score) with an algorithm identical or
similar to the current score_lesson_attempt()

calc_practice_streak(struct stats &stats, struct attempt_record &r, double
score_goal); // updates stats.practice_streak, which is the number of days from
today backwards on which the player got at least score_goal points (calculated
via the same formula as calc_score uses)

calc_next(...?...) needs to be very much reworked to select the next lesson with
Anki-style scheduling. In fact it becomes very simple: return the lesson for
which lesson_meta.due_on is the lowest number. To support that, we need another
function:

calc_schedule(struct stats &stats, struct attempt_record &r) updates the ease,
interval, and due_on, depending on the score received in the most recent
lesson-attempt. Note again that the lesson-attempt is over only when
r.col_id==stats.lesson_cache[r.lesson_id].total_columns-1, otherwise this
function does nothing, right?

state should be stored in the appropriate place in struct stats and struct
lesson_meta.

If the user does not complete a lesson, they do not get any positive score from
it, but they do get penalized for any mistakes made in the partial attempt. For
scheduling purposes, such a lesson would have a lower ease, for instance. Make
use of the state stored in struct stats and struct lesson_meta to detect when a
lesson is abandoned in this way.

If anything does not make perfect sense, ask clarifying questions before writing
code!

### Todo

- next lesson should be chosen with spaced repetition in mind
- generate lots of intro lessons in Python, and also targeted ones
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
