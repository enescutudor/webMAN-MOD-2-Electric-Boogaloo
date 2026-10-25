// Minimal runner: each test runs in its own process (see run.sh), so an ASan abort fails only that test.
//   ./t --list     test names
//   ./t <name>     run one test; exit 0 = passed
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*test_fn)(void);
static struct { const char *name; test_fn fn; } tests[64];
static int n_tests;

#define TEST(t) \
	static void t(void); \
	__attribute__((constructor)) static void reg_##t(void) { tests[n_tests].name = #t; tests[n_tests++].fn = t; } \
	static void t(void)

#define CHECK(cond) do { if(!(cond)) { fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); exit(1); } } while(0)
#define CHECK_STR(a, b) do { if(strcmp((a), (b))) { fprintf(stderr, "%s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); exit(1); } } while(0)

int main(int argc, char **argv)
{
	if(argc == 2 && !strcmp(argv[1], "--list")) {
		for(int i = 0; i < n_tests; i++) puts(tests[i].name);
		return 0;
	}
	for(int i = 0; argc == 2 && i < n_tests; i++)
		if(!strcmp(argv[1], tests[i].name)) { tests[i].fn(); return 0; }
	fprintf(stderr, "usage: %s --list | <test>\n", argv[0]);
	return 2;
}
