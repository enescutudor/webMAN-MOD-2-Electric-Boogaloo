#!/usr/bin/env bash
# Construiește jquery-ui.min.js doar cu sortable (W4): webMAN folosește din jQuery UI numai $(...).sortable()
# (games_html.h, sman.htm, links.html). Pachetul complet are 254 KB, acesta ~39 KB.
#
#   tools/build-jqui-sortable.sh [ieșire]   (implicit _Projects_/updater/pkgfiles/USRDIR/html/jquery-ui.min.js)
#
# Cere esbuild în PATH. Sursa jQuery UI vine din registry-ul npm, verificată cu SHA-1 din metadatele lui.
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
# Ordinea dependențelor din define([...]) ale fiecărui modul
{
	printf '/*! jQuery UI v%s, doar sortable (version, data, scroll-parent, widget, mouse, sortable) | jquery.org/license */\n' "$VERSION"
	cat "$ui/version.js" "$ui/data.js" "$ui/scroll-parent.js" "$ui/widget.js" "$ui/widgets/mouse.js" "$ui/widgets/sortable.js"
} > "$tmp/sortable.js"
# es5: browserul PS3 e vechi; esbuild refuză dacă ar trebui să emită sintaxă mai nouă
esbuild "$tmp/sortable.js" --minify --target=es5 --log-level=warning > "$out"
echo "$out: $(stat -c %s "$out") octeți"
