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

### Claude TODO

Select the first item from the following paragraphs and present a thorough
implementation plan, then confirm with me before implementing. When I give the
ho ahead to implement, remove the paragaph from this file and prepare a commit
message for me to accept or reject. You are allowed to "read ahead" to prepare a
plan that will make sense in light of the future edits, but each edit show be
entirely self-contained and working and usable.

---

Don't rescan at every startup. Currently, startup is very slow because we rescan
all chunks. Instead, chunking will be done a single time through the command
line `lua src/all.lua | lua src/stats.lua statsfile.log` or similar, at which
time we also run the Lilypond makefile recipe to make the scheet music PNGs, and
verify there are no stale chunks in the stats log (print all stale entries in
such case). To keep the startup fast, stats only needs to read the stats file
and select the most appropriate thing to practice first thing.

The chunk shown on boot should be the same as chunk shown when [S]uggest button
pressed. Currently, pressing [S] shows a chunk which is different from the chunk
shown after quitting and restarting the app---that's not good. The scheduling
algorithm should always precesily determine which is the "next" chunk; the
algorithm is deterministic and so should be the app.

Schedule full leson first and then chunks only if needed. This represents a
partial inversion of the current scheduling algorithm which goes from highest
level to lowest. Instead, display the lowest level (probably level 0) chunk
which needs to be practiced (meaning it's mastery/power is low). Always show the
chunks with easiest skills first, so that the app takes the player through a
progression of skill levels. If there's a chunk that's not mastered yet, prefer
it over brand new (never practiced) chunk. After the user plays a level-n chunk,
stats.lua can tell which level-(n+1) subchunks are easy and which are hard
(based on mistakes made and relative speed): the scheduling algorithm should
advance the SRS properties for the easy ones, so that they won't be shown soon,
and prioritize the hard ones, so that next time [S] is pressed, these hard ones
will be shown. In other words, let the user play big lessons which are musically
more meaningful, but if they make mistakes, let them drill the parts where the
mistakes happen.

Change chunking algorithm: no more skill-based "heart" chunking, instead, do
3-level chunking: level-0 is just the base seq/ lesson, level-1 chunks are less
than or equal to 3.5 bars long, level-2 chunks are at least 1.5 bars long. No
chunks are shorter than 1.5 bars. All chunks begin on the first note of the bar,
so no partial measures at the start of the chunk. The 0.5 bars are used to add a
bit over overlap between chunks: for example, if the level-0 chunk is 6 bars, it
should be split into two level-1 chunks (inclusive, bars 1 through 3 and half of
bar 4, and the other other chunk is bars 4 through 6), and five level-2 chunks
(inclusive: bar 1 + first half of bar 2; b2 + b3 half; and so on till the last
chunk which is bars 5 and 6 to satisfy the ">=1.5" bars rule.)

Make nicer visual displays of Mastery, Power, and progress towards the daily
goal. Mastery/Power of the current lesson could be two tiny progress bars, while
daily points could be a progress bar which turns into streak display when the
daily points goal has been achieved. Status bar text, for the duration that it
is present, should be shown as the bottom-most line of the GUI, so that it does
not cover the lesson/daily stats. To be clear: buttons are top left, stats are
to right (same line as buttons); below the buttons/stats, there's the sheet
music display; below that, the squares that show progress through lesson; below
that, if present any comments from rules; then blank space, and the last line of
the GUI, the info messages ("Try something else", "Good job", ...).

### Later potential features

"Journey" GUI: skill progression unlocking.

Translate into Rust

Convert into reactor pattern (read about it)
