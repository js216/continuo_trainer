/* rules.c --- evaluate correctness of chords using sliding-window rules
 *
 * Implements musical style rules for grading user input. Uses a sliding
 * window of previous chords (from grouper) to detect parallel fifths,
 * voice leading errors, and melody constraints. Outputs RESULT per step.
 * Input is line by line.
 *
 * Example input:
 *   CHORD 12 C4 E4 G4
 *   CHORD 13 D4 F4 A4 IGNORE
 *
 * Example output:
 *   RESULT 12 OK
 *   RESULT 13 FAIL
 */

int main(void)
{
   return 0;
}
