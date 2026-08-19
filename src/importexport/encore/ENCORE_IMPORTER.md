# The Encore importer

This document describes how MuseScore turns an Encore file into a score. The bytes themselves are described in [ENCORE_FORMAT.md](ENCORE_FORMAT.md), and nothing here repeats them: where a decision depends on a field, the field is named and the section of that document is cited. It is written to be read start to finish by a person who has to change the importer, and to explain why each decision is what it is, because most of them were forced by a real file that broke.

## How to read this document

Chapters 1 and 2 are the ones to read first: they describe the two layers and the path a file takes through them. After that the order follows the score being built, from the staves down to the marks attached to a note, so a reader looking for one subject can jump straight to its chapter.

Two shorter documents sit beside these and are not repeated here either: [ENCORE_CORPUS.md](ENCORE_CORPUS.md) counts what real files are, which programs wrote them and when, and answers the questions the encrypted container raises; [ENCORE_READERS.md](ENCORE_READERS.md) covers how a reader is chosen per generation, what has been proven about that choice, and where the reading is still untested.

Paths are relative to `src/importexport/encore`. A name in code font is a symbol in that tree unless it is said to be a MuseScore one. Encore's own field names are the ones ENCORE_FORMAT.md uses.

Two words recur and are worth fixing here. A **generation** is a version of the format, named by the offset its header ends at: v0xA6 for Encore 2.x, v0xC2 for Encore 3.x and 4.x, v0xC4 for Encore 5.x, with SCO5 as the big-endian Macintosh container of the last one. A **track key** is the pair of staff index and voice that the importer accumulates time against, and it is the unit almost every per-voice rule works on.

---

# 1. The shape of the importer

## 1.1 Two layers

The importer is two layers with a data boundary between them.

The **parser** reads bytes and produces a tree of plain structs rooted at `EncRoot`. It knows every generation and every quirk of the binary, and it owns all of that knowledge.

The **importer** walks that tree and emits MuseScore engraving elements. It knows nothing about which generation produced the data, and it never reads a byte.

Neither layer reaches into the other. The boundary is what makes a new generation a contained change, and it is worth defending: a format check that leaks into an emitter has to be repeated in every emitter that follows.

## 1.2 The source tree

```
src/importexport/encore/
├── enc-module.{h,cpp}            Entry point: registers the module and the enc/mus readers
├── internal/
│   ├── notationencreader.{h,cpp} INotationReader adapter; calls importEncore()
│   │
│   ├── parser/                   LAYER 1, binary bytes to an EncRoot tree
│   │   ├── elem*.h               Parsed data structs: EncRoot, EncNote, EncOrnament, ...
│   │   ├── parsers-*.cpp         Per-block and per-element parsers
│   │   ├── parsers-encoding.*    Text encoding probe, Latin-1 against UTF-16 LE
│   │   ├── readers.{h,cpp}       EncFormatReader base and dispatch; findNextKnownMagic
│   │   ├── readers-v0x*.{h,cpp}  Per-generation readers: v0xC4, v0xC2, v0xA6
│   │   ├── zbot.{h,cpp}          Stream cipher for the encrypted containers
│   │   ├── zbot_table.cpp        The substitution table the cipher reads, and its provenance
│   │   └── ticks.{h,cpp}         Raw tick table, face value against ticks, implied-tuplet probe
│   │
│   └── importer/                 LAYER 2, an EncRoot tree to the MuseScore DOM
│       ├── import.{h,cpp}        importEncore() and buildScore(), the top-level order
│       ├── import-options.h      EncImportOptions, the user-configurable flags
│       ├── ctx.h                 BuildCtx, the state every pass shares
│       ├── builders-*.{h,cpp}    Score, part and measure setup
│       ├── emitters-*.{h,cpp}    Per-type emitters: notes, rests, ornaments, chords, fills
│       ├── mappers-*.{h,cpp}     Encore to MuseScore type conversions
│       ├── resolvers-*.{h,cpp}   Post-passes for deferred links: slurs, hairpins, ottavas
│       ├── durations.{h,cpp}     Duration, dot and tuplet derivation from raw ticks
│       ├── coords.{h,cpp}        Tick and beat-position arithmetic
│       ├── page-layout.{h,cpp}   Page size, margins, system locks, page breaks
│       └── debug-dump.{h,cpp}    Optional diagnostic dump of the parsed tree
│
└── tests/
    └── tst_*.cpp                 Per-feature tests
```

## 1.3 The path a file takes

```
.enc or .mus file
    │
    ▼  zbot.cpp, when the magic is an encrypted one
plain bytes
    │
    ▼  readers-v0x*.cpp and parsers-*.cpp
EncRoot: EncInstrument[], EncLine[], EncMeasure[], EncTitle
    │
    ▼  import.cpp, importEncore() then buildScore()
    ├── buildParts()              parts, staves, instruments
    ├── buildMeasures()           empty measure frames
    ├── buildInitialSignatures()  clef, key and time signature
    └── emitMeasures()            per measure, then reconcileMeasureLength()
    │
    ▼  resolveAll()
deferred cross-element links: slurs, hairpins, ottavas, ornaments
    │
    ▼  MIDI mapping, layout, optional voice merge, first-page fit, repeat list
MasterScore
```

Two things about that order matter downstream. Layout runs before the repeat list is invalidated, and the voice merge runs after the first layout, because it needs real chords to look at.

## 1.4 Fat parse, thin import

All format-specific interpretation is resolved before `EncRoot` reaches the importer. `postProcessElement` in each `EncFormatReader` subclass is the single hook where a raw binary quirk becomes a semantic field. The importer then uses the semantic field and never asks which generation it came from.

| Quirk                            | Raw encoding                  | Normalized field                | Where                                  |
|----------------------------------|-------------------------------|---------------------------------|----------------------------------------|
| the note's own tie flag          | `grace1` bit 0                | `EncNote::isTieSender`          | the base reader, every generation      |
| the older articulation numbering | ORN subtype six codes higher  | the shared subtype vocabulary   | `normalizeOrnamentSubtype`, below 3.07 |
| the two TEMPO layouts            | BPM at `+28` or at `+30`      | `EncOrnament::tempo` and `noto` | the v0xC2 reader                       |
| which forward count to trust     | a count on any ornament       | `EncOrnament::alMezuroValid`    | the v0xC2 reader, true on a slur start |

The note's own tie flag is decoded by the base `EncFormatReader::postProcessElement`, so every reader inherits it and a format-specific override calls the base first. It used to be decoded only for v0xC2, which left the same flag unread in the other three generations. It is a second record of a tie that usually has a TIE element as well, so on real files it rarely changes the outcome; it matters for the notes where that element is missing.

## 1.5 Adding a generation

A new generation needs a new `EncFormatReader` subclass carrying whatever offsets and quirks it overrides, and a `calculateRealDurations` phase when its tuplet semantics differ. Nothing in the importer layer should need to change.

The same discipline applies to per-format behaviour in general. It belongs in a virtual on the reader, never in an inline magic check at the point of use, because an inline check has to be found and repeated every time a generation is added.

---

# 2. Reading the file

## 2.1 Block dispatch and resync

The top-level loop in `parsers-root.cpp` reads block magics and dispatches per type. Unknown bytes between two known magics are skipped by `findNextKnownMagic`, which scans byte by byte until the next recognised magic appears.

The scan is capped at a 1 MiB window. The largest legitimate Encore block is around 2 KiB, so a longer gap of junk means a corrupt file, and the loop stops rather than walking the whole payload. Without the cap a corrupt file drives a scan of hundreds of megabytes.

## 2.2 The encrypted container

Old Encore releases save into an encrypted container, magic `ZBOT`, `ZBOP` or `ZBO6`. ENCORE_FORMAT.md §2.3 describes the wrapper. Encryption is a layer, not a format: under it sits an ordinary document that everything else in this importer already handles.

`importEncore` reads the whole file, tests the first four bytes with `isZbotMagic`, and calls `zbotDecrypt` on the buffer before the reader sees anything. Below that call no layer knows the file was ever encrypted.

The cipher lives in `parser/zbot.cpp` and nothing else. It walks the buffer one byte at a time and exclusive-ors each with a byte of the keystream. Two tables drive it. `kTableA` holds 17 jump deltas and is cycled once per byte. The substitution table holds 39104 bytes, addressed as 9776 rows of four, and the row advances once every four bytes while the column cycles within the row. The starting row is 0xAB.

The substitution table lives apart, in `parser/zbot_table.cpp`, with its provenance recorded there. It is stored packed as nibbles, which halves it, plus a short list of nine entries whose true value exceeds a nibble and is patched in. `tableFlat` expands the two into the flat table once, on first use, behind a `std::call_once`.

`ZBOP` is the one container with no sample in the corpus, so it is implemented from the keystream alone. The risk is contained: a decrypted buffer still has to pass the magic and header check, and a file that fails it is reported rather than imported as wrong music.

## 2.3 MusicTime documents

MusicTime was the smaller and cheaper sibling of Encore from the same publisher, and it writes the very same format under its own magic. Its files reach the same readers.

`isReadableEncoreMagic` accepts `MTIW` and `MTIM` beside `SCOW` and `SCO5`. `EncHeader::readMagicAndVersion` treats `MTIM` as big-endian the way it treats `SCO5`. `EncFormatReader::create` matches both by magic, because a Macintosh container has no version byte at `0x04`. Everything below that is unchanged, and format 2.62 selects the compact geometry through the path a format 2.50 file takes.

The module registers the reader for `mus` beside `enc`, so the open dialog lists both under the Encore filter.

Of the nine distinct MusicTime documents to hand, eight import clean. The ninth, a 6/8 tutorial score on three staves, comes out with six bars split into a 1/8 and a 7/8 measure. Compound meters are where this generation states a face value in beats rather than as an absolute note value, so that is the first place to look.

## 2.4 Text encoding probes

Every text-bearing path probes its payload, so modern UTF-16 LE files and legacy Latin-1 files both decode without a manual hint.

| Site                         | Function              | Probe                                        |
|------------------------------|-----------------------|----------------------------------------------|
| TK block instrument name     | `EncInstrument::read` | printable then NUL means UTF-16, else Latin-1 |
| TK name recovery             | `EncRoot::read`       | same as the TK name                          |
| LYRIC element                | `EncLyric::read`      | bytes 0 and 1 at the payload start           |
| TEXT block entry             | `EncTextBlock::read`  | bytes 14 and 15; `0x04 0x00` is a line break |
| CHORD-symbol text            | `EncChordSym::read`   | bytes 0 and 1 of the 36-byte slot            |
| TITL block                   | `EncTitle::read`      | varsize below 5000 is 1-byte, 10000 or more is 2-byte |

