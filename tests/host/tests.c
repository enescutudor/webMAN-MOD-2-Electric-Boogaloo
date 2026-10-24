// Teste pe PC pentru funcțiile pure din webMAN. Funcțiile vin din sursa reală (gen/webman.inc, vezi Makefile).
// Testele atk_* reproduc probleme din ANALIZA-webMAN-MOD.md; cele încă nereparate sunt în known-bugs.txt.
#include "shim.h"
#include "gen/webman.inc"
#include "test.h"

// Copie pe heap de exact `size` octeți, ca ASan să prindă orice scriere sau citire în afara ei.
static char *exact(size_t size) { return calloc(1, size); }

// ---------- ssplit (ftp.h) ----------

TEST(ssplit_basic)
{
	char buf[64] = "RETR file.bin", cmd[16], param[64];
	CHECK(ssplit(buf, cmd, 15, param, 63) == 1);
	CHECK_STR(cmd, "RETR");
	CHECK_STR(param, "file.bin");
}

TEST(ssplit_no_separator)
{
	char buf[64] = "PWD", cmd[16], param[64];
	CHECK(ssplit(buf, cmd, 15, param, 63) == 0);
	CHECK_STR(cmd, "PWD");
	CHECK_STR(param, "");
}

TEST(ssplit_truncates)
{
	char buf[64] = "ABCDEFGHIJKLMNOPQRST rest-of-line", cmd[16], param[8];
	CHECK(ssplit(buf, cmd, 15, param, 7) == 1);
	CHECK_STR(cmd, "ABCDEFGHIJKLMNO");
	CHECK_STR(param, "rest-of");
}

// #12: FTP declară param[STD_PATH_LEN] și apelează ssplit(..., param, STD_PATH_LEN) (ftp.h:246, :292),
// înainte de autentificare. NUL-ul final ajunge la param[STD_PATH_LEN], un octet după buffer.
TEST(atk_ftp_ssplit_off_by_one)
{
	char buf[300] = "USER ", cmd[16];
	memset(buf + 5, 'A', 270);
	char *param = exact(STD_PATH_LEN);
	ssplit(buf, cmd, 15, param, STD_PATH_LEN);
	free(param);
}

// ---------- get_param (eval.h) ----------

TEST(get_param_basic)
{
	char v[16];
	CHECK(get_param("id=", v, "a=1&id=42&x=3", 15) == 2);
	CHECK_STR(v, "42");
	CHECK(get_param("x=", v, "a=1&id=42&x=3", 15) == 1);
	CHECK_STR(v, "3");
}

TEST(get_param_name_without_equals)
{
	char v[16];
	CHECK(get_param("id", v, "id=7", 15) == 1);
	CHECK_STR(v, "7");
}

TEST(get_param_missing)
{
	char v[16] = "old";
	CHECK(get_param("id=", v, "a=1", 15) == 0);
	CHECK_STR(v, "");
}

TEST(get_param_truncates)
{
	char *v = exact(5);
	CHECK(get_param("p=", v, "p=123456789", 4) == 4);
	CHECK_STR(v, "1234");
	free(v);
}

// ---------- htmlenc (html.h) ----------

TEST(htmlenc_ascii_and_high)
{
	char src[32] = "Abc \xE9", dst[64];
	CHECK(htmlenc(dst, src, 0) == 10);
	CHECK_STR(dst, "Abc &#xE9;");
}

// #7: fiecare octet „high” devine 6 octeți, iar limita MAX_LINE_LEN se aplică abia după scriere.
TEST(atk_htmlenc_overflow)
{
	char *src = exact(4096);
	memset(src, 0xFF, 500);
	char *dst = exact(MAX_LINE_LEN + 1);
	htmlenc(dst, src, 0);
	CHECK(strlen(dst) <= MAX_LINE_LEN);
	free(dst); free(src);
}

// Raport, secțiunea 4 (html.h:208): &, <, >, " trec neescapate și strică XML-ul XMB.
TEST(atk_htmlenc_xml_special)
{
	char src[32] = "Tom & \"Jerry\" <1>", dst[128];
	htmlenc(dst, src, 0);
	CHECK(!strpbrk(dst, "<>\""));
	CHECK(strstr(dst, "&amp;"));
}

// ---------- get_line (cue_file.h) ----------

TEST(get_line_basic)
{
	const char cue[] = "FILE \"a.bin\" BINARY\r\n  TRACK 01 MODE2/2352\n";
	char line[MAX_LINE_LEN + 1];
	int lp = get_line(line, cue, sizeof(cue) - 1, 0);
	CHECK_STR(line, "FILE \"a.bin\" BINARY");
	lp = get_line(line, cue, sizeof(cue) - 1, lp);
	CHECK_STR(line, "  TRACK 01 MODE2/2352");
	CHECK(get_line(line, cue, sizeof(cue) - 1, lp) == NONE);
}

// #9: mount.h:1303 trimite _path[STD_PATH_LEN] ca destinație, dar get_line scrie până la MAX_LINE_LEN + 1.
TEST(atk_cue_long_line)
{
	char cue[512];
	memset(cue, 'A', 400); strcpy(cue + 400, "\n");
	char *path = exact(STD_PATH_LEN);
	get_line(path, cue, strlen(cue), 0);
	free(path);
}

// ---------- absPath / parsePath (ftp.h) ----------

TEST(abspath_relative)
{
	char out[STD_PATH_LEN];
	absPath(out, "tmp", "/dev_hdd0/");
	CHECK_STR(out, "/dev_hdd0/tmp");
	absPath(out, "tmp", "/dev_hdd0");
	CHECK_STR(out, "/dev_hdd0/tmp");
}

// Raport, secțiunea 4 (ftp.h:64): strcat(cwd + path) fără limită; ambele pot avea până la STD_PATH_LEN - 1 caractere.
TEST(atk_abspath_overflow)
{
	char cwd[STD_PATH_LEN], path[STD_PATH_LEN];
	cwd[0] = '/'; memset(cwd + 1, 'c', 200); cwd[201] = '\0';
	memset(path, 'p', 200); path[200] = '\0';
	char *out = exact(STD_PATH_LEN);
	absPath(out, path, cwd);
	free(out);
}
