#!/usr/bin/env bash
#
# Rebuild an integration line from its manifest.
#
#   fork/rebuild.sh 5.0
#   fork/rebuild.sh 4.7 --no-push-framework
#
# The line is always rebuilt from upstream, never updated in place: a pull
# request merged upstream disappears by deleting its manifest entry, and
# nothing of it is left behind. Conflict resolutions are remembered by rerere,
# so a rebuild that changed nothing asks nothing.
#
# The integration is built ONCE, onto <line>-integration, and each brand named
# in the manifest is then stamped onto its own branch <line>-<brand> as a
# single commit on top of it. The brand branches therefore share one parent
# commit exactly, which is what makes putting two of them side by side mean
# anything: they differ in a name and in nothing else.

set -o errexit
set -o nounset
set -o pipefail

usage() {
    cat >&2 <<'EOF'
usage: rebuild.sh <line> [options]

  <line>                  5.0 or 4.7
  --repo <path>           repository to work in (default: autodetected)
  --manifest <path>       manifest file (default: fork/<line>.yml)
  --brand <name>          stamp only this brand (default: all in the manifest)
  --no-push-framework     build the framework branch but do not push it
  --dry-run               report what would happen, change nothing
EOF
    exit 2
}

LINE=""
REPO=""
MANIFEST=""
ONLY_BRAND=""
PUSH_FRAMEWORK=1
DRY_RUN=0

while [ $# -gt 0 ]; do
    case "$1" in
        --repo)              REPO="$2"; shift 2 ;;
        --manifest)          MANIFEST="$2"; shift 2 ;;
        --brand)             ONLY_BRAND="$2"; shift 2 ;;
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