Every probe runs in both directions. Forcing one encoding is not a safe simplification: reading a Latin-1 payload as UTF-16 pairs adjacent bytes into Chinese-looking gibberish, and reading a UTF-16 payload as Latin-1 silently drops half of every character.

The TEXT block deserves its own note, because it carries the payload of every staff-text ornament. The decoded text runs from offset 14 up to the first double null, not up to the first `0x04 0x00`. That value is a line separator inside a multi-line comment, and it follows every line including the last, so stopping at it truncates a multi-line comment to its first line. The reader decodes the whole region, truncates at the null, turns each separator into a newline and drops the trailing one. Padding after the null is ignored. `Tst_Text.staff_text_multiline_preserved` covers it.

## 2.5 What the file says it holds

Three cases where the file offers more than it means.

**Ghost measures.** Encore 5 sometimes leaves trailing MEAS blocks from edits the user truncated. The header `measureCount` at `0x34` is authoritative and reflects what Encore displays, so `EncRoot::read` stops appending once that many measures are read. Without the cap, a file with a rendered count of 36 and 56 blocks on disk imports as 56 measures, the last 20 of them stale.

**Several TEXT blocks.** A multi-part file writes one per part view. They hold the same strings in a different order and count, and the `tind` index on an ornament is relative to the first one only. `parsers-root.cpp` therefore keeps the first non-empty block and ignores the rest. Overwriting with the last block resolved every index against a reordered table, so staff text came out wrong and some marks vanished. `Tst_Text.staff_text_uses_first_text_block` covers it.

**Duplicate TITL blocks.** Some files save the title block twice. `EncTitle::read` clears its slot vectors at the start of every pass, so the second block replaces the first instead of appending to it and doubling every line.

## 2.6 When a file cannot be read

`encoreLoadErrorMessage` produces what the user sees. It reads a short head of the file and compares it against the opening signatures of the few other programs whose documents arrive under these extensions, Finale among them, since `.mus` was never Encore's alone.

It then says one of two things. When a signature matches, the message names that program and suggests exporting from it as MusicXML, which is the only route that leads anywhere. Otherwise it says the file is not a recognisable Encore document and suggests re-saving it from Encore, which also covers a truncated or empty file.

The rule the message rests on is ENCORE_FORMAT.md §1.2: the magic is the whole test. There is no fallback signature and no recovery, because the byte order, the header layout and the position of the first block all follow from it.

## 2.7 The v0xC2 instrument table without TK blocks

Some v0xC2 files carry no TK blocks and store instrument names and MIDI programs in a linear table of 112 bytes per entry. Two sub-layouts exist, described in ENCORE_FORMAT.md §5.1. `readers-v0xc4-base.cpp` detects which applies, in `readMidiProgramsNoTk` and `recoverMissingNames`.

The detection runs in this order.

1. `findTildeBlockOffset` decides the variant. A valid offset means variant A, otherwise variant B.
2. Variant B reads names at `NAME_BASE + n * 112` and MIDI at `262 + n * 112`, with no further checks.
3. Variant A first tries the TK-style stride of 2158 for names, then falls back to `314 + k * 112` for the ones still unnamed, and reads MIDI at `374 + k * 112` for instruments with no primary block. An instrument that does have one, detected by probing `202 + n * 2158` for printable ASCII, reads its MIDI 60 bytes into that block instead.
4. When `data[390]` is at least 1 and sits before the first block, MIDI comes from `390 + n * 276`, the compact v0xC4 layout.

v0xC4 files have their own two quirks in the same area. Encore 5.0.2 always writes instrument names as UTF-16, even at a TK offset where the older releases would use one byte per character, so the probe is skipped and UTF-16 assumed. And that release occasionally omits a TK header altogether, in which case the name is recovered by scanning back from the formula-derived program offset.

Two details of the name path belong here rather than with instrument matching. Trailing punctuation is stripped when building word-level needles, so an abbreviation reaches the template its full name would. And `findTemplateByMidi` inspects only the first channel of each template, because tremolo and secondary channels otherwise pull an instrument to a wrong program, for instance an acoustic bass whose channel 44 collides with a main channel 32 elsewhere.
---

# 3. The score skeleton

## 3.1 Choosing an instrument

`findEncoreInstrumentTemplate`, in `mappers-instruments.cpp`, scores every non-drumset template against the Encore instrument, combining its name and its MIDI program into one number. The comparison ignores diacritics, so a Spanish name matches its accented template. A template whose track name contains the needle gains two points, so a truncated part label still reaches the right family. A template that carries the file's MIDI program on any channel gains a bonus, which is what pulls a plucked bass away from the choral Bass voice. Where two templates share a program, a genre tiebreaker prefers the everyday one over the specialised variant.

Scoring alone is not enough, because Encore percussion tracks always report MIDI program 1 and a strict lookup would send every drum part to Grand Piano. `applyBestInstrument` therefore runs an ordered chain, and names the step that matched in the import log.

1. **Percussion clef.** A first staff carrying `EncClefType::PERC` in the LINE block goes to the drumset template unconditionally. This runs before any name or MIDI inspection, it is language-agnostic, and it produces no false positives.
2. **GM percussive range.** A MIDI program of 113 or above is the General MIDI percussive section, from agogo to synth drum, and also routes to the drumset template. Encore files often name these parts after a performer or a catalogue number, which matches no template at all, so without this step they reach the Grand Piano fallback with their program ignored. `Tst_Instruments.gm_perc_range_midi_program_routes_to_drumset` covers it.
3. **Name and MIDI scoring** over non-drumset templates. This is the main path, and it is not restricted to pitched instruments, so a correctly named drum part still passes through it when the clef check was inconclusive.
4. **Name scoring over drumset templates**, in `findDrumsetTemplate`, with the same rules restricted to templates that use a drumset. MuseScore's own localized template names drive the match, so no keyword list is needed and any interface language works.
5. **A generic percussion keyword** in the name, for the names the localized scoring did not reach.
6. **A rhythm staff**, which takes the snare drum template. The MIDI step is skipped for it, so program 0 cannot pull it back to Grand Piano.
7. **MIDI program lookup**, for any instrument with a non-zero program that nothing above matched. This is the only signal when the name is absent, so it has no name-length gate.
8. **Nearest template in the same GM family**, which catches the programs no template claims as its primary sound, such as pizzicato strings or muted trumpet, and keeps the part in its category instead of collapsing it to Grand Piano.

**Short names are not scored.** A name shorter than four characters skips both scoring steps. These are the SATB choir labels and their translations, and with a needle that short the substring rule matches almost any template that happens to contain the letter: `S` lands on Bass Clarinet and `C` on Piccolo. The keyword and MIDI steps still run, because when the name is empty the program is the only signal left and suppressing it would send every unnamed instrument to Grand Piano.

The chain falls through to Grand Piano only when name and MIDI both give nothing. The original label is kept, so the user can reassign from the instrument browser.

## 3.2 The names the user sees

Once a template is chosen, the part's long name is set to the Encore instrument name and the short name is cleared.

A few generic templates in `instruments.xml`, among them recorder, clarinet, trumpet and double bass, carry no track name of their own, and MuseScore keeps exactly those out of the instrument list it builds for the interface. A part assigned to one of them shows a blank instrument in the staff properties dialog and cannot be found in the Instruments panel.

`resolveListedTemplate` swaps such a template for the named sibling that stands for it: same family, same primary MIDI program, same written range and same transposition, which identifies one candidate and no more. Three GM programs reach a generic through the ranking, and they resolve to Soprano Recorder, 10-Hole Diatonic Harmonica and Prima Balalaika.

When no sibling matches, the template is kept and a track name is derived from its id, so `bass-clarinet` becomes `Bass Clarinet` and the mixer is not left blank. The Encore name stays as the part long name and is never copied into the track name, so the track name always reflects the instrument that will play rather than the user's part label.

**Template brackets are cleared.** `Staff::init` copies bracket data from the template, which in a multi-instrument score produces braces spanning unrelated parts, so the importer clears brackets and spans on every staff afterwards.

## 3.3 Percussion staves

An instrument routed to a drumset needs its clef forced as well. The LINE block clef for such a staff is often a C or F clef in band files, and it would override the percussion clef, so `buildInitialSignatures` checks whether the staff carries a drumset and writes `ClefType::PERC` instead of asking `pickStaffClef`.

## 3.4 Tablature staves

Tablature is decided per staff, from `clef == TAB` or `staffType == TAB` in the LINE block.

`setupTablatureStaff`, called from `buildParts`, attaches `StringData` to the part instrument and a matching TAB staff type, with one line per string and the full variants for four, five and six strings. Notes on the staff are then fretted by the layout itself. The tuning comes from Encore's tab-tuning array, described in ENCORE_FORMAT.md §5.3, and falls back to a matched fretted template's `StringData`, then to a standard six-string guitar.

An Encore tab staff is a derived view with no notes of its own: its element stream holds only rests. A post-pass, `applyTablatureImportMode`, supplies the notes according to the option the user chose.

- **Linked**, the shipped default. Each empty tab staff is paired with the notation staff immediately above it, which is how Encore stores the pair, and the two are merged into one instrument. `Excerpt::cloneStaff` clones the notation music into the tab staff as linked clones, so the same notes render as frets, the tab staff is reparented into the notation part and the empty tab part is dropped. Per-staff visibility is applied from Encore's show flags, so a hidden notation staff behind a visible tab survives, which a merged part could not express as a whole.
- **Separate.** The staves stay as Encore stores them, notation with notes and tab as an empty view, each its own instrument.
- **Ignore.** Tab staves are removed with `cmdRemovePart`.

A tab-only score has no notation staff to pair with. Its tab staff carries its own notes as pitch-bearing rest elements, which the parser reads as notes, so the standalone tab shows fret numbers.

## 3.5 Per-instrument transposition and clef

Encore's Staff Sheet has a per-instrument Key dropdown that adds a chromatic transposition at playback time. The value is a signed count of semitones, and ENCORE_FORMAT.md §5.1 gives its position.

For regular TK files it sits 23 bytes before the MIDI program byte in the same fixed-offset table, and `EncRoot::read` fills `EncInstrument::keyTransposeSemitones` next to the program read. Compact TK files do not follow that layout at all, so the lookup is skipped for them: the formula would read unrelated bytes and any non-zero result would shift every pitch on the staff. On regular files a sanity bound of -33 to +24, Encore's own interface range, catches the cases where the offset lands on something else.

