#pragma once
#include <Arduino.h>
#include "CommandDispatcher.h"

// Reads lines from a Stream without blocking and passes each one to a
// CommandDispatcher. Usage: README.md.

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
