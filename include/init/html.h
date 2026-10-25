#define HTML_RECV_SIZE	2048
#define HTML_RECV_LAST	2047

// fork: line_value() is defined in fork/http_cache.h, which is included later than its first users
// (ps3mapi_server.h, boot_scan.h); declare it once here, included early in every edition.
static bool line_value(const char *text, const char *name, char sep, char *value, size_t max);

#define ORANGE		0xf90
#define MAGENTA		0xf0f
#define CYAN		0x0ff

static char html_base_path[HTML_RECV_SIZE]; // used as temporary buffer

#define custom_popup_msg		(html_base_path + 1845)

#define ITEM_CHECKED			" checked=\"checked\""
#define ITEM_SELECTED			" selected=\"selected\""

#define IS_MARKED(key)			(strstr(param, key) != NULL)
#define IS_UNMARKED(key)		(!strstr(param, key))

#define HTML_REFRESH			"<meta http-equiv=\"refresh\" content=\""

#define HTML_URL				"<a href=\"%s\">%s</a>"
#define HTML_URL2				"<a href=\"%s%s\">%s</a>"

#define HTML_URL_STYLE			"color:#ccc;text-decoration:none;"

#define HTML_DIR				"&lt;dir>"
#define HTML_BUTTON_FMT			"%s%s\" %s'%s';\">"
#define HTML_BUTTON_FMT2		"%s%s\" %s'%s%s';\">"
#define HTML_BUTTON				" <input type=\"button\" value=\""
#define HTML_ONCLICK			"onclick=\"location.href="
#define HTML_ONCLICK2			"onclick=\"var req=new XMLHttpRequest();req.open('GET',"
#define HTML_SEND_CMD			"',true);req.send();return'"
#define HTML_INPUT(n, v, m, s)	"<input name=\"" n "\" type=\"text\" value=\"" v "\" maxlength=\"" m "\" size=\"" s "\">"
#define HTML_PASSW(n, v, m, s)	"<input name=\"" n "\" type=\"password\" value=\"" v "\" maxlength=\"" m "\" size=\"" s "\">"
#define HTML_NUMBER(n, v, min, max)	"<input name=\"" n "\" type=\"number\" value=\"" v "\" min=\"" min "\" max=\"" max "\" style=\"width:45px;\">"
#define HTML_PORT(n, v)			"<input name=\"" n "\" type=\"number\" value=\"" v "\" min=\"1\" max=\"65535\" style=\"width:60px;\">"

#define HTML_DISABLED_CHECKBOX	"disabled"

#define HTML_FORM_METHOD_FMT(a)	"<form action=\"" a "%s"

#define HTML_FORM_METHOD		".ps3mapi\" method=\"get\" enctype=\"application/x-www-form-urlencoded\" target=\"_self\">"

#define HTML_ENTRY_DATE			" &nbsp; " \
								"<td>11-Nov-2006 11:11"

#define _BR_					NULL

#define HTML_RESPONSE_FMT		"HTTP/1.1 %i OK\r\n" \
								"X-PS3-Info: [%s]\r\n" \
								"Content-Type: text/html;charset=UTF-8\r\n" \
								"Cache-Control: no-cache\r\n" \
								"\r\n" \
								"%s%s%s"

#define HTML_HEADER				" <!DOCTYPE html>" \
								"<html xmlns=\"http://www.w3.org/1999/xhtml\">" \
								"<meta http-equiv=\"Content-type\" content=\"text/html;charset=UTF-8\">" \
								"<meta http-equiv=\"Cache-Control\" content=\"no-cache\">" \
								"<meta name=\"viewport\" content=\"width=device-width,initial-scale=0.6,maximum-scale=1.0\">"  /* size: 264 */

#define HTML_RESPONSE_TITLE		WM_APP_VERSION EDITION "<hr><h2>"

#define HTML_BODY				"<body bgcolor=\"#101010\" text=\"#c0c0c0\">" \
								"<font face=\"Courier New\">" /* size: 64 */