v0xA6 stores the same field in a different place, at TK content offset +42 inside its 64-byte blocks. Two adjustments follow. `EncHeader::read` must end at `0xA6` for these files, because reading to `0xC2` consumes the first TK block and shifts every per-instrument field by one slot. And the per-instrument loop reads the byte from the in-flight TK content before delegating to `EncInstrument::read`, storing it in the same field the other generations use.

Encore writes the **written** staff position into `EncNote::semiTonePitch` and shifts the sounding pitch by the Key at playback. MuseScore plays `Note::m_pitch` directly. The importer therefore captures one offset per staff and adds it to every note pitch, at both `applyConcertPitch` call sites, the regular notes and the grace notes.

The staff clef then carries the visual half of the same idea, in `pickStaffClef`. It is derived from the Encore clef and the Key offset alone, with no need for a matched template.

| Encore clef | Key in semitones     | MuseScore clef                      |
|-------------|----------------------|-------------------------------------|
| G           | -12                  | G8_VB                               |
| G           | +12                  | G8_VA                               |
| G           | -24                  | G15_MB                              |
| G           | +24                  | G15_MA                              |
| F           | -12                  | F8_VB                               |
| F           | +12                  | F_8VA                               |
| F           | -24                  | F15_MB                              |
| F           | +24                  | F_15MA                              |
| any         | 0                    | the Encore clef                     |
| any         | not a whole octave   | the Encore clef, the notes shift    |

The rule is one sentence: when the offset is a whole number of octaves, look for a clef in the same glyph family whose octave offset equals it, and use it if there is one. C clefs, percussion and tablature have no octave variants and always keep the Encore clef.

## 3.6 Staff scale

The header byte at `0x52` holds a staff-size selector from 1 to 4, defaulting to 4. `applyStaffScale` maps it to `Pid::MAG` on every staff before the resolvers run, and leaves the global spatium alone.

| Header value | Staff scale |
|--------------|-------------|
| 1            | 60%         |
| 2            | 75%         |
| 3            | 100%        |
| 4            | 130%        |

## 3.7 Page size and margins

Margins come from the optional WINI block. ENCORE_FORMAT.md §5.8 describes its layout, its two varsize forms, the two unit variants and the rounding quirks; only the score-side behaviour belongs here.

`applyPageMargins`, in `page-layout.cpp`, reads the parsed `EncPageSetup`. The screen-pixel variant does not store the paper size, so `detectWiniPageSize` matches the recovered width against the standard sizes, ISO A-series first and then Letter, Legal and the B-series. On a match it sets the page width and height before the margins are computed, so the right and bottom margins derive from the correct paper. With no match the current MuseScore page size is kept.

A file that was never saved through Page Setup has no WINI block. `EncPageSetup::hasData` is then false, `applyPageMargins` does nothing and the MuseScore defaults of 15 mm per side remain.

## 3.8 Page breaks and the first page

A page break is inserted after the last measure of each system where the LINE block's row-on-page counter resets. The last measure of a system comes from the line span, which prefers the per-line measure count and falls back to the gap to the next line's start when that count is absent, so page breaks survive on SCO5 where it is not stored.

After layout the first page gets a second look, in `fitFirstPageStaffSpace`. Encore's own first page may hold one system more than MuseScore fits at the default staff space, and that system then spills onto page two, which is visible and wrong on the very first page the user sees. The pass shrinks the staff space by up to 0.022 inch, in steps of 0.002 inch, until the measure that carries the first page break returns to the first page. The smallest reduction that works is kept, and if even the largest is not enough the original value is restored.

## 3.9 Titles, headers and footers

Each TITL category that reserves several slots, described in ENCORE_FORMAT.md §5.6, can carry one visible line per slot. The importer joins all non-empty slots of a category with newlines and writes the result to the frame text and the score property.

| Category    | Frame text style | Score property |
|-------------|------------------|----------------|
| title       | `TITLE`          | `workTitle`    |
| subtitle    | `SUBTITLE`       | `subtitle`     |
| instruction | `LYRICIST`       | `lyricist`     |
| author      | `COMPOSER`       | `composer`     |
| copyright   | not on the frame | `copyright`    |

A file with three populated author slots becomes one composer text of three lines, which is what Encore's own MusicXML export writes as a single creator with newline separators.

Headers and footers map each non-empty line into both the odd and the even style slot for the same corner, so the text shows on every page whatever the parity.

| Alignment byte +14 | Header style slots          |
|--------------------|-----------------------------|
| `0x02`, right      | `oddHeaderR`, `evenHeaderR` |
| `0x04`, left       | `oddHeaderL`, `evenHeaderL` |
| `0x06`, centre     | `oddHeaderC`, `evenHeaderC` |

Footers use the matching odd and even footer slots. Several slots sharing one alignment join with newlines into a single value and render stacked in that corner; slots with different alignments stay apart.

Encore embeds its own tokens in header and footer text. Left alone they would print literally on every page, so each known one is rewritten before the text reaches the style slot.

| Encore | MuseScore | Meaning                                                   |
|--------|-----------|-----------------------------------------------------------|
| `#P`   | `$P`      | page number on every page, page 1 included                |
| `#D`   | `$D`      | creation date                                             |
| `#T`   | `$m`      | time, mapped to last-modification time, the closest macro |

An unknown token is left untouched, so text a user happened to begin with a hash survives as typed.

## 3.10 Barlines

`Measure::setEndBarLineType` takes a track index. Passing a boolean converts to track 0 and leaves a double or final barline visible on the first instrument only, so the importer iterates every staff and the barline appears on every line of the system.

## 3.11 MIDI mapping and the repeat list

Two things the file read path does for a score loaded from disk, which a direct import has to do for itself.

`rebuildMidiMapping` assigns ports and channels to every part. Without it each channel stays at -1, and `Part::midiPort` then indexes the mapping with -1 and crashes on a straight-to-MusicXML export.

`invalidateRepeatList` runs last, after layout. Layout computes and caches the repeat list, and at that moment the voltas may not be anchored yet, so the cached expansion ignores the endings and replays the first one on every pass. Invalidating it means playback right after import is correct.

---

# 4. Measures

## 4.1 One authority for measure length

Every rule in this chapter converges on one function. `reconcileMeasureLength`, in `emitters-fill.cpp`, is called once per finished measure and owns the invariant that every voice on every staff sums to the measure length. Nothing else may adjust a measure's duration.

It runs five steps, and the order is load-bearing.

1. `adjustPickupMeasure`, the pickup shorten described below.
2. `fillTrailingGaps`, which pads a short voice or shrinks the measure to its content.
3. `Measure::checkMeasure` per staff, MuseScore's own fill of the voices that are entirely empty.
4. `correctMeasureLength`, the small-delta correction.
5. `fitOverfullMeasure`, the overfull strategy.

MuseScore's own check has to sit between the underfull fill and the corrections, because it assumes the voices it inspects are already coherent.

## 4.2 Pickup detection

Encore does not flag a pickup measure, so it is inferred from the first one, in two cases.

**Case A, an explicit short time signature.** When measure 0's signature differs from measure 1's, Encore stored a shorter signature for the pickup. Measure 0 is displayed with its own signature, its actual duration is its `durTicks`, and every later measure starts at that value.

**Case B, an implicit underflow.** The two signatures agree, but the placed content of measure 0 is greater than zero and shorter than `durTicks`. Measure 0 then shrinks to what is placed and all later measures shift back by the difference. Any forward-looking spanner endpoint that pointed past the new end of measure 0 is reduced by the same amount.

Case B is skipped when Case A already shortened measure 0, which would otherwise reduce it twice. When the pickup option is off the whole inference is bypassed.

## 4.3 Completeness and tolerance

A measure is valid with fewer ticks than `durTicks` in a voice. Encore does not require every voice to be full, and a great many real files are not.

A voice that falls short is filled with implicit rests, visible or hidden by context, as §5.5 describes. A voice that overshoots is resolved by the overfull strategy, but only after a tolerance of `durTicks / 24`, which is 40 ticks in 4/4, absorbs the rounding-sized overshoots that need no correction at all.

## 4.4 Underfull measures

The trailing gap of a short voice is handled by `underfillMeasureStrategy`: pad it with invisible rests, pad it with visible rests, or shrink the measure's actual duration to its content.

A shrink applies only when every staff is genuinely short. A measure where one staff is silent and the others are full is not an irregular measure, and shrinking it there truncates the full staves.

## 4.5 Overfull measures

Some measures carry more than the time signature allows, either a trailing tuplet that overshoots the barline by a rounding-sized amount or a plain note whose value simply runs past it.

The note loop never cuts such content mid-stream. It lets the voice overshoot for every strategy, and the single post-pass `fitOverfullMeasure`, in `emitters-overfill.cpp`, resolves it. A tuplet is always preserved whole, compressed whole or dissolved whole; a partial tuplet is never produced.

**Remove last notes.** A trailing tuplet that would be cut is dissolved and its members revert to their plain face value, then trailing notes are removed from the right until the content fits. A plain note that begins inside the bar and runs past the barline is not dropped: it is recut to end exactly at the barline, keeping its full value up to that point as a chain of tied figures, so a dotted half stranded in a 5/8 bar becomes a half tied to an eighth. Only a note that begins at or after the barline, with no room at all, is removed outright. The last surviving note is then lengthened by up to three dots and any remainder is filled with an exact rest. The result is always a standard measure.

**Stretch last notes.** All the notes are preserved, in three tiers. Tier 1 reclaims preceding rests: when the overflow can be absorbed by shortening or dropping rests that come before it, those rests are reclaimed and the following notes shift earlier, so the whole voice fits a standard bar at full value. This takes rest space and never note value, and it applies only when the reclaimable rest is at least the overflow and the voice holds no tuplet. It covers the common case of a figure after a beat of rest overrunning the bar. Tier 2 compresses the trailing tuplet's bracket to the largest value that fits, with the base limited to three dots so the notation survives layout, and fills the remainder with an exact rest; a lone trailing note that crosses the barline gets the same tied-chain recut as the previous strategy. Tier 3 is a fallback to the irregular measure for that bar, taken when the compressed bracket would be smaller than half the tuplet's natural span or there is not enough rest to reclaim.

Because Stretch preserves notes, the note loop also keeps the notes that arrive after the voice is already full, rather than dropping them, so tier 1 has something to reclaim rests for. Prefer Remove when a standard bar length is required unconditionally.

**Mark as irregular measure.** The measure's actual duration is extended to hold all the content, which preserves the exact rhythm at the cost of a non-standard bar length. The extended duration is stored in lowest terms: summing triplet content yields an unreduced fraction such as 21/24, the same duration as 7/8 but read as a disproportionate time signature, so it is reduced to its canonical form.

