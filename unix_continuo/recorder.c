/* recorder.c --- record user MIDI input into a persistent event log
 *
 * Receives CHORD events from midi or grouper and writes them to a
 * per-user log file for later analysis, scoring, or replay. Input
 * is line by line. Timestamps are preserved. Outputs confirmations
 * of logged events to stdout.
 *
 * Example input:
 *   CHORD 12 C4 E4 G4 TIME:123456
 *   CHORD 13 D4 F4 A4 TIME:123567
 *
 * Example output:
 *   LOGGED 12
 *   LOGGED 13
 */

int main(void)
{
   return 0;
}