#define HTML_BODY_END			"</font></body></html>" /* size: 21 */

#define HTML_BLU_SEPARATOR		"<hr color=\"blue\"/>"
#define HTML_RED_SEPARATOR		"<hr color=\"red\"/>"

#define SCRIPT_SRC_FMT			"<script src=\"%s\"></script>"
#define HTML_REDIRECT_TO_URL	"<script>setTimeout(function(){self.location=\"%s\"},%i);</script>"
#define HTML_REDIRECT_TO_BACK	"<script>history.back();</script>"
#define HTML_CLOSE_BROWSER		"<script>window.close(this);</script>"
#define HTML_REDIRECT_WAIT		3000

#define HTML_TOGGLER			HTML_BLU_SEPARATOR "<b><a class=\"tg\" href=\"javascript:tgl("

#define HTML_SHOW_LAST_GAME		"<span style=\"position:absolute;right:8px\"><font size=2>"
#define HTML_SHOW_LAST_GAME_END	"</font></span>"

// add_html(): id codes for WM_RES_PATH/setup/setup%c.dat
#define dat_QUERY_VIDEOS1	'i'
#define dat_QUERY_VIDEOS2	'j'
#define dat_SETUP_FORM		'0'
#define dat_ICON_TYPE		'1'
#define dat_LANGUAGES		'2'
#define dat_INFO_TAGS		'3'
#define dat_INFO_MSGS		'4'
#define dat_VIEW_IDPS		'p'
#define dat_MEMORY_USAGE	'5'
#define dat_MEM_CONTAINER	'6'
#define dat_BD_REGIONS		'7'
#define dat_DVD_REGIONS		'8'
#define dat_VIDEOREC_FMT	'v'
#define dat_AUDIOREC_FMT	'a'
#define dat_CUSTOM_COMBOS	'x'
#define dat_WAIT_USB_TIME1	'd'
#define dat_WAIT_USB_TIME2	's'
#define dat_SETUP_FOOTER	'z'
#define dat_DEBUG_MEM_KEYS	'k'
#define dat_MEM_OPERATORS	'o'
#define dat_SEL_ICON_SIZE	'c'
#define dat_GET_ICON_SIZE	'g'
#define dat_SFO_ADD_LINKS	't'
#define dat_GPU_CORE_CLOCK1	'm'
#define dat_GPU_VRAM_CLOCK1	'n'
#define dat_GPU_CORE_CLOCK2	'e'
#define dat_GPU_VRAM_CLOCK2	'f'

static void open_browser(const char *url, int mode)
{
	int is_ingame = View_Find("game_plugin");

	if(is_ingame)
	{
		game_interface = (game_plugin_interface *)plugin_GetInterface(is_ingame, 1);
		game_interface->wakeupWithGameExit((char*)url, 1);
	}
	else
	{
		vshmain_AE35CF2D((char*)url, mode); // xmb_plugin->Function 23
	}
}

static void urldec(char *url, char *original)
{
	if(!url) return;
	if(strchr(url, '+') || strchr(url, '%'))
	{
		if(original) strcpy(original, url); // return original url

		u16 pos = 0;
		for(u16 i = 0; url[i] >= ' '; i++, pos++)
		{
			if(url[i] == '+')
				url[pos] = ' ';
			else if(url[i] != '%')
				url[pos] = url[i];
			else
			{
				url[pos] = 0; u8 n = 2; if(url[i+1]=='u') {i++, n+=2;}
				while(n--)
				{
					url[pos] <<= 4, url[pos] += h2b(url[++i]);
				}
			}
		}
		url[pos] = '\0';
	}
}