Every fill duration is split into individually notatable figures of up to three dots, so a residual that is not a single note value becomes a tied sequence rather than a duration that cannot be written.

## 4.6 Multi-measure rests

A MEAS block whose lone REST carries a count above 1 expands to that many MuseScore measures. `buildMeasures` and the emitters both compute the count, and both must agree, or the frames and the notes end up in different measures.

Expansion is guarded only against a cascade, by requiring that the predecessor is not itself a single-REST block. It used to be guarded on the successor holding pitched notes as well, which collapsed a legitimate multi-measure rest whenever a rest measure followed it. The successor's content is irrelevant: Encore's own count is authoritative. `Tst_Importer.mrest_single_block_expands_when_successor_is_rest` covers the case.

`Sid::createMultiMeasureRests` is set only when the file actually contains such a block. With no block the flag keeps its MuseScore default of false, so individual rest measures render as individual whole rests, which is what Encore shows.

## 4.7 Time signatures

`buildInitialSignatures` writes a time signature wherever it differs from the previous measure, and the comparison must be `Fraction::identical` rather than `Fraction::operator==`. The equality operator cross-multiplies, so 6/8 equals 3/4, and a score that changes between them has the same tick duration on both sides: the change was silently skipped and no signature appeared. The same holds for 2/2 against 4/4 and 3/8 against 6/16. Two tests in `Tst_Structure` cover the pairs.

The MEAS header byte at `0x02` carries the visual form. `0x43` is written by Encore 3.x and 4.x and `0x63` by Encore 5.x, and both mean common time, so both map to `TimeSigType::FOUR_FOUR`. A zero means the ordinary numeric display. `buildMeasures` fills a tick-to-type map so the change points keep the symbol, and the signature is created with the resolved type so the C survives a round trip through the MuseScore file format.

## 4.8 Repeats, voltas and jumps

`EncMeasure::repeatMark` returns the **low** byte of the four-byte coda field. The earlier accessor took the second byte and silently dropped every jump and marker in every Encore file. `addRepeatMark`, in `mappers-title.cpp`, routes each value to its `Jump` or `Marker`.

Encore distinguishes the measure that sends from the measure that receives with two different bytes: `0x85` is the source and maps to `TOCODA`, `0x89` is the destination and maps to `CODA`. Mapping both to the coda collapsed the pair and drew two coda glyphs where Encore showed "To Coda" and then the sign. The ornament encoding `0xA5` is the parallel form of the same direction and also routes to `TOCODA`.

Encore renders its first coda marker as the sign followed by the word "Coda". That word is Encore's own display convention and is not stored anywhere in the file, not in the TEXT block and not as a staff-text ornament. MuseScore renders the sign alone, which is the standard engraving convention, so the omission is correct.

**Voltas.** Encore marks every measure inside an ending with a bitmask rather than storing a bracket. Importing one volta per measure produces N brackets of one measure each, none of them numbered, because MuseScore reads the visible label from the volta's begin text and not from its endings list.

The importer keeps an active volta across the measure loop. A measure that shares the previous measure's bitmask extends the active volta; a change or a drop to zero closes it and opens a new one at the next non-zero measure. The begin text is built from the endings list so the bracket is labelled.

Encore sometimes sets a bit in a later bracket that an earlier bracket already displayed. `BuildCtx::usedVoltaBits` accumulates everything emitted so far in the current repeat block, and each new bracket filters its mask against it, so a bracket following "1.-3." with raw bits 2 and 4 is labelled "4." and not "2., 4.". The counter resets when the bitmask drops to zero. `Tst_Importer.v0c4_volta_overlapping_bits_filtered` covers it.

**How many times a repeat plays.** Encore stores no pass count. `encRepeatPlayCount`, in `builders-measures.cpp`, derives it from the endings instead: it starts from the bitmask on the end-repeat measure, adds the masks of the measures that follow until a repeat start or a measure with no ending, and takes the highest ending number in the result. A plain repeat with no endings keeps the default of two passes, and only a higher count is written to the measure. A section with three endings therefore plays three times, which is what the brackets say.
---

# 5. Voices and time inside the measure

## 5.1 Which staff and which voice

Encore packs the staff and the voice into one byte, described in ENCORE_FORMAT.md §6.2. The importer reads it through two paths.

**Path A, the high bits of the staff byte.** Grand-staff instruments such as piano, harp and organ set `staffWithin = staffByte >> 6`. All their notes share system staff 0 and the high bits select the staff inside the instrument. The importer adds `staffWithin` to the staff index and subtracts `staffWithin * (VOICES / 2)` from the voice, so voices 0 and 1 stay on the upper staff and voices 2 and 3 become voices 0 and 1 of the lower one. The tie pre-pass applies the same routing, so its keys agree with the note loop.

**Path B, a voice nibble out of range.** A voice of 4 or more maps down to voice 0, and it means one of two things. System-level ornaments, dynamics and technical marks, are written with voice 4 and the `staffWithin` bit set, and they anchor on voice 0 of the target staff. Some v0xC4 choir scores instead carry real bass-staff notes, rests and beams at voice 4 with no valid `staffWithin`, and dropping them empties the bass staff.

Path B is tested first, so a system ornament is never routed to a second instrument staff by the other path.

The rest of the mapping is plain: Encore voices 0 to 3 become MuseScore voices 0 to 3 on the same staff, and voices 5 to 7 collapse to voice 0 of their own staff.

## 5.2 Several streams in one voice

A single Encore voice byte can hold more than one MIDI tick stream. The importer splits the overflow into separate MuseScore voices with a per staff and per Encore voice counter:

```cpp
auto encVoiceKey = std::make_pair(staffIdx, voice);
int msVoice = voice + streamOffset[encVoiceKey];
```

When a non-chord event arrives and the current voice is already full, the counter increases and the event moves to the next voice. One step is not always enough, since a previous rest may have filled that voice too, so the search continues until the event fits or all four voices are exhausted, in which case it is dropped.

**The chord-extension guard.** Two events within `CHORD_MIDI_THRESHOLD`, which is 8 Encore ticks, in the same MuseScore voice are treated as one chord. That is allowed only when the previous event in that voice came from the same Encore voice, tracked per track key, otherwise a spill from one Encore voice would attach itself to a chord belonging to another.

Without the split the second stream silently merges into the first and the importer emits a tick gap of 1/3072 that aborts layout further down.

## 5.3 Overflow and duplicate rests

Once a voice has reached the measure length, further elements carrying the same voice byte are dropped and never promoted to the next voice. Encore stores several MIDI recording passes under one voice byte, and only the first fill is valid notation.

When two out-of-range voice bytes map to the same output voice and both carry an explicit rest at the same tick, the second rest is a no-op and its position is not advanced. Advancing it would shift every later element in the voice.

## 5.4 Collapsing voices that never overlap

The split above, and Encore files that simply notate one line across several voices, often leave a staff with more voices than the music needs. When `mergeVoices` is on, `mergeNonOverlappingVoices` in `import.cpp` collapses such staves back into one voice, after the score is fully built. It mirrors the manual workflow of moving every note to voice 1 and running Implode.

The pass is conservative and works per staff, all or nothing.

A first read-only sweep collects, per staff, the distinct intervals from onset to onset plus duration of every chord in all four voices. A staff qualifies only when it has notes beyond voice 1 and those intervals never overlap. Two notes sharing an onset and a duration count as one interval, since they can become a chord; any other overlap marks the staff as genuinely polyphonic and it is left exactly as imported.

For a qualifying staff the pass moves every note into voice 1, filling that voice's rests and merging simultaneous same-duration notes into chords, then implodes the staff to drop the empty upper voices. Timings never change.

The move uses the generic voice-change editing command, which rebuilds the destination chord and does not carry a single-chord tremolo across. The pass therefore snapshots each staff's tremolos by onset tick before the move and re-attaches the ones that were dropped.

The editing helpers used here record undo steps, so the whole import runs inside a `ScoreLoad` sentinel and opens no undo transaction: each step is performed and freed at once instead of accumulating on the undo stack.

## 5.5 Implicit silence and the gap snap

Encore encodes leading and interior silences implicitly, in the element's absolute tick. Placing every note at the running sum of face values would collapse those silences and shift the rest of the bar earlier, changing the music. The common shape is a 3/4 bar with notes at ticks 240 and 480 and no rest element: the user wrote a quarter rest and two quarters, and a naive placement writes two quarters and a rest.

At the start of the tick computation the importer compares the element's absolute Encore tick against the running sum for its track key. When the difference exceeds `CHORD_MIDI_THRESHOLD`, the running sum snaps forward to the Encore tick, and the per-staff gap pass later inserts the fill rests.

The 8-tick threshold is the same constant the chord-extension test uses, so the two agree: drift inside a chord cluster stays absorbed, and anything beyond it is treated as a silence the user notated. The smallest face value with non-degenerate ticks is the 64th at 15 ticks, so any real silence is comfortably above the threshold.

The snap applies to notes and rests in the non-chord-extension branch only. Chord extensions reuse the previous chord position, and ornaments, ties and other annotations follow their own anchoring.

**The whole-note grid.** The conversion to a fraction of the measure needs the number of Encore ticks in a whole note, from `encWholeNoteTicks`. It is derived from the measure's own fields as `durTicks * timeSigDen / timeSigNum`, and falls back to the constant 960 only when those fields are unusable.

Deriving it from `beatTicks` instead is wrong twice over. A file that stores a non-standard value, for instance 2/2 with `beatTicks` at 240 rather than 480, produces a grid of 480: a note at tick 360 then reads as three quarters of the measure, the snap fires against a running sum of three eighths, and every note in the second half of the bar is dropped. And in x/8 meters the same derivation gives half the correct denominator, so every snap pushes twice as far as intended and the measure overflows. `Tst_Importer.v0c4_2_2_beatticks240_gap_snap_no_false_fire` covers the first case.

The same helper is used for chord-symbol placement, for the same reason.

## 5.6 The chord column

Notes that Encore draws in one vertical column share an `xoffset`, described in ENCORE_FORMAT.md §7.7, and that column is a more reliable statement of what was notated than the playback ticks are. `normalizeChordColumnTicks` and `reconcileStaleNoteTicksByColumn`, in `parsers-measure.cpp`, reconcile the two per staff and voice group, already sorted by tick.

**Strum collapse.** A run of consecutive notes sharing one non-zero column and one face value is collapsed to the run's earliest tick, so the grouping downstream sees a single chord. The window is capped at one notated face duration and at 48 Encore ticks, so a short face value stays tight and a long one never swallows a genuine later note that happens to reuse the column. Notes at column zero are left alone.

