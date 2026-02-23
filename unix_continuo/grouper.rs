/* grouper.c --- align MIDI notes to lesson bassline, figures, and melody
 *
 * Receives MIDI note on/off events and lesson info from loader. Groups the lesson data (bass note,
 * figures, melody) together with MIDI performance data (played bass note and continuo realization
 * chord). A group is considered complete when all the notes have been released.
 *
 * Some bass notes are labeled as "passing" notes. That means that the harmony from the previous
 * note is still held. When the grouper detects that a passing note has been played, it will output
 * the previous group even though the harmony notes have not yet been released.
 *
 * MIDI has only 12 notes per octave, collapsing enharmonic equivalents into a single note. Grouper
 * makes use of the lesson key signature to spell the MIDI notes appropriately when outputting the
 * group.
 *
 * When a LESSON command is received, grouper resets its internal state, expecting a new set of
 * BASSNOTE, FIGURES, MELODY commands and then the NOTE_ON and NOTE_OFF pairs.
 *
 * Output group format:
 *   GROUP <id> <expected bassnote> <figures> <melody> <actual basenote> <realization chord>
 *
 * Example input (line by line):
 *   LESSON 1 G 3/2 Purcell bars 48-52
 *   BASSNOTE 0: g2
 *   FIGURES 0: 0
 *   MELODY 0: g'2.
 *   BASSNOTE 1: fis2 passing
 *   FIGURES 1: 0
 *   MELODY 1: b'4
 *   BASSNOTE 2: e2
 *   FIGURES 2: #6
 *   MELODY 2: b'4 cis''4
 *   NOTE_ON g, VELOCITY:79 TIME:6059
 *   NOTE_ON d' VELOCITY:79 TIME:21858
 *   NOTE_ON b VELOCITY:83 TIME:21858
 *   NOTE_OFF g, TIME:22858
 *   NOTE_OFF b TIME:22860
 *   NOTE_OFF d' TIME:22861
 *
 * Example output:
 *   GROUP 0 g2 0 g'2 g, d'/b/g
 */
