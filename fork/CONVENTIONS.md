# Conventions for the integration lines

## Every commit on a line answers to something

A commit on `5.0-tmp` or `4.7-tmp` is only ever one of two things:

1. **Work that exists as a pull request.** It arrives by merging that pull request's branch, named in the manifest with its number. It is never typed into the line by hand: if it needs fixing, fix it on its own branch and rebuild.
2. **A feature the fork needs and upstream will not take.** These live on their own branch, `fork/4.7-ports` for the 4.7 line, and enter as a manifest component like any other.

Nothing else belongs on a line. A change with no pull request behind it and no place in a fork branch has nowhere to live, and the next rebuild throws it away, silently. If you find yourself wanting to commit straight onto a line, that is the signal that it needs a branch first.

Commits are **thematic**: one subject each, with the pull request number or the fork feature in the subject line. A line rebuilt from a manifest is read by looking at what went into it, and a pile of "fix" commits makes that unreadable.

## CI changes go in one commit, or in a few thematic ones

The workflow the fork adds arrives through the overlay, as a **single commit** at the end of the rebuild. That is deliberate: it keeps everything that is fork scaffolding, rather than product, in one place that is trivial to identify and to drop.

When the CI grows enough that one commit stops describing it, split it by subject, never by file: one commit for the build workflow, one for packaging, one for whatever publishes. Do not scatter CI changes through the line's history, and never fold them into a commit that belongs to a pull request. Anything upstream might one day want has to be separable from the scaffolding that only the fork needs.

The same rule holds in reverse: a commit that belongs to a pull request must not carry CI changes. The importer branch already paid for ignoring that, with a stale submodule pointer riding inside a feature commit and breaking the pull request and four platform builds.

## Upstream CI on a fork

Checked against `origin/main` on 2026-09-28. The short version: it behaves better than expected, and the parts that would misbehave already guard themselves.

### Nothing upstream fires on push

No upstream workflow triggers on `push`. They are all `pull_request`, `workflow_call`, `workflow_dispatch`, `schedule`, `issues` or `pull_request_target`. Pushing a rebuilt line therefore runs **only** the fork's own `fork_builds.yml`, which is what makes pushing a line cheap.

### Scheduled workflows: guarded, except one

Six workflows carry a `schedule`. Five test the repository name and skip on a fork, which is visible in the fork's run list as `skipped`, including `translate_tx_pull_to_s3`, which would otherwise fire every fifteen minutes from Monday to Saturday.

The exception is `cleanup_ccache`, hourly and unguarded. It succeeds, and it is cleaning the fork's own cache, so it is useful rather than harmful. Left alone on purpose.

### Secrets: absent, and that is handled

`build_macos` reads eight secrets, `build_windows` three, `build_linux` four. None of them is required: the workflows derive flags from whether the secret is empty and skip the step.

```
HAS_SIGN_CERTIFICATE: ${{ secrets.MAC_SIGN_CERTIFICATE_ENCRYPT_SECRET != '' }}
...
DO_SIGN='false'
if [ "$HAS_SIGN_CERTIFICATE" == 'true' ]; then DO_SIGN='true'
else echo "::warning::MAC_SIGN_CERTIFICATE_ENCRYPT_SECRET is empty; code signing disabled"
```

A fork build is therefore **unsigned and un-notarised**, with warnings rather than failures. That matches what a local build produces anyway, and the macOS packaging step re-signs ad hoc after thinning to arm64.

Publishing is avoided by passing `publish: 'off'`, which is what the fork workflows do. Do not turn it on: it would need the FTP and OSUOSL credentials, which a fork has no business holding.

### What does misbehave

**Pull requests opened inside the fork** pull in the whole upstream matrix, `check_unit_tests`, `check_visual_tests`, codestyle, submodules and the three builds. On `4.7-tmp` both test workflows fail, for reasons belonging to that line rather than to the fork. If a line is not meant to pass the upstream suites, do not open a pull request for it inside the fork; push the branch and let `fork_builds.yml` do the work.

**`check_submodules`** insists the pinned `muse` commit exists upstream. `5.0-tmp` pins a commit that lives only on the framework fork, so this check would fail. It only runs on `pull_request`, so pushing a line never triggers it, and the fork workflows sidestep the problem entirely by passing `framework_repo` and `framework_ref` instead of changing `.gitmodules`.

**The Windows portable job on the 4.7 line** builds fine and then dies signing. It uploads to `s3://muse-sign`, a service only MuseScore holds credentials for, and unlike the macOS signing it never checks whether the secret is empty. Worse, it cannot be switched off from the caller: its condition reads

```
github.event_name != 'pull_request' &&
(github.event_name != 'workflow_dispatch' || contains(inputs.platforms, 'windows_portable'))
```

so under `push` the first branch of the or is already true and `platforms` is ignored. The 5.0 line tests the input itself and skips the job properly. The fork workflow for 4.7 therefore builds Windows only on `workflow_dispatch`, where the filter does apply, and its packaging step tolerates the missing artifact. This is one of the reasons the two workflow files are separate rather than shared.

**`triage_issues` and `triage_prs`** act on labels and use `pull_request_target`. Both are repository guarded and do nothing on a fork.

### Configuration worth setting on the fork

- Leave Actions enabled; they already are.
- Do not add the signing, Transifex, Sentry or FTP secrets. The builds degrade cleanly without them, and holding them on a fork is a liability with no upside.
- Keep the fork's default branch as `main`: schedules only fire from the default branch, and upstream's guards are written against that.
- If Actions minutes become a concern, the first thing to cut is the `push` trigger in `fork_builds.yml`, leaving `workflow_dispatch`. A rebuild is often pushed several times while a conflict is being sorted out, and three platforms on each is the waste.
