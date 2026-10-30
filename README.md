# CommandDispatcher

Turns a line of text into a call. The first word picks a command; the rest are
its arguments, checked against what the command said it takes before its
handler runs. `LineReader` collects those lines from any `Stream`.
Header-only, no heap, no `String`, and on AVR the names and patterns stay in
flash.

## Usage

```cpp
#include <CommandDispatcher.h>

CommandDispatcher<16> commands(Serial);    // capacity; parse errors are printed to Serial
LineReader<32>        console(Serial);     // longest line + 1

static void go(const CommandArgs& args) {
    axes.moveToStep(0, args.getLong(0));
}

static void jog(const CommandArgs& args) {
    axes.moveSteps(0, args.getLong(0, 100));   // 100 when left out
}

void setup() {
    commands.add(F("g"), F("n"), go);       // g -500
    commands.add(F("f"), F("[n]"), jog);    // f    f 3200
}

void loop() {
    console.poll(commands);   // reads what is waiting, runs each finished line
}
```

## Reading lines

`LineReader<SIZE>` reads from any `Stream` - USB `Serial`, a `SoftwareSerial`
Bluetooth module, the ESP32's `BluetoothSerial` - and never blocks: `poll()`
takes only what has already arrived. Several readers can feed one table:

```cpp
LineReader<32> usb(Serial);              // a line ends on \r or \n
LineReader<32> phone(Bluetooth, ';');    // or on a character of your own
```

What a terminal sends besides text is dropped: arrow-key escape sequences and
other control bytes; backspace takes back a character; blank lines (and the LF
of a CRLF) are skipped. A line longer than `SIZE - 1` characters is not run
cut short - it is thrown away and `line too long, at most N characters` is
printed back to its stream.

To feed lines from somewhere else, call `commands.execute(line)` with a
finished line in a char buffer. It splits that buffer in place, so pass one it
may write into.

## Syntax

Words separated by spaces or tabs: `lim 0 3200`, `g -500`, `home`. Command names
match whatever the case. A glued `f3200` is a command called `f3200`, not `f`
with an argument.

## Patterns

One letter per argument:

| Pattern | Means |
| --- | --- |
| `n` | a number: decimal, optionally signed (`42`, `-500`, `+3`) |
| `w` | a word: anything without a space |
| `[ ]` | what is inside may be left out: `n[n]` is one number or two |

An argument the pattern calls a number is one, so `getLong()` in a handler has
no error to handle. When nothing fits, the handler is not called and a usage
line built from the pattern is printed instead:

```
> g ten
usage: g <number>
```

The same name can be added more than once with different patterns; the first
one that fits is called, and the usage line lists them all:

```cpp
commands.add(F("lim"), F("nn"), setLimits);    // lim 0 3200
commands.add(F("lim"), F("w"), limitsWord);    // lim off
```

```
> lim 1 2 3
usage: lim <number> <number> | lim <word>
```

## Handlers

`void handler(const CommandArgs& args)`:

| Call | Gives |
| --- | --- |
| `args.count()` | how many arguments came |
| `args.has(i)` | whether argument `i` came |
| `args.getLong(i, fallback)` | argument `i` as a number, or `fallback` when it was left out |
| `args.getWord(i, fallback)` | argument `i` as text |
| `args.is(i, F("off"))` | whether argument `i` is that word, ignoring case |
| `args.name()` | the command as typed, for one handler under several names |

The arguments point into the line, so they last only as long as the handler.

A line no command answers to prints `unknown command: <word>`, or goes to
`commands.onUnknown(handler)` when one is set.

## Limits

- `CAPACITY` (the template argument) entries; `add()` returns false when full.
  Each entry is 6 bytes of RAM on an Uno.
- `LineReader<SIZE>` holds `SIZE` bytes, the line plus its terminator.
- `CD_MAX_ARGS` arguments per line, 4 by default. Raise it with
  `build_flags = -D CD_MAX_ARGS=6`.

## Tests

```sh
pio test -e native     # unit tests, against the fake core in test/fake/
scripts/check.sh       # unit tests, plus the example built for the Uno and ESP32
```
