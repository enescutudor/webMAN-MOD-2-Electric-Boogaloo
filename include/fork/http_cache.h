// Fork: HTTP caching for files served from disk (W1), byte ranges (6.4) and CORS only for allowed origins (7b.3).
// The pure functions (line_value, http_etag, http_status, http_range, http_cache_control) are tested in tests/host.

#define WMFORK_CFG	"/dev_hdd0/tmp/wmfork.cfg"

// Value of the "<name><sep> value" line in text (the HTTP request or wmfork.cfg); the name is case-insensitive.
// A value longer than max - 1 is ignored, not truncated: a truncated ETag or origin could match wrongly.
static bool line_value(const char *text, const char *name, char sep, char *value, size_t max)
{
	*value = '\0';
	size_t n = strlen(name);

	for(const char *p = text; p; p = strchr(p, '\n'))
	{
		if(*p == '\n') p++;
		if(strncasecmp(p, name, n) || (p[n] != sep)) continue;

		p += n + 1; while(*p == ' ') p++;
		size_t len = 0; while(p[len] && (p[len] != '\r') && (p[len] != '\n')) len++;
		if(len >= max) return false;

		memcpy(value, p, len); value[len] = '\0';
		return true;
	}
	return false;
}

// ETag from the modification time and size. Without mtime (generated file, NTFS without stat) there is no caching.
static int http_etag(char *etag, u64 mtime, u64 size)
{
	*etag = '\0';
	if(!mtime) return 0;
	return sprintf(etag, "\"%llx-%llx\"", (unsigned long long)mtime, (unsigned long long)size);
}

// Replaces the status of the first line ("HTTP/1.1 200 OK\r\n..." from prepare_header) with `status`, keeping the
// other headers. Returns the new length.
static size_t http_status(char *header, size_t len, const char *status)
{
	const char *eol = strstr(header + 9, "\r\n"); // after "HTTP/1.1 "
	if(!eol) return len;
	size_t old = eol - (header + 9), sl = strlen(status);
	memmove(header + 9 + sl, header + 9 + old, len - 9 - old + 1);
	memcpy(header + 9, status, sl);
	return len + sl - old;
}

// Parses digits into *v; returns the position after them, or NULL if there are none or more than 18.
static const char *parse_u64(const char *p, u64 *v)
{
	u8 digits = 0;
	for(*v = 0; (*p >= '0') && (*p <= '9'); p++)
	{
		if(++digits > 18) return NULL;
		*v = *v * 10 + (u64)(*p - '0');
	}
	return digits ? p : NULL;
}

// Single byte range of a "Range: bytes=..." header (6.4), for a file of `size` bytes: "a-b", "a-" or "-n".
// Returns 1 with *start/*end set (inclusive), 0 to send the whole file (no, invalid or multi-part range),
// -1 when the range is outside the file (416).
static int http_range(const char *value, u64 size, u64 *start, u64 *end)
{
	if(!islike(value, "bytes=") || strchr(value, ',')) return 0;
	const char *p = value + 6;
	u64 a, b;

	if(*p == '-') // the last n bytes
	{
		p = parse_u64(p + 1, &b);
		if(!p || *p) return 0;
		if(!b || !size) return -1;
		*start = (b >= size) ? 0 : size - b;
		*end = size - 1;
		return 1;
	}

	p = parse_u64(p, &a);
	if(!p || (*p != '-')) return 0;
	p++;
	if(*p)
	{
		p = parse_u64(p, &b);
		if(!p || *p || (b < a)) return 0;
	}
	else
		b = size - 1;

	if(a >= size) return -1;
	*start = a;
	*end = (b >= size) ? size - 1 : b;
	return 1;
}

// The UI assets change only with a webMAN update: cache them for a day. Everything else (covers, files) is revalidated on every request.
static const char *http_cache_control(const char *path)
{
	if(islike(path, "/dev_hdd0/xmlhost/game_plugin/"))
	{
		const char *ext = get_ext(path);
		if(_IS(ext, ".js") || _IS(ext, ".css") || _IS(ext, ".png") || _IS(ext, ".jpg") || _IS(ext, ".gif") || _IS(ext, ".svg"))
			return "max-age=86400";
	}
	return "no-cache";
}

// CORS header, only if the request origin is the one in wmfork.cfg (line cors=<origin>). The file is read once.
static char cors_origin[64];
static bool cors_read = false;

static int http_cors(char *out, const char *origin)
{
	if(!cors_read)
	{
		char cfg[512];
		size_t len = read_file(WMFORK_CFG, cfg, sizeof(cfg) - 1, 0); cfg[len] = '\0';
		line_value(cfg, "cors", '=', cors_origin, sizeof(cors_origin));
		cors_read = true;
	}

	if(!*origin || !IS(origin, cors_origin)) return 0;
	return sprintf(out, "Access-Control-Allow-Origin: %s\r\nVary: Origin\r\n", origin);
}
