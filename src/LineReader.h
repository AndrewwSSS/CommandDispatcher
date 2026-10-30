#pragma once
#include <Arduino.h>
#include "CommandDispatcher.h"

// Collects characters from a Stream into lines and hands each finished one
// to a CommandDispatcher - the loop every console sketch would otherwise write.
//
//   CommandDispatcher<16> commands(Serial);
//   LineReader<32>        usb(Serial);              // a line ends on \r or \n
//   LineReader<32>        phone(Bluetooth, ';');    // or on a character of your own
//
//   void loop() {
//       usb.poll(commands);
//       phone.poll(commands);
//   }
//
// poll() reads only what is already waiting, so it never blocks the loop.
//
// What a terminal sends besides text is dropped: the escape sequences of the
// arrow keys, other control bytes, and backspace takes back the last
// character. Blank lines are skipped, which also covers the LF of a CRLF.
//
// A line longer than SIZE - 1 characters is not run cut short: the rest of it
// is thrown away and "line too long" is printed back to the stream.

template <uint8_t SIZE>
class LineReader {
public:
    // A terminator of '\0' means \r or \n.
    explicit LineReader(Stream& in, char terminator = '\0')
        : in(in), terminator(terminator), length(0), inEscape(false), overflow(false) {}

    template <uint8_t CAPACITY>
    void poll(CommandDispatcher<CAPACITY>& commands) {
        while (in.available() > 0) {
            int c = in.read();
            if (c < 0) break;
            if (take((char)c)) commands.execute(line);
        }
    }

private:
    // True when c finished a line that is now in `line`.
    bool take(char c) {
        if (inEscape) {
            // ESC [ or ESC O, then parameter bytes, then one final byte.
            if (c == '[' || c == 'O' || (c >= '0' && c <= '?')) return false;
            inEscape = false;
            return false;
        }
        if (c == 0x1B) {
            inEscape = true;
            return false;
        }

        if (endsLine(c)) {
            bool ready = length > 0 && !overflow;
            if (overflow) {
                in.print(F("line too long, at most "));
                in.print((int)(SIZE - 1));
                in.println(F(" characters"));
            }
            line[length] = '\0';
            length = 0;
            overflow = false;
            return ready;
        }

        if (c == 0x08 || c == 0x7F) {
            if (length > 0 && !overflow) length--;
            return false;
        }

        if ((unsigned char)c < 0x20) return false;

        if (length < SIZE - 1) {
            line[length++] = c;
        } else {
            overflow = true;
        }
        return false;
    }

    bool endsLine(char c) const {
        return terminator ? c == terminator : (c == '\r' || c == '\n');
    }

    Stream& in;
    char    terminator;
    char    line[SIZE];
    uint8_t length;
    bool    inEscape;
    bool    overflow;
};
