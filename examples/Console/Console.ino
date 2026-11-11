// A serial console with three commands. Try:
//
//   led on        led off
//   blink 200     blink 200 5
//   add 2 -7

#include <Arduino.h>
#include <CommandDispatcher.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

CommandDispatcher<8> commands(Serial);
LineReader<32>    console(Serial);

static void led(const CommandArgs& args) {
    if (args.is(0, F("on"))) {
        digitalWrite(LED_BUILTIN, HIGH);
    } else if (args.is(0, F("off"))) {
        digitalWrite(LED_BUILTIN, LOW);
    } else {
        Serial.println(F("led: say on or off"));
    }
}

// blink <ms> [times]
static void blink(const CommandArgs& args) {
    long ms = args.getLong(0);
    long times = args.getLong(1, 3);

    for (long i = 0; i < times; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(ms);
        digitalWrite(LED_BUILTIN, LOW);
        delay(ms);
    }
}

static void add(const CommandArgs& args) {
    Serial.println(args.getLong(0) + args.getLong(1));
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    commands.add(F("led"), F("w"), led);
    commands.add(F("blink"), F("n[n]"), blink);
    commands.add(F("add"), F("nn"), add);
}

void loop() {
    console.poll(commands);
}