**Near-simultaneous split.** Two notes a few ticks apart merge into a chord only when their columns agree. Columns that differ by at least the minimum visible distance of about 8 pixels keep the notes distinct, which preserves the full member count of a tightly played tuplet whose positions sit a few ticks apart.

**Stale-tick snap-back.** A note edited in Encore can keep its old playback tick while its column moved. Such a note is snapped back to its column's earliest tick, keeping the duration already computed from the stale tick, so the time it vacates becomes a rest. Only a note that is the earliest in its own staff and voice is moved, and only when a different column does not already occupy the target tick.

**The opposite case is not reconciled.** A note whose MIDI tick is earlier than the position Encore draws it at is left where the file puts it. This happens most often when a voice carries a note at the same tick and pitch as a member of another voice's chord: Encore keeps both at that tick internally and nudges the single note rightward so they do not overprint. The importer places it at its stored tick, so the two land together and overprint. That matches Encore's own MusicXML export, which emits the same tick, but not its screen layout. Recovering the drawn position would mean translating an absolute pixel offset into a tick through Encore's spacing model, and a linear interpolation across the sparse columns is unreliable enough to land several beats away, so the note stays at its tick and keeps the file's canonical reading.

---

# 6. Notes

## 6.1 Face value and dots

The face value nibble is authoritative for the notated duration. The playback duration is never used to lengthen a note's visible value; it is consulted only to flag the note as a tuplet member.

Dots are resolved by `computeDotCount` in priority order: treat `dotControl` as a tick value, then snap the real duration to a dotted multiple within one tick, then fall back to bit 0 of `dotControl`. That fallback is guarded to fire only when the real duration exceeds the plain face value, since a note whose duration is its face value has nothing to add a dot to without lengthening the bar.

**The durations are a reconstruction, and the note states the count itself.** The low two bits of the layout byte are the dot count Encore draws, and ENCORE_FORMAT.md §7.3 measures it against the durations. The importer derives dots from the durations because they carry the triple dots the two-bit field cannot express and because they need no per-note trust. Where the stated count is the only witness, `EncMeasure::restoreHintedDots` in the parser puts it back, and the next section describes the arithmetic that makes that safe.

Making the stated count the primary source, with the durations as the fallback, would be the truer reading of the format. It is not what the importer does today: measured against the corpus it would change roughly 380 further notes, mostly ones the durations dot and the count does not, and that is its own change with its own validation round.

**A dot the durations cannot see.** Dotting a note that already has neighbours does not move them in Encore: the drawn figure grows and every note-on stays where it was, so the bar's face values come up short by exactly the dot and every duration in the voice reads as plain. `restoreHintedDots` runs per staff and voice, right after the durations are computed, and puts those dots back, lengthening the notes and moving everything past them, notes and annotations alike, so the stored ticks line up with the notation and a mark anchored by tick keeps its note.

It acts only when the arithmetic closes: the written durations the group already produces, plus what the stated dots would add, must come to exactly the bar. Exactly, not merely within it: room alone lets a spurious bit dot a plain note, which is what `Tst_Notes.v0c2_plain_sixteenth_with_spurious_dotctrl_bit0_no_dot` and `Tst_Grace.trailing_grace_does_not_dot_preceding_note` pin down. That is what separates this case from the other reason a stated dot goes missing, which is that Encore lets the **last** note of a bar be written longer than the space left and clips its playback to what remains. Acting on one of those would push content into a bar that is already full, and in a score of many staves it would stretch a bar the others fill exactly. Measured over the corpus, outside tuplets, the rule applies to eleven notes and declines thirty four, twenty five of them last notes of their voice.

A group carrying an explicit tuplet is left alone entirely. The ratio rescales everything the group writes and the tuplet passes own that arithmetic, so comparing a stated dot against unscaled face values there would be comparing two different units.

Covered by `Tst_Notes.v0c4_dotted_hint_fills_bar` and its v0xC2 twin, each with a control bar whose count is zero and which must stay short.

**The last note of a bar keeps the figure Encore draws.** That is the other half, and it is the user's decision rather than the importer's, so `keepStatedFigureOfLastNote` reports what the file says and leaves the verdict to `overfillMeasureStrategy` of §9. The face value already survived on its own, being authoritative for the notation, so a whole note written into the last two beats has always overflowed the bar and been resolved by that option; only the dots were lost, because they live in the layout byte and no duration in the bar shows them. Restoring them completes the figure and puts those bars on the same footing as every other overfull one.

What each strategy then does is what it does everywhere else. **Expand measure**, which Preferences ships, keeps the figure and grows the bar, so the page matches what Encore prints, which is the point for a performer reading from it. **Remove last notes** recuts the note to the barline, which is the length Encore plays, and leaves the bar standard. The trade cannot be avoided: MuseScore's written duration is the time it occupies, while Encore draws one thing and plays another. Two hundred and nine notes in the corpus are affected, against the 9022 whose face value already reached this path. Covered by the two `Tst_Options.overfill_*_last_note` tests, one per strategy.

**Dotted values that are not integers must not match.** For some face values the theoretical dotted duration is fractional in the 960-tick grid: a triple-dotted 16th is 112.5 ticks, which integer division truncates to 112, and a live-recorded note whose measured gap happens to be 112 would match it. `calcDots` and `calcDotsSnap` therefore skip a threshold whenever the dotted value is not exactly representable. The affected face values are the 16th at three dots, the 32nd at two and three, and the 64th and 128th at all three. A unit test and `Tst_Notes.rdur112_16th_note_not_triple_dotted` cover it.

**An inflated real duration does not promote the face value.** A voice carrying a single chord with no following event has its real duration inflated to the gap to the end of the measure. In a 3/4 bar a quarter chord at tick 0 inflates to 720, which lands exactly on the dotted-half bucket. The mapping therefore rejects the dotted reading when the real duration exceeds the face value **and** is not a genuine dotted multiple of it. When either test fails, a truncated duration or a real dotted note, the dotted mapping still applies.

**A triplet playback duration does not override the face value.** A notated 16th with a playback duration of 80 ticks, a triplet eighth in the 240 grid, stays a 16th. The earlier code upgraded it to an eighth, which misclassified the note as longer and pushed the rest of the bar into a spurious second voice. Verified on a real plucked-string score whose first bar has 14 events in one voice and previously came out as 10 plus 4.

**A following grace does not inflate the note before it.** When the next element is a grace, the current note's held duration is capped at its face value. A beat trailed by an ornament then stays a note and a rest, instead of being promoted to a longer or dotted value when the gap happens to match a dotted ratio.

## 6.2 Tuplets

A tuplet is read from the explicit ratio byte where there is one, and otherwise inferred, and the group is then placed as a unit. Several real shapes need more than that.

**Ticks that cannot be written.** For a ratio whose denominator is not a power of two, the placed duration is not representable as a MuseScore duration, and setting the bracket to such a value aborts the beam layout later. The importer detects this with a truncating duration snap and falls back to the canonical base times the normal count, filling the unused positions with invisible rests.

**Chord and rest ticks must agree.** When the remaining space cannot fit any standard duration, the note is dropped rather than created with a non-standard one, which would leave chord ticks with garbage values. When a cap fires on a chord extension, the chord ticks are updated to match the advance whether or not the note is in a tuplet.

**One member missing its ratio byte, the sandwich orphan.** Live-recorded v0xC4 files sometimes carry a zero ratio byte on one note in the middle of a triplet run, surrounded by notes with the correct one. The group then breaks at the orphan, all three notes are placed as plain eighths, the measure overflows and the last note is dropped. The fix has two halves. In `computeImpliedTupletMembers`, when the explicit loop breaks on an incomplete group, the orphan is accepted if it has the group's face value, the note after it resumes the same ratio, and its binary tick is within `max(4, advance / 4)` of the expected position. In `handleNote`, a guard borrows the active tuplet's ratio when the note recomputes a zero ratio from its own byte, is a known member and the group is not yet full, so it joins the bracket instead of closing it. Two tests in `Tst_Notes` cover the orphan with and without a preceding complete group.

**More notes than the ratio states.** Encore can encode a run longer than the group size, for example 15 notes all marked 9:5. When a contiguous run of same-voice, same-face-value notes shares one ratio, the count exceeds the actual number, the count is not a multiple of it, and the standard reading overflows the bar, the ratio is recomputed to fit as the count against `round(available / faceTicks)`. The denominator is kept to the standard set 1, 2, 3, 4, 6, 7 and 8, and a 5 or a 10 is rounded to the nearest safe value.

**Exactly 9 notes marked 9:5** that do fit form a single bracket. Its duration spans five eighths, which is not a standard value, so it is set after all nine notes are placed and group construction cannot reject it.

**Nested triplets.** When an outer group closes and the triggering note plus the following ones form a complete inner triplet of smaller face value, nested groups are created, and each inner note advances by the product of the two ratios.

**A group truncated at the barline.** Encore omits the final note of a group whose tick equals the measure length, so the face-value sum falls short even when the count matches. An invisible rest for the missing sum completes the group.

**A last note that looks too short.** The last note of a measure-spanning tuplet often has a playback duration far below its face value, because Encore truncates playback at the barline. It is kept by group membership, not by its duration.

**No gap snap inside a group.** The implicit-silence snap is suppressed while a group is active, since tuplet positions come from accumulated face values and not from the raw MIDI tick.

## 6.3 Ties

Both the arc-direction byte at +5 and the secondary tie-start flag at +6 are inspected, and an element with the high bit set on either one is a tie start. An element with neither bit set marks the receiving side and is dropped from the tie queue; the receiving note is matched by staff, voice and pitch when it is placed.

| Bytes at +5 and +6 | Role         |
|--------------------|--------------|
| `0xFC`, `0x80`     | tie start    |
| `0xFC`, `0x00`     | tie start    |
| `0xFE`, `0x00`     | tie start    |
| `0x04`, `0x80`     | tie start    |
| `0x04`, `0x00`     | arc-only end |
| `0x02`, `0x00`     | arc-only end |

A significant share of outgoing ties use the secondary flag with an arc-only direction byte, so ignoring the byte at +6 loses them. The note's own tie flag, §1.4, is the third record of the same thing and covers the notes where the element is missing.

## 6.4 Grace and cue notes

Encore's Grace and Cue Note dialog produces three kinds of small note, decoded from the two grace bytes as described in ENCORE_FORMAT.md §6.3: a cue, an acciaccatura with its slash, and an appoggiatura. MuseScore has no dedicated cue element, and it cannot attach a grace note to a rest, which is issue #19701, so the three are mapped as follows.

