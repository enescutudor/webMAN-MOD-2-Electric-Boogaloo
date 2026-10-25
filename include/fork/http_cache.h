// Fork: HTTP caching for files served from disk (W1) and CORS only for allowed origins (7b.3).
// The pure functions (line_value, http_etag, http_status_304, http_cache_control) are tested in tests/host.

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

// Turns "HTTP/1.1 200 OK\r\n..." (from prepare_header) into "HTTP/1.1 304 Not Modified\r\n...", keeping the other headers.
// Returns the new length.
static size_t http_status_304(char *header, size_t len)
{
	const size_t ok = 15, nm = 25; // strlen("HTTP/1.1 200 OK"), strlen("HTTP/1.1 304 Not Modified")
	memmove(header + nm, header + ok, len - ok + 1);
	memcpy(header, "HTTP/1.1 304 Not Modified", nm);
	return len + nm - ok;
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
