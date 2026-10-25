// PC tests for the pure functions in webMAN. The functions come from the real sources (gen/webman.inc, see Makefile).
// The atk_* tests reproduce issues from ANALIZA-webMAN-MOD.md; those not fixed yet are listed in known-bugs.txt.
#include "shim.h"
#include "gen/webman.inc"
#include "test.h"

// Mock send() for fork/ftp_list.h: records every chunk; fail_after makes the n-th call fail.
static char sent[65536]; static size_t sent_len; static int sends, fail_after = -1;
static int send(int s, const void *buf, size_t n, int flags)
{
	(void)s; (void)flags;
	if(sends++ == fail_after) return -1;
	CHECK(n <= 4096);
	memcpy(sent + sent_len, buf, n); sent_len += n;
	return (int)n;
}
#include "../../include/fork/ftp_list.h"

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

// ---------- http_etag / http_status / http_range / http_cache_control (fork/http_cache.h) ----------

TEST(http_etag_format)
{
	char etag[40];
	CHECK(http_etag(etag, 0x5f1a2b3c, 9667) > 0);
	CHECK_STR(etag, "\"5f1a2b3c-25c3\"");
	CHECK(http_etag(etag, 0, 9667) == 0);   // no mtime: no caching
	CHECK_STR(etag, "");
}

TEST(http_status_keeps_headers)
{
	char h[256] = "HTTP/1.1 200 OK\r\nContent-Type: text/css\r\nETag: \"1-2\"\r\n";
	size_t len = http_status(h, strlen(h), "304 Not Modified");
	CHECK_STR(h, "HTTP/1.1 304 Not Modified\r\nContent-Type: text/css\r\nETag: \"1-2\"\r\n");
	CHECK(len == strlen(h));
	len = http_status(h, len, "200 OK");   // and back, same length
	CHECK_STR(h, "HTTP/1.1 200 OK\r\nContent-Type: text/css\r\nETag: \"1-2\"\r\n");
	CHECK(len == strlen(h));
}

#define RANGE(v, size, ret, s, e) do { u64 s_ = 77, e_ = 77; CHECK(http_range(v, size, &s_, &e_) == (ret)); \
	if((ret) > 0) { CHECK(s_ == (u64)(s)); CHECK(e_ == (u64)(e)); } } while(0)

TEST(http_range_forms)
{
	RANGE("bytes=0-99", 1000, 1, 0, 99);
	RANGE("bytes=500-", 1000, 1, 500, 999);
	RANGE("bytes=-100", 1000, 1, 900, 999);       // the last 100 bytes
	RANGE("bytes=-5000", 1000, 1, 0, 999);        // suffix longer than the file
	RANGE("bytes=900-5000", 1000, 1, 900, 999);   // end past the file is clamped
	RANGE("bytes=999-999", 1000, 1, 999, 999);
}

