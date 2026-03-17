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

Make nicer visual displays of Mastery, Power, and progress towards the daily
goal. Mastery/Power of the current lesson could be two tiny progress bars, while
daily points could be a progress bar which turns into streak display when the
daily points goal has been achieved. If stats are three progress bars and chunk
name to the left, that's be cleaner than the current ever-changing text-based
display. Next: Status bar text, for the duration that it is present, should be
shown as the bottom-most line of the GUI, so that it does not cover the
lesson/daily stats. To be clear: buttons are top left, stats are to right (same
line as buttons); below the buttons/stats, there's the sheet music display;
below that, the squares that show progress through lesson; below that, if
present any comments from rules; then blank space, and the last line of the GUI,
the info messages ("Try something else", "Good job", ...). Also, tiny change for
gui.cpp keyboard handling: spacebar does the same as `s` which is the same as
the [S]uggest button (i.e., just add the spacebar handler and that's it).

### Later potential features

"Journey" GUI: skill progression unlocking.

Unravel the play.dot graph so that the data flow is linear pipes as much as
possible rather than branching or loops.

Translate into Rust

Convert into reactor pattern (read about it)
