#pragma once

// Just enough of the Arduino core for CommandDispatcher.h to compile and run on the
// host under `pio test -e native`: Print, Stream, a Serial that collects
// what is printed and replays what a test feeds it, and F(). Only the native env puts this directory on the include
// path; boards build against their real core.

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))

class Print {
public:
    virtual ~Print() = default;

    virtual size_t write(uint8_t c) = 0;

    size_t write(const char* s) {
        size_t n = 0;
        while (*s) n += write((uint8_t)*s++);
        return n;
    }

    size_t print(const char* s)                { return write(s); }
    size_t print(const __FlashStringHelper* s) { return write(reinterpret_cast<const char*>(s)); }
    size_t print(char c)                       { return write((uint8_t)c); }

    size_t print(long n) {
        char buf[24];
        snprintf(buf, sizeof buf, "%ld", n);
        return write(buf);
    }

    size_t print(int n) { return print((long)n); }

    size_t println() { return write("\r\n"); }

    template <typename T>
    size_t println(const T& value) {
        size_t n = print(value);
        return n + println();
    }
};

class Stream : public Print {
public:
    virtual int available() = 0;
    virtual int read() = 0;
};

class FakeSerial : public Stream {
public:
    size_t write(uint8_t c) override {
        output += (char)c;
        return 1;
    }

    int available() override { return (int)(input.size() - readAt); }

    int read() override {
        if (readAt >= input.size()) return -1;
        return (unsigned char)input[readAt++];
    }

    // What the "other end" has sent and is waiting to be read.
    void feed(const std::string& s) { input += s; }

    void clear() {
        output.clear();
        input.clear();
        readAt = 0;
    }

    std::string output;
    std::string input;
    size_t      readAt = 0;
};

inline FakeSerial Serial;
