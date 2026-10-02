# Fork tooling

This branch holds no MuseScore source. It carries the definition of the integration lines I maintain on top of upstream, and the script that rebuilds them. It is an orphan branch on purpose: nothing here ever reaches a branch that goes upstream, so `feature/enc-importer` and friends stay exactly as reviewers see them.

| Line | Base | What it is |
|---|---|---|
| `5.0` | `origin/main` | Today's upstream plus every open pull request of mine |
| `4.7` | `origin/4.7` | The 4.7 line plus ports that mostly never go upstream |

A line is not a branch. Each one is built once and then stamped with every brand its manifest declares, one branch per brand: today that is one brand, so `5.0-plectrascore` and `4.7-plectrascore`. Where there are several they share their parent commit exactly, so they are the same build under different names, which is the only way comparing them means anything.

The brand is `plectrascore`. `fork/brand/musemore/` holds one that is deliberately not built, and says why.

Conventions for what may live on a line, how CI changes are committed, and how upstream's own workflows behave on a fork: [CONVENTIONS.md](CONVENTIONS.md).

## The idea

A line is **never updated in place**. It is thrown away and rebuilt from upstream every time, from the list in its manifest. That is what makes a merged pull request cost nothing: delete its entry, rebuild, and it arrives through the base instead, with no leftovers to hunt down.

What made rebuilding painful before was re-resolving the same conflicts on every pass. `git rerere` fixes that: it records how each conflict was resolved and replays it silently next time. The script turns it on, so the second rebuild of an unchanged line asks nothing at all.

## Rebuilding a line

```sh
fork/rebuild.sh 5.0 --dry-run              # see what would happen
fork/rebuild.sh 5.0
fork/rebuild.sh 5.0 --brand plectrascore   # just the one
```

The script fetches, rebuilds the framework branch and pushes it, archives the previous tips as `archive/<line>-<brand>-pre-<date>`, rebuilds the integration from the base onto `<line>-integration`, applies the overlay, pins the framework, stamps each brand onto its own branch, and then checks the result. It stops on any conflict it cannot replay, tells you where, and picks up where it left off when you rerun.

Nothing is pushed except the framework branch. Build and try the line before pushing it.

## When a pull request is merged upstream

Delete its entry from the manifest and rebuild. Do not try to remove it from the branch.

## When a rebuild stops on a conflict

Resolve it in the source worktree, `git add` and `git commit --no-edit`, then run the script again. `rerere` will have recorded the resolution, so the next rebuild passes through it silently. If the resolution is one a future reader would not guess, write it into the manifest next to the component, as the existing entries do.

## Why the framework branch has to be pushed

`5.0` pins a `muse` commit that is upstream's main plus my own framework pull requests. That commit does not exist in `musescore/muse_framework`, so CI cannot fetch it from the pin alone.

The upstream reusable workflows solve this themselves: `build_macos.yml`, `build_linux.yml` and `build_windows.yml` accept `framework_repo` and `framework_ref` and check the framework out over `muse/`. The fork workflow passes my fork and the rebuilt branch, so `.gitmodules` is never touched and `check_submodules.yml` has nothing to complain about.

`4.7` has no framework section at all: that line still uses the old monolithic layout with no submodules.

## Fixups: what rerere cannot reach

`rerere` only ever sees conflicts. Everything it therefore cannot help with lives in `fork/fixups/`, as small idempotent scripts run after the merges, in two phases:

| Phase | What it is | Commit |
|---|---|---|
| `integration` | making the merged pull requests build together | `Fork integration: ...` |
| `branding` | giving the build a name, once per brand | `Fork branding: ...` |

Integration becomes one commit; branding becomes one commit per brand, each on its own branch. That is the point of the split: branding is a name, an icon and two screens, so a reader chasing a code change can skip that commit whole, dropping it gives back a plain build of the same source, and two brands can share one integration.

