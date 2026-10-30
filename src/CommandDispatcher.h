#pragma once
#include <Arduino.h>
#include <ctype.h>
#include <stdlib.h>

// Turns a line of text into a call: the first word picks a command, the rest
// are its arguments, checked against what the command said it takes before
// its handler ever runs.
//
//   CommandDispatcher<16> commands(Serial);       // parse errors are printed here
//
//   static void go(const CommandArgs& args) { axes.moveToStep(0, args.getLong(0)); }
//   static void jog(const CommandArgs& args) { axes.moveSteps(0, args.getLong(0, 100)); }
//
//   commands.add(F("g"), F("n"), go);       // g -500
//   commands.add(F("f"), F("[n]"), jog);    // f, f 3200
//   commands.execute(line);                 // line is a char buffer
//
// Words are separated by spaces or tabs. Command names match whatever the
// case. Nothing is allocated: the table is a fixed CAPACITY, and the line is
// split in place, so execute() writes into the buffer it is given.
//
// What a command takes is a pattern, one letter per argument:
//
//   n    a number, decimal and optionally signed: 42, -500, +3
//   w    a word: anything without a space
//   [ ]  what is inside is optional, so "n[n]" is one number or two
//
// The same name can be added more than once with different patterns, and the
// first one that fits is called: "lim" with "nn" and again with "w" takes both
// `lim 0 3200` and `lim off`. When none fits, the handler is not called and
// the usage line built from the patterns is printed instead.
//
// On AVR the names and patterns stay in flash, as F() strings, so a command
// costs only its table entry in RAM.

// Most arguments a line can carry. Raise per project with
// build_flags = -D CD_MAX_ARGS=6.
#ifndef CD_MAX_ARGS
#define CD_MAX_ARGS 4
#endif

namespace cd_detail {

inline char flashChar(const __FlashStringHelper* s, size_t i) {
#if defined(__AVR__)
    return (char)pgm_read_byte(reinterpret_cast<const char*>(s) + i);
#else
    return reinterpret_cast<const char*>(s)[i];
#endif
}

inline bool equalsIgnoreCase(const char* ram, const __FlashStringHelper* flash) {
    for (size_t i = 0;; i++) {
        char a = (char)tolower((unsigned char)ram[i]);
        char b = (char)tolower((unsigned char)flashChar(flash, i));
        if (a != b) return false;
        if (a == '\0') return true;
    }
}

// The whole word is a number: an optional sign, then digits and nothing else.
inline bool isNumber(const char* s) {
    if (*s == '+' || *s == '-') s++;
    if (!isdigit((unsigned char)*s)) return false;
    while (isdigit((unsigned char)*s)) s++;
    return *s == '\0';
}

}

// The arguments of one call, already checked against the command's pattern:
// an argument the pattern calls a number is one, so getLong() needs no
// error path. Valid only for the duration of the handler.
class CommandArgs {
public:
    // The command's name as it was typed, for a handler added under several
    // names (one per axis letter, say).
    const char* name() const {
        return commandName;
    }

    uint8_t count() const {
        return n;
    }

    bool has(uint8_t i) const {
        return i < n;
    }

    // fallback is for an optional argument that was left out.
    long getLong(uint8_t i, long fallback = 0) const {
        return has(i) ? strtol(words[i], nullptr, 10) : fallback;
    }

    const char* getWord(uint8_t i, const char* fallback = "") const {
        return has(i) ? words[i] : fallback;
    }

    // Case-insensitive comparison with a fixed word: args.is(0, F("off")).
    bool is(uint8_t i, const __FlashStringHelper* word) const {
        return has(i) && cd_detail::equalsIgnoreCase(words[i], word);
    }

private:
    template <uint8_t> friend class CommandDispatcher;

    const char* commandName = "";
    const char* words[CD_MAX_ARGS];
    uint8_t     n = 0;
};

typedef void (*CommandHandler)(const CommandArgs& args);

// Called with the first word of a line no command answers to.
typedef void (*UnknownCommandHandler)(const char* name);

