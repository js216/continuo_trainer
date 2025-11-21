# Continuo

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

### Stage 1 — Core Loop MVP

- Since there's the description and score etc. text in the staff box, all the
  music (staves, clefs, notes, continuo figures, etc) need to move down a bit.
  Can we introduce a parameter like musicVertOffs (or a similar name) that
  controls the offset of the music from the top of the staff box (canvas)?
- The lesson description is shown top left above the music and this is great.
  But as the first 5 or so notes are played, the description should fade away
  such that by the time the 10th note is played, it has disappeared. Of course,
  reset or going to next lesson etc should make it visible again.
- When there's 8 notes in a measure (e.g., 8th notes in 4/4 time), and the first
  note in the lesson has an accidental, that accidental appears to be cut off.
  Is this because of the logic that prevents notes to be drawn on top of the
  clefs/signatures? Either way, make sure the first accidental can be visible,
  if necessary by moving all the notes a tiny bit to the right.
- When key signature is F major (one flat), pressing B-flat on the on-screen or
  midi keyboard should not draw an A-sharp! Check the keyboard-to-drawing logic
  to make sure that the note shown in the staves is consistent with the key
  signature (i.e., prefer the enharmonic note consistent with the key signature,
  so that the accidental is not necessary).
- Lesson ID should be auto-generated (maybe it could the SHA256 hash of lesson
  title, description, and the note sequence content if that's not too hard?) and
  not part of the LESSONS list.
- Make sure to not reset the total lesson time when the 5-second timeout
  expires.
- Remove hardcoded correctness checking and instead have the correct answer as a
  part of the lesson data. If a particular bassnote has an empty string "" as
  the correct answer, then that note does not require harmonic realization, so
  the app should just play the bass notes (if sound is enabled) until reaching
  the next note which does have a correct answer supplied in the LESSON data.
  Thus, each lesson needs a "tempo" property (in bps) so you know how quickly to
  play the notes when a lesson is loaded. Make sure to emphasize the note
  currently played (as we already do) using the highlighted background so that
  even with sound disabled the user can know which note is currently active or
  being played. If a note DOES have a correct answer coded in, then do not
  proceed until the user has pressed and released their chord. But do not
  proceed faster than the speed given in the "tempo" property. Also display the
  tempo at the start of the sheet music, as is standard in modern musical
  notation.
- When calculating the score, use a formula with weights giving parametrized
  importance to the following criteria: how many of the required correct notes
  have been played (the more the better), how many incorrect notes have been
  played (the fewer the better, and the score can actually go down or even
  negative if there's lots of incorrect notes), and how accurate the timing is
  (the duration between when the beat happens and when the user presses the
  FIRST note of the chord, allowing for arpeggiated chords; if the first-note or
  chord is entered very slightly before the beat, make sure to count it in the
  following beat and not the previous one). Make sure the tempo is transmitted
  to the backend (because future features will allow for changing the tempo
  dynamically), so that the backend can also calculate the score and arrive at
  the same answer if needed (but don't implement score calculation in the
  backend yet).
- When reaching the end of the lesson (last chord played), send the data to
  backend as before, and only if that worked successfully and the backend
  received the data correctly, overlay three large-ish square buttons on top of
  the sheet music: (1) Targeted practice (does nothing for now, will implement
  later, so mark the button as "coming soon!" in the GUI), (2) Repeat
  faster/slower (depending on if the score was good or bad, and when pressed,
  the lesson resets with the new tempo); (3) Go to next lesson (when clicked,
  should be the same effect as navigating to the next lesson using the pulldown
  menu).

### Stage 2 — Variable Rewards & Early Feedback

- Targeted practice
- Dark mode / light mode
- Very low bass notes are inaudible, use a different timbre for them
- Check for contrary motion, and specially flag parallel 5ths
- Implement variable-ratio reward system (small/none/medium/jackpot)
- Audio flourishes of varying lengths for rewards
- Visual cues for correct input and reward types
- Display per-lessons stats obtained from back-end
- Streak multiplier basics

### Stage 3 — Short Sequences & Challenge Escalation

- Generate 2–4 chord sequences
- Introduce timing pressure per chord
- Unlock slightly more complex figures for jackpots
- Implement progression feedback (XP, points, streaks)
- Track fastest completion times per sequence

### Stage 4 — Progression & Collectibles

- Introduce rare collectible items (ornaments, timbres, historical figures)
- Unlock new rewards based on streaks and performance
- Add leaderboard placeholders (local for MVP)
- Refine variable-ratio jackpot probabilities
- Audio/visual reward variety expansion
- Make a favicon, choose domain

### Stage 5 — Longer Bass Lines & Flow Loops

- Generate extended continuo lines (longer sequences)
- Introduce branching voicings / ornament choices
- Advanced scoring system factoring accuracy, speed, and stylistic rules
- Tighter feedback loops (shorter latency between input and reward)
- More complex audio/visual flourishes for jackpot hits

### Stage 6 — Orchestral Layering & Multi-Instrument Feedback

- Add pre-recorded/synthesized orchestral accompaniment
- Real-time audio feedback based on user's realized continuo
- Layered visual feedback (score animation, instrument cues)
- Dynamic scaling of reward magnitude with ensemble complexity
- Phase-based progression to keep “one more challenge” loop compelling

### Stage 7 — Social & Competitive Features

- Implement leaderboards (global/friends)
- Show streaks, ranks, and relative performance notifications
- Optional social comparison features (avatars, status badges)
- Limited-time weekly/daily challenges
- Unlock rare musical content for top performers

### Stage 8 — Identity & Engagement Optimization

- Experiment with fantasy identity framing (Virtuoso, Scholar, Assassin)
- Adjust audio/visual reward metaphors per identity
- Collect engagement data for identity A/B testing
- Tune reward probabilities, jackpot intensity, and pacing based on metrics
- Implement hybrid rewards (music + social + collectibles)

### Stage 9 — Full Orchestral Realization

- Multi-part orchestral textures fully driven by user input
- High-level challenges: modulations, tempo changes, complex ornaments
- Enhanced jackpot/flourish feedback for maximum engagement
- Full progression system visible: points, streaks, ranks, collectibles
- Adaptive difficulty scaling for skill retention and challenge
