# PlectraScore

A MuseScore fork for pulso y púa ensembles. Bandurria, laúd and guitar, in the Spanish and Latin American tradition.

It is a fork of [MuseScore Studio](https://github.com/musescore/MuseScore), and it stays one: the same file formats, the same plugins, the same styles and the same settings directory, so a score moves between the two without noticing. What changes is that the things a plectrum ensemble needs are already in it, instead of waiting in a pull request.

## Why it exists

Bandurria, laúd and guitar ensembles, the tuna and rondalla tradition and its Latin American relatives, are a small enough audience that notation software never quite gets to them. Their repertoire sits in file formats nobody reads any more, their instruments play techniques that general purpose playback approximates badly, and the fixes for both are the kind of change that is correct, narrow and of no interest to a maintainer with a thousand other issues open.

So the fixes get written, offered upstream, and wait. This is where they run in the meantime.

Everything here is offered upstream first and carried here second. When one is merged it leaves this fork, because at that point MuseScore does it and the fork should not.

## What this line adds

This line follows MuseScore's `4.7`, the current stable series. Most of what it carries is the same work as the 5.0 line, adapted to the older API, plus things that only ever existed here.


## What the installer brings with it

Installing this is meant to leave the machine ready to write for a rondalla, so the pieces that would otherwise have to be hunted down one by one are already inside:

- **Bandurria and laúd soundfonts**, with the tremolo split by velocity, so a tremolo roll sounds like a tremolo roll and not like repeated notes.
- **The Plectro VST**, found by the program without installing it into the system plugin folder.
- **Pulso y Púa**, the plugin for the writing this repertoire actually needs.
- **Lyrics and Chords Extractor**, for getting the words and the chords out of a score.

Each is taken from where it is published and the exact version is listed in `fork/CONTENTS.md`, which is also where the release notes get it from. The Linux build for arm64 ships without the VST, because there is no arm64 build of it published yet; everything else is there.

### Sound and playback

- Tremolo plays through the tremolo channel rather than as repeated notes, gated on the `tremoloUnmeasuredMinStrokes` style so that measured tremolo is left alone.
- VST events in offline export no longer carry a double counted buffer offset, which was pushing notes late in exported audio while live playback was fine.
- Each part keeps its own mixer volume and balance, saved in the score, so a part can be balanced for rehearsal without touching the full score.
- The playback cursor can be hidden from the playback settings menu.
- A floating mixer panel fits its channels and the screen it is on, and a double click on its title toggles full width.

### Tablature

- Fret numbers for half notes are circled.
- Fret assignment understands re-entrant tunings, so an instrument whose strings are not in ascending pitch order gets playable fingerings instead of arbitrary ones.

### Encore import

The same native `.enc` importer as the 5.0 line, adapted to this API.

Tuplets read by the Power Tab, MIDI, Overture and TablEdit importers get their position set, which the 4.7 fix for #33366 left out.

### Plugin API

This is where the 4.7 line is furthest ahead of anything upstream, because none of it exists as a pull request:

- Parts and excerpts: `createExcerptFromPart`, `duplicateExcerpt`, `openExcerpt`, `removeExcerpt`, `resetExcerpt`, `addLinkedStaff`, `removeStaff`, `Staff.isTabStaff`, `Staff.linkedStavesCount`, `Staff.setShow`, `resetTextStyleOverrides`.
- The mixer: `Part.mixerChannel` gives a plugin the volume, balance, mute, solo and sound of each part, and `Score.masterScore` reaches every part from inside an excerpt.
- `Score.setStaffVisible()`.
- `loadStyle()` and `setGraceNote()`.
- Shortcuts keep reaching the score while an extension's own dialog is open.
- An API dump dialog, for finding out what is actually callable.
- QML plugins reload without restarting the program, which turns plugin development from a cycle of restarts into editing a file.

### Engraving and command line

- A `lyricsWordSpacing` style, for lyrics under dense plectrum writing.
- `--style-parts`, to apply a style to parts only when converting from the command line.

## Where each piece comes from


Line `4.7`, rebuilt from `origin/4.7` at 66dedfd8f6.

| Component | Pull request |
|---|---|
| 4.7 ports and fork features | not a pull request |

Generated from the manifest by `fork/rebuild.sh`.

## Installing

Builds for macOS, Windows and Linux are published as [releases](github.com/manolo/MuseScore/releases). They are unsigned, because a fork holds no Apple or Microsoft certificate: macOS will refuse the first launch, so open it once from the context menu and choose Open, and Windows will show a SmartScreen warning.

PlectraScore shares its scores, plugins, styles and preferences with MuseScore on the same machine. It is a MuseScore that calls itself something else, not a separate program, and installing it changes nothing about an existing MuseScore.

## Building, and everything else

Unchanged from upstream: [MuseScore's README](https://github.com/musescore/MuseScore/blob/master/README.md) is the reference, and is not copied here because a copy goes stale.

How the fork is maintained, what each line carries and how a release is cut: the `fork/tools` branch, starting at `fork/README.md`.

## Licence

GPL-3.0-only, the same as MuseScore Studio. See [LICENSE.txt](LICENSE.txt).

MuseScore is a trademark of MuseScore Limited and is used here only to say what this is based on. The name is not licensed with the code, and this build claims no endorsement.
