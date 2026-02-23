/* midi.c --- read MIDI input from device and output timed events
 *
 * Reads incoming MIDI messages from connected devices. Outputs text
 * events with timestamp and channel info suitable for downstream
 * programs. Accepts commands via stdin to select input/output device
 * and optionally forward input to output (hardware synth). On startup
 * reports available MIDI devices.
 *
 * Example input (stdin commands):
 *   SELECT_INPUT 1
 *   SELECT_OUTPUT 2
 *   FORWARD_ON
 *
 * Example output (stdout events, line by line):
 *   CHORD 12 C4 E4 G4 VELOCITY:64 TIME:123456
 *   CHORD 13 D4 F4 A4 VELOCITY:70 TIME:123567
 */

int main(void)
{
   return 0;
}
