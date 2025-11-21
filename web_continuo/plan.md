# Continuo

A web-based musical training game that turns mastering continuo into a fast,
rewarding, endlessly engaging loop, using surprise, streaks, and variable
rewards to make skill-building as compelling as social media.

"Master the art of continuo realization through fast, interactive challenges
that sharpen your ear, technique, and musical intuition."

### Implementation Details

This is a highly engaging web-based musical training system designed to teach
users how to realize basso continuo from traditional notation. The platform
transforms real-time musical skill execution into a fast, rewarding loop, using
techniques inspired by the most engaging elements of social media and
incremental games.

Users interact by receiving bass notes with figures, realizing the corresponding
chords, and immediately receiving dynamic audio-visual feedback, variable
rewards, and progression cues. Over time, challenges escalate from single chords
to extended continuo lines and ultimately full orchestral accompaniment.

The system leverages anticipation, surprise, variable rewards, streaks, social
comparison, and fantasy identity framing to maintain deep engagement,
redirecting attention that would otherwise go to passive media consumption
toward active skill development and mastery. Every action that earns a reward is
grounded in real musical accomplishment, ensuring engagement and learning go
hand in hand.

In short: it’s a compulsively playable musical “skill machine” that turns the
addictive mechanics of modern media into rapid, focused, and creative music
practice.

### Sprint 1 — Core Loop MVP

- Generate single bass note + figure
- Accept user input via on-screen keyboard / MIDI
- Immediate correctness checking
- Trigger small audio/visual feedback on correct input
- Repeat loop endlessly
- Basic session logging (attempts, correct/incorrect)

### Sprint 2 — Variable Rewards & Early Feedback

- Implement variable-ratio reward system (small/none/medium/jackpot)
- Audio flourishes of varying lengths for rewards
- Visual cues for correct input and reward types
- Streak multiplier basics
- Basic point system / score tracking

### Sprint 3 — Short Sequences & Challenge Escalation

- Generate 2–4 chord sequences
- Introduce timing pressure per chord
- Unlock slightly more complex figures for jackpots
- Implement progression feedback (XP, points, streaks)
- Track fastest completion times per sequence

### Sprint 4 — Progression & Collectibles

- Introduce rare collectible items (ornaments, timbres, historical figures)
- Unlock new rewards based on streaks and performance
- Add leaderboard placeholders (local for MVP)
- Refine variable-ratio jackpot probabilities
- Audio/visual reward variety expansion

### Sprint 5 — Longer Bass Lines & Flow Loops

- Generate extended continuo lines (longer sequences)
- Introduce branching voicings / ornament choices
- Advanced scoring system factoring accuracy, speed, and stylistic rules
- Tighter feedback loops (shorter latency between input and reward)
- More complex audio/visual flourishes for jackpot hits

### Sprint 6 — Orchestral Layering & Multi-Instrument Feedback

- Add pre-recorded/synthesized orchestral accompaniment
- Real-time audio feedback based on user’s realized continuo
- Layered visual feedback (score animation, instrument cues)
- Dynamic scaling of reward magnitude with ensemble complexity
- Phase-based progression to keep “one more challenge” loop compelling

### Sprint 7 — Social & Competitive Features

- Implement leaderboards (global/friends)
- Show streaks, ranks, and relative performance notifications
- Optional social comparison features (avatars, status badges)
- Limited-time weekly/daily challenges
- Unlock rare musical content for top performers

### Sprint 8 — Identity & Engagement Optimization

- Experiment with fantasy identity framing (Virtuoso, Scholar, Assassin)
- Adjust audio/visual reward metaphors per identity
- Collect engagement data for identity A/B testing
- Tune reward probabilities, jackpot intensity, and pacing based on metrics
- Implement hybrid rewards (music + social + collectibles)

### Sprint 9 — Full Orchestral Realization

- Multi-part orchestral textures fully driven by user input
- High-level challenges: modulations, tempo changes, complex ornaments
- Enhanced jackpot/flourish feedback for maximum engagement
- Full progression system visible: points, streaks, ranks, collectibles
- Adaptive difficulty scaling for skill retention and challenge

### TODO

- Make sure notes are not displayed in the same place as the clef/key/time
  signature. Right now, when the score moves left as the user plays notes,
  the old played notes are drawn on top of the clefs/signatures.
- Allow dragging the score left/right if there are more notes than fit in the
  display. However, as soon as a note is played, immediately snap back to the
  default position (the position as currently implemented).
- In one browser, the music is displayed correctly. But in another, the clefs
  are on the wrong lines, and the note stems are detached from note heads.
  This suggests that different fonts can be used for the notehead/clef
  symbols which affect the spacing. Can I automatically detect the size of
  each symbol, regardless of the font used, and correct spacing accordingly?
  If not, use manual drawing primitives (you may have do define small helper
  functions to keep the code clean) for all the music rendering: clefs and
  notes.
- Right now quarter and eighth notes appear the same, but eighth notes should
  get a flag.
- Add support for dotted notes. For example, duration=1.5 means a dotted
  whole note.
- Add support for anacrusis.
- In 4/4 time, group 4 eighth notes together with a shared flag
- Left align the title ("Continuo Trainer") and the lesson selector pulldown
  menu. The settings bar (MIDI, Sound) should be positioned at the very upper
  right side of the website (right aligned perfectly with the right edge of
  the staff box and keyboard, and in the same line as the title), as is
  common on simple websites. Later we'll add the user profile and maybe a
  settings button (gear icon) in the same place. So: first row is title on
  the left and settings on the right, second row is the lesson selector and
  reset button, then there's the music staff box, and below the keyboard.
- The lesson description should be displayed in the upper left corner of the
  music staff box, while the score/duration should be in the upper right
  corner, also inside the staff box.
- Let's implement a very simple form of data storage: for each lesson,
  remember the top score and shortest duration till completion. When
  switching between lessons, load the appropriate top score for each of them.
  For now, keep all this in memory.
- We want to communicate all the notes pressed in each lesson for each user,
  and the time at which the notes were pressed (offset from the first note
  pressed in the lesson), for analysis and generating new user-specific
  lessons. Suggest some ways of implementing that, both the communication of
  the notes pressed and their backend data storage, and prioritize
  simplest-to-implement things with fewest "frameworks", ideally just
  standard plain vanilla JS and simple SQLite and Python etc.
- Implement the simple backend that we will choose and it should be very
  simple at the start: just remember each user and their top scores for each
  lesson. How to login and authenticate with a minimum of fuss? None of this
  is particularly security critical, it's just a simple music game. It should
  be possible to play without any login, but to store/remember data, and
  generate new custom lessons, the login would be needed.

