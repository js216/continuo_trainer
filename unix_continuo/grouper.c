/* grouper.c --- align MIDI notes to lesson bassline and melody
 *
 * Receives MIDI CHORD events and lesson batch from loader. Groups notes
 * into lesson steps, tagging passing notes with "ignore". Supports
 * multiple melodies (MELODY_1, MELODY_2...). Input is line by line
 * (CHORD events) and batch (BEGIN_LESSON ... END_LESSON). Output is
 * CHORD events with step and ignore flags, without timing info.
 *
 * Example input (line by line):
 *   CHORD 12 C4 E4 G4 TIME:123456
 *
 * Example input (bulk from loader):
 *   BEGIN_LESSON 3
 *   BASSLINE: C2 D2 E2 F2
 *   MELODY_1: G4 A4 B4 C5
 *   MELODY_2: E4 F4 G4 A4
 *   END_LESSON
 *
 * Example output:
 *   CHORD 12 C4 E4 G4
 *   CHORD 13 D4 F4 A4 IGNORE
 */

int main(void)
{
   return 0;
}