**The mute flag.** Bit `0x01` of the second grace byte is a per-note mute, independent of size. Any note carrying it is imported with playback off, whether it is a normal note, a cue or a grace.

**Cue notes** are imported as normal notes of full duration, drawn small, audible unless muted. The whole chord is marked small and not just the notehead, because the note magnitude multiplies the chord magnitude and a note-only flag shrinks the head while leaving a full-size stem. A cue that stands alone in its bar does not overlap the principal line, so it needs no separate voice.

**Which of the three a small note is** is decided in `tryHandleGraceNote`, from the raw measure on the same staff and voice. A slash always means an acciaccatura. A small note without a slash is a grace after when a contiguous preceding principal note reaches its tick with no silence in between, which keeps it in its own bar without displacing it leftward. It is an appoggiatura when a principal note is at its tick or follows it. And it is a cue, handed back to the normal note path, when it stands alone with no principal at, after or contiguously before it.

That fallback has one subtlety worth keeping. `tryHandleGraceNote` rolls the track's previous tick and last chord position back so the next note is not read as a chord extension of a grace. The rollback belongs only to the paths that really take the note as a grace: a small note handed back as a cue keeps its measure time, and rolling it back hid the note from the chord-extension test and split a two-note cue chord into two single notes on consecutive beats.

**An acciaccatura with only silence before it**, a percussion ruff after the last beat, is a grace before the following principal, which through the carry below is the next bar's downbeat. It is written as consecutive grace figures, a beamed group, and not at its sub-tick playback spacing. A beamed group keeps its written figure, so sixteenths stay sixteenths, while a lone acciaccatura uses the slashed eighth glyph.

**The carry across the barline.** `resetPerMeasureState` does not discard pending graces at the measure boundary, so a trailing grace attaches to the first principal chord of the next bar. A grace that never finds one, a ruff in the final bar, is re-placed by `handleDanglingGraces` as a small cue note in the spare voice of its own bar, flush to the barline, rather than being dropped.

**Where a grace chord is parented.** Under its main chord, with `Chord::add`, never under a segment: a segment parent crashes the position computation during beam layout. The importer queues pending grace chords until the next main chord appears in the same track key and attaches them there.

**The order inside a group.** MuseScore inserts a grace at its grace index, and the default index of 0 prepends each new one, which reverses a multi-grace group. The importer sets the index to the current count before each add, so the chords are appended in tick order and read left to right as Encore drew them.

**v0xC4 writes the main note first.** Encore 5 serialises the principal before its acciaccatura at the same beat, the opposite of v0xC2. When the main note arrives first and a grace follows within the chord threshold, it is a retroactive extension of the chord already placed, and it is attached to that chord instead of being queued as a prefix for the next one.

**v0xA6 grace groups.** These files can carry a leading grace and one or more inner ones, distinguished by the same nibble, and an inner grace is always shorter than the leader. A note is an inner grace when it is a v0xA6 note slot, carries the inner flag, has a leading grace queued for its track, and has a higher face-value number than the leader. The leader's face value is tracked per track key and cleared when the queue flushes. A note with the inner flag but a **longer** duration than the leader is a regular note following the group, and real scores contain both shapes: one bar with a 64th inner grace after a 32nd leader, another with regular 16ths after the same leader.

**v0xA6 stores graces at real tick positions**, which has two consequences. The face-grid snap must be suppressed while a grace is pending, or a spurious rest of the grace's own duration appears before it. And the last real note of a group ends with a gap to the end of the measure that is shorter than its face value, because the graces borrowed that time. `calculateRealDurations` detects it by summing the face values of the graces that precede the note in the same staff and voice: when that sum equals the face value minus the measured gap, the note is restored to its face value. Without it, in a real 3/8 bar, an eighth at tick 270 came out as a 16th followed by a rest of 30 ticks. The check fires only for v0xA6 note slots.

Left unhandled, the combination of a spurious pre-grace rest, an inner grace read as a regular note and the resulting irregular timing produced a score that survived the command-line export path and crashed the layout engine in the interface. The `v0xa6_inner_grace_group` test calls the sanity check so the shape is caught before layout.

## 6.5 Notes the file records twice

**The same pitch twice in one cluster.** Some files encode a pitch twice in a chord, identical in tick, staff, voice and pitch, differing only in bit `0x40` of the first grace byte: one copy is the chord note and the other a chord-extension marker. Added as they are, the two produce a double notehead on one stem that the user has to delete by hand. The importer suppresses the marked copy when the pitch is already in the chord. The guard is scoped to that bit, because in v0xC2 clusters the raw pitch byte is unreliable and several members can share a value. `Tst_Notes.duplicate_pitch_in_chord_cluster_suppressed` covers it.

**Notes that are MIDI artifacts.** `isMidiArtifact`, in `emitters-note.cpp`, drops notes whose real duration falls between 5 and 14 ticks when the face value is an eighth or longer. Two valid cases were caught by it and are now bypassed. The first note on a staff in a measure cannot be a tie-continuation artifact, because there is no earlier note in the bar to generate one, and its short duration comes from the next chord member starting a few ticks later. A chord extension, within the chord threshold of the previous note, is a real chord tone recorded with tight timing. With both bypassed, every note of a simultaneous chord survives even when the measured gap is very short.

**Rests that are not real.** A rest's computed duration is the gap to the next event, which timing slop can make far shorter than its face value, so the same short-duration filter would drop it. The face value decides instead: a rest of a 32nd or longer, 30 ticks or more, is kept whatever its measured duration, and only a rest whose face value is also very short can be dropped. Separately, a voice may carry a redundant plain rest at the same tick as a real note, described in ENCORE_FORMAT.md §6.2. It is dropped, or the note is pushed after it and the bar overflows. Two tuplet members at one tick, a tuplet rest followed by a tuplet note, are kept.

## 6.6 Two rules that were removed

Both were measured against the corpus and found to be inventing music, and both are recorded here so they are not reintroduced.

**A dot on an eighth followed by a sixteenth.** A rule added one whenever the voice group came out 60 ticks short. It read the surrounding shape and never the note's own dot count, and that shape arises at the same rate in every generation, 0.35% of voice groups in format 4.20 against 0.33% in 3.05, so it was inventing dots. What replaced it is §6.1: the count the note states, acted on only when it explains the shortfall exactly.

**A pitch moved out of the tuplet slot** for v0xC2 notes in the later layout, guarded on an empty pitch slot. It dates from when the element body was read at one fixed offset for both generations. With the body offset selected by the format version the condition never holds: across eleven million notes in the corpus it fires zero times.
---

# 7. Marks attached to a note

## 7.1 Articulations and technical markings

`encArticulation2SymIds`, in `mappers-articulations.cpp`, maps the articulation byte to a list of symbols, since a combined byte yields more than one. An unmapped value is dropped silently unless the user asked for the option in §9.

Two families need a specific element rather than a plain articulation. A symbol in the ornament family is wrapped in MuseScore's `Ornament`, so a MusicXML export writes it under ornaments instead of articulations. A fermata becomes a `Fermata` attached to the segment, so the export writes a fermata rather than an anonymous symbol, and the upright or inverted variant follows the slot the byte came from.

The fermata rule has one exception. Bytes `0x20` and `0x21` on a note that belongs to a tuplet are not fermatas: they state the tuplet bracket's placement above or below, which is what Encore exports as a placement attribute on the tuplet stop. No fermata is created there.

| Byte           | Element                                                              |
|----------------|----------------------------------------------------------------------|
| `0x0D` to `0x11` | `Fingering` text 1 to 5                                            |
| `0x1E`, `0x1F` | `Articulation` with the harmonic symbol                              |
| `0x44`, `0x45` | `Articulation` with the thumb-position symbol                        |
| `0x46`         | `Fingering` as a string number 0, exported as an open string         |

Fingerings and the open string attach to the note. The remaining technical marks attach to the chord and export under the technical block.

**Chord-level staccato.** Encore stores it as a separate ornament at the chord's tick, subtype `0xC9`, and its own MusicXML exporter drops that subtype entirely, so a file showing staccato dots on hundreds of notes exports with almost none. The importer attaches the staccato symbol and deduplicates it against the per-note articulation byte `0x1D`, which recovers the full set the export path loses.

## 7.2 Tremolos

Encore records a single-chord tremolo in two ways, and both map to the same element.

The first is the stroke count packed into the articulation bytes, `0x41` for one stroke, `0x42` for two and `0x43` for three, which become the matching tremolo types.

The second is an ornament element with subtype `0xAF`, the standard triple tremolo of plectrum instruments, or `0xEF`, the form Encore writes when it places the ornament at the measure length after the last note of a long passage. Both become a three-slash single-chord tremolo, which is the plectrum-ensemble tremolo these files are full of.

Resolution is deferred to a post-pass, because the ornament may not sit on a tick where a chord exists yet. The pass first tries the exact tick. When the ornament was at the measure length, that tick falls into the next measure, which holds only a filler rest, so the fallback re-anchors to the source measure and takes its last chord-rest segment.

There is one correction on top. If the chord resolved this way begins a tie back, the tremolo belongs on the note the tie starts from: Encore writes the ornament after the tied-from note, so the stream cursor lands on the continuation chord. The pass walks back through the tie and attaches it there. Without that, the tremolo appears on the shorter continuation instead of the longer note that carries it.

Subtype `0xBE` is the accent, and it is anchored by tick like the other attached marks, which is why anything that moves a note has to move its ornaments with it.

## 7.3 Trills

Encore writes a trill span with three ornament subtypes.

| Subtype       | Value  | Role                                                              |
|---------------|--------|-------------------------------------------------------------------|
| `TRILL_START` | `0x36` | start of the span; the forward count says how many measures it runs |
| `TRILL_ALT`   | `0x37` | a secondary mark inside the span, not a start                     |
| `TRILL_END`   | `0x35` | end of the span, no visible glyph, dropped by Encore's own export  |

`resolvers-ornaments.cpp` resolves each start in three ways. A matching end on the same track at a later tick gives a spanner to that tick. A non-zero forward count gives a spanner to the end of the target measure. With neither, the start degrades to a single-beat trill glyph.

The secondary subtype always produces a glyph and never a spanner: it marks a note inside the span that Encore annotates with a redundant sign. End ticks are held per track and cleared by the resolver.

## 7.4 Fingerings and bowings in grand-staff scores

In v0xC4 grand-staff instruments every element shares staff index 0 and the second staff's notes use voice 4, but a stand-alone fingering or bowing ornament always carries voice 0, whichever staff it belongs to. The ambiguity is resolved in a deferred pass from two facts collected in a per-measure pre-scan: the ticks that carry a second-staff note, the number of voice-0 notes at each tick, the number of fingering ornaments at each tick, and the largest tick carrying a voice-0 note.

