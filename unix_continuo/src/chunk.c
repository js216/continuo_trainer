/* chunk.c --- split long lessons into mini-lessons based on difficulty
 *
 * Consumes lesson data and scor step stats to identify hard sections.
 * Generates separate mini-lessons and writes them directly as new lesson
 * files with metadata linking to the parent lesson.
 *
 * Example input:
 *   LESSON 3 STEP_STATS: 2 FAIL, 3 OK, 4 FAIL
 *
 * Example output (new lesson files):
 *   lesson_3-1.txt containing steps 2-4
 *   lesson_3-2.txt containing steps 5-7
 */

int main(void)
{
	return 0;
}
