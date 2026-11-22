// scrubby — tests/sb_test.h
// plain-assert tests: a registry, CHECK, CHECK_MSG, nothing else.
#pragma once

#include <cstdio>

namespace sbtest {

struct Case {
    const char* name;
    void (*fn)();
    Case* next;
};
Case*& head();
int& failures();
int& checks();

struct Register {
    Register(const char* name, void (*fn)()) {
        static Case pool[256];
        static int used = 0;
        Case& c = pool[used++];
        c.name = name;
        c.fn = fn;
        c.next = nullptr;
        Case** tail = &head();
        while (*tail) tail = &(*tail)->next;
        *tail = &c;
    }
};

}  // namespace sbtest

#define SB_TEST(name)                                         \
    static void name();                                       \
    static sbtest::Register sb_reg_##name(#name, name);       \
    static void name()

#define CHECK(cond)                                                        \
    do {                                                                   \
        ++sbtest::checks();                                                \
        if (!(cond)) {                                                     \
            ++sbtest::failures();                                          \
            printf("    FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);     \
        }                                                                  \
    } while (0)

// a CHECK that explains its numbers when it fails
#define CHECK_MSG(cond, ...)                                               \
    do {                                                                   \
        ++sbtest::checks();                                                \
        if (!(cond)) {                                                     \
            ++sbtest::failures();                                          \
            printf("    FAIL %s:%d  %s  -- ", __FILE__, __LINE__, #cond);  \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
        }                                                                  \
    } while (0)
