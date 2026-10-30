#include <unity.h>
#include <Arduino.h>

#include "CommandDispatcher.h"

// What the last handler saw, copied out because CommandArgs points into the
// line, which each test reuses.
static int         calls;
static const char* lastHandler;
static std::string lastName;
static uint8_t     lastCount;
static long        lastLong[CD_MAX_ARGS];
static std::string lastWord[CD_MAX_ARGS];
static bool        lastIsOff;

static void record(const char* handler, const CommandArgs& args) {
    calls++;
    lastHandler = handler;
    lastName = args.name();
    lastCount = args.count();
    for (uint8_t i = 0; i < CD_MAX_ARGS; i++) {
        lastLong[i] = args.getLong(i, -999);
        lastWord[i] = args.getWord(i, "-");
    }
    lastIsOff = args.is(0, F("off"));
}

static void onGo(const CommandArgs& a)        { record("go", a); }
static void onJog(const CommandArgs& a)       { record("jog", a); }
static void onHome(const CommandArgs& a)      { record("home", a); }
static void onLimits(const CommandArgs& a)    { record("limits", a); }
static void onLimitsWord(const CommandArgs& a) { record("limitsWord", a); }
static void onRange(const CommandArgs& a)     { record("range", a); }
static void onAxis(const CommandArgs& a)      { record("axis", a); }

static std::string unknownName;
static void onUnknown(const char* name) { unknownName = name; }

// execute() writes into its line, so every call gets a fresh copy.
template <uint8_t N>
static bool run(CommandDispatcher<N>& commands, const char* text) {
    char line[64];
    strncpy(line, text, sizeof line - 1);
    line[sizeof line - 1] = '\0';
    return commands.execute(line);
}

static const char* output() {
    return Serial.output.c_str();
}

static CommandDispatcher<12>* commands;

void setUp() {
    Serial.clear();
    calls = 0;
    lastHandler = "";
    unknownName.clear();

    commands = new CommandDispatcher<12>(Serial);

    commands->add(F("g"), F("n"), onGo);
    commands->add(F("f"), F("[n]"), onJog);
    commands->add(F("home"), onHome);
    commands->add(F("lim"), F("nn"), onLimits);
    commands->add(F("lim"), F("w"), onLimitsWord);
    commands->add(F("range"), F("n[n]"), onRange);
    commands->add(F("x"), onAxis);
    commands->add(F("y"), onAxis);
}

void tearDown() {
    delete commands;
}

// --- Dispatch -----------------------------------------------------------------

void test_a_command_without_arguments_runs() {
    TEST_ASSERT_TRUE(run(*commands, "home"));
    TEST_ASSERT_EQUAL(1, calls);
    TEST_ASSERT_EQUAL_STRING("home", lastHandler);
    TEST_ASSERT_EQUAL(0, lastCount);
}

void test_names_match_whatever_the_case() {
    TEST_ASSERT_TRUE(run(*commands, "HoMe"));
    TEST_ASSERT_EQUAL_STRING("home", lastHandler);
    TEST_ASSERT_EQUAL_STRING("HoMe", lastName.c_str());   // as typed
}

void test_one_handler_can_serve_several_names() {
    run(*commands, "x");
    TEST_ASSERT_EQUAL_STRING("x", lastName.c_str());
    run(*commands, "Y");
    TEST_ASSERT_EQUAL_STRING("Y", lastName.c_str());
    TEST_ASSERT_EQUAL(2, calls);
}

void test_spaces_and_tabs_around_words_are_ignored() {
    TEST_ASSERT_TRUE(run(*commands, "  \tlim   0 \t 3200  "));
    TEST_ASSERT_EQUAL_STRING("limits", lastHandler);
    TEST_ASSERT_EQUAL(0, lastLong[0]);
    TEST_ASSERT_EQUAL(3200, lastLong[1]);
}

void test_a_blank_line_does_nothing_and_says_nothing() {
    TEST_ASSERT_FALSE(run(*commands, ""));
    TEST_ASSERT_FALSE(run(*commands, "   \t "));
    TEST_ASSERT_EQUAL(0, calls);
    TEST_ASSERT_EQUAL_STRING("", output());
}

// --- Numbers ------------------------------------------------------------------

void test_numbers_may_be_signed() {
    run(*commands, "g -500");
    TEST_ASSERT_EQUAL(-500, lastLong[0]);
    run(*commands, "g +12");
    TEST_ASSERT_EQUAL(12, lastLong[0]);
    run(*commands, "g 0");
    TEST_ASSERT_EQUAL(0, lastLong[0]);
}

void test_a_word_where_a_number_belongs_is_refused_with_usage() {
    TEST_ASSERT_FALSE(run(*commands, "g ten"));
    TEST_ASSERT_EQUAL(0, calls);
    TEST_ASSERT_EQUAL_STRING("usage: g <number>\r\n", output());
}

void test_a_number_with_trailing_letters_is_not_a_number() {
    TEST_ASSERT_FALSE(run(*commands, "g 12abc"));
    TEST_ASSERT_FALSE(run(*commands, "g -"));
    TEST_ASSERT_FALSE(run(*commands, "g 1.5"));
    TEST_ASSERT_EQUAL(0, calls);
}

void test_glued_arguments_are_not_split() {
    // Words and spaces only: "f3200" is a command called f3200.
    TEST_ASSERT_FALSE(run(*commands, "f3200"));
    TEST_ASSERT_EQUAL(0, calls);
    TEST_ASSERT_EQUAL_STRING("unknown command: f3200\r\n", output());
}

