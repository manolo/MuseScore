# Fork tooling

This branch holds no MuseScore source. It carries the definition of the integration lines I maintain on top of upstream, and the script that rebuilds them. It is an orphan branch on purpose: nothing here ever reaches a branch that goes upstream, so `feature/enc-importer` and friends stay exactly as reviewers see them.

| Line | Base | What it is |
|---|---|---|
| `5.0-tmp` | `origin/main` | Today's upstream plus every open pull request of mine |
| `4.7-tmp` | `origin/4.7` | The 4.7 line plus ports that mostly never go upstream |

## The idea

A line is **never updated in place**. It is thrown away and rebuilt from upstream every time, from the list in its manifest. That is what makes a merged pull request cost nothing: delete its entry, rebuild, and it arrives through the base instead, with no leftovers to hunt down.

What made rebuilding painful before was re-resolving the same conflicts on every pass. `git rerere` fixes that: it records how each conflict was resolved and replays it silently next time. The script turns it on, so the second rebuild of an unchanged line asks nothing at all.

## Rebuilding a line

```sh
fork/rebuild.sh 5.0-tmp --dry-run    # see what would happen
fork/rebuild.sh 5.0-tmp
```

The script fetches, rebuilds the framework branch and pushes it, archives the previous tip as `archive/<line>-pre-<date>`, rebuilds the line from its base, applies the overlay, pins the framework, and then checks the result. It stops on any conflict it cannot replay, tells you where, and picks up where it left off when you rerun.

Nothing is pushed except the framework branch. Build and try the line before pushing it.

## When a pull request is merged upstream

Delete its entry from the manifest and rebuild. Do not try to remove it from the branch.

## When a rebuild stops on a conflict

Resolve it in the source worktree, `git add` and `git commit --no-edit`, then run the script again. `rerere` will have recorded the resolution, so the next rebuild passes through it silently. If the resolution is one a future reader would not guess, write it into the manifest next to the component, as the existing entries do.

## Why the framework branch has to be pushed

`5.0-tmp` pins a `muse` commit that is upstream's main plus my own framework pull requests. That commit does not exist in `musescore/muse_framework`, so CI cannot fetch it from the pin alone.

The upstream reusable workflows solve this themselves: `build_macos.yml`, `build_linux.yml` and `build_windows.yml` accept `framework_repo` and `framework_ref` and check the framework out over `muse/`. The fork workflow passes my fork and the rebuilt branch, so `.gitmodules` is never touched and `check_submodules.yml` has nothing to complain about.

`4.7-tmp` has no framework section at all: that line still uses the old monolithic layout with no submodules.

## The submodule check

After a rebuild the script verifies that **only the overlay commit touches `muse` or `muse_deps`**.

This is not decoration. In September 2026 a single feature commit on the importer branch carried stale pointers for both submodules. It broke the pull request as an unresolvable delete/modify against main, and then broke all four platform builds with a crashpad link error, twice, because the first repair fixed `muse` and never looked at `muse_deps`. A feature commit has no business moving the framework; if this check fires, strip the pointer from the offending commit rather than resolving it here, or it returns on the next rebuild.

## Builds

CI builds the three platforms on push to a line, and by hand through `workflow_dispatch`. Artifacts keep the date and the short SHA, and the macOS one comes both as the universal DMG and thinned to arm64.

The two workflows are separate files on purpose. The 4.7 reusable workflows accept only `build_mode`, `publish`, `sentry_project` and `build_number`; passing them the submodule override inputs fails at parse time.

Local builds are unchanged and still the fastest way to iterate:

```sh
cd <repo> && ms-build-release
```

When installing into `/Applications`, replace `Contents/Resources` with a real copy. `ms-build-release` leaves it a symlink into the repository's `share/`, and an app in `/Applications` that follows the working tree breaks the moment the branch changes.