manifest_components() {  # emits: ref|name|skip|fetch
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
    print('|'.join([field('ref'), field('name') or field('ref'), skip, field('fetch')]))
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

manifest_brands() {
    python3 - "$MANIFEST" <<'PY'
import re, sys
text = open(sys.argv[1]).read()
m = re.search(r'^brands:\n((?:\s*-\s*\S+\n)+)', text, re.M)
if not m:
    sys.exit(0)
for line in m.group(1).splitlines():
    name = line.strip().lstrip('-').strip()
    if name:
        print(name)
PY
}

BASE="$(manifest_scalar base)"
[ -n "$BASE" ] || { echo "manifest has no base" >&2; exit 1; }

BRANDS=()
while read -r b; do [ -n "$b" ] && BRANDS+=("$b"); done < <(manifest_brands)
if [ -n "$ONLY_BRAND" ]; then
    printf '%s\n' "${BRANDS[@]}" | grep -qx "$ONLY_BRAND" || {
        echo "brand $ONLY_BRAND is not in $MANIFEST" >&2; exit 1; }
    BRANDS=("$ONLY_BRAND")
fi
[ ${#BRANDS[@]} -gt 0 ] || { echo "manifest declares no brands" >&2; exit 1; }

INTEGRATION="$LINE-integration"

STAMP="$(date +%Y%m%d)"
SKIPPED=()
MANUAL=()

say "line     : $LINE"
say "repo     : $REPO"
say "manifest : $MANIFEST"
say "base     : $BASE"
say "brands   : ${BRANDS[*]}"
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
step "Archiving the previous tips"
archived=0
for brand in "${BRANDS[@]}"; do
    if g rev-parse --verify --quiet "$LINE-$brand" >/dev/null; then
        run g tag -f "archive/$LINE-$brand-pre-$STAMP" "$LINE-$brand"
        say "   archive/$LINE-$brand-pre-$STAMP -> $(g rev-parse --short "$LINE-$brand" 2>/dev/null || echo '?')"
        archived=1
    fi
done
[ "$archived" = 1 ] || say "   nothing built here yet, nothing to archive"

step "Rebuilding the integration from $BASE"
run g checkout --quiet -B "$INTEGRATION" "$BASE"

while IFS='|' read -r ref name skip fetch; do
    [ -n "$ref$name" ] || continue
    if [ -n "$skip" ]; then
        printf '   skip  %-44s see manifest\n' "$name"
        SKIPPED+=("$name")
        continue
    fi
    # A pull request from somebody else's fork has no branch here. Fetch it by
    # its number from upstream instead of adding a remote per contributor.
    if [ -n "$fetch" ]; then
        if [ "$DRY_RUN" = 1 ]; then
            printf '   fetch %-44s %s\n' "$name" "$fetch"
        else
            g fetch origin --quiet "+$fetch:$ref" || {
                echo "   cannot fetch $fetch for $name" >&2
                exit 1
            }
        fi
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
#
# They run in two phases. Integration is what it takes to make the merged pull
# requests build together and is committed once; branding is what gives the
# build a name, and is committed once per brand. Keeping them apart is what
# lets either be read, reverted or carried elsewhere without dragging the
# other along, and it is what lets two brands share one integration.
#
# fixups/common/<phase> runs before fixups/<line>/<phase>. Most branding is
# identical on both lines, and a copy per line is a copy that drifts.
run_fixups() {
    local phase="$1" ran=0 f
    for dir in "$TOOLS_DIR/fixups/common/$phase" "$TOOLS_DIR/fixups/$LINE/$phase"; do
        [ -d "$dir" ] || continue
        for f in "$dir"/*.sh; do
            [ -e "$f" ] || continue
            ran=1
            if [ "$DRY_RUN" = 1 ]; then
                say "   would run: $(basename "$f")"
                continue
            fi
            say "   $(basename "$f")"
            ( cd "$REPO" && bash "$f" ) || {
                echo "   fixup failed; it is probably stale, read its header" >&2
                exit 1
            }
        done
    done
    [ "$ran" = 1 ] || say "   none for this line"
}

# What the rebuild may commit is only what it just created. The safety check
# at the top ignores untracked files, and it has to: a worktree used for
# building collects test output and install leftovers that no rebuild should
# ever sweep into a commit. So the untracked files present before the fixups
# run are photographed here, and staging takes only tracked edits plus files
# that were not in that photograph.
PRISTINE_UNTRACKED="$(mktemp)"
trap 'rm -f "$PRISTINE_UNTRACKED"' EXIT
if [ "$DRY_RUN" = 0 ]; then
    g ls-files --others --exclude-standard | sort > "$PRISTINE_UNTRACKED"
    IGNORED=$(wc -l < "$PRISTINE_UNTRACKED" | tr -d ' ')
    # Said out loud rather than assumed. A file a fixup owns, left behind
    # untracked by an earlier hand run, looks identical to build litter from
    # here and would be dropped from the commit without a word.
    [ "$IGNORED" = 0 ] || say "   ignoring $IGNORED untracked file(s) already in the worktree"
fi

stage_ours() {
    g add -u
    g ls-files --others --exclude-standard | sort | comm -13 "$PRISTINE_UNTRACKED" - \
    | while IFS= read -r f; do
        [ -n "$f" ] && g add -- "$f"
    done
}

step "Integration fixups"
run_fixups integration

# -------------------------------------------------------------- provenance --
# What went into this line, written into the line itself. Artifacts and
# releases outlive anyone's memory of which pull requests were open the day
# they were built, and this is the only place that can answer it later.
step "Recording what went in"
if [ "$DRY_RUN" = 0 ]; then
    mkdir -p "$REPO/fork"
    {
        echo "# What this build contains"
        echo
        echo "Line \`$LINE\`, rebuilt from \`$BASE\` at $(g rev-parse --short "$BASE")."
        echo
        echo "| Component | Pull request |"
        echo "|---|---|"
        python3 - "$MANIFEST" <<'PYIN'
import re, sys
text = open(sys.argv[1]).read()
m = re.search(r'^components:\n(.*?)(?=^\w|\Z)', text, re.S | re.M)
for item in re.split(r'\n  - ', '\n' + (m.group(1) if m else '')):
    if not item.strip():
        continue
    def f(k):
        g = re.search(r'^\s*%s:\s*(.+?)\s*$' % k, item, re.M)
        return g.group(1).strip() if g else ''
    name, pr = f('name'), f('pr')
    skipped = re.search(r'^\s*skip:', item, re.M)
    if skipped:
        print("| %s | left out, see the manifest |" % name)
    else:
        print("| %s | %s |" % (name, ("#" + pr) if pr else "not a pull request"))
PYIN
        echo
        echo "Built from the manifest by \`fork/rebuild.sh\`. Editing this file by"
        echo "hand achieves nothing: the next rebuild overwrites it."
    } > "$REPO/fork/CONTENTS.md"
    say "   fork/CONTENTS.md"
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
    fi

    # Stage by rule, never by a hand picked list. The fixups touch whatever the
    # change needs, version.cmake and the packaging scripts among them, and a
    # list of directories silently drops the ones nobody remembered: branding
    # was applied and then left out of the commit exactly that way.
    stage_ours
fi

if [ -n "$FRAMEWORK_PIN" ] && [ "$DRY_RUN" = 0 ]; then
    say "   pinning muse to ${FRAMEWORK_PIN:0:12}"
    g update-index --add --cacheinfo "160000,$FRAMEWORK_PIN,muse"
fi

INTEGRATION_COMMIT=""
if [ "$DRY_RUN" = 0 ] && [ -n "$(g status --porcelain --untracked-files=no)" ]; then
    g commit --quiet -m "Fork integration: CI workflow and framework pin for $LINE"
    INTEGRATION_COMMIT="$(g rev-parse HEAD)"
fi

# ---------------------------------------------------------------- branding --
# One commit per brand, each on its own branch, all of them sitting directly
# on the integration commit. What a brand changes is a name, an icon and two
# screens, so a reader chasing a code change can skip that commit whole, and
# dropping it gives back a plain build of the same source.
step "Branding"
for brand in "${BRANDS[@]}"; do
    BRAND_DIR="$TOOLS_DIR/brand/$brand"
    [ -f "$BRAND_DIR/identity.sh" ] || {
        echo "   no identity for brand $brand at $BRAND_DIR" >&2; exit 1; }
    # shellcheck disable=SC1091
    APP_NAME=""; . "$BRAND_DIR/identity.sh"
    # The fixups run as separate processes, so what they need has to be
    # exported. LINE lets a fixup pick up per line material, such as the
    # README fragment saying what this particular line adds.
    export BRAND_DIR LINE TOOLS_DIR

    say ""
    say "   $brand -> $LINE-$brand"
    if [ "$DRY_RUN" = 1 ]; then
        run_fixups branding
        continue
    fi

    g checkout --quiet -B "$LINE-$brand" "$INTEGRATION"
    run_fixups branding
    stage_ours
    if [ -n "$(g status --porcelain --untracked-files=no)" ]; then
        g commit --quiet -m "Fork branding: $APP_NAME name, icon, loading screen and about box"
    else
        say "   nothing to commit"
    fi
done

# ------------------------------------------------------------------ checks --
step "Checking the result"

# Every submodule change must live in the integration commit, the one that
# sets the pin. A stale pin smuggled into a feature commit is what broke CI
# twice in September 2026: once the muse pin, once muse_deps, both from the
# same commit.
#
# The integration commit is matched by its hash, not by being the newest.
# Branding lands after it, so "everything except the tip" would now let a bad
# pin through unnoticed.
BAD=0
# Nothing was rebuilt in a dry run, so there is no integration commit to
# compare against and the branch still carries the previous pass.
for sm in $([ "$DRY_RUN" = 0 ] && g ls-tree "$BASE" | awk '$2=="commit"{print $4}'); do
    for brand in "${BRANDS[@]}"; do
        # The grep exits 1 when nothing is left, which is the good case, so its
        # status must not reach errexit.
        OFFENDERS="$(g log --format='%H %h %s' "$BASE..$LINE-$brand" -- "$sm" \
                     | grep -v "^${INTEGRATION_COMMIT:-no-such-commit} " \
                     | cut -d' ' -f2- || true)"
        if [ -n "$OFFENDERS" ]; then
            echo "   FAIL: commits other than the integration commit touch $sm on $LINE-$brand:" >&2
            printf '%s\n' "$OFFENDERS" | sed 's/^/        /' >&2
            BAD=1
        fi
    done
    [ "$BAD" = 1 ] || say "   ok: only the integration commit touches $sm"
done

# The brands must differ by exactly one commit, or they are not comparable.
if [ "$DRY_RUN" = 0 ] && [ ${#BRANDS[@]} -gt 1 ]; then
    PARENTS="$(for brand in "${BRANDS[@]}"; do g rev-parse "$LINE-$brand^"; done | sort -u | wc -l)"
    if [ "$PARENTS" -eq 1 ]; then
        say "   ok: every brand sits on the same integration commit"
    else
        echo "   FAIL: the brand branches do not share one parent" >&2
        BAD=1
    fi
fi
[ "$BAD" = 0 ] || { echo "
   Something above is wrong with the shape of the result. A submodule pointer
   in a feature commit has to be stripped there rather than resolved here, or
   it comes back on every rebuild; brands that do not share a parent mean the
   branding phase built on the wrong thing." >&2; exit 1; }

step "Result"
if [ "$DRY_RUN" = 0 ]; then
    for brand in "${BRANDS[@]}"; do
        say "   $LINE-$brand -> $(g rev-parse --short "$LINE-$brand")"
    done
    say "   commits over $BASE: $(g rev-list --count "$BASE..$LINE-${BRANDS[0]}" 2>/dev/null || echo '?')"
else
    say "   dry run"
fi
if [ ${#SKIPPED[@]} -gt 0 ]; then
    say "   left out on purpose:"
    printf '     %s\n' "${SKIPPED[@]}"
fi
# Leave the worktree on the everyday brand rather than on whichever one the
# loop happened to end with.
[ "$DRY_RUN" = 0 ] && g checkout --quiet "$LINE-${BRANDS[0]}"

say ""
say "   Not pushed. Build and try it before you do:"
say "       cd $REPO && ms-build-release          # now on $LINE-${BRANDS[0]}"
