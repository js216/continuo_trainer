/* stats.c --- provide per-lesson and per-user aggregate statistics
 *
 * Reads scorer logs or queries scorer state on request. Supports multiple
 * kinds of queries:
 *   - Per-lesson totals
 *   - Per-user totals
 *   - Per-step success/failure counts
 *   - Realization frequencies
 * Input is line by line.
 *
 * Example input (stdin):
 *   STATS_REQUEST LESSON 3
 *   STATS_REQUEST USER alice
 *   STATS_REQUEST LESSON 3 STEP 12
 *
 * Example output (stdout):
 *   LESSON 3 STATS: TOTAL_ATTEMPTS:17 SUCCESSFUL:12 AVG_POINTS:850
 *   USER alice STATS: TOTAL_POINTS:23450 STREAK:5
 *   LESSON 3 STEP 12: OK:3 FAIL:1
 */

int main(void)
{
	return 0;
}
