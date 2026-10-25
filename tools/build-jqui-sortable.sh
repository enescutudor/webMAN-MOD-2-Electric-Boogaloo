#!/usr/bin/env bash
# Builds jquery-ui.min.js with sortable only (W4): webMAN uses nothing from jQuery UI except $(...).sortable()
# (games_html.h, sman.htm, links.html). The full bundle is 254 KB, this one ~39 KB.
#
#   tools/build-jqui-sortable.sh [output]   (default _Projects_/updater/pkgfiles/USRDIR/html/jquery-ui.min.js)
#
# Requires esbuild in PATH. The jQuery UI source comes from the npm registry, checked against the SHA-1 from its metadata.
set -euo pipefail

VERSION=1.14.2
SHA1=515288b5c730b720acca6e53a0366827ad834053
root=$(git rev-parse --show-toplevel)
out=${1:-$root/_Projects_/updater/pkgfiles/USRDIR/html/jquery-ui.min.js}
cache=${XDG_CACHE_HOME:-$HOME/.cache}/wmfork
tgz=$cache/jquery-ui-$VERSION.tgz

mkdir -p "$cache"
[[ -f $tgz ]] || curl -sfL "https://registry.npmjs.org/jquery-ui/-/jquery-ui-$VERSION.tgz" -o "$tgz"
echo "$SHA1  $tgz" | sha1sum -c --quiet

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
tar -xzf "$tgz" -C "$tmp"
ui=$tmp/package/ui
# Dependency order, from each module's define([...])
{
	printf '/*! jQuery UI v%s, sortable only (version, data, scroll-parent, widget, mouse, sortable) | jquery.org/license */\n' "$VERSION"
	cat "$ui/version.js" "$ui/data.js" "$ui/scroll-parent.js" "$ui/widget.js" "$ui/widgets/mouse.js" "$ui/widgets/sortable.js"
} > "$tmp/sortable.js"
# es5: the PS3 browser is old; esbuild refuses if it would have to emit newer syntax
esbuild "$tmp/sortable.js" --minify --target=es5 --log-level=warning > "$out"
echo "$out: $(stat -c %s "$out") bytes"
