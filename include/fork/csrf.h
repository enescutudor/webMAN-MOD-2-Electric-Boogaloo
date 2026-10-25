// Fork (S19): CSRF protection for the state-changing web commands.
//
// Every webMAN command is a GET with no token, so a page on another site that a LAN browser opens can make that
// browser fire /delete.ps3, /write.ps3, /poke.lv2, /ps3mapi... at the console. A browser marks such a cross-site
// request with an Origin (or at least a Referer) that points at the attacker's site. We refuse a dangerous command
// when its Origin/Referer is a different host than the console. A request with no Origin and no Referer is allowed:
// curl, the SFTP relay, apps and the XMB itself send none, so they keep working. localhost is never restricted.
// An extra allowed origin can be set in wmfork.cfg (line cors=<origin>), reusing the CORS whitelist.

#ifndef WMFORK_CFG
#define WMFORK_CFG	"/dev_hdd0/tmp/wmfork.cfg"
#endif

// line_value() comes from fork/http_cache.h, which is included before this header.

// host part of "http://host[:port]/..." or "host[:port]", written to out (bounded); lowercased.
static void csrf_host(char *out, size_t max, const char *url)
{
	const char *p = url;
	const char *s = strstr(p, "://"); if(s) p = s + 3;
	size_t n = 0;
	while(p[n] && p[n] != '/' && p[n] != ':' && n < max - 1) { char c = p[n]; out[n] = (c >= 'A' && c <= 'Z') ? c + 32 : c; n++; }
	out[n] = '\0';
}

static bool csrf_is_localhost(const char *host)
{
	return !host[0] || islike(host, "127.") || IS(host, "localhost");
}

// true when a dangerous command must be refused because it came cross-site from a browser.
static bool csrf_blocked(const char *param, const char *origin, const char *referer, const char *host, const char *remote_ip)
{
	// only the state-changing / dangerous commands are gated; reads and navigation are not
	static const char *guarded[] = {
		"/delete.ps3", "/delete_ps3", "/rename.ps3", "/move.ps3", "/copy.ps3", "/cpy.ps3", "/cut.ps3", "/paste.ps3",
		"/write.ps3", "/chmod.ps3", "/rmdir.ps3", "/mkdir.ps3", "/setup.ps3?", "/ps3mapi", "/poke", "/peek",
		"/lv1.", "/lv2.", "/dump.ps3", "/flash", "/install", "/syscall", "/make.ps3"
	};
	bool guard = false;
	for(u8 i = 0; i < (sizeof(guarded) / sizeof(guarded[0])); i++)
		if(islike(param, guarded[i])) { guard = true; break; }
	if(!guard) return false;

	// the request's own origin/referer; no header at all = non-browser client (curl, relay, app, XMB) -> allow
	const char *src = (*origin) ? origin : referer;
	if(!*src) return false;

	if(remote_ip && csrf_is_localhost(remote_ip)) return false; // localhost is never restricted

	char src_host[80]; csrf_host(src_host, sizeof(src_host), src);
	if(csrf_is_localhost(src_host)) return false;

	// same host as the console (Host header) -> same-origin, allow
	char our_host[80]; csrf_host(our_host, sizeof(our_host), host ? host : "");
	if(*our_host && IS(src_host, our_host)) return false;

	// allowed origin from wmfork.cfg (line cors=<origin>, same the CORS check uses); read once
	static char csrf_allow[64]; static bool csrf_allow_read = false;
	if(!csrf_allow_read)
	{
		char cfg[512]; size_t len = read_file(WMFORK_CFG, cfg, sizeof(cfg) - 1, 0); cfg[len] = '\0';
		line_value(cfg, "cors", '=', csrf_allow, sizeof(csrf_allow));
		csrf_allow_read = true;
	}
	if(*csrf_allow) { char w[80]; csrf_host(w, sizeof(w), csrf_allow); if(*w && IS(src_host, w)) return false; }

	return true; // dangerous command, cross-site browser request
}
