/* gui.c --- render lesson, progress, user chords, and feedback
 *
 * Receives textual event streams from driver or stdin. Displays full
 * lesson bassline/melody (supports multi-line melodies), overlays user
 * input, correct realizations, points, streaks, and hints. GUI is
 * reactive only, does not modify lesson state.
 *
 * Example input:
 *   BEGIN_LESSON 3
 *   BASSLINE: C2 D2 E2 F2
 *   MELODY_1: G4 A4 B4 C5
 *   MELODY_2: E4 F4 G4 A4
 *   END_LESSON
 *   CHORD 12 C4 E4 G4
 *   POINTS 12:5
 *   HINT: Focus on steps 2-4
 *
 * Example output:
 *   (graphical rendering; no stdout)
 */

int main(void)
{
	return 0;
}
