# Continuo Trainer

An interactive basso continuo trainer that analyzes your performance on a MIDI
keyboard against a displayed bassline, giving feedback on harmony, rhythm, and
style while adapting exercises through spaced repetition and gamified learning.

![Screenshot](screenshot.png)

### Usage

First, make sure all chunks are up to date:

    cd continuo_trainer
    make index all

Then, open the program and practice:

    bin/run src/play.dot

### Implementation

The app consists of a graph of tiny programs communicating mostly via their
standard input and output. (See `play.dot` for the graph.)

### Claude TODO

Select the first item from the following paragraphs and present a thorough
implementation plan, then confirm with me before implementing. When I give the
go ahead to implement, remove the paragaph from this file and prepare a commit
message for me to accept or reject. You are allowed to "read ahead" to prepare a
plan that will make sense in light of the future edits, but each edit show be
entirely self-contained and working and usable.

---

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

Unravel the play.dot graph so that the data flow is linear pipes as much as
possible rather than branching or loops.

Translate into Rust

Convert into reactor pattern (read about it)