template <uint8_t CAPACITY>
class CommandDispatcher {
public:
    // Parse errors - a usage line, an unknown command - are printed to out.
    explicit CommandDispatcher(Print& out) : out(out), size(0), unknown(nullptr) {}

    // Returns false when the table is full: raise CAPACITY.
    bool add(const __FlashStringHelper* name, const __FlashStringHelper* pattern,
             CommandHandler handler) {
        if (size >= CAPACITY || !name || !handler) return false;
        table[size++] = { name, pattern, handler };
        return true;
    }

    // A command that takes no arguments.
    bool add(const __FlashStringHelper* name, CommandHandler handler) {
        return add(name, F(""), handler);
    }

    // Replaces the default "unknown command" message.
    void onUnknown(UnknownCommandHandler handler) {
        unknown = handler;
    }

    // Splits line in place and calls the matching handler. Returns true when
    // one ran; false for a blank line, an unknown command or arguments that
    // fit no pattern, each of which (but the blank line) has been reported.
    bool execute(char* line) {
        char* word = nextWord(line);
        if (!word) return false;

        CommandArgs args;
        args.commandName = word;

        while (char* arg = nextWord(line)) {
            if (args.n == CD_MAX_ARGS) {
                out.print(word);
                out.println(F(": too many arguments"));
                return false;
            }
            args.words[args.n++] = arg;
        }

        bool known = false;
        for (uint8_t i = 0; i < size; i++) {
            if (!cd_detail::equalsIgnoreCase(word, table[i].name)) continue;

            known = true;
            if (fits(table[i].pattern, args)) {
                table[i].handler(args);
                return true;
            }
        }

        if (known) {
            printUsage(word);
        } else if (unknown) {
            unknown(word);
        } else {
            out.print(F("unknown command: "));
            out.println(word);
        }
        return false;
    }

private:
    struct Entry {
        const __FlashStringHelper* name;
        const __FlashStringHelper* pattern;
        CommandHandler             handler;
    };

    static bool isBlank(char c) {
        return c == ' ' || c == '\t';
    }

    // Cuts the next word off the front of `rest`, terminating it in place, and
    // moves `rest` past it. nullptr once the line is used up.
    static char* nextWord(char*& rest) {
        while (isBlank(*rest)) rest++;
        if (*rest == '\0') return nullptr;

        char* word = rest;
        while (*rest && !isBlank(*rest)) rest++;
        if (*rest) *rest++ = '\0';
        return word;
    }

    static bool fits(const __FlashStringHelper* pattern, const CommandArgs& args) {
        uint8_t i = 0;          // argument being matched
        bool optional = false;

        for (size_t p = 0;; p++) {
            char c = cd_detail::flashChar(pattern, p);

            if (c == '\0') return i == args.n;   // every argument accounted for
            if (c == '[') { optional = true; continue; }
            if (c == ']') continue;

            if (i == args.n) return optional;    // ran out: fine if the rest is optional
            if (c == 'n' && !cd_detail::isNumber(args.words[i])) return false;
            i++;
        }
    }

    void printUsage(const char* word) {
        out.print(F("usage: "));

        bool first = true;
        for (uint8_t i = 0; i < size; i++) {
            if (!cd_detail::equalsIgnoreCase(word, table[i].name)) continue;

            if (!first) out.print(F(" | "));
            first = false;

            out.print(table[i].name);
            for (size_t p = 0;; p++) {
                char c = cd_detail::flashChar(table[i].pattern, p);
                if (c == '\0') break;
                if (c == '[') out.print(F(" ["));
                else if (c == ']') out.print(']');
                else {
                    // A space before each argument, unless it opens a bracket
                    // that already brought one.
                    if (p == 0 || cd_detail::flashChar(table[i].pattern, p - 1) != '[') out.print(' ');
                    out.print(c == 'n' ? F("<number>") : F("<word>"));
                }
            }
        }
        out.println();
    }

    Print&                out;
    Entry                 table[CAPACITY];
    uint8_t               size;
    UnknownCommandHandler unknown;
};

// The usual way to feed it: LineReader turns a Stream into lines.
#include "LineReader.h"
