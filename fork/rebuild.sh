#!/usr/bin/env bash
#
# Rebuild an integration line from its manifest.
#
#   fork/rebuild.sh 5.0-tmp
#   fork/rebuild.sh 4.7-tmp --no-push-framework
#
# The line is always rebuilt from upstream, never updated in place: a pull
# request merged upstream disappears by deleting its manifest entry, and
# nothing of it is left behind. Conflict resolutions are remembered by rerere,
# so a rebuild that changed nothing asks nothing.

set -o errexit
set -o nounset
set -o pipefail

usage() {
    cat >&2 <<'EOF'
usage: rebuild.sh <line> [options]

  <line>                  5.0-tmp or 4.7-tmp
  --repo <path>           repository to work in (default: autodetected)
  --manifest <path>       manifest file (default: fork/<line>.yml)
  --no-push-framework     build the framework branch but do not push it
  --dry-run               report what would happen, change nothing
EOF
    exit 2
}

LINE=""
REPO=""
MANIFEST=""
PUSH_FRAMEWORK=1
DRY_RUN=0

while [ $# -gt 0 ]; do
    case "$1" in
        --repo)              REPO="$2"; shift 2 ;;
        --manifest)          MANIFEST="$2"; shift 2 ;;
        --no-push-framework) PUSH_FRAMEWORK=0; shift ;;
        --dry-run)           DRY_RUN=1; shift ;;
        -h|--help)           usage ;;
        -*)                  echo "unknown option: $1" >&2; usage ;;
        *)                   [ -n "$LINE" ] && usage; LINE="$1"; shift ;;
    esac
done

[ -n "$LINE" ] || usage

TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
[ -n "$MANIFEST" ] || MANIFEST="$TOOLS_DIR/$LINE.yml"
[ -f "$MANIFEST" ] || { echo "no manifest at $MANIFEST" >&2; exit 1; }

# The tools branch is a worktree of the same repository, so the source
# worktree is a sibling unless told otherwise.
if [ -z "$REPO" ]; then
    REPO="$(git -C "$TOOLS_DIR" rev-parse --path-format=absolute --git-common-dir)"
    REPO="$(dirname "$REPO")"
fi
[ -d "$REPO/.git" ] || [ -f "$REPO/.git" ] || { echo "not a repository: $REPO" >&2; exit 1; }

say()  { printf '%s\n' "$*"; }
step() { printf '\n== %s\n' "$*"; }
run()  { if [ "$DRY_RUN" = 1 ]; then say "   would run: $*"; else "$@"; fi; }

g() { git -C "$REPO" "$@"; }

# ---------------------------------------------------------------- manifest --
# Deliberately a small reader rather than a YAML dependency: the manifest is
# ours and stays within this shape.

manifest_scalar() {   # manifest_scalar <key>
    sed -n "s/^${1}:[[:space:]]*//p" "$MANIFEST" | head -1
}

manifest_components() {  # emits: ref|name|skip
    python3 - "$MANIFEST" <<'PY'
import re, sys
text = open(sys.argv[1]).read()
# the top level components block, not the framework one
m = re.search(r'^components:\n(.*?)(?=^\w|\Z)', text, re.S | re.M)
if not m:
    sys.exit(0)
for item in re.split(r'\n  - ', '\n' + m.group(1)):
    if not item.strip():
        continue
    def field(k):
        f = re.search(r'^\s*%s:\s*(.+?)\s*$' % k, item, re.M)
        return f.group(1).strip() if f else ''
    skip = 'yes' if re.search(r'^\s*skip:', item, re.M) else ''
    print('|'.join([field('ref'), field('name') or field('ref'), skip]))
PY
}

framework_field() {  # framework_field <key>
    python3 - "$MANIFEST" "$1" <<'PY'
import re, sys
text = open(sys.argv[1]).read()
m = re.search(r'^framework:\n(.*?)(?=^\w|\Z)', text, re.S | re.M)
if not m:
    sys.exit(0)
f = re.search(r'^\s*%s:\s*(.+?)\s*$' % sys.argv[2], m.group(1), re.M)
print(f.group(1).strip() if f else '')
PY
}