A brand is a directory under `fork/brand/` holding an `identity.sh`, which is the only place its name and its wording are written down. The drawing lives in `fork/brand/art/` and is shared; a brand that wants its own drops the file in its own directory and it wins.

The branding phase also rewrites the line's `README.md`, assembled from the brand, from `fork/readme/<line>.md` saying what that line adds, and from the component table the integration phase already wrote. So what the README claims went in cannot drift from what actually did.

`fixups/common/<phase>` runs before `fixups/<line>/<phase>`. Branding is identical on both lines and lives in `common`; a copy per line is a copy that drifts.

Two things integration fixes that a merge never reports:

**Clean but wrong automerges.** Merging #31200 leaves `#include "masklayout.h"` twice, because main and the pull request each add it at a different line and git's three way merge takes both as independent insertions. It is never a conflict, so it never reaches `rerere`, and it comes back on every rebuild.

**A component whose base predates something main has since added.** #31738 changes the excerpt interface but was written before `masternotationmock.h` existed, so the mock main carries now goes abstract when the two meet. The fixup derives the difference from the interface instead of hardcoding it, so it keeps working when that pull request is finally rebased, and says it has nothing to do once the mismatch is gone.

Every fixup is idempotent and fails loudly rather than silently when the file stops looking the way it assumed. Two consecutive rebuilds produce identical source.

## The submodule check

After a rebuild the script verifies two things. That **only the integration commit touches `muse` or `muse_deps`**, matched by hash rather than by being the newest commit, since branding lands after it. And that **every brand branch has the same parent**, because the moment they do not, the two builds differ by more than a name and comparing them proves nothing.

This is not decoration. In September 2026 a single feature commit on the importer branch carried stale pointers for both submodules. It broke the pull request as an unresolvable delete/modify against main, and then broke all four platform builds with a crashpad link error, twice, because the first repair fixed `muse` and never looked at `muse_deps`. A feature commit has no business moving the framework; if this check fires, strip the pointer from the offending commit rather than resolving it here, or it returns on the next rebuild.

## Cutting a release

The push builds are for iterating: they are `devel`, so the app carries a
Development suffix and keeps its settings and data directory away from a real
install. A release is the opposite, `stable`, so it carries its own name
rather than MuseScore Studio Development, and uses the ordinary MuseScore
paths.

Releases are cut by pushing a tag:

```sh
git tag release-5.0.0-$(date +%Y%m%d) 5.0-plectrascore
git push manolo release-5.0.0-$(date +%Y%m%d)
```

That builds the three platforms and publishes a GitHub release on the fork,
with the five downloads and notes listing which pull requests went in, taken
from `fork/CONTENTS.md`, which the rebuild writes into the line.

The tag says nothing about the brand and the workflow names none: it reads
`MUSE_APP_NAME_HUMAN_READABLE` out of `version.cmake` on the tagged commit, so
the release is titled and the downloads named after whichever brand the tag
sits on.

It has to be a tag rather than a button. `workflow_dispatch` requires the
workflow file to sit on the repository's default branch, and these live on the
line; GitHub answers a dispatch of an unregistered workflow with a flat 404.
Once a tag run has registered the workflow, the button works too.

Several releases share a version, since what differs is what the manifest held
that day, so put the date in the tag.

## Builds

CI builds the three platforms on push to a line, and by hand through `workflow_dispatch`. Artifacts keep the date and the short SHA, and the macOS one comes both as the universal DMG and thinned to arm64.

The two workflows are separate files on purpose. The 4.7 reusable workflows accept only `build_mode`, `publish`, `sentry_project` and `build_number`; passing them the submodule override inputs fails at parse time.

Local builds are unchanged and still the fastest way to iterate:

```sh
cd <repo> && ms-build-release
```

When installing into `/Applications`, replace `Contents/Resources` with a real copy. `ms-build-release` leaves it a symlink into the repository's `share/`, and an app in `/Applications` that follows the working tree breaks the moment the branch changes.
