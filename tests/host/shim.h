// Mediul minim în care funcțiile extrase din webMAN compilează pe PC.
// Tipurile urmează ABI-ul PPU folosit de webMAN (vezi types.h), nu cel al PC-ului.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

#define MAX(a, b)	((a) >= (b) ? (a) : (b))
#define MIN(a, b)	((a) <= (b) ? (a) : (b))
#define NONE		-1

#include "gen/consts.h"   // MAX_LINE_LEN, STD_PATH_LEN, MAX_PATH_LEN din include/init/paths.h

// Funcții VSH/FS de care depinde parsePath; în harness nu ating nimic.
static void mount_device(const char *a, const char *b, const char *c) { (void)a; (void)b; (void)c; }
static void filepath_check(char *f) { (void)f; }
static void check_path_alias(char *p) { (void)p; }
static bool file_exists(const char *p) { (void)p; return false; }
static bool not_exists(const char *p) { (void)p; return false; }
