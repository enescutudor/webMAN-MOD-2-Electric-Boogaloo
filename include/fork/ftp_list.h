// Fork: FTP LIST/MLSD/NLST lines collected in a buffer and sent when it fills up (F1), instead of one send() per entry.
// The bytes on the wire are the same. Without the buffer (malloc failed) every line is sent at once, as upstream.
// list_send/list_flush are tested in tests/host.

#define LIST_BUFFER_SIZE	4096
#define LIST_LINE_SIZE		(STD_PATH_LEN + 128) // longest line: ~105 chars of MLSD facts + a 255-char name + CRLF

typedef struct
{
	char *buf;
	u16 len;
} list_buffer;

static int list_flush(list_buffer *lb, int s)
{
	int ret = 0;
	if(lb->buf && lb->len) ret = send(s, lb->buf, lb->len, 0);
	lb->len = 0;
	return ret;
}

static int list_send(list_buffer *lb, int s, const char *line, u16 slen)
{
	if(!lb->buf) return send(s, line, slen, 0);

	if(lb->len + slen > LIST_BUFFER_SIZE)
		if(list_flush(lb, s) < 0) return -1;

	memcpy(lb->buf + lb->len, line, slen);
	lb->len += slen;
	return slen;
}