framework_components() {
    python3 - "$MANIFEST" <<'PY'
import re, sys
text = open(sys.argv[1]).read()
m = re.search(r'^framework:\n(.*?)(?=^\w|\Z)', text, re.S | re.M)
if not m:
    sys.exit(0)
c = re.search(r'^  components:\n(.*)', m.group(1), re.S | re.M)
if not c:
    sys.exit(0)
for item in re.split(r'\n    - ', '\n' + c.group(1)):
    if not item.strip():
        continue
    f = re.search(r'^\s*ref:\s*(.+?)\s*$', item, re.M)
    n = re.search(r'^\s*name:\s*(.+?)\s*$', item, re.M)
    if f:
        print('|'.join([f.group(1).strip(), (n.group(1).strip() if n else f.group(1).strip())]))
PY
}

BASE="$(manifest_scalar base)"
[ -n "$BASE" ] || { echo "manifest has no base" >&2; exit 1; }

STAMP="$(date +%Y%m%d)"
SKIPPED=()
MANUAL=()

say "line     : $LINE"
say "repo     : $REPO"
say "manifest : $MANIFEST"
say "base     : $BASE"
[ "$DRY_RUN" = 1 ] && say "mode     : dry run, nothing will change"

# ------------------------------------------------------------------ safety --
if [ "$DRY_RUN" = 0 ]; then
    # Submodules are excluded: this script moves the muse pointer itself, and a
    # previous run legitimately leaves it on the framework branch.
    if [ -n "$(g status --porcelain --untracked-files=no --ignore-submodules=all)" ]; then
        echo "the worktree at $REPO has uncommitted changes; stash or commit first" >&2
        g status --short --untracked-files=no --ignore-submodules=all >&2
        exit 1
    fi
fi

step "Remembering conflict resolutions"
run g config rerere.enabled true
run g config rerere.autoupdate true

step "Fetching"
run g fetch origin --quiet
run g fetch manolo --quiet || true

# --------------------------------------------------------------- framework --
FRAMEWORK_REPO="$(framework_field repo)"
FRAMEWORK_BRANCH="$(framework_field branch)"
FRAMEWORK_PIN=""

if [ -n "$FRAMEWORK_REPO" ] && [ "$(manifest_scalar framework)" != "none" ]; then
    step "Rebuilding the framework branch $FRAMEWORK_BRANCH"
    PIN="$(g ls-tree "$BASE" muse | awk '{print $3}')"
    say "   starting from the pin the base asks for: ${PIN:0:12}"
    # Upstream too, not just the fork: the base may have moved the pin to a
    # commit this clone has never seen.
    run git -C "$REPO/muse" fetch origin --quiet || true
    run git -C "$REPO/muse" fetch manolo --quiet || true
    run git -C "$REPO/muse" checkout --quiet -B "$FRAMEWORK_BRANCH" "$PIN"
    while IFS='|' read -r ref name; do
        [ -n "$ref" ] || continue
        printf '   merge %-44s ' "$name"
        if [ "$DRY_RUN" = 1 ]; then echo "(dry run)"; continue; fi
        if git -C "$REPO/muse" merge --no-edit --quiet "$ref" >/dev/null 2>&1; then
            echo "ok"
        else
            echo "CONFLICT"
            git -C "$REPO/muse" diff --name-only --diff-filter=U | sed 's/^/        /'
            MANUAL+=("muse: $name")
            echo "   resolve in $REPO/muse, then rerun" >&2
            exit 1
        fi
    done < <(framework_components)
    [ "$DRY_RUN" = 0 ] && FRAMEWORK_PIN="$(git -C "$REPO/muse" rev-parse HEAD)"
    if [ "$PUSH_FRAMEWORK" = 1 ]; then
        say "   pushing $FRAMEWORK_BRANCH so CI can fetch the pin"
        run git -C "$REPO/muse" push --force-with-lease manolo "$FRAMEWORK_BRANCH"
    else
        say "   not pushed; CI cannot build this line until it is"
    fi
fi

# -------------------------------------------------------------------- line --
step "Archiving the previous tip"
if g rev-parse --verify --quiet "$LINE" >/dev/null; then
    run g tag -f "archive/$LINE-pre-$STAMP" "$LINE"
    say "   archive/$LINE-pre-$STAMP -> $(g rev-parse --short "$LINE" 2>/dev/null || echo '?')"
else
    say "   no previous $LINE, nothing to archive"
fi

step "Rebuilding $LINE from $BASE"
run g checkout --quiet -B "$LINE" "$BASE"

