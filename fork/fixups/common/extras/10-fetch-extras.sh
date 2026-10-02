#!/usr/bin/env bash
#
# Put into the tree what the installer carries besides MuseScore itself.
#
# Everything here is taken from where it is published, never from a working
# copy. The point is that the installer ships what other people can also get,
# at a version that can be named, rather than whatever happened to be on the
# machine that built it.
#
# Three of the four destinations need no build change at all: share/plugins
# and share/extensions are installed wholesale by share/CMakeLists.txt, and
# soundfonts are discovered by a recursive scan of the whole resources
# directory. Only share/sound and share/vst3 need the rules that
# 20-install-rules.sh adds, and the VST only works from Contents/VST3 and its
# siblings, never from share/, because the VST module does not look there.
#
# Downloads are cached under fork/.cache so a rebuild that changes nothing
# costs nothing.
#
# Idempotent: a piece already in place with the same bytes is left alone.

set -o errexit
set -o nounset
set -o pipefail

[ -n "${MANIFEST:-}" ]  || { echo "   $0: MANIFEST is not set" >&2; exit 1; }
[ -n "${TOOLS_DIR:-}" ] || { echo "   $0: TOOLS_DIR is not set" >&2; exit 1; }

command -v gh >/dev/null || { echo "   $0: gh is required to fetch the extras" >&2; exit 1; }

CACHE="$TOOLS_DIR/.cache/extras"
mkdir -p "$CACHE"

RECORD="$(mktemp)"
trap 'rm -f "$RECORD"' EXIT

changed=0

# ------------------------------------------------------------- the manifest --
extras() {
    python3 - "$MANIFEST" <<'PY'
import re, sys
text = open(sys.argv[1]).read()
m = re.search(r'^extras:\n(.*?)(?=^\w|\Z)', text, re.S | re.M)
if not m:
    sys.exit(0)
for item in re.split(r'\n  - ', '\n' + m.group(1)):
    if not item.strip():
        continue
    def f(k):
        g = re.search(r'^\s*%s:\s*(.+?)\s*$' % k, item, re.M)
        return g.group(1).strip().strip("'\"") if g else ''
    def asset(platform):
        g = re.search(r'^\s*%s:\s*(.+?)\s*$' % platform, item, re.M)
        return g.group(1).strip().strip("'\"") if g else ''
    print('|'.join([f('name'), f('kind'), f('repo'), f('ref'), f('path'),
                    f('dest'), f('asset'), f('unpack'), f('license'),
                    asset('macos'), asset('windows'), asset('linux_x86_64')]))
PY
}

latest_tag() {   # latest_tag <repo>
    gh release view --repo "$1" --json tagName --jq .tagName 2>/dev/null
}

# place <source file> <destination path>, reporting only real changes
place() {
    local src="$1" dst="$2"
    mkdir -p "$(dirname "$dst")"
    if [ -f "$dst" ] && cmp -s "$src" "$dst"; then
        return 1
    fi
    cp "$src" "$dst"
    return 0
}

# place_dir <source dir> <destination dir>
place_dir() {
    local src="$1" dst="$2"
    if [ -d "$dst" ] && diff -rq "$src" "$dst" >/dev/null 2>&1; then
        return 1
    fi
    rm -rf "$dst"
    mkdir -p "$(dirname "$dst")"
    cp -R "$src" "$dst"
    return 0
}

