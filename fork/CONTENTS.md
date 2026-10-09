# What this build contains

Line `5.0`, rebuilt from `origin/main` at b15675f2d5.

| Component | Pull request |
|---|---|
| Encore importer | #34129 |
| tablature fret circle | #31200 |
| uninitialised excerpts | #31738 |
| plugin api scoreStateChanged | #34893 |
| 5.0 ports and fork features | not a pull request |
| tremolo articulation mapping | left out, see the manifest |
| layered articulation dynamics (framework) | muse_framework#179 |
| VST3 keyswitch articulations (framework) | muse_framework#183 |
| tremolo strings preset (framework) | muse_framework#263 |
| dialog lost from the interactive stack (framework) | muse_framework#334 |
| version 1 plugins never receive run (framework) | muse_framework#347 |
| floating mixer panel (framework) | not a pull request |

Built from the manifest by `fork/rebuild.sh`. Editing this file by
hand achieves nothing: the next rebuild overwrites it.

## Bundled with the installer

| Extra | Taken from | Licence |
|---|---|---|
| Plectro VST, free edition | manolo/vst-plectro v0.2.4 | MIT |
| Lyrics and Chords Extractor | manolo/lyrics-extractor v2.2.0 | GPL-3.0-or-later |
| Bandurria with tremolo | manolo/sound-fonts@1fddc2ef3 | CC0-1.0 |
| Laud with tremolo | manolo/sound-fonts@169ad18bc | CC0-1.0 |
| Pulso y Pua | manolo/sound-fonts@2a63a08be | CC0-1.0 |