**A cross-measure ornament.** Encore puts the fingerings for the second-staff chord of the next measure at the end of the current measure's block, at the same tick as the last voice-0 note. It is detected as a grand-staff measure with no second-staff note at that tick where the tick is the largest voice-0 one, and it is routed to the first chord of the next measure on the sibling track, falling back to the original track.

**A cluster for a multi-note second-staff chord.** When a second-staff chord shares a tick with a voice-0 note and there are more fingering ornaments at that tick than voice-0 notes, the extra ones belong to the second-staff chord. The resolver tries the sibling track first and falls back to the original.

A score that is not grand-staff has no second-staff ticks at all, so both flags stay false and the resolution is the plain exact-tick lookup with a sibling fallback.

## 7.5 Dynamics

The contiguous ladder from `0x80` to `0x8A` is fully decoded, from triple piano to `fp`, and two outliers, `0xAA` and `0xAB`, cover `fz` and `sf`. All 13 levels map, each byte to exactly one dynamic.

**Duplicates.** Encore occasionally stores the same dynamic twice on one staff and voice at one tick with slightly different horizontal offsets, which is what a user leaves behind after dragging a glyph. Encore renders one. Before adding a dynamic the importer checks whether one of the same type already exists on that segment and track, and drops the second.

**A dynamic dragged onto the staff above.** A dynamic normally has a negative vertical offset, Encore's convention for below the staff. When the user drags the glyph upward onto the staff above, the offset turns positive while the staff byte still names the lower staff, so the importer moves it one staff up in that case.

**A dynamic past the end of its measure.** Encore can place a dynamic or a staff text at a tick beyond the measure length, as a section-end marker rendered just before the barline. A real 2/4 score stores a first-ending `pp` and its text at tick 960 in a measure of 480. The reader keeps such marks instead of filtering them out, and the placement code clamps the tick to the last existing chord-rest segment of the measure, so the marker lands inside the right bar. The earlier filter dropped them and the user saw one of two dynamics.

## 7.6 Staff text and tempo

A staff-text ornament takes its payload from the TEXT block through the index byte at +32, and its vertical offset drives the placement: a negative value means below the staff, anything else keeps the default above.

**Italian tempo terms are promoted.** An anonymous staff text leaves a tempo word untracked in MuseScore's tempo map, so both the spacing and the playback speed are wrong. `encTextToTempoBps`, in `mappers-tempo.cpp`, recognises the canonical set and promotes those strings to a tempo text.

| Term        | BPM | Term        | BPM |
|-------------|-----|-------------|-----|
| Grave       | 35  | Moderato    | 114 |
| Largo       | 50  | Allegretto  | 116 |
| Lento       | 52  | Allegro     | 144 |
| Larghetto   | 63  | Vivace      | 172 |
| Adagio      | 71  | Presto      | 187 |
| Andante     | 92  | Prestissimo | 200 |
| Andantino   | 94  |             |     |

The values mirror MuseScore's own tempo palette. Relative markings such as "a tempo" or "Tempo I" stay tempo texts, so the layout treats them as such, but carry no absolute speed and fall back to the previous tempo. Any other string keeps the plain staff-text path.

**Every measure carries a tempo.** The MEAS header holds a quarter-note BPM. A pass over the finished measure list emits a tempo text at the first measure and at every measure whose BPM differs from the last applied value, and sets the score tempo at the same tick so playback follows. Back-to-back identical measures produce nothing.

The pass skips both the visible mark and the tempo map update when a tempo text already sits at the target segment. That covers the tempo ornament below, which has already written both, and a staff text promoted by the Italian lookup, which already provides the right speed and a visible label.

The display follows the beat unit, taken from the measure's `beatTicks`.

| beatTicks | Beat unit      | Display   | Speed factor |
|-----------|----------------|-----------|--------------|
| 240       | quarter        | quarter   | 1            |
| 360       | dotted quarter | dotted    | 1.5          |
| 120       | eighth         | eighth    | 0.5          |

Compound meters, whether they state 360 or the legacy 240, display a dotted quarter and use the 1.5 factor, so the number means dotted-quarter BPM. A piece in 5/8 or 7/8 felt in eighths displays an eighth and uses 0.5.

**The tempo ornament**, subtype `0x32`, stores the BPM of that same beat unit, so the conversion is identical. It is normally suppressed when it disagrees with the measure header, because Encore sometimes places a tempo mark one system too early and the header is authoritative. That comparison is valid only when both are in quarter-note units: for a dotted or an eighth beat the two values are in different units and cannot be compared, so the ornament is used whatever the header says. The conversion uses the measure's nominal signature, so a pickup with an actual 4/8 but a nominal 6/8 still inherits the compound factor.

## 7.7 Lyrics

Each lyric element is decoded on its own, with the probe of §2.4, because reading a Latin-1 syllable as UTF-16 pairs its bytes into a meaningless CJK character.

Hyphen and word-break elements are filtered out of the per-track queue and consumed only to set each surviving syllable's syllabic value, single, begin, middle or end.

**Attachment is by tick, not by position in the stream.** A syllable carries the raw Encore tick of its element, which Encore's layout can offset from the note's own tick by 30 to 80 ticks. At the end of the measure pass the importer walks the chord-rest segments and gives each chord the syllable whose tick is closest, within half a beat.

The reference tick of a segment is taken **positionally from the Encore note elements**, not derived from the accumulated MuseScore duration. The accumulated value is not proportional to Encore ticks, and in 6/8 with a quarter beat the old formula applied a compound correction that halved every reference and shifted every syllable by one note, losing the last one entirely. The kth chord-rest corresponds to the kth note element, and using that directly removes the conversion.

Rests do not consume a note tick. The cursor advances only for chord segments, because a measure that begins with a rest, common in 6/8, would otherwise hand the first note's tick to the rest and rotate every syllable by one. Rest segments fall back to the beat-grid estimate.

When a syllable could match two chords, a note at or before the syllable's tick is preferred over one that starts later, even when the later one is closer in absolute distance, and within the same tier the smallest distance wins. Pure nearest-distance matching mis-assigned syllables whose layout offset put them slightly closer to the following note.

Multiple verses come from the lyric element's voice field, which becomes the verse number, and every verse attaches to the voice-0 chord at the same tick.

Three tests in `Tst_Text` cover the offset case, the compound-meter case and the leading-rest case. Unmatched syllables fall back to the nearest rest in the measure instead of being discarded.

## 7.8 Chord symbols and fretboard diagrams

Encore writes chord symbols as their own element type, and `handleChordSym` in `emitters-chords.cpp` creates a `Harmony` for each one.

**Text mode**, when bit 0 of the type byte is set, stores the name verbatim in a 36-byte slot with the usual encoding probe, and the string is passed through as it is.

**Numeric mode** encodes the chord in three bytes, described in ENCORE_FORMAT.md §6.10.

| Field    | Meaning                                                                       |
|----------|-------------------------------------------------------------------------------|
| `radiko` | root: the low nibble names it, the high nibble is the accidental               |
| `toniko` | quality, an index from 0 to 63 into the quality table                          |
| `baso`   | slash bass, encoded like the root, present when bit 1 of the type byte is set   |

`EncChordSym::chordName` assembles root, quality and optional bass and hands the string over. Several quality indices are undefined in the format, and their table entries are empty, so such a chord degrades to its root read as major, which is a safe reading for a file using an undocumented type.

After the harmony is set, MuseScore's own parser normalizes the name, so a test must assert on the normalized form and not on the input string.

**Fretboard diagrams.** Bit 2 of the type byte records that Encore draws a guitar frame above the symbol. Only then does the importer wrap the harmony in a `FretDiagram`, a segment annotation that takes the harmony as its child, and it asks the fretboard database to fill the frame from the chord name. If the database has nothing for that chord the frame comes back empty, and the diagram is discarded and the plain text symbol kept, so an unknown chord never leaves an empty grid on the page. A chord without the flag is never given a diagram, whether or not the database knows it.

Unit tests in `tst_parser_chord.cpp` cover the name assembly in isolation, over every natural root, both accidentals, the common qualities, a slash chord and the invalid inputs. Two integration tests in `tst_text.cpp` cover a score with one numeric chord per measure across the whole quality range, and a slash chord with its bass note.

## 7.9 MIDI control change

Encore stores control-change events inline in the element stream, described in ENCORE_FORMAT.md §6.12. They are playback data with no notation: sustain pedal at controller 64, volume at 7, modulation at 1.

The parser decodes controller and value into `EncMidiCc` and the importer emits nothing. What the decode buys is the diagnostic: the dump counts them by kind and logs one line saying how many sustain, volume, modulation and other events were dropped, instead of one unknown-element line per event. A file with a pedal recorded live is otherwise a wall of noise in the log that hides the elements worth looking at.

## 7.10 Beams

The importer relies on MuseScore's automatic beaming, which produces about 30% more beam segments than Encore's explicit decisions. Honouring the explicit beam elements would mean pairing each with the chord range it covers and setting the beam mode on those chords. It is left as future work, and in practice the visual difference is small.

---

# 8. Spanners

## 8.1 How an endpoint is found at all

Encore writes no stop element for a hairpin or a slur. The endpoint is reconstructed from the forward measure count and the horizontal offsets, in a post-pass over the measure list, which is why every spanner in this chapter is collected as a pending record first and resolved later.

The coordinates need care. An attached ornament stores a rendered x in its offset field and an end x in the second one, and that x does **not** share the origin the note offsets use: the two differ by a per-file constant that varies with the staff scale, small in a tightly engraved score and large in a widely spaced one. A raw offset can therefore never be compared against a note offset directly.

Two anchoring patterns recur.

**The start snap**, `snapStartTickByXoffset` in `coords.cpp`, shared by dynamics, tempo marks, hairpin starts and trills. Encore tags a glyph at the chord-rest at or after its visible position, and when the glyph's x is smaller than the note's it visually pulls back to an earlier chord-rest. The helper reads the offset of the note at the element's own tick, keeps the tick when the glyph sits at or after it, and otherwise returns the latest earlier tick whose note is at or before the glyph, falling back to the tick when nothing qualifies.

**The end snap by the second offset**, used by the hairpin end: in the target measure it takes the last note or rest at or before that offset, and clamps to the barline when there is none.

Everything else anchors by related logic. Fingerings and bowings trust the raw tick first and fall back to the closest note offset. Lyrics attach to the nearest chord within a threshold. Staff text follows the start convention but is not snapped, because text positions are reliable as they are.