while IFS='|' read -r name kind repo ref path dest asset unpack license mac win lin; do
    [ -n "$name" ] || continue

    case "$kind" in

    # ------------------------------------------------------------- raw file --
    raw)
        [ -n "$repo" ] && [ -n "$ref" ] && [ -n "$path" ] && [ -n "$dest" ] || {
            echo "   $name: a raw extra needs repo, ref, path and dest" >&2; exit 1; }

        cached="$CACHE/$(echo "$repo/$ref/$path" | tr '/' '_')"
        if [ ! -s "$cached" ]; then
            gh api "repos/$repo/contents/$path?ref=$ref" \
               -H 'Accept: application/vnd.github.raw' > "$cached" || {
                echo "   $name: cannot fetch $path from $repo@$ref" >&2
                rm -f "$cached"; exit 1; }
        fi
        [ -s "$cached" ] || { echo "   $name: fetched an empty file" >&2; exit 1; }

        sha="$(gh api "repos/$repo/commits?path=$path&sha=$ref&per_page=1" \
               --jq '.[0].sha' 2>/dev/null | cut -c1-9)"
        if place "$cached" "$dest"; then
            echo "   $name: $dest ($(wc -c < "$cached" | tr -d ' ') bytes)"
            changed=1
        fi
        printf '%s|%s@%s|%s\n' "$name" "$repo" "${sha:-$ref}" "$license" >> "$RECORD"
        ;;

    # -------------------------------------------------------- release asset --
    release)
        [ -n "$repo" ] || { echo "   $name: a release extra needs a repo" >&2; exit 1; }
        tag="$(latest_tag "$repo")"
        [ -n "$tag" ] || { echo "   $name: $repo has no release to take" >&2; exit 1; }

        tagdir="$CACHE/$(echo "$repo" | tr '/' '_')/$tag"
        mkdir -p "$tagdir"

        # A single named asset, unpacked or not
        if [ -n "$asset" ]; then
            if [ ! -s "$tagdir/$asset" ]; then
                ( cd "$tagdir" && gh release download "$tag" --repo "$repo" \
                    --pattern "$asset" --clobber ) || {
                    echo "   $name: cannot download $asset from $repo $tag" >&2; exit 1; }
            fi
            [ -s "$tagdir/$asset" ] || { echo "   $name: $asset came down empty" >&2; exit 1; }

            if [ "$unpack" = "yes" ]; then
                # A .mext is a zip holding manifest.json and what it names
                work="$tagdir/unpacked"
                if [ ! -d "$work" ]; then
                    mkdir -p "$work"
                    unzip -q -o "$tagdir/$asset" -d "$work" || {
                        echo "   $name: $asset is not a zip" >&2; exit 1; }
                fi
                root="$work"
                [ -f "$root/manifest.json" ] || root="$(dirname "$(find "$work" -name manifest.json | head -1)")"
                [ -f "$root/manifest.json" ] || {
                    echo "   $name: no manifest.json inside $asset" >&2; exit 1; }
                if place_dir "$root" "$dest"; then
                    echo "   $name: $dest"
                    changed=1
                fi
            else
                if place "$tagdir/$asset" "$dest"; then
                    echo "   $name: $dest"
                    changed=1
                fi
            fi
        fi

        # One asset per platform, each unpacked beside the others
        for pair in "macos:$mac" "windows:$win" "linux_x86_64:$lin"; do
            platform="${pair%%:*}"
            pattern="${pair#*:}"
            [ -n "$pattern" ] || continue

            pdir="$tagdir/$platform"
            if [ ! -d "$pdir" ]; then
                mkdir -p "$pdir"
                ( cd "$pdir" && gh release download "$tag" --repo "$repo" \
                    --pattern "$pattern" --clobber ) || {
                    echo "   $name: cannot download $pattern from $repo $tag" >&2
                    rm -rf "$pdir"; exit 1; }
                archive="$(find "$pdir" -maxdepth 1 -type f | head -1)"
                [ -s "$archive" ] || { echo "   $name: $pattern came down empty" >&2; exit 1; }
                case "$archive" in
                    *.zip)    unzip -q -o "$archive" -d "$pdir" ;;
                    *.tar.gz) tar -xzf "$archive" -C "$pdir" ;;
                    *) echo "   $name: do not know how to unpack $archive" >&2; exit 1 ;;
                esac
                rm -f "$archive"
            fi

            bundle="$(find "$pdir" -maxdepth 3 -name '*.vst3' | head -1)"
            [ -n "$bundle" ] || { echo "   $name: no .vst3 inside $pattern" >&2; exit 1; }
            if place_dir "$bundle" "$dest/$platform/$(basename "$bundle")"; then
                echo "   $name: $dest/$platform/$(basename "$bundle")"
                changed=1
            fi
        done

        printf '%s|%s %s|%s\n' "$name" "$repo" "$tag" "$license" >> "$RECORD"
        ;;

    *)
        echo "   $name: unknown kind '$kind'" >&2; exit 1 ;;
    esac
done < <(extras)

# ------------------------------------------------------------ attributes --
# A .vst3 is a bundle of files, and git's text=auto would rewrite the ones that
# look like text. On macOS that list includes Contents/_CodeSignature, so a
# checkout could hand CI a plugin whose signature no longer matches its own
# contents. Everything under here is shipped exactly as published.
VST_ATTRS="share/vst3/.gitattributes"
if [ -d "share/vst3" ] && [ ! -f "$VST_ATTRS" ]; then
    cat > "$VST_ATTRS" <<'ATTRS'
# Published binaries, shipped byte for byte. Nothing here is text, and letting
# git normalise line endings inside a bundle can invalidate its signature.
* -text -diff
ATTRS
    echo "   vst3: marked as binary so git leaves the bundles alone"
    changed=1
fi

# ------------------------------------------------------------- provenance --
# What went in, by version, appended to the file the release notes are built
# from. Without this the notes would say the build carries a VST without
# saying which one, which is the same as not saying anything.
if [ -s "$RECORD" ] && [ -f "fork/CONTENTS.md" ]; then
    if ! grep -q '^## Bundled with the installer' fork/CONTENTS.md; then
        {
            echo
            echo "## Bundled with the installer"
            echo
            echo "| Extra | Taken from | Licence |"
            echo "|---|---|---|"
            sed 's/|/ | /g; s/^/| /; s/$/ |/' "$RECORD"
        } >> fork/CONTENTS.md
        echo "   recorded $(wc -l < "$RECORD" | tr -d ' ') extras in fork/CONTENTS.md"
        changed=1
    fi
fi

[ "$changed" = 1 ] || echo "   nothing to do, the extras are current"
