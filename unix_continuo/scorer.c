/* realizer.c --- generate candidate realizations of a bassline
 *
 * Enumerates all possible realizations for the current lesson step,
 * within note range and interval constraints. Passes candidates through
 * rules instance to validate correctness. Input can be batch or line
 * by line (LESSON/STEP/BASSLINE). Output is correct chords per step.
 *
 * Example input:
 *   BEGIN_LESSON 3
 *   STEP 12 BASSLINE: C2 E2
 *   END_LESSON
 *
 * Example output:
 *   CORRECT_CHORD 12 C4 E4 G4
 *   CORRECT_CHORD 12 D4 F4 A4
 */

int main(void)
{
	return 0;
}
