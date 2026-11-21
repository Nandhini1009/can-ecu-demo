#ifndef TEST_H
#define TEST_H
#include <stdio.h>
#include <stdlib.h>
static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); g_fail++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { printf("  FAIL %s:%d  %s == %s  (got %ld, want %ld)\n", __FILE__, __LINE__, #a, #b, _a, _b); g_fail++; } } while (0)
#define TEST_DONE(name) do { printf("%s: %s\n", name, g_fail ? "FAILED" : "ok"); return g_fail ? 1 : 0; } while (0)
#endif
