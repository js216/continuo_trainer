/* driver.c --- orchestrate subprograms and manage internal pipes
 *
 * Forks/execs midi, grouper, rules, realizer, scorer, gui, etc. Passes
 * text events between programs through pipes. Accepts high-level commands
 * (NEW_LESSON, NEXT) from GUI and spawns new processes or pipes events
 * accordingly. Does not read event data from stdin except commands.
 *
 * Example input (stdin):
 *   NEW_LESSON 3
 *
 * Example output:
 *   (internal pipes; forwards events to subprograms)
 */

int main(void)
{
   return 0;
}
