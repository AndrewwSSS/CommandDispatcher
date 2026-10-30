#include <unity.h>
#include <Arduino.h>

#include "CommandDispatcher.h"
#include "LineReader.h"

// Every line that reached a handler, as the handler saw it.
static std::vector<std::string> ran;

static void record(const CommandArgs& args) {
    std::string seen = args.name();
    for (uint8_t i = 0; i < args.count(); i++) {
        seen += ' ';
        seen += args.getWord(i);
    }
    ran.push_back(seen);
}

static CommandDispatcher<4>* commands;

void setUp() {
    Serial.clear();
    ran.clear();

    commands = new CommandDispatcher<4>(Serial);
    commands->add(F("g"), F("n"), record);
    commands->add(F("say"), F("[w][w]"), record);
}

void tearDown() {
    delete commands;
}

static void feed(const char* text) {
    Serial.feed(text);
}

// --- Lines --------------------------------------------------------------------

void test_a_line_runs_when_it_ends() {
    LineReader<16> reader(Serial);
    feed("g 12");
    reader.poll(*commands);
    TEST_ASSERT_EQUAL(0, ran.size());   // not finished yet

    feed("\n");
    reader.poll(*commands);
    TEST_ASSERT_EQUAL(1, ran.size());
    TEST_ASSERT_EQUAL_STRING("g 12", ran[0].c_str());
}

void test_a_line_may_arrive_in_pieces() {
    LineReader<16> reader(Serial);
    feed("g ");
    reader.poll(*commands);
    feed("-5");
    reader.poll(*commands);
    feed("00\r");
    reader.poll(*commands);
    TEST_ASSERT_EQUAL(1, ran.size());
    TEST_ASSERT_EQUAL_STRING("g -500", ran[0].c_str());
}

void test_several_lines_in_one_poll_all_run() {
    LineReader<16> reader(Serial);
    feed("g 1\ng 2\rg 3\n");
    reader.poll(*commands);
    TEST_ASSERT_EQUAL(3, ran.size());
    TEST_ASSERT_EQUAL_STRING("g 3", ran[2].c_str());
}

void test_crlf_runs_the_line_once_and_says_nothing_more() {
    LineReader<16> reader(Serial);
    feed("g 1\r\n\r\n\n");
    reader.poll(*commands);
    TEST_ASSERT_EQUAL(1, ran.size());
    TEST_ASSERT_EQUAL_STRING("", Serial.output.c_str());
}

void test_a_terminator_of_your_own() {
    LineReader<16> reader(Serial, ';');
    feed("g 1;g 2\n;");      // the newline is dropped, not a line end
    reader.poll(*commands);
    TEST_ASSERT_EQUAL(2, ran.size());
    TEST_ASSERT_EQUAL_STRING("g 1", ran[0].c_str());
    TEST_ASSERT_EQUAL_STRING("g 2", ran[1].c_str());
}

// --- What a terminal sends besides text -----------------------------------------

void test_arrow_keys_are_dropped() {
    LineReader<16> reader(Serial);
    feed("\x1b[Ag\x1b[1;5C 7\x1bOD\n");
    reader.poll(*commands);
    TEST_ASSERT_EQUAL(1, ran.size());
    TEST_ASSERT_EQUAL_STRING("g 7", ran[0].c_str());
}

void test_backspace_takes_back_a_character() {
    LineReader<16> reader(Serial);
    feed("g 19\x08" "2\x7f" "5\n");
    reader.poll(*commands);
    TEST_ASSERT_EQUAL_STRING("g 15", ran[0].c_str());
}

void test_backspace_on_an_empty_line_does_nothing() {
    LineReader<16> reader(Serial);
    feed("\x08\x08g 3\n");
    reader.poll(*commands);
    TEST_ASSERT_EQUAL_STRING("g 3", ran[0].c_str());
}

void test_other_control_bytes_are_dropped() {
    LineReader<16> reader(Serial);
    feed("g\x01 \x07" "4\n");
    reader.poll(*commands);
    TEST_ASSERT_EQUAL_STRING("g 4", ran[0].c_str());
}

// --- Too long -----------------------------------------------------------------

void test_a_line_that_just_fits_runs() {
    LineReader<8> reader(Serial);      // 7 characters
    feed("say abc\n");
    reader.poll(*commands);
    TEST_ASSERT_EQUAL(1, ran.size());
    TEST_ASSERT_EQUAL_STRING("say abc", ran[0].c_str());
}

void test_a_line_too_long_is_refused_not_cut_short() {
    LineReader<8> reader(Serial);
    feed("g 123456789\n");   // cut to 7 it would still be a valid "g 12345"
    reader.poll(*commands);
    TEST_ASSERT_EQUAL(0, ran.size());
    TEST_ASSERT_EQUAL_STRING("line too long, at most 7 characters\r\n",
                             Serial.output.c_str());
}

void test_the_line_after_a_long_one_is_read_whole() {
    LineReader<8> reader(Serial);
    feed("g 123456789\ng 5\n");
    reader.poll(*commands);
    TEST_ASSERT_EQUAL(1, ran.size());
    TEST_ASSERT_EQUAL_STRING("g 5", ran[0].c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_line_runs_when_it_ends);
    RUN_TEST(test_a_line_may_arrive_in_pieces);
    RUN_TEST(test_several_lines_in_one_poll_all_run);
    RUN_TEST(test_crlf_runs_the_line_once_and_says_nothing_more);
    RUN_TEST(test_a_terminator_of_your_own);
    RUN_TEST(test_arrow_keys_are_dropped);
    RUN_TEST(test_backspace_takes_back_a_character);
    RUN_TEST(test_backspace_on_an_empty_line_does_nothing);
    RUN_TEST(test_other_control_bytes_are_dropped);
    RUN_TEST(test_a_line_that_just_fits_runs);
    RUN_TEST(test_a_line_too_long_is_refused_not_cut_short);
    RUN_TEST(test_the_line_after_a_long_one_is_read_whole);
    return UNITY_END();
}
