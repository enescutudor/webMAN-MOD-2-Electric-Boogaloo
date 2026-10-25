// Fork (S15): where the fork may change how writes behave.
//
// Only /dev_hdd0, /dev_usbNNN and /dev_ntfsN count as user storage. Everything else is "system": /dev_flash*,
// /dev_blind, /dev_rewrite, /dev_bdvd, unknown devices, a path with a ".." component, a path in another case.
// Writes to system paths keep the upstream behavior exactly (e.g. a failed copy is never deleted there).

static bool is_system_path(const char *path)
{
	if(!path) return true;

	// a ".." component can leave the device
	for(const char *p = path; (p = strstr(p, "..")); p += 2)
		if(((p == path) || (p[-1] == '/')) && ((p[2] == '/') || !p[2])) return true;

	// "//dev_hdd0" and "/./dev_hdd0" reach the same device
	while((*path == '/') || ((path[0] == '.') && (path[1] == '/'))) path += (*path == '/') ? 1 : 2;

	const char *end;
	if(!strncmp(path, "dev_hdd0", 8)) end = path + 8; else
	if(!strncmp(path, "dev_usb", 7) && ISDIGIT(path[7]) && ISDIGIT(path[8]) && ISDIGIT(path[9])) end = path + 10; else
	if(!strncmp(path, "dev_ntfs", 8) && ISDIGIT(path[8])) {end = path + 9; if(*end == ':') end++;} else
		return true;

	return (*end != '/') && (*end != '\0');
}