// --- Counts and optional arguments ---------------------------------------------

void test_an_optional_argument_may_be_left_out() {
    run(*commands, "f");
    TEST_ASSERT_EQUAL_STRING("jog", lastHandler);
    TEST_ASSERT_EQUAL(0, lastCount);
    TEST_ASSERT_EQUAL(-999, lastLong[0]);   // the handler's fallback

    run(*commands, "f 3200");
    TEST_ASSERT_EQUAL(1, lastCount);
    TEST_ASSERT_EQUAL(3200, lastLong[0]);
}

void test_too_few_or_too_many_arguments_are_refused() {
    TEST_ASSERT_FALSE(run(*commands, "g"));
    TEST_ASSERT_FALSE(run(*commands, "g 1 2"));
    TEST_ASSERT_FALSE(run(*commands, "home now"));
    TEST_ASSERT_EQUAL(0, calls);
}

void test_required_then_optional() {
    run(*commands, "range 5");
    TEST_ASSERT_EQUAL(1, lastCount);
    run(*commands, "range 5 9");
    TEST_ASSERT_EQUAL(2, lastCount);
    TEST_ASSERT_EQUAL(9, lastLong[1]);
    TEST_ASSERT_FALSE(run(*commands, "range"));
    TEST_ASSERT_EQUAL_STRING("usage: range <number> [<number>]\r\n", output());
}

void test_more_words_than_the_line_can_hold_are_refused() {
    TEST_ASSERT_FALSE(run(*commands, "range 1 2 3 4 5"));
    TEST_ASSERT_EQUAL(0, calls);
    TEST_ASSERT_EQUAL_STRING("range: too many arguments\r\n", output());
}

// --- Overloads ----------------------------------------------------------------

void test_the_first_pattern_that_fits_is_called() {
    run(*commands, "lim 0 3200");
    TEST_ASSERT_EQUAL_STRING("limits", lastHandler);

    run(*commands, "lim off");
    TEST_ASSERT_EQUAL_STRING("limitsWord", lastHandler);
    TEST_ASSERT_TRUE(lastIsOff);

    run(*commands, "lim OFF");
    TEST_ASSERT_TRUE(lastIsOff);
}

void test_usage_lists_every_pattern_of_the_name() {
    TEST_ASSERT_FALSE(run(*commands, "lim 1 2 3"));
    TEST_ASSERT_EQUAL_STRING("usage: lim <number> <number> | lim <word>\r\n", output());
}

void test_usage_shows_an_optional_argument_in_brackets() {
    TEST_ASSERT_FALSE(run(*commands, "f x"));
    TEST_ASSERT_EQUAL_STRING("usage: f [<number>]\r\n", output());
}

// --- Unknown and full ---------------------------------------------------------

void test_an_unknown_command_goes_to_its_handler_when_set() {
    commands->onUnknown(onUnknown);
    TEST_ASSERT_FALSE(run(*commands, "jump 3"));
    TEST_ASSERT_EQUAL_STRING("jump", unknownName.c_str());
    TEST_ASSERT_EQUAL_STRING("", output());
}

void test_add_refuses_once_the_table_is_full() {
    CommandDispatcher<2> small(Serial);
    TEST_ASSERT_TRUE(small.add(F("a"), onHome));
    TEST_ASSERT_TRUE(small.add(F("b"), onHome));
    TEST_ASSERT_FALSE(small.add(F("c"), onHome));

    TEST_ASSERT_TRUE(run(small, "b"));
    TEST_ASSERT_FALSE(run(small, "c"));
}

void test_execute_splits_the_buffer_in_place() {
    char line[] = "lim 10 20";
    commands->execute(line);
    // The spaces became terminators; the words are still in the buffer.
    TEST_ASSERT_EQUAL_STRING("lim", line);
    TEST_ASSERT_EQUAL_STRING("10", line + 4);
    TEST_ASSERT_EQUAL_STRING("20", line + 7);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_command_without_arguments_runs);
    RUN_TEST(test_names_match_whatever_the_case);
    RUN_TEST(test_one_handler_can_serve_several_names);
    RUN_TEST(test_spaces_and_tabs_around_words_are_ignored);
    RUN_TEST(test_a_blank_line_does_nothing_and_says_nothing);
    RUN_TEST(test_numbers_may_be_signed);
    RUN_TEST(test_a_word_where_a_number_belongs_is_refused_with_usage);
    RUN_TEST(test_a_number_with_trailing_letters_is_not_a_number);
    RUN_TEST(test_glued_arguments_are_not_split);
    RUN_TEST(test_an_optional_argument_may_be_left_out);
    RUN_TEST(test_too_few_or_too_many_arguments_are_refused);
    RUN_TEST(test_required_then_optional);
    RUN_TEST(test_more_words_than_the_line_can_hold_are_refused);
    RUN_TEST(test_the_first_pattern_that_fits_is_called);
    RUN_TEST(test_usage_lists_every_pattern_of_the_name);
    RUN_TEST(test_usage_shows_an_optional_argument_in_brackets);
    RUN_TEST(test_an_unknown_command_goes_to_its_handler_when_set);
    RUN_TEST(test_add_refuses_once_the_table_is_full);
    RUN_TEST(test_execute_splits_the_buffer_in_place);
    return UNITY_END();
}