The slur end is the least robust of all of them, and it is worth saying why: a tie anchors by matching a pitch on the next note, but a slur is a pure graphic with no stored end note, so it can only be inferred from the coordinate.

Two invariants protect the layout. A spanner whose computed end lands on its start tick would assert during layout, so degenerate spans are dropped instead. And a hairpin start at exactly the measure length is legitimate, because Encore lets the user put it on the barline: ornaments are kept up to and including that tick and only excluded strictly beyond it, while the note and rest filter stays strict.

## 8.2 Slurs

How the end is found depends on the generation, because the two differ in how far their coordinates can be trusted.

**v0xC4 and SCO5, by pixel span.** The start element carries both offsets, and each is displaced from the underlying note by a per-element drawing constant, so neither matches a note directly. Their difference, however, is exactly the distance between the first and the last covered note:

```
slurXoffset2 - slurXoffset == endNote.xoffset - firstNote.xoffset
```

The post-pass finds the first note at the slur's start tick on the same staff and Encore voice, reads its offset, adds the difference, then walks the notes of that measure and takes the one closest to the target. A non-zero forward count means the span crosses a barline, where offsets reset, so the heuristic is skipped and the second offset is matched directly against the target measure's notes, falling back to its last chord-rest.

**v0xC2, by the forward measure count.** Here the absolute end offset lives in a stale ornament-coordinate origin, so matching it directly over-extends slurs: an arc from note 1 to note 2 comes out reaching note 4. The dependable signal is the forward count, which the parser reads at the offset the generation uses, +16 below format 3.07 and +18 from it on, and marks valid.

A non-zero count means Encore drew the arc between bar starts, so the end anchors to the downbeat of the target measure. Both endpoints are located by iterating chord-rest segments, since tick-to-segment lookup is unreliable at a bar boundary, and both elements are set explicitly. Such slurs are recorded in a set so the orphan-removal pass does not recompute and null them.

A zero count means the slur stays inside its measure. A tiny pixel span, two or less, is a note-to-next-note slur and ends at the next note on the staff. A grace note at the start instead resolves as a grace slur, below.

## 8.3 Slurs that start on a grace note

When a slur start shares a tick with an appoggiatura, the pixel-span heuristic has nothing to work with, because the grace and its principal share a written tick and no note sits at the proportional position the offset implies. Two cases follow.

**Grace to main**, when start and end resolve to the same tick: the slur is created with the grace chord as its start element and the main chord as its end, and both automatic resolvers are skipped so neither overrides what was set.

**Grace to a later note**, when the end is further on: the chord at or after the start tick provides its grace notes, the first becomes the start element, and only the start resolver is skipped.

Three rules apply when a grace and its principal share a tick. The reference offset is the **grace** note's, not the principal's, since the principal has a larger one and using it inflates the target and selects too late an end; the scan therefore continues past regular notes at that tick until it finds the grace, because v0xC4 writes the principal first. After scanning, if the co-located principal matches the target better than any later note, the slur resolves grace to main, and otherwise it takes the heuristic end. And if no end note is found at all, the end is set to the start tick, which a later pass reads as grace to main rather than letting the general resolver find a rest or a note in the next measure.

Attaching such a slur must use the spanner call that skips the start computation, or the regular path replaces the explicitly set grace with the main chord.

## 8.4 Hairpins

**Direction** is bit 0 of the direction byte: clear is a crescendo, set is a diminuendo. Encore 5 also sets bit 1 on the same byte, so a crescendo reads as `0x02` and a diminuendo as `0x03`, while older files still use `0x00` and `0x01`. Testing the byte for equality with zero reads every Encore 5 hairpin as a diminuendo and flips every pair on disk, so the importer tests bit 0 alone and both encodings agree.

**The endpoint** is resolved after every dynamic has been placed, in three tiers.

1. The first dynamic on the same track after the start and within the forward-count bound. This is what Encore actually draws: in an `mf<f>mf` chain each hairpin stops exactly at the next dynamic glyph, even though the count nominally points at a whole measure.
2. Failing that, the last note or rest in the target measure at or before the second offset.
3. Failing that, the target measure's start, for a second offset that precedes every note in it.

With no dynamic in the window the fall-back is the upper bound itself, so a lone trailing hairpin still spans its measure. Without this pass two adjacent hairpins on the same beat both extended to their barline and overlapped visually. The last two tiers are skipped when the coordinates are absent, either offset being zero.

**A start on the barline.** A hairpin start at exactly the measure length has no chord-rest at its tick, and the snap used to return the start of the next measure, giving a zero span after clamping. The backwards scan now also fires when nothing is found at the default tick, taking the latest note in the source measure at or before the glyph.

**Grand-staff instruments** need three corrections, all from the same root: the ornament always carries voice 0 while the notes it spans may not.

The offset snap filters notes by staff, and it must compare against the **raw** Encore staff index, not the mapped MuseScore slot. In a single-instrument piano every note has raw staff 0, because the staff inside the instrument is selected by the high bits, so comparing against a mapped slot of 1 matches nothing and silently disables the snap for every lower-staff hairpin. For the same reason the snap does not filter by voice at all: the ornament and the notes legitimately differ there.

The track is derived from the notes rather than from the ornament. When the ornament carries a staff-within value, the importer scans the measure for the first note on that same sub-staff and uses its Encore voice, so the hairpin lands in the voice that has notes. On voice 0 of a lower staff there is usually nothing but a whole-measure rest, and a hairpin attached there pins to beat 1 whatever its intended start.

The start tick is computed from the raw element tick for the same instruments, because the accumulated position for voice 0 of that staff never advances: both hairpins of a swell pair would otherwise start at the measure tick.

**A swell pair in one measure.** Two consecutive starts in the same measure are the crescendo and diminuendo of a swell, and each should cover about half of it. The pixel coordinates do not map cleanly to ticks in a measure with empty beats, so they produce two short hairpins crowded into the first half. A pre-pass in `resolveHairpins` detects a pair that begins in the same MuseScore measure and assigns the crescendo's end and the diminuendo's start to the measure midpoint, and the diminuendo's end to the barline. The offset snap and the dynamic clipping are skipped for such a pair. Hairpins on the same track in different measures are untouched. `Tst_Importer.v0c4_swell_pair_splits_at_measure_midpoint` covers it.

## 8.5 Ottavas

Encore writes 8va and 8vb as ornaments with no endpoint at all: subtype `0x10` for the line above the staff and `0x12` for the one below.

`resolveOttavas` sorts the pending ottavas by staff and start tick and ends each one at the start of the next ottava on the same staff, or at the end of the score for the last one on a staff. Both ends use the segment anchor.

---

# 9. Import options

## 9.1 The options

`EncImportOptions`, in `importer/import-options.h`, holds twelve flags. `EncImportConfiguration` persists them and exposes a change signal per option, and `NotationEncoreReader` reads the configuration on every import and passes the filled struct into `importEncore`. The struct lives in `BuildCtx` and is consulted throughout the emitters and resolvers.

| Field                                  | Shipped default  | Effect                                        |
|----------------------------------------|------------------|-----------------------------------------------|
| `importPageLayout`                     | true             | page margins from the WINI block               |
| `importPageBreaks`                     | true             | page breaks from the LINE page counter         |
| `importSystemLocks`                    | true             | system locks from the LINE show byte           |
| `importStaffSize`                      | true             | the LINE staff-size hint                       |
| `importTempoTextSemantic`              | true             | Italian tempo terms become tempo marks         |
| `importUnsupportedArticulationsAsText` | false            | unmapped articulation bytes become staff text  |
| `instrumentSearchMode`                 | NameAndMidi      | name and MIDI, MIDI only, or everything piano  |
| `tablatureImportMode`                  | Linked           | linked, separate, or ignore                    |
| `underfillMeasureStrategy`             | IrregularMeasure | how a short measure is filled                  |
| `overfillMeasureStrategy`              | IrregularMeasure | how a long measure is resolved                 |
| `firstMeasureIsPickup`                 | true             | shorten the first measure as a pickup          |
| `mergeVoices`                          | true             | collapse voices that never overlap             |

The defaults in that column are what Preferences ships. Four options deliberately differ in the struct itself, which is what direct callers and the unit tests get: the two measure strategies fall back to Remove and to invisible rests, `mergeVoices` to false so a fixture keeps its voices unless the test asks otherwise, and `tablatureImportMode` to Separate.

## 9.2 Details behind the table

**importPageBreaks** also drives the first-page fit described in §3.8.

**importUnsupportedArticulationsAsText** emits a staff text for the articulation bytes that have no MuseScore equivalent, `0x01`, `0x02`, `0x09` and `0x47` to `0x4A`, which are otherwise dropped.

**underfillMeasureStrategy** chooses invisible rests, visible rests, or shrinking the measure to its content.

**overfillMeasureStrategy** chooses removing trailing notes, stretching them, or extending the measure, all three described in §4.5.

**firstMeasureIsPickup**, when false, bypasses the inference of §4.2 and leaves the first measure at its nominal duration with the leading beats as rests.

That last option has an invariant worth stating on its own. When it is false and the first Encore measure is a Case A pickup, `buildMeasures` sets the first measure's ticks to the nominal signature and must advance the running tick by that same value, not by the shorter Encore duration. If the two diverge, later measures sit at inconsistent positions, and when the irregular strategy then fires during the trailing-gap fill it computes a shift from the nominal value and applies it to positions anchored at the shorter one. Every later measure's internal ticks move away from their barlines, and a spanner placed by measure tick, a volta bracket for instance, lands mid-measure. `Tst_Options.firstMeasure_not_pickup_irregular_volta_at_barline` covers it.

---

# 10. What is not imported

Everything here is a deliberate omission, and each one is recorded so the next reader does not treat it as a bug to be found.

**Beams** are left to the automatic beaming, §7.10.

**MIDI control change** events are decoded for the log and never emitted, §7.9. They are playback data with no notation.

**The articulation bytes with no MuseScore equivalent** are dropped unless the option in §9 is on.

**A dot the note states but the durations contradict** stays as the durations have it, §6.1. That is 380 notes in the corpus, against the 76 the importer does put back.

**The word "Coda"** is not imported because it is not in the file, §4.8.

**A note drawn later than its tick** keeps its tick and can overprint, §5.6.

**One MusicTime document in nine** splits its compound bars, §2.3.

**The `ZBOP` container** is implemented from the keystream alone, since no file carrying that magic has ever been seen, §2.2.

**A file whose first four bytes match no known magic** is declined with the message of §2.6 rather than guessed at. The magic is the whole test, and nothing below it can be read without one.