static bool urlenc_ex(char *dst, const char *src, bool gurl)
{
	if(!src || !dst) return false;

	u16 i, j = 0, pos = 0, max_len = gurl ? HTML_RECV_LAST : MAX_PATH_LEN;

	if(islike(src, "http") && (src[4] == ':' || src[5] == ':') && (src[6] == '/') && src[7]) { for(i = 8; src[i]; i++) if(src[i] == '/') {pos = i; break;} }

#ifdef USE_NTFS
	if(islike(src + pos, "/dev_nt")) pos += 11;
#endif

	for(i = 0; src[i]; i++, j++)
	{
		if(j >= max_len) {j = max_len; break;}

		if((unsigned char)src[i] & 0x80)
		{
			dst[j++] = '%';
			dst[j++] = h2a((unsigned char)src[i]>>4);
			dst[j] = h2a(src[i] & 0xf);
		}
		else if((src[i]=='?' || ((src[i]==':') && (i >= pos))) && gurl)
		{
			dst[j++] = '%';
			dst[j++] = '3';
			dst[j] = h2a(src[i] & 0xf);
		}
		else if(src[i]==' ')
			dst[j] = '+';
		else if(src[i]=='\'' || src[i]=='"' || src[i]=='%' || src[i]=='&' || src[i]=='+' || src[i]=='#')
		{
			dst[j++] = '%';
			dst[j++] = '2';
			dst[j] = h2a(src[i] & 0xf);
		}
		else
			dst[j] = src[i];
	}
	dst[j] = '\0';

	return (j > i); // true if dst != src
}

static bool urlenc(char *dst, const char *src)
{
	return urlenc_ex(dst, src, true);
}

static size_t htmlenc(char *dst, char *src, u8 cpy2src)
{
	if(!src || !dst) return 0;

	size_t j = 0;
	char tmp[10]; u8 t, c;
	for(size_t i = 0; src[i]; i++)
	{
		const char *esc = NULL;
		// fork (#7 / html.h): escape the HTML/XML special characters so a title with & " < > ' cannot break the page
		switch(src[i])
		{
			case '&': esc = "&amp;";  break;
			case '"': esc = "&quot;"; break;
			case '<': esc = "&lt;";   break;
			case '>': esc = "&gt;";   break;
			case '\'': esc = "&#x27;"; break;
		}

		if(esc)
		{
			t = strlen(esc); c = 0;
			if(j + t > MAX_LINE_LEN) break;
			while(t--) {dst[j++] = esc[c++];}
		}
		else if((unsigned char)src[i] >= 0x7F || (unsigned char)src[i] < 0x20)
		{
			t = sprintf(tmp, "&#x%X;", (unsigned char)src[i]); c = 0;
			if(j + t > MAX_LINE_LEN) break; // fork (#7): stop before overflowing dst (was clamped only after writing)
			while(t--) {dst[j++] = tmp[c++];}
		}
		else
		{
			if(j + 1 > MAX_LINE_LEN) break; // fork (#7)
			dst[j++] = src[i];
		}
	}

	dst[j] = '\0';

	if(cpy2src) strcpy(src, dst);
	return j;
}