TEST(http_range_whole_file_or_416)
{
	RANGE("", 1000, 0, 0, 0);
	RANGE("bytes=0-99,200-299", 1000, 0, 0, 0);   // multi-part: the whole file
	RANGE("items=0-99", 1000, 0, 0, 0);
	RANGE("bytes=99-0", 1000, 0, 0, 0);           // invalid: ignored
	RANGE("bytes=a-b", 1000, 0, 0, 0);
	RANGE("bytes=-", 1000, 0, 0, 0);
	RANGE("bytes=5-9x", 1000, 0, 0, 0);
	RANGE("bytes=1234567890123456789-", 1000, 0, 0, 0); // too many digits
	RANGE("bytes=1000-", 1000, -1, 0, 0);         // starts after the end: 416
	RANGE("bytes=-0", 1000, -1, 0, 0);
	RANGE("bytes=0-", 0, -1, 0, 0);               // empty file
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

// ---------- list_send / list_flush (fork/ftp_list.h) ----------

// Sends `count` lines of `size` bytes through a list buffer (or without one) and checks the bytes on the wire.
static void list_roundtrip(int count, int size, bool buffered, int expected_sends)
{
	char expected[65536]; size_t expected_len = 0;
	list_buffer lb = {buffered ? malloc(LIST_BUFFER_SIZE) : NULL, 0};
	sent_len = 0; sends = 0; fail_after = -1;
	for(int i = 0; i < count; i++)
	{
		char line[LIST_LINE_SIZE];
		memset(line, 'a' + i % 26, size - 2); memcpy(line + size - 2, "\r\n", 2);
		CHECK(list_send(&lb, 3, line, size) == size);
		memcpy(expected + expected_len, line, size); expected_len += size;
	}
	list_flush(&lb, 3); free(lb.buf);
	CHECK(sent_len == expected_len && !memcmp(sent, expected, expected_len));
	if(expected_sends >= 0) CHECK(sends == expected_sends);
}

TEST(list_empty) { list_roundtrip(0, 0, true, 0); }
TEST(list_one_line) { list_roundtrip(1, 60, true, 1); }
TEST(list_many_lines) { list_roundtrip(500, 70, true, 9); }          // 35,000 bytes in 4,096-byte chunks
TEST(list_exactly_full) { list_roundtrip(64, 64, true, 1); }         // 4,096 bytes: one send
TEST(list_long_lines) { list_roundtrip(40, LIST_LINE_SIZE, true, -1); }
TEST(list_unbuffered_fallback) { list_roundtrip(10, 50, false, 10); } // malloc failed: one send per line, as upstream

TEST(list_send_error)
{
	list_buffer lb = {malloc(LIST_BUFFER_SIZE), 0};
	char line[100]; memset(line, 'x', sizeof(line));
	sent_len = 0; sends = 0; fail_after = 0;
	int ret = 0;
	for(int i = 0; i < 100 && ret >= 0; i++) ret = list_send(&lb, 3, line, sizeof(line));
	CHECK(ret < 0);   // the first flush fails and the error reaches the listing loop
	free(lb.buf);
}

// #16: the longest MLSD line (format from ftp.h) with a 255-char name fits in LIST_LINE_SIZE, not in FTP_RECV_SIZE.
TEST(list_longest_mlsd_line)
{
	char name[256]; memset(name, 'n', 255); name[255] = '\0';
	char line[1024];
	int len = sprintf(line, "%stype=%s%s;siz%s=%llu;modify=%04i%02i%02i%02i%02i%02i;UNIX.mode=0%i%i%i;UNIX.uid=root;UNIX.gid=root; %s\r\n",
					  " ", "c", "file", "e", 18446744073709551615ULL, 2026, 12, 31, 23, 59, 59, 7, 7, 7, name);
	CHECK(len + 1 <= LIST_LINE_SIZE);
	CHECK(len + 1 > STD_PATH_LEN + 20);  // FTP_RECV_SIZE: the old buffer was too small
}

// ---------- is_system_path (fork/sys_path.h) ----------

#ifndef ISDIGIT
#define ISDIGIT(a) (('0' <= (a)) && ((a) <= '9'))
#endif
#include "../../include/fork/sys_path.h"

TEST(system_path_truth_table)
{
	static const struct { const char *path; bool system; } t[] = {
		{"/dev_hdd0", false}, {"/dev_hdd0/", false}, {"/dev_hdd0/tmp/x.bin", false},
		{"/dev_usb000/a", false}, {"/dev_usb007", false}, {"/dev_ntfs0:/PS3ISO/a.iso", false}, {"/dev_ntfs1/x", false},
		{"//dev_hdd0/x", false}, {"/./dev_hdd0/x", false}, {"dev_hdd0/x", false}, {"/dev_hdd0/a/./b", false},
		{"/dev_hdd0/a..b/c", false}, {"/dev_hdd0/..x", false},
		{"/dev_flash/vsh/module/x.sprx", true}, {"/dev_flash2/x", true}, {"/dev_flash3", true}, {"/dev_blind/x", true},
		{"/dev_rewrite/x", true}, {"/dev_bdvd/x", true}, {"/dev_hdd1/x", true}, {"/app_home/x", true}, {"/", true},
		{"", true}, {NULL, true},
		{"/dev_hdd0/../dev_flash/x", true}, {"/dev_hdd0/..", true}, {"/dev_usb000/a/../../dev_blind", true},
		{"/DEV_HDD0/x", true}, {"/dev_hdd0x/y", true}, {"/dev_usb00/x", true}, {"/dev_usbxxx/x", true},
		{"/dev_ntfs/x", true}, {"/dev_hdd00/x", true},
	};
	for(size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++)
		if(is_system_path(t[i].path) != t[i].system)
		{
			printf("    is_system_path(\"%s\") should be %d\n", t[i].path ? t[i].path : "(null)", t[i].system);
			CHECK(0);
		}
}