while IFS='|' read -r ref name skip; do
    [ -n "$ref$name" ] || continue
    if [ -n "$skip" ]; then
        printf '   skip  %-44s see manifest\n' "$name"
        SKIPPED+=("$name")
        continue
    fi
    printf '   merge %-44s ' "$name"
    if [ "$DRY_RUN" = 1 ]; then echo "(dry run)"; continue; fi
    if g merge --no-edit --quiet "$ref" >/dev/null 2>&1; then
        echo "ok"
    else
        if [ -z "$(g diff --name-only --diff-filter=U)" ]; then
            # rerere resolved everything; finish the merge it left staged
            g commit --no-edit --quiet
            echo "ok (rerere)"
        else
            echo "CONFLICT"
            g diff --name-only --diff-filter=U | sed 's/^/        /'
            MANUAL+=("$name")
            cat >&2 <<EOF

   Resolve in $REPO, then:
       git add <files> && git commit --no-edit
       $0 $LINE          # rerun; rerere will remember this next time

   The manifest records how earlier conflicts in these files were resolved.
EOF
            exit 1
        fi
    fi
done < <(manifest_components)

# ------------------------------------------------------------------ fixups --
# What rerere cannot reach. Two kinds live here: clean but wrong automerges,
# which are never conflicts so rerere never sees them, and adaptations a
# component needs because its base predates something main has since added.
step "Running fixups"
FIXUPS="$TOOLS_DIR/fixups/$LINE"
if [ -d "$FIXUPS" ] && [ "$DRY_RUN" = 0 ]; then
    for f in "$FIXUPS"/*.sh; do
        [ -e "$f" ] || continue
        say "   $(basename "$f")"
        ( cd "$REPO" && bash "$f" ) || {
            echo "   fixup failed; it is probably stale, read its header" >&2
            exit 1
        }
    done
elif [ -d "$FIXUPS" ]; then
    for f in "$FIXUPS"/*.sh; do
        [ -e "$f" ] || continue
        say "   would run: $(basename "$f")"
    done
else
    say "   none for this line"
fi

# ----------------------------------------------------------------- overlay --
step "Applying the fork overlay"
if [ "$DRY_RUN" = 0 ]; then
    # Per line, not shared: the 4.7 reusable workflows accept none of the
    # submodule override inputs the 5.0 ones do, so one file cannot serve both.
    OVERLAY="$TOOLS_DIR/overlay/$LINE"
    if [ -d "$OVERLAY" ]; then
        ( cd "$OVERLAY" && find . -type f -print0 ) | while IFS= read -r -d '' f; do
            mkdir -p "$REPO/$(dirname "$f")"
            cp "$OVERLAY/$f" "$REPO/$f"
            say "   $f"
        done
        g add -A .github 2>/dev/null || true
    fi
    if [ -n "$(g status --porcelain --untracked-files=no)" ]; then
        g add -A src 2>/dev/null || true
    fi
fi

if [ -n "$FRAMEWORK_PIN" ] && [ "$DRY_RUN" = 0 ]; then
    say "   pinning muse to ${FRAMEWORK_PIN:0:12}"
    g update-index --add --cacheinfo "160000,$FRAMEWORK_PIN,muse"
fi

if [ "$DRY_RUN" = 0 ] && [ -n "$(g status --porcelain --untracked-files=no)" ]; then
    g commit --quiet -m "Fork overlay: CI workflow and framework pin for $LINE"
fi

# ------------------------------------------------------------------ checks --
step "Checking the result"

# Every submodule change must live in the single overlay commit. A stale pin
# smuggled into a feature commit is what broke CI twice in September 2026:
# once the muse pin, once muse_deps, both from the same commit.
BAD=0
for sm in $(g ls-tree "$BASE" | awk '$2=="commit"{print $4}'); do
    OFFENDERS="$(g log --format='%h %s' "$BASE..$LINE" -- "$sm" | tail -n +2)"
    if [ -n "$OFFENDERS" ]; then
        echo "   FAIL: commits other than the overlay touch $sm:" >&2
        printf '%s\n' "$OFFENDERS" | sed 's/^/        /' >&2
        BAD=1
    else
        say "   ok: only the overlay commit touches $sm"
    fi
done
[ "$BAD" = 0 ] || { echo "
   A feature commit is carrying a submodule pointer. Strip it there rather
   than resolving it here, or it comes back on every rebuild." >&2; exit 1; }

step "Result"
say "   $LINE -> $(g rev-parse --short "$LINE" 2>/dev/null || echo 'dry run')"
say "   commits over $BASE: $(g rev-list --count "$BASE..$LINE" 2>/dev/null || echo '?')"
if [ ${#SKIPPED[@]} -gt 0 ]; then
    say "   left out on purpose:"
    printf '     %s\n' "${SKIPPED[@]}"
fi
say ""
say "   Not pushed. Build and try it before you do:"
say "       cd $REPO && ms-build-release"