static size_t utf8enc(char *dst, char *src, u8 cpy2src)
{
	if(!src || !dst) return 0;
	size_t j = 0; unsigned int c;
	for(size_t i = 0; src[i]; i++)
	{
		c = (unsigned char)src[i];
		if(c <= 0x7F)
			dst[j++] = c;
		else { if(c <= 0x07FF) {
			dst[j++] = 0xC0 | (c>>06);
		} else { if(c <= 0xFFFF) {
			dst[j++] = 0xE0 | (0x0F & (c>>12));
		} else { if(c <= 0x1FFFFF) {
			dst[j++] = 0xF0 | (0x0F & (c>>18));
		} else { if(c <= 0x3FFFFFF) {
			dst[j++] = 0xF8 | (0x0F & (c>>24));
		} else { if(c <= 0x7FFFFFFF) {
			dst[j++] = 0xFC | (0x0F & (c>>30));
			dst[j++] = 0x80 | (0x3F & (c>>24)); }
		}	dst[j++] = 0x80 | (0x3F & (c>>18)); }
			dst[j++] = 0x80 | (0x3F & (c>>12)); }
			dst[j++] = 0x80 | (0x3F & (c>>06)); }
			dst[j++] = 0x80 | (0x3F & c); }
	}

	j = MIN(j, MAX_LINE_LEN);
	dst[j] = '\0';

	if(cpy2src) strcpy(src, dst);
	return j;
}
/*
static size_t utf8dec(char *dst, char *src, u8 cpy2src)
{
	size_t j = 0;
	unsigned char c;
	for(size_t i = 0; src[i]; i++, j++)
	{
		c = (unsigned char)src[i];
		if( (c & 0x80) == 0 )
		{
			dst[j] = c;
		}
		else if( (c & 0xE0) == 0xC0 )
		{
			dst[j] = ((c & 0x1F)<<6) |
					 (src[i+1] & 0x3F);
			i+=1;
		}
		else if( (c & 0xF0) == 0xE0 )
		{
			dst[j]  = ((c & 0xF)<<12);
			dst[j] |= ((src[i+1] & 0x3F)<<6);
			dst[j] |= ((src[i+2] & 0x3F));
			i+=2;
		}
		else if ( (c & 0xF8) == 0xF0 )
		{
			dst[j]  = ((c & 0x7)<<18);
			dst[j] |= ((src[i+1] & 0x3F)<<12);
			dst[j] |= ((src[i+2] & 0x3F)<<06);
			dst[j] |= ((src[i+3] & 0x3F));
			i+=3;
		}
		else if ( (c & 0xFC) == 0xF8 )
		{
			dst[j]  = ((c & 0x3)<<24);
			dst[j] |= ((src[i+1] & 0x3F)<<18);
			dst[j] |= ((src[i+2] & 0x3F)<<12);
			dst[j] |= ((src[i+3] & 0x3F)<<06);
			dst[j] |= ((src[i+4] & 0x3F));
			i+=4;
		}
		else if ( (c & 0xFE) == 0xFC )
		{
			dst[j]  = ((c & 0x1)<<30);
			dst[j] |= ((src[i+1] & 0x3F)<<24);
			dst[j] |= ((src[i+2] & 0x3F)<<18);
			dst[j] |= ((src[i+3] & 0x3F)<<12);
			dst[j] |= ((src[i+4] & 0x3F)<<06);
			dst[j] |= ((src[i+5] & 0x3F));
			i+=5;
		}
	}

	j = MIN(j, MAX_LINE_LEN);
	dst[j] = '\0';

	if(cpy2src) strcpy(src, dst);
	return j;
}
*/
static void add_url(char *body, const char *prefix, const char *url, const char *sufix)
{
	if(!body) return;

	if(prefix) concat(body, prefix);
	if(url)    concat(body, url);
	if(sufix)  concat(body, sufix);
}

static size_t add_html(u8 id, int value, char *buffer, char *data)
{
	if(!buffer || !data) return 0;

	char res_file[40];
	sprintf(res_file, "%s/setup/setup%c.dat", WM_RES_PATH, id);

	size_t size = read_file(res_file, data, 1023, 0);
	if(size > 0)
	{
		char *pos = strstr(data, "    ");
		if(pos)
		{
			if((id == dat_AUDIOREC_FMT) || (id == dat_VIDEOREC_FMT))
				sprintf(res_file, "%04x", value);
			else
				sprintf(res_file, "%04i", value);
			memcpy(pos, res_file, 4);
		}
		concat(buffer, data);
	}
	return size;
}

static size_t add_radio_button(const char *name, int value, const char *id, const char *label, const char *sufix, bool checked, char *buffer)
{
	if(!buffer || !label) return 0;

	char html[MAX_LINE_LEN];
	sprintf(html, "<label><input type=\"radio\" name=\"%s\" value=\"%i\" id=\"%s\"%s/> %s%s</label>",
					name, value, id,
					checked ? ITEM_CHECKED : "",
					label, (sufix) ? sufix : "<br>");
	return concat(buffer, html);
}

