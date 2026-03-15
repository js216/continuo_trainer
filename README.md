# Continuo Trainer

An interactive basso continuo trainer that analyzes your performance on a MIDI
keyboard against a displayed bassline, giving feedback on harmony, rhythm, and
style while adapting exercises through spaced repetition and gamified learning.

![Screenshot](screenshot.png)

### Usage

    cd continuo_trainer
    make
    bin/run src/play.dot

### Implementation

The app consists of a graph of tiny programs communicating mostly via their
standard input and output. (See `play.dot` for the graph.)

### Unification plan

**Goal:** every musical excerpt — full lesson or skill slice — is a chunk in
`chn/`, identified by SHA1 hash and tagged with a `level:` integer.  Level-0
chunks are full lessons; level-1 chunks are skill slices derived from them;
higher levels are reserved for future sub-slices.  The parent→child tree is
implicit: all.lua re-derives children on demand from the note data rather than
storing the mapping.

Each stage touches one or two files, leaves the app runnable, and can be
committed and tested independently.  `src/karaoke.rs` and `src/group.rs`
require no changes throughout: they consume the unchanged `LESSON` /
`BASSNOTE` / `FIGURES` / `MELODY` / `LESSON_END` protocol.

---

**Stage 1 — all.lua absorbs chunk.lua**

*Files:* `src/all.lua` (rewrite), `src/chunk.lua` (delete)

Move all logic from chunk.lua into all.lua: SHA1, write\_chunk, skill-slicing,
bar/partial computation.  In addition, for each seq file generate the
corresponding **level-0** chunk (seq body + `level: 0` + `bpm:`).  Change
`CHUNK_NAME` output to `CHUNK_NAME <hash> <level>`.  `RESCAN` repeats the
full scan as before.  all.lua does **not** cache the parent→children mapping;
it re-derives children from the note data each time `QUERY_CHILDREN` is
received (added in Stage 5).

*Verify:* `lua src/all.lua` exits cleanly; `chn/` contains both level-0 and
level-1 files each with a `level:` field; `CHUNK_NAME` lines carry a level
suffix.  Downstream programs ignore the extra token for now.

*Commit:* "all.lua absorbs chunk.lua; emit level-0 chunks"

---

**Stage 2 — all.lua handles LOAD\_CHUNK (absorbs load.rs for chunks)**

*Files:* `src/all.lua`, `src/play.dot`

Add `LOAD_CHUNK <hash>` command to all.lua: read `chn/<hash>.txt`, parse it
with the same logic as the old `bin/load`, and emit `LESSON` / `BASSNOTE` /
`FIGURES` / `MELODY` / `LESSON_END`.  In play.dot, add the all.lua →
group/karaoke/stats edges that carry this protocol.  Keep `bin/load` alive for
`LOAD_LESSON` (gui still uses it); the two paths coexist temporarily.

*Verify:* `echo "LOAD_CHUNK <any-hash>" | lua src/all.lua` produces the same
output as `echo "LOAD_CHUNK <same-hash>" | bin/load`.

*Commit:* "all.lua: handle LOAD\_CHUNK; add protocol edges to play.dot"

---

**Stage 3 — gui.cpp switches to LOAD\_CHUNK; bin/load deleted**

*Files:* `src/gui.cpp`, `src/play.dot`, `src/load.rs` (delete), `bin/load`
(delete)

GUI now holds a list of level-0 chunk hashes (from `CHUNK_NAME … 0` lines)
instead of lesson numbers.  Prev/Next send `LOAD_CHUNK <level0-hash>` to
all.lua.  Remove `LOAD_LESSON` handling from gui.cpp.  Delete `src/load.rs`
and `bin/load`; remove them from play.dot and the Makefile.

*Verify:* app starts, Prev/Next/Suggest load pieces, karaoke and group behave
as before.

*Commit:* "gui: LOAD\_CHUNK only; delete bin/load"

---

**Stage 4 — stats.lua: unified chunk table; remove CHUNK\_SESSION**

*Files:* `src/stats.lua`

Replace the separate `lessons` and `chunks` tables with a single
`stats.chunks` table keyed by hash.  Populate `level` from the new
`CHUNK_NAME <hash> <level>` lines.  Drop all `CHUNK_SESSION` handling (every
session is already a chunk session after Stage 3).

*Verify:* play a session; `src/report.lua` (or log file) shows scores recorded
against hash keys; no crashes on startup.

*Commit:* "stats: unified chunk table; remove CHUNK\_SESSION"

---

**Stage 5 — stats.lua: downward finalize via QUERY\_CHILDREN**

*Files:* `src/all.lua`, `src/stats.lua`, `src/play.dot`

Add `QUERY_CHILDREN <hash>` command to all.lua; response:
`CHILDREN <hash> <child>:<s>:<e> …` (one token per child).  After finalising
a score, stats sends `QUERY_CHILDREN` and recursively updates each child with
its sub-range score, then discards the children list — do **not** store it
between sessions.  The recursion handles arbitrary depth (level 2+), not just
two levels.  Add the bidirectional stats ↔ all.lua edge pair to play.dot.
Cyclic edges in the graph are safe: programs never echo their input, so
messages cannot circulate indefinitely.

*Verify:* play a level-0 chunk; confirm that each level-1 child accumulates a
score in the stats table without playing it directly.

*Commit:* "stats: downward finalize; all.lua: QUERY\_CHILDREN"

---

**Stage 6 — stats.lua absorbs sched.lua; level-aware scheduler**

*Files:* `src/stats.lua` (add scheduler), `src/sched.lua` (delete),
`src/play.dot`

Move next-chunk scheduling from sched.lua into stats.lua.  A level-N chunk is
eligible only when all its level-(N+1) children are mastered (or it has none).
This applies at every depth: walk before you run — drill the small slices
before the full lesson.  Remove sched.lua from play.dot.

*Verify:* [S]uggest returns level-1 chunks until they are mastered, then
promotes to level-0.

*Commit:* "stats absorbs sched.lua; level-aware scheduler"

---

**Stage 7 — report.lua: unified table**

*Files:* `src/report.lua`

Single table with columns: hash (short), Lvl, title, and performance metrics.
Rows sorted by level then mastery.

*Verify:* `lua src/report.lua` renders without error; both level-0 and level-1
rows appear.

*Commit:* "report: unified single table with Lvl column"

---

**Stage 8 — gui.cpp: suggest-only navigation**

*Files:* `src/gui.cpp`

Remove Prev/Next buttons; only the Suggest button remains for navigation.

*Verify:* UI compiles; suggest loads the recommended chunk.

*Commit:* "gui: remove prev/next; suggest-only navigation"

### Next steps

- skill order for new lessons/chunks
- add all lessons from Handel, Purcell, Couperin
- overlearning: require 5-10, depending on how many mistakes made recently
  (exponential moving average)
- mistakes hurt power
- slow exp decay of mastery on startup using `last_played` (or, better yet,
  `last_updated_mastery` to distinguish playing from mere algorithmic
  adjustment)
- each chunk should have independent and transitive properties: independent are
  adjusted only when chunk is practiced directly, and transitive ones are
  adjusted when a lower-lever chunk is played
- combine into a single rust program without losing modularity, testability
- make a web app
