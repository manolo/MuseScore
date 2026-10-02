# PlectraScore

A MuseScore fork for pulso y púa ensembles. Bandurria, laúd and guitar, in the Spanish and Latin American tradition.

It is a fork of [MuseScore Studio](https://github.com/musescore/MuseScore), and it stays one: the same file formats, the same plugins, the same styles and the same settings directory, so a score moves between the two without noticing. What changes is that the things a plectrum ensemble needs are already in it, instead of waiting in a pull request.

## Why it exists

Bandurria, laúd and guitar ensembles, the tuna and rondalla tradition and its Latin American relatives, are a small enough audience that notation software never quite gets to them. Their repertoire sits in file formats nobody reads any more, their instruments play techniques that general purpose playback approximates badly, and the fixes for both are the kind of change that is correct, narrow and of no interest to a maintainer with a thousand other issues open.

So the fixes get written, offered upstream, and wait. This is where they run in the meantime.

Everything here is offered upstream first and carried here second. When one is merged it leaves this fork, because at that point MuseScore does it and the fork should not.

## What this line adds

This line follows MuseScore's `main`, so it is a preview of the next major version with the fork's work merged on top.


## What the installer brings with it

Installing this is meant to leave the machine ready to write for a rondalla, so the pieces that would otherwise have to be hunted down one by one are already inside:

- **Bandurria and laúd soundfonts**, with the tremolo split by velocity, so a tremolo roll sounds like a tremolo roll and not like repeated notes.
- **The Plectro VST**, found by the program without installing it into the system plugin folder.
- **Pulso y Púa**, the plugin for the writing this repertoire actually needs.
- **Lyrics and Chords Extractor**, for getting the words and the chords out of a score.

Each is taken from where it is published and the exact version is listed in `fork/CONTENTS.md`, which is also where the release notes get it from. The Linux build for arm64 ships without the VST, because there is no arm64 build of it published yet; everything else is there.

### Sound and playback

Plucked strings are where most of the work goes, because this is where a general purpose notation program has least reason to care and a plectrum ensemble has most.

- Tremolo on a single string instrument maps to the sampled tremolo preset instead of being synthesised as repeated notes, which is what makes a bandurria tremolo sound like a bandurria.
- Articulations reach VST3 instruments as keyswitches, so a library that distinguishes legato, staccato or tremolo is actually told which one to play.
- A layered articulation no longer loses the written dynamic when it carries no expression pattern of its own.

### Tablature

- Fret numbers for half notes are circled, the convention tablature uses to mark them and which MuseScore does not draw.

### Encore import

A native importer for Encore `.enc` files, the format a great deal of Spanish and Latin American plectrum repertoire exists in and nothing else reads.

It covers Encore 2.x through 5.x including the encrypted variant, MusicTime documents, tablature staves, lyrics, chord symbols, fretboard diagrams, page setup and voice merging, with import preferences for the decisions that have no single right answer.

### Scores with many parts

- Excerpts are created on demand rather than all at once, which cuts memory and file size on scores with many parts and fixes part renames not surviving a save. A rondalla score with a part per instrument is exactly the case this was written for.

### Plugin API

- `scoreStateChanged` is available again, so plugins can react to the score being edited.

## Where each piece comes from


Line `5.0`, rebuilt from `origin/main` at 9140c5e4b7.

| Component | Pull request |
|---|---|
| Encore importer | #34129 |
| tablature fret circle | #31200 |
| uninitialised excerpts | #31738 |
| plugin api scoreStateChanged | #34893 |
| tremolo articulation mapping | left out, see the manifest |

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