static size_t add_check_box(const char *name, bool disabled, const char *label, const char *sufix, bool checked, char *buffer)
{
	if(!buffer || !label) return 0;

	char html[MAX_LINE_LEN];
	const char *pos = strstr(label, AUTOBOOT_PATH), *clabel = label;
	if(pos)
	{
		char *autob = html + 100; clabel = autob;
		int n = (pos - label); strcpy(autob, label); // prefix
		const char *on_startup = pos + strlen(AUTOBOOT_PATH); // sufix
		sprintf(autob + n, HTML_INPUT("autop", "%s", "255", "40") "%s", webman_config->autoboot_path, on_startup);
	}
	sprintf(html, "<label><input type=\"checkbox\" name=\"%s\" value=\"1\" %s%s/> %s</label>%s",
					name,
					disabled ? HTML_DISABLED_CHECKBOX : "",
					checked ? ITEM_CHECKED : "",
					clabel, (!sufix) ? "<br>" : sufix);
	return concat(buffer, html);
}

static size_t add_checkbox(const char *name, const char *label, const char *sufix, bool checked, char *buffer)
{
	return add_check_box(name, false, label, sufix, checked, buffer);
}

static size_t add_checkbox_line(const char *name, const char *label, bool checked, char *buffer)
{
	return add_checkbox(name, label, _BR_, checked, buffer);
}

static size_t add_option_item(int value, const char *label, bool selected, char *buffer)
{
	char html[MAX_LINE_LEN], svalue[20];
	if(value >= 0x1000) sprintf(svalue, "0x%x", value); else sprintf(svalue, "%i", value);
	sprintf(html, "<option value=\"%s\"%s/>%s", svalue, selected ? ITEM_SELECTED : "", label);
	return concat(buffer, html);
}

#ifdef PS3MAPI
static size_t add_option_item2(int id, const char *label, const char *var, const char *param, char *buffer)
{
	char value[20]; sprintf(value, "%s=%i", var, id);
	return add_option_item(id, label, IS_MARKED(value), buffer);
}
#endif

//#if defined(VIDEO_REC) || defined(USE_UACCOUNT)
//static size_t add_string_item(const char *value, const char *label, bool selected, char *buffer)
//{
//	char templn[MAX_LINE_LEN];
//	sprintf(templn, "<option value=\"%s\"%s/>%s", value, selected ? ITEM_SELECTED : "", label);
//	return concat(buffer, templn);
//}
//#endif

