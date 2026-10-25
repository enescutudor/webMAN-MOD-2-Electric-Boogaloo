// PC tests for the pure functions in webMAN. The functions come from the real sources (gen/webman.inc, see Makefile).
// The atk_* tests reproduce issues from ANALIZA-webMAN-MOD.md; those not fixed yet are listed in known-bugs.txt.
#include "shim.h"
#include "gen/webman.inc"
#include "test.h"

// Heap copy of exactly `size` bytes, so ASan catches any write or read outside it.
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

// #12: FTP declares param[STD_PATH_LEN] and calls ssplit(..., param, STD_PATH_LEN) (ftp.h:246, :292),
// before authentication. The final NUL lands at param[STD_PATH_LEN], one byte past the buffer.
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

// #7: every "high" byte becomes 6 bytes, and the MAX_LINE_LEN limit is applied only after writing.
TEST(atk_htmlenc_overflow)
{
	char *src = exact(4096);
	memset(src, 0xFF, 500);
	char *dst = exact(MAX_LINE_LEN + 1);
	htmlenc(dst, src, 0);
	CHECK(strlen(dst) <= MAX_LINE_LEN);
	free(dst); free(src);
}

// Report, section 4 (html.h:208): &, <, >, " pass unescaped and break the XMB XML.
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

// #9: mount.h:1303 passes _path[STD_PATH_LEN] as the destination, but get_line writes up to MAX_LINE_LEN + 1.
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

// Report, section 4 (ftp.h:64): unbounded strcat(cwd + path); both can have up to STD_PATH_LEN - 1 characters.
TEST(atk_abspath_overflow)
{
	char cwd[STD_PATH_LEN], path[STD_PATH_LEN];
	cwd[0] = '/'; memset(cwd + 1, 'c', 200); cwd[201] = '\0';
	memset(path, 'p', 200); path[200] = '\0';
	char *out = exact(STD_PATH_LEN);
	absPath(out, path, cwd);
	free(out);
}

// ---------- line_value (fork/http_cache.h) ----------

static const char REQUEST[] =
	"GET /dev_hdd0/xmlhost/game_plugin/sman.css HTTP/1.1\r\n"
	"Host: 192.168.0.4\r\n"
	"if-none-match: \"5f1a2b3c-25c3\"\r\n"
	"Origin: http://example.lan\r\n"
	"\r\n";

TEST(line_value_request_headers)
{
	char v[48];
	CHECK(line_value(REQUEST, "If-None-Match", ':', v, sizeof(v)));
	CHECK_STR(v, "\"5f1a2b3c-25c3\"");
	CHECK(line_value(REQUEST, "Origin", ':', v, sizeof(v)));
	CHECK_STR(v, "http://example.lan");
}

TEST(line_value_missing_or_partial_name)
{
	char v[48] = "old";
	CHECK(!line_value(REQUEST, "Referer", ':', v, sizeof(v)));
	CHECK_STR(v, "");
	CHECK(!line_value(REQUEST, "Hos", ':', v, sizeof(v)));     // a prefix of "Host" is not "Host"
	CHECK(!line_value(REQUEST, "GET /dev_hdd0", ':', v, sizeof(v)));
}

// A value that does not fit is ignored, not truncated: a truncated ETag must never match.
TEST(line_value_too_long_ignored)
{
	char v[8] = "old";
	CHECK(!line_value(REQUEST, "If-None-Match", ':', v, sizeof(v)));
	CHECK_STR(v, "");
}

TEST(line_value_config)
{
	char v[64];
	CHECK(line_value("# wmfork\ncors=http://dash.lan\r\nother=1\n", "cors", '=', v, sizeof(v)));
	CHECK_STR(v, "http://dash.lan");
}

// A request that fills the whole receive buffer, without any CR/LF (www_client.h receives at most HTML_RECV_LAST bytes).
TEST(line_value_full_buffer)
{
	char *req = exact(2048);
	memset(req, 'A', 2047);
	char v[64];
	CHECK(!line_value(req, "Origin", ':', v, sizeof(v)));
	free(req);
}

// ---------- http_etag / http_status_304 / http_cache_control (fork/http_cache.h) ----------

TEST(http_etag_format)
{
	char etag[40];
	CHECK(http_etag(etag, 0x5f1a2b3c, 9667) > 0);
	CHECK_STR(etag, "\"5f1a2b3c-25c3\"");
	CHECK(http_etag(etag, 0, 9667) == 0);   // no mtime: no caching
	CHECK_STR(etag, "");
}

TEST(http_status_304_keeps_headers)
{
	char h[256] = "HTTP/1.1 200 OK\r\nContent-Type: text/css\r\nETag: \"1-2\"\r\n";
	size_t len = http_status_304(h, strlen(h));
	CHECK_STR(h, "HTTP/1.1 304 Not Modified\r\nContent-Type: text/css\r\nETag: \"1-2\"\r\n");
	CHECK(len == strlen(h));
}

TEST(http_cache_control_paths)
{
	CHECK_STR(http_cache_control("/dev_hdd0/xmlhost/game_plugin/jquery.min.js"), "max-age=86400");
	CHECK_STR(http_cache_control("/dev_hdd0/xmlhost/game_plugin/sman.css"), "max-age=86400");
	CHECK_STR(http_cache_control("/dev_hdd0/xmlhost/game_plugin/games.html"), "no-cache");   // the game list changes
	CHECK_STR(http_cache_control("/dev_hdd0/xmlhost/game_plugin/mygames.xml"), "no-cache");
	CHECK_STR(http_cache_control("/dev_hdd0/game/BLES80608/USRDIR/covers/BCES01007.JPG"), "no-cache");
	CHECK_STR(http_cache_control("/dev_usb000/x/jquery.min.js"), "no-cache");
}
