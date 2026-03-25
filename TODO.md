Implement one bullet at a time, tell me how to test, and when I confirmed that
it works, prepare commit message for me to accept. Then, remove the finished
item from this file and get started with the next one.

Web version:

- The tab that opens when you press [L]essons should be titled "Lessons", not
  Stats
- Make sure that the "points today" stat resets every day (at a time that's an
  algorithm parameter, e.g. `midnight_time`)
- When opening the right split, the default should be not 50/50 but golden ratio
  (left pane bigger, right pane smaller)
- Pressing spacebar loads the suggested lesson but it should also update which
  lesson is shown bold and red in the Lessons pane
- Clicking on a chunk in the [L]essons menu should clear the squares/rules
  output for the previous practice
- When the `skill_order` changes, make sure to show the new skills in the
  [L]essons pane
- Remove the X button since users can just close the tab

Both versions:

- For passing notes, the bass passing note itself must be played correctly
- For each chunk, for each bassnote ID, keep track of EMA of correctness of that
  bassnote
- Integrate the files under man/ into the corresponding source code files as the
  "doc block" comment just like most other source code files have