static size_t prepare_header(char *buffer, const char *param, u8 is_binary)
{
	if(!buffer || !param) return 0;

	bool set_base_path = false;

	// fork: no "Access-Control-Allow-Origin: *" by default; CORS only for the origin set in wmfork.cfg (http_cors)
	size_t slen = sprintf(buffer,	"HTTP/1.1 200 OK\r\n"
									"Content-Type: ");

	t_string *header, mimetype; _set(&mimetype, buffer, slen); header = &mimetype;

	// get mime type
	if(is_binary == BINARY_FILE)
	{
		const char *ext = get_ext(param);

		if(_IS(ext, ".png"))
			_concat2(&mimetype, "image/", "png");
		else
		if(_IS(ext, ".jpg") || _IS(ext, ".jpeg") || IS(ext, ".STH"))
			_concat2(&mimetype, "image/", "jpeg");
		else
		if(_IS(ext, ".htm") || _IS(ext, ".html") || _IS(ext, ".shtm"))
			{_concat2(&mimetype, "text/", "html;charset=UTF-8"); set_base_path = true;}
		else
		if(_IS(ext, ".js"))
			_concat2(&mimetype, "text/", "javascript");
		else
		if(_IS(ext, ".css"))
			_concat2(&mimetype, "text/", "css");
		else
		if(_IS(ext, ".txt") || _IS(ext, ".log") || _IS(ext, ".ini") || _IS(ext, ".cfg") || IS(ext, ".HIP") || IS(ext, ".HIS") || IS(ext, ".HIP") || IS(ext, ".CNF"))
			_concat2(&mimetype, "text/", "plain;charset=UTF-8");
		else
		if(_IS(ext, ".svg"))
			_concat2(&mimetype, "image/", "svg+xml");
		#ifndef LITE_EDITION
		else
		if(_IS(ext, ".gif"))
			_concat2(&mimetype, "image/", "gif");
		else
		if(_IS(ext, ".bmp"))
			_concat2(&mimetype, "image/", "bmp");
		else
		if(_IS(ext, ".tif"))
			_concat2(&mimetype, "image/", "tiff");
		else
		if(_IS(ext, ".avi"))
			_concat2(&mimetype, "video/", "x-msvideo");
		else
		if(_IS(ext, ".mp4") || IS(ext, ".MTH"))
			_concat2(&mimetype, "video/", "mp4");
		else
		if(_IS(ext, ".mkv"))
			_concat2(&mimetype, "video/", "x-matroska");
		else
		if(_IS(ext, ".mpg") || _IS(ext, ".mp2") || strcasestr(ext, ".mpe"))
			_concat2(&mimetype, "video/", "mpeg");
		else
		if(_IS(ext, ".vob"))
			_concat2(&mimetype, "video/", "vob");
		else
		if(_IS(ext, ".wmv"))
			_concat2(&mimetype, "video/", "x-ms-wmv");
		else
		if(_IS(ext, ".flv"))
			_concat2(&mimetype, "video/", "x-flv");
		else
		if(_IS(ext, ".mov"))
			_concat2(&mimetype, "video/", "quicktime");
		else
		if(_IS(ext, ".webm"))
			_concat2(&mimetype, "video/", "webm");
		else
		if(_IS(ext, ".mp3"))
			_concat2(&mimetype, "audio/", "mpeg");
		else
		if(_IS(ext, ".wav"))
			_concat2(&mimetype, "audio/", "x-wav");
		else
		if(_IS(ext, ".wma"))
			_concat2(&mimetype, "audio/", "x-ms-wma");
		else
		if(_IS(ext, ".mid") || _IS(ext, ".kar"))
			_concat2(&mimetype, "audio/", "midi");
		else
		if(_IS(ext, ".mod"))
			_concat2(&mimetype, "audio/", "mod");
		else
		if(_IS(ext, ".zip"))
			_concat2(&mimetype, "application/", "zip");
		else
		if(_IS(ext, ".pdf"))
			_concat2(&mimetype, "application/", "pdf");
		else
		if(_IS(ext, ".doc"))
			_concat2(&mimetype, "application/", "msword");
		else
		if(_IS(ext, ".docx"))
			_concat2(&mimetype, "application/", "vnd.openxmlformats-officedocument.wordprocessingml.document");
		else
		if(_IS(ext, ".xls"))
			_concat2(&mimetype, "application/", "vnd.ms-excel");
		else
		if(_IS(ext, ".xlsx"))
			_concat2(&mimetype, "application/", "vnd.openxmlformats-officedocument.spreadsheetml.sheet");
		else
		if(_IS(ext, ".ppt") || _IS(ext, ".pps"))
			_concat2(&mimetype, "application/", "vnd.ms-powerpoint");
		else
		if(_IS(ext, ".swf"))
			_concat2(&mimetype, "application/", "x-shockwave-flash");
		#endif
		else
			_concat2(&mimetype, "application/", "octet-stream");
	}
	else
		{_concat2(&mimetype, "text/", "html;charset=UTF-8"); set_base_path = true;}

	_concat(&mimetype, "\r\n");

	if(set_base_path && (is_binary != WEB_COMMAND) && param[0] == '/') {strcpy(html_base_path, param); if(!isDir(param)) remove_filename(html_base_path);}

	return header->size;
}
