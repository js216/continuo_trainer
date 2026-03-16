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

Stage 10: Make a plan and check in with me before implementing: scheduling
should be done in skill order for new chunks. Add another default algorithm
parameter, which is also written to stats.log, and defines the order in which
chunks are presented.  The order is space separated string, like "root 4-3_sus,
6/4_chords".  Skill-based ordering takes precedence over level-based oreding.
This means that first schedule the highest-level chunks that have just root
skill, then the slightly lower level that have just root skill, and so on till
you schedule the level-0 chunks with just root skill. Next, schedule highest
level chunks that have both root and 4-3_sus skill, and so on till level-0
chunks with these two skills. All skills in all chunks must be mentioned in the
space-separated ordering string, otherwise append the missing skills to the end
of the string in stats.log and print a warning message to stderr so that I can
manually integrate the new skills in the next version of the program. Also
explain which programs other than stats, if any, must be changed---maybe all.lua
needs to emit the skills in addition to hash and level? Etc.

Stage 11: Make a plan and check in with me before implementing: Currently, the
app recommends trying a new lesson after a fixed number of attempts. Let's
implement overlearning: require 5-15 attempts, depending on how many mistakes
have been made recently (exponential moving average---don't we track something
like this in stats already?).

Stage 12: Make a plan and check in with me before implementing: Mistakes hurt
power---not just time since last practice. If the player makes a mistake, reduce
the power of the current chunk, as well as all child (grand-child,
grand-grand-child, etc.) chunk that involve the group IDs where the mistake was
made.

Stage 13: Make a plan and check in with me before implementing: slow exp decay
of mastery on startup using `last_played` (or, better yet,
`last_updated_mastery` to distinguish playing from mere algorithmic adjustment).

### Not yet!

- combine into a single rust program without losing modularity, testability

- make a web app
