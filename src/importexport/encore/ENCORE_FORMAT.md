# Encore (.enc) binary format

Binary format reference for Encore `.enc` files, independent of any implementation.
First documented by Felipe Castro (enc2ly) and Leon Vinken (Enc2MusicXML, GPL v3+).
Extended from a corpus of 5000+ files covering v0xA6 (Enc 2.x), v0xC2 (3.x/4.x), v0xC4 (5.x).

## File structure

| Block              | Description                                  |
|--------------------|----------------------------------------------|
| header (194 bytes) | version, line/page/measure counts            |
| TK00 … TKnn        | one per instrument: name + MIDI program data |
| PAGE               | page geometry                                |
| LINE … LINE        | one per system: staves, clef/key per staff   |
| MEAS … MEAS        | one per measure: notes, rests, ornaments …   |
| PREC *(optional)*  | page-rendering cache                         |
| TITL               | title / subtitle / author / copyright / …    |
| TEXT               | free-text annotations for staff text         |

Every block starts with a 4-byte ASCII magic (`TK00`, `PAGE`, `LINE`, `MEAS`, `TITL`, `TEXT`, `PREC`) followed by a 4-byte varsize.

---

## Format versions

Byte at file offset 4 identifies the version:

| Byte | Version | Encore release                                     |
|------|---------|----------------------------------------------------|
| 0xA6 | v0xA6   | Encore 2.x and MusicTime, ~1991-1999 (app 592)            |
| 0xC2 | v0xC2   | Encore 3.x (app 773, 1993+) and 4.x before 4.5 (app 775)  |
| 0xC4 | v0xC4   | Encore 4.5 (2001) through 5.x (app 0x28 = 1056)        |

v0xA6 carries app version 0x28 = 592. Encore 4.5 opens a v0xA6 file with a font / file-conversion prompt and re-saves it as v0xC4, so a v0xA6 file cannot be produced again from 4.5 or 5.x. Encore 3.0 already uses SCOW with format byte 0xC2 and app version 773 (verified on a genuine Windows Encore 3.0 save), so 0xA6 is Encore 2.x only; the later 0xC2 app version 775 is Encore 4.0-4.2 (pre-4.5).

The v0xC4 format spans Encore 4.5 through 5.x: both write file byte 0x04 = 0xC4 and app version 0x28 = 1056, so neither field identifies the release. The byte at offset 0x3E is a format-revision counter that does, and it is constant for a given Encore build regardless of score content. Observed values: 0 on early 0xC4 files (pre-4.5), 1 on Encore 4.5, 4 on Encore 5.0 (verified against files saved by each installed version). Encore 4.5 refuses a file whose 0x3E revision is newer than it supports, which is why a 5.0-saved file fails to open in 4.5; a 5.0 file also adds a WINI block and embeds the printer DEVMODE. The MuseScore importer keys only on byte 0x04 and ignores 0x3E, so it reads every v0xC4 revision. A corpus of dated saves confirms the era split: 0x3E=1 files run 1999-2008 (Encore 4.5, released November 2001) and 0x3E=4 files never appear before 2009 (Encore 5.0; 5.0.2 was October 2009), so app version 1056 is not unique to 5.x.

File magic at offset 0 (byte order of multi-byte integers follows the magic):

| Magic  | Storage   | Byte order    | Notes              |
|--------|-----------|---------------|--------------------|
| `SCOW` | plaintext | little-endian | Encore 5.x default |
| `SCO5` | plaintext | big-endian    |                    |
| `SCOX` | plaintext |               | rare variant       |
| `SCOR` | plaintext |               | rare variant       |
| `SCOS` | plaintext |               | rare variant       |
| `ZBOT` | encrypted |               | Encore 4.x default |
| `ZBOP` | encrypted |               | encrypted variant  |
| `ZBO6` | encrypted |               | encrypted variant  |

**ZBOT encryption.** Only the first 42 bytes decrypt with a known XOR key.
Beyond that the stream is algorithmically generated and unbroken.
The plaintext `SCOW` equivalent is structurally different, so re-saving from Encore 5 is the only practical path.

---

## Header (194 bytes)

| Offset | Size | Description                                                                |
|--------|------|----------------------------------------------------------------------------|
| 0x00   | 4    | magic (`SCOW` or `SCO5`)                                                   |
| 0x04   | 1    | format version (see above)                                                 |
| 0x28   | 2    | Encore app version (uint16 LE): 592 = 2.x (0xA6); 773 = 3.x, 775 = 4.x<4.5 (0xC2); 1056 = 4.5/5.x (0xC4) |
| 0x2A   | 2    | purpose unconfirmed (possibly total LINE-staff entries)                    |
| 0x2C   | 2    | default beatTicks (uint16 LE): 240 = quarter-note grid; matches MEAS +0x04 |
| 0x2E   | 2    | number of system blocks                                                    |
| 0x30   | 2    | number of pages                                                            |
| 0x32   | 1    | number of instrument blocks                                                |
| 0x33   | 1    | staves per system                                                          |
| 0x34   | 2    | **rendered measure count** (see below)                                     |
| 0x3E   | 1    | **v0xC4 format revision** (see Format versions): 0 = pre-4.5, 1 = 4.5, 4 = 5.0 |
| 0x52   | 1    | **global staff-size selector** for v0xC2/C4 (see below)                    |
| 0x8D   | 1    | **global staff-size selector** for v0xA6 (see below)                       |

Bytes 0x36..0xC1 are padding except 0x3D (constant 0x01 in v0xC4), 0x3E (format revision) and 0x52 / 0x8D.

The selector at 0x52 holds 1-4 (4 = default) for v0xC2/C4 and is only a fallback for when LINE block data is absent.
In Encore 4.x this field is unrelated (values 1-8, no monotone mapping to a size); there per-instrument size always comes from LINE staff entry byte +13.

For v0xA6 the selector is at 0x8D instead, with 1=60%, 2=75%, 3=100%, 4=130%. v0xA6 has no per-staff LINE size, so this single value applies to every staff, and byte 0x52 is unrelated in v0xA6.

**Rendered measure count.** The field at 0x34 is the number of measures Encore displays.
Files can contain extra "ghost" MEAS blocks from prior edits.
Parsers should stop after `header.measureCount` MEAS blocks; extra blocks beyond that count are phantom entries from prior edits (observed: 36 rendered / 56 on disk in one Encore 5 file).

---

## Instrument block (TKnn)

Carries the instrument name as Latin-1 or UTF-16 LE.
Probe: byte 0 printable ASCII + byte 1 == `0x00` → UTF-16 LE; else Latin-1.

**MIDI program.** Layout depends on the TK block size (`varSize`, stored in the 4-byte size field of each TK block):

- **Large-TK** (`varSize > 250`): fixed-offset table after TK blocks:
  ```
  base = 2278    (header 194 + first block 120 + intra-data 1964)
  step = 2158    (block 120 + data block 2038)
  instrument n → file offset  base + n * step
  ```
- **Small-TK Encore 5.x** (`0 < varSize ≤ 250`, varSize = content size only): MIDI is in
  the extra-data region that follows each TK block's named content, at a fixed offset of 76
  bytes into that region; Key is 23 bytes before MIDI (offset +53):
  ```
  contentFilePos = TK_block_start + 8        (after 4-byte magic + 4-byte size)
  stride between blocks = 8 + varSize
  MIDI offset = contentFilePos[n] + varSize + 76
  Key  offset = contentFilePos[n] + varSize + 53
  ```
- **Small-TK Encore 4.x total-size variant** (`varSize = TOTAL block size including 8-byte header`,
  e.g. 112 bytes total / 104 bytes content; detected by stride == varSize, not stride == varSize+8):
  MIDI and Key are at fixed offsets within the 104-byte content block, matching the v0xA6 layout:
  ```
  stride between blocks = varSize (total)
  actual content = varSize - 8 = 104 bytes
  MIDI offset = contentFilePos[n] + 60    (bytes 60-67, value repeated 8 times)
  Key  offset = contentFilePos[n] + 42    (single byte, matches v0xA6 TK layout)
  ```
  Confirmed on an Encore 4.x file (app_version=775) with 6 instruments.
- **v0xA6** (Encore 2.x/4.0 legacy): each TK block is 64 bytes total (`varSize = 64`, 56-byte
  content). MIDI and Key live inside that content:
  ```
  contentFilePos = TK_block_start + 8
  stride between blocks = varSize (total, = 64)
  MIDI offset = contentFilePos[n] + 52    (block offset 60; single byte, 1-indexed GM)
  Key  offset = contentFilePos[n] + 42    (block offset 50; signed int8, octave-only: 0/±12/±24)
  ```
  Note the MIDI byte is at content +52 here, NOT +60 as in the 112-byte 4.x total-size
  variant above. Confirmed across the 10 instruments of a v0xA6 score. A MIDI value at or
  above 113 (GM percussive range) routes the instrument to a drumset.

**Key transposition.** At `base - 23 + n * step` (large-TK/no-TK), or `contentFilePos + varSize + 53` (small-TK 5.x) / `contentFilePos + 42` (small-TK 4.x total-size variant): signed `int8` semitones matching the Encore Staff Sheet "Key" dropdown (`0` = sounds as written, `-12` = octave lower, range ±33 semitones).
Encore shifts every note pitch by this value.

**No-TK-block files.** Some v0xC4 files and many v0xC2 files have no TK blocks at all.
These files fall into three sub-layouts determined by where the first PAGE/LINE/MEAS block starts and whether a `~~~~` compact-table marker is present:

- **Compact v0xC4 layout** (first block ≤ offset 2278): MIDI at offset 390, Key at 367.
  Single-instrument only for Key; multi-instrument Key is not read.
- **Large-TK layout** (first block > offset 2278): MIDI at base 2278, Key at 2255,
  using the same offsets as TK-based files. This handles Encore 5 files exported
  without TK blocks but with the standard instrument metadata tables.
- **Compact v0xC2 layout (two variants).** v0xC2 files (Encore 3.x/4.x) store instruments
  in a fixed 112-byte-entry table. Two sub-variants exist based on whether the file contains
  a `~~~~` (`0x7e7e7e7e`) compact-table marker in the first 1 KiB:

  **Variant A: WITH `~~~~` marker** (files with mixed TK + compact tables):
  ```
  Entry table start  = 281  (after ~~~~ header)
  Entry step         = 112
  Name at entry +33  → COMPACT_NAME_BASE = 314
  MIDI at entry +93  → COMPACT_MIDI_BASE = 374
  ```
  Some instruments also have an explicit "primary block" at `NAME_BASE + n*NAME_STEP`
  (NAME_BASE=202, NAME_STEP=2158). When the first byte at that position is printable ASCII,
  the instrument is a "primary-block instrument" (e.g. "Voz " style). Its MIDI is stored
  at `NAME_BASE + n*NAME_STEP + 60` (not in the compact table entry).

  **Variant B: WITHOUT `~~~~` marker** (compact-table-only files):
  ```
  Entry table start  = 176
  Entry step         = 112
  Name at entry +26  → NAME_BASE = 202  (= 176 + 26)
  MIDI at entry +86  → MIDI_BASE = 262  (= 176 + 86)
  ```
  All instruments are in this linear table. The name for instrument n is at
  `202 + n*112`; MIDI is at `262 + n*112`. No hasPrimaryBlock logic applies.

**Name recovery for no-TK files.** The read position for instrument names depends on the sub-layout (see above).
The canonical constant `NAME_BASE = 202` is the name offset for instrument 0 in both Variant B (202 = entry_base 176 + name_off 26) and in the large-TK / primary-probe path.
The step between consecutive instrument names is resolved at runtime using the same `firstBlockOff > 2278` test that applies to MIDI/Key reading:
- `firstBlockOff > 2278`: step=2158 (large-TK or primary-probe layout)
- otherwise: step=112 (Variant B compact layout)

This step resolution applies only when recovering names from a no-TK file that also lacks a `~~~~` marker.
Files with a `~~~~` marker (Variant A) follow the compact table path regardless of the first-block offset.

**Percussion quirk.** Percussion tracks always report MIDI program 1 (GM Grand Piano); infer the actual kit from the track name.

---

## System block (LINE)

21-byte header (start tick, measure count) + N × 30-byte staff entries (N = staves-per-system from header).

### LINE header (21 bytes)

| Offset | Size | Description                                                |
|--------|------|------------------------------------------------------------|
| +10    | 2    | `start`, 0-based index of the first measure in this system |
| +12    | 1    | `measureCount`, number of measures in this system          |

In `SCO5` (big-endian Encore 5) this single-byte `measureCount` reads as 0 (the meaningful byte sits in the other half of a wider big-endian field), but the `start` indices remain correct.
When `measureCount` is 0, derive each system's length from the gap to the next system's `start` (and to the total measure count for the last system) so line breaks still apply.

### LINE staff entry (30 bytes, repeated N times)

| Offset | Size | Description                                                                  |
|--------|------|------------------------------------------------------------------------------|
| +13    | 1    | **staff display size** (0-indexed): 0=60%, 1=75%, 2=100%, 3=130% (see below) |
| +14    | 1    | clef type                                                                    |
| +15    | 1    | key signature                                                                |
| +16    | 1    | page-row counter (per system; NOT the page number)                           |
| +19    | 1    | visibility: `0x00` = hidden; any non-zero = visible                          |
| +20    | 1    | staff type: `0` = MELODY, `1` = TAB, `2` = RHYTHM (1-line percussion)        |
| +21    | 1    | packed index: bits 0-5 = instrument, bits 6-7 = staff within instrument      |

The staff display size at +13 is populated in both 4.x and 5.x files and is the authoritative per-instrument size source; header byte 0x52 is only a global fallback used when LINE data is absent.
The four values map to the Staff Sheet sizes 60%, 75%, 100% and 130%.

True page count is in `header.pageCount`; page break positions are not yet decoded.

A RHYTHM staff (byte +20 = 2) encodes a single-line percussion staff.
The staff type is constant across all LINE blocks for the same staff position.

### v0xA6 staff size and clef

v0xA6 reports `staffPerSystem = 0` and its LINE staff entries use a 22-byte layout (different from the 30-byte v0xC2/C4 entry above).
Each entry carries a `0x0E 0xFC` marker at offset 16, which bounds the run of entries.
Consequently:
- **Key signature**: the written key index is at **offset 14** of each 22-byte staff entry
  (same index encoding as the v0xC2/C4 key field: 0=C, 1=F, 2=Bb ... 8=G, 9=D, 10=A, 11=E ...).
  This is NOT where v0xC2/C4 keep it; a generic 30-byte parse (which reads `staffPerSystem` as 0)
  misses it, so the key must be read directly from these 22-byte entries. Offset 14 is consistently
  a valid key index (0-14) with a realistic distribution and matches Encore's displayed key.
- **Staff size**: single global value at header byte 0x8D (see Header), applied to all staves.
- **Clef (location not decoded)**: a per-staff clef field has **not been located** in v0xA6. This
  is an open gap, not proof that the clef is absent: Encore renders the correct clefs from these
  files in every version, so the information is available to Encore (either stored per staff in a
  byte not yet decoded, or derived from Encore's own instrument database). This applies to every
  `SCOW` file with format byte `0xA6`, including Encore 4.0 (header app version `592` at offset
  0x28), not just the oldest Encore 2.x files. Searched without finding it: TK content bytes, the
  22-byte LINE staff entries (only staff index and Y-coordinate/layout bytes vary per staff), MEAS
  blocks (no initial CLEF elements), and PAGE blocks (page metadata only). Across 765 v0xA6 files no
  single byte separates F-clef from G-clef staves other than the MIDI-program byte; the nearest
  candidate, TK content+49, reads 7 (`PERC`) on a snare staff but 0 on a bass staff that Encore
  still shows in F, so it is more likely a staff-type field than the clef.

  Whether the clef is **stored** somewhere not yet decoded or **derived from the instrument** is
  unproven. One observation is consistent with the instrument-derived hypothesis but does not prove
  it: a file whose last staff (name "Bajo", MIDI program 59) opens in Encore as the Tuba instrument
  and shows bass clef, which is the Tuba default. This should be investigated once an Encore version
  that writes v0xA6 (Encore 2.x to 4.0) is available: save the same score twice changing only one
  staff's clef and diff the files; a changed byte is the clef field, no change means it is
  instrument-derived. (Encore 5.x always saves as v0xC4, where the clef IS stored explicitly at LINE
  entry +14, so it cannot run this test.)

  *Suggested workaround until decoded*: take the displayed clef from the staff's instrument default,
  or re-save the score in Encore 5 to obtain an explicit per-staff clef.

---

## Measure block (MEAS)

54-byte header + variable element body terminated by `0xFFFF` tick.

### Measure header

| Offset       | Size | Description                                                         |
|--------------|------|---------------------------------------------------------------------|
| 0x00         | 2    | BPM (quarter-note); applies forward until next change               |
| 0x02         | 1    | time-signature glyph (see table below)                              |
| 0x04         | 2    | ticks per beat (beatTicks); standard 240=x/4 (see below)            |
| 0x06         | 2    | total ticks in measure (durTicks)                                   |
| 0x08         | 1    | time-signature numerator                                            |
| 0x09         | 1    | time-signature denominator                                          |
| 0x0C         | 1    | start barline type (see table)                                      |
| 0x0D         | 1    | end barline type (same table)                                       |
| 0x0F         | 1    | repeat-alternative bitmask (see Repeat alternatives)                |
| 0x1A         | 4    | repeat-mark field: low byte = repeat type; upper 3 = position/style |
| 0x10 to 0x35 | 38   | layout data: measure width, x-offsets, "Writer" UTF-16 tag          |

The `beatTicks` standard values are 240 for x/4 meters, 120 for x/8, 480 for x/2, and 360 for a compound dotted-quarter beat.
Some Encore builds store 240 even for 2/2 (the correct value is 480), so do not compute the whole-note tick count from `beatTicks × timeSigDen`; use the constant 960 directly (see Rhythm encoding).

#### Time-signature glyph values

| Value | Meaning                                                                         |
|-------|---------------------------------------------------------------------------------|
| 0x00  | Numeric display, show numerator / denominator digits (e.g. "4/4", "3/4", "6/8") |
| 0x43  | Common time "C"; numerator=4, denominator=4; produced by Encore 3.x / 4.x       |
| 0x63  | Common time "C"; numerator=4, denominator=4; produced by Encore 5.x             |

Values other than 0x00, 0x43, and 0x63 have been observed (0x01, 0x02, 0x06, 0x07) in files with unusual meter strings; treat as numeric display.

#### Barline types

| Value | Meaning        |
|-------|----------------|
| 0     | normal         |
| 2     | repeat start   |
| 3     | double (left)  |
| 4     | repeat end     |
| 5     | final          |
| 6     | double (right) |
| 8     | dotted         |

#### Volta (repeat-alternative) bitmask

Byte 0x0F is a bitmask, bit `n` set means the measure belongs to ending `n+1`.
Encore sets the same bitmask on **every** measure inside the ending (not just the first).
Consecutive measures with the same non-zero bitmask form one multi-ending bracket; the bitmask bit-positions give the ending numbers (bit 0 = ending 1, bit 1 = ending 2, etc.).

#### Repeat-mark ladder (LOW byte of 0x1A)

| Byte | Meaning                                                      |
|------|--------------------------------------------------------------|
| 0x80 | D.C. al Coda                                                 |
| 0x81 | D.S. al Coda                                                 |
| 0x82 | D.C. al Fine                                                 |
| 0x83 | D.S. al Fine                                                 |
| 0x84 | D.S.                                                         |
| 0x85 | "To Coda" source, displays "To Coda", player jumps from here |
| 0x86 | Fine                                                         |
| 0x87 | D.C.                                                         |
| 0x88 | Segno marker                                                 |
| 0x89 | Coda destination, displays Coda glyph, player jumps to here  |

`0x85` (CODA1) and `0x89` (CODA2) are paired: `0x85` → To-Coda navigation point, `0x89` → Coda target.
Mapping both to CODA was a bug.
ORN subtype `0xA5` and repeat-mark `0x85` are parallel encodings of "To Coda"; both encode a To-Coda navigation point.

#### BPM semantics

Quarter-note BPM regardless of time signature.
In 3/8, 5/8, etc., Encore's UI shows eighth-BPM (= 2× on-disk value), but the binary always stores quarter-BPM.
An unrelated layout field at +0x18 always holds 200 in v0xC4 files, do not confuse with BPM.

---

### Element body

Each element begins with a fixed 5-byte header:

| Offset | Size | Field                                                              |
|--------|------|--------------------------------------------------------------------|
| +0     | 2    | tick (uint16 LE); a tick of `0xFFFF` terminates the element stream |
| +2     | 1    | type/voice byte (high nibble = type, low nibble = voice)           |
| +3     | 1    | size (total element length in bytes, counted from +0)              |
| +4     | 1    | staff byte (see encoding below)                                    |

The element body follows at +5. **Every element table in this document gives offsets relative to the element start (+0 = the first tick byte), so the first body byte is always +5.**

**Staff byte encoding** (same format as `instrStaffIdx` in the LINE block):

| Bits | Mask   | Meaning                                                                   |
|------|--------|---------------------------------------------------------------------------|
| 0-5  | `0x3F` | Instrument index: 0-based (= bits 0-5 of LINE `instrStaffIdx`; see below) |
| 6-7  | `0xC0` | Staff-within-instrument (`staffWithin`): which staff of the instrument    |

`staffWithin = staffByte >> 6`.
Values: 0 = first staff, 1 = second (bass), 2 or 3 = further staves.

The instrument index in bits 0-5 equals the LINE slot only when every instrument has exactly one staff; for multi-staff instruments it does not.

The raw staff byte `(staffWithin<<6)|instrIdx` is identical to `instrStaffIdx` stored in the LINE block for the target staff.
Readers resolve the byte to a global LINE slot by inverse-lookup through the LINE block's `instrStaffIdx` array.

For a piano grand staff (single instrument), notes on the treble staff use `staffWithin = 0`; notes on the bass staff use `staffWithin = 1`.
All notes in the MEAS stream share `instrIdx = 0` (the instrument's sequential index) and the `staffWithin` field distinguishes the destination.
The voice field (low nibble of the type/voice byte) is distributed across staves: voices 0-1 belong to `staffWithin=0`, voices 2-3 to `staffWithin=1`.

For multi-instrument scores where earlier instruments have more than one staff, subsequent instruments still use their sequential instrument index (not the global LINE slot) as bits 0-5. For example in a piano+organ score (both grand staff), organ notes carry `instrIdx=1` even though organ's first LINE slot is 2.

| Type | Name      |
|------|-----------|
| 0    | NONE      |
| 1    | CLEF      |
| 2    | KEYCHANGE |
| 3    | TIE       |
| 4    | BEAM      |
| 5    | ORNAMENT  |
| 6    | LYRIC     |
| 7    | CHORD     |
| 8    | REST      |
| 9    | NOTE      |

Type 4 (BEAM) is parsed but intentionally not modeled by the importer: MuseScore auto-beams from note durations and the time signature, so Encore's explicit beam groups are dropped. See BEAM element below.

**Type 0xB (MIDI CC events).** Elements with high nibble 0xB are MIDI Control Change events stored inline for playback-only use; they have no visual representation.
Observed always with size=12 (total 12 bytes from element start).
Byte layout: ``` d[0..1] tick (uint16 LE) d[2] typeVoice = 0xBn (type=11, voice=n) d[3] size = 12 d[4] MIDI channel / track index d[5] MIDI CC event marker (0xB0 = CC channel-0) d[6..9] zeros d[10] MIDI CC controller number (64=sustain pedal, 7=volume, 1=modulation) d[11] MIDI CC value (127=max/on, 0=off) ``` Examples observed: `40 7F` (sustain pedal ON), `40 00` (sustain off), `07 6A` (volume 106).
These elements carry no notation data useful to a score parser; skip them.
Playback uses its own expression/velocity system.

### Multi-stream voices

One Encore voice slot can contain multiple interleaved MIDI tick streams (e.g. from live recording).
Secondary streams are detectable at tick level: a backwards tick, or a non-chord event arriving after the voice is already full, signals a fresh stream.

### Implicit silences

Encore does **not** always emit explicit REST elements.
A gap between two consecutive same-voice events where `event.tick > prevEvent.tick + prevFaceValueTicks` represents silence the user wrote as a rest.

**Detection threshold:** gaps ≥ 8 ticks are treated as intentional rests.
Gaps < 8 ticks are MIDI timing slop (quantisation jitter) and should be snapped to the nearest on-grid position rather than turned into rests.

---

## CHORD element

Type 7. Variable size.
Encodes a chord symbol (harmony marking) above the staff.

### Byte layout

| Offset | Size | Field     | Description                                                        |
|--------|------|-----------|--------------------------------------------------------------------|
| +5     | 1    | `toniko`  | Chord quality type (index 0-62 into the quality table below)       |
| +6     | 1    | `tipo`    | Flags: bit 0 = text present, bit 1 = bass note present             |
| +7-9   | 3    | ,         | skipped                                                            |
| +10    | 1    | `xoffset` | Horizontal display offset                                          |
| +11    | 1    | ,         | skipped                                                            |
| +12    | 1    | `radiko`  | Root note (see note encoding below)                                |
| +13    | 1    | `baso`    | Bass note (encoding as `radiko`; valid only when `tipo & 0x02`)    |
| +14    | 36   | `teksto`  | Chord text slot (present when `tipo & 0x01`; UTF-16 LE or Latin-1) |

Trailing bytes (beyond `+14` when no text, beyond `+50` when text is present) are skipped using the element `size` field.

### Root note encoding (`radiko` / `baso`)

`lower nibble (bits 0-3)` = note name: 0=C, 1=D, 2=E, 3=F, 4=G, 5=A, 6=B

`upper nibble (bits 4-7)` = accidental: 0x0=natural, 0x1=sharp, 0x2=flat

Examples: `0x05`=A, `0x26`=Bb, `0x13`=F#, `0x21`=Db.

### Chord quality table (`toniko`)

When `tipo & 0x01` is set, `teksto` overrides `toniko` and `radiko` (the chord name is taken directly from the text field).
When `tipo & 0x01` is clear, the chord name is constructed as `root + quality` from the table below.

| Index | Quality suffix     | Index | Quality suffix |
|-------|--------------------|-------|----------------|
| 0     | (major, no suffix) | 32    | 9              |
| 1     | m                  | 33    | 9(b5)          |
| 2     | +                  | 34    | 11             |
| 3     | dim                | 35    | 13             |
| 4     | 7                  | 36    | 13(b5)         |
| 5     | 5                  | 37    | 13(b9)         |
| 6     | 6                  | 38    | 13(#9)         |
| 7     | 6/9                | 39    | (undefined)    |
| 8     | (add2)             | 40    | +7             |
| 9     | (add9)             | 41    | +7(b9)         |
| 10    | (omit3)            | 42    | +7(#9)         |
| 11    | (omit5)            | 43    | +9             |
| 12    | maj7               | 44    | sus2           |
| 13    | maj7(b5)           | 45    | sus2sus4       |
| 14    | maj7(6/9)          | 46    | sus4           |
| 15    | maj7(#5)           | 47    | 7sus4          |
| 16    | (undefined)        | 48    | 9sus4          |
| 17    | maj9               | 49    | 13sus4         |
| 18    | maj9(b5)           | 50    | m(add2)        |
| 19    | maj9(#5)           | 51    | m(add9)        |
| 20    | (undefined)        | 52    | m6             |
| 21    | maj13              | 53    | m6/9           |
| 22    | maj13(b5)          | 54    | m7             |
| 23    | (undefined)        | 55    | m(maj7)        |
| 24    | 7 (alternate)      | 56    | m7(b5)         |
| 25    | 7(b5)              | 57    | m7(add4)       |
| 26    | 7(b9)              | 58    | m7(add11)      |
| 27    | 7(#9)              | 59    | m9             |
| 28-31 | (undefined)        | 60    | m(maj9)        |
|       |                    | 61    | m11            |
|       |                    | 62    | m13            |

Indices 16, 20, 23, 28-31, 39 are undefined; treat as major (empty quality suffix).
Index 45 encodes "sus2,sus4"; use "sus2sus4" (no comma) to avoid chord-parser conflicts.

---

## CLEF element

Type 1. Variable size (typically 16 bytes).
Encodes a mid-measure clef change on a staff.

Byte +5 holds the clef type using the same encoding as the LINE staff entry byte at content offset +14 (see LINE block above):

| Value | Clef       |
|-------|------------|
| 0     | G (treble) |
| 1     | F (bass)   |
| 2     | C3 (alto)  |
| 3     | C4 (tenor) |
| 4     | G8va       |
| 5     | G8vb       |
| 6     | F8vb       |
| 7     | Percussion |
| 8     | Tab        |

The clef element applies to the staff indicated by its `staffIdx` field (low 6 bits of byte +4).
Bytes +6 onward are padding; skip using the `size` field.

A CLEF element does not take effect at its own stored tick.
It takes effect before the note or rest that physically follows it in the element stream on the same staff: the clef is drawn in front of that note, regardless of the tick value the clef element carries (Encore frequently stamps the clef with an earlier tick, e.g. the tick of the preceding beat).
The new clef therefore governs from that following note onward.

When a CLEF element is the last element of the measure on its staff (no note or rest follows it), it is a cautionary clef that takes effect on the downbeat of the next measure; Encore draws it just before the final barline of the current measure.

A CLEF element's tick position also acts as a duration boundary for any preceding rest on the same staff: a rest that would otherwise fill the measure gap is capped at the CLEF's tick.

---

## KEYCHANGE element

Type 2. Size 6 bytes.
Byte at +5 = key index into the fifths table:
- 0 = C / 0 fifths
- 1 to 7 = F..Cb / −1..−7 fifths
- 8 to 14 = G..C# / +1..+7 fifths

Value 0 is a legitimate change (naturals cancel prior accidentals).

---

## TIE element

Type 3. Size 16 or 18 bytes.

Byte +5 is a signed arc-curvature value (the vertical bow of the slur arc), NOT a bitfield; byte +6 is a tie-start flag.

| Byte +5 | Signed value | Arc curvature |
|---------|--------------|---------------|
| `0x02`  | +2           | curve down    |
| `0x04`  | +4           | curve down    |
| `0xFE`  | -2           | curve up      |
| `0xFC`  | -4           | curve up      |

All four values mark a real outgoing tie.
Treating +5 as a bitfield is wrong: a rule like `(+5 & 0x80) || (+5 & 0x02)` happens to catch `0x02`, `0xFC`, and `0xFE` but silently drops the equally-valid `0x04` (which sets neither bit 7 nor bit 1).
In a multi-staff chord, staves whose ties curve down (`0x04`) lost their ties while staves curving up (`0xFC`) kept theirs.

**Arc x-positions (18-byte elements only).** For size ≥ 18, two additional bytes encode the visual x-positions of the arc endpoints, and these, not the +5 curvature, are the authoritative forward-tie signal:

| Offset | Size | Description                                                     |
|--------|------|-----------------------------------------------------------------|
| +10    | 1    | `arcX1`, x-position of arc start (measure-relative pixel units) |
| +12    | 1    | `arcX2`, x-position of arc end                                  |
| +14    | 1    | `sourcePosition`, staff position of the source note (see below) |

`sourcePosition` matches the note's own staff-position field and disambiguates which note in a multi-note chord carries the tie.

**Forward-tie rule (18-byte form).**

- `arcX1 < arcX2`: genuine left-to-right horizontal span, a real forward tie, regardless of
  the +5 curvature value.
- `arcX1 == arcX2`: zero horizontal extent. This is an intra-chord decorative mark (Encore
  connects two notes of the same chord vertically) and is NOT a forward tie, unless byte +6
  has bit 7 set, which marks a cross-measure tie whose destination lives in the next measure
  and for which Encore stores `arcX2 = arcX1` as a placeholder. Intra-chord arcs often appear
  in groups of 2 to 4 identical copies at the same tick (one per chord note).

**Tie-start flag (byte +6).** Bit 7 set marks a tie-start explicitly; it overrides the `arcX1 == arcX2` intra-chord suppression (the cross-measure placeholder case above).

**16-byte form.** When no arc x-positions are stored, tie-start falls back to the byte-level signal `(+5 & 0x80) || (+5 & 0x02) || (+6 & 0x80)`.

**Receiver resolution.** A TIE element marks only the start note; the format has no matching tie-stop element.
The receiver is the next note of the same pitch in the same staff and voice.
Because that match is by pitch alone, a tie-start whose intended neighbour is missing (for example a tie-start written on a note that is not actually followed by the same pitch) would otherwise pair with a far-away later note and draw an arc spanning several measures.
A genuine tie always joins consecutive notes: the receiver must be the first note that follows the start note in that voice, with only rests allowed in between.
When any other note (of any pitch) sits between the start note and the same-pitch candidate, the tie-start is spurious and produces no tie.

---

## Ornament element

Type 5. Variable size.
Offsets from element start:

| Offset | Size | Description                                                             |
|--------|------|-------------------------------------------------------------------------|
| +5     | 1    | ornament subtype (see table)                                            |
| +10    | 1    | `xoffset`, start x-position within the measure (layout units)           |
| +11    | 1    | skipped (high byte of the layout x, ignored)                            |
| +12    | 2    | signed s16 Cartesian y (negative = below staff)                         |
| +16    | 1    | altMezuro (v0xC2 only): measures forward to slur end (see caveat below) |
| +18    | 1    | alMezuro, measures forward to the end measure                           |
| +20    | 1    | xoffset2, end x-position in the target measure                          |
| +26    | 1    | speguleco, bit 0: 0 = crescendo, 1 = diminuendo                         |
| +28    | 1    | noto, tempo beat unit (TEMPO subtype); see below                        |
| +30    | 2    | BPM (TEMPO subtype, v0xC4); see below                                   |
| +32    | 1    | TEXT block entry index (STAFFTEXT subtype)                              |

**Tempo beat unit (+28, `noto`).** Low 7 bits = note value (0 = whole, 1 = half, 2 = quarter, 3 = eighth, ...) and high bit 0x80 = dotted, so 0x02 is a quarter, 0x82 a dotted quarter, 0x81 a dotted half.
A value of 0 (or an out-of-range byte from an older format) means no explicit unit.

**Tempo BPM.** In v0xC4 the BPM is at +30, expressed in the beat unit from +28. v0xC2 (Encore 3.x/4.x) has two layouts. Newer files match v0xC4: the BPM is at +30 and +28 holds a beat-unit code (low 7 bits in 0..6). Older files store the BPM directly at +28 and leave a constant unrelated byte (observed 0x34 = 52) in the +30 slot. Distinguish by +28: when it is a valid beat-unit code the BPM is at +30; otherwise the +28 byte itself is the BPM.

The TEXT block entry index at +32 is present only when the element is at least 33 bytes long.
In shorter ornaments (notably v0xC2 size-32 STAFFTEXT elements) the +32 slot does not exist, and the entry index is read from the +30 slot instead, sharing it with the tempo byte.

In v0xA6 the ornament is compact (declared size 15, i.e. a 30-byte slot) and the STAFFTEXT entry index lives at a fixed offset +26 from the type/voice byte, not at the size-based slot above. Its signed s16 Cartesian y (the above/below placement, negative = below staff) is likewise compact: it sits at +6 from the type/voice byte, not at the +12-from-element-start slot used by the later formats. Reading it at the later offset lands on an unrelated byte, so a v0xA6 staff text that Encore places below the staff would otherwise import above it.

### Ornament subtypes

| Value | Name          | Notes                                                            |
|-------|---------------|------------------------------------------------------------------|
| 0x1D  | WEDGESTART    | hairpin; end encoded by alMezuro (+18) and xoffset2 (+20)        |
| 0x1E  | STAFFTEXT     | text from TEXT block at entry index +32                          |
| 0x21  | SLURSTART     | slur; endpoint encoded by alMezuro and xoffset2                  |
| 0x22  | ARPEGGIO      | chord arpeggio                                                   |
| 0x32  | TEMPO         | tempo mark; BPM at +30 in the beat unit at +28 (`noto`)          |
| 0x35  | TRILL_END     | end of trill+wavy-line span; no visible glyph                    |
| 0x36  | TRILL_START   | trill span start (tr + wavy line); see trill notes               |
| 0x37  | TRILL_ALT     | secondary trill mark within a span; plain trill glyph            |
| 0xB0  | TRILL_TR      | standalone 16-byte "tr" mark; plain trill, never a span          |
| 0xB6  | TRILL_SHORT   | standalone 16-byte short-trill mark; never a span                |
| 0x41  | SLURSTOP      | reserved, not emitted in practice                                |
| 0x4D  | WEDGESTOP     | reserved, not emitted in practice                                |
| 0x80  | DYN_PPP       | dynamic `ppp` (size-16)                                          |
| 0x81  | DYN_PP        | dynamic `pp`                                                     |
| 0x82  | DYN_P         | dynamic `p`                                                      |
| 0x83  | DYN_MP        | dynamic `mp`                                                     |
| 0x84  | DYN_MF        | dynamic `mf`                                                     |
| 0x85  | DYN_F         | dynamic `f`                                                      |
| 0x86  | DYN_FF        | dynamic `ff`                                                     |
| 0x87  | DYN_FFF       | dynamic `fff`                                                    |
| 0x88  | DYN_SFZ       | dynamic `sfz`                                                    |
| 0x89  | DYN_SFFZ      | dynamic `sffz`                                                   |
| 0x8A  | DYN_FP        | dynamic `fp`                                                     |
| 0xA2  | SEGNO         | segno marker on the measure                                      |
| 0xA5  | TO_CODA       | "To Coda" navigation point                                       |
| 0xA6  | CODA          | Coda glyph marker                                                |
| 0xAA  | DYN_FZ        | dynamic `fz`                                                     |
| 0xAB  | DYN_SF        | dynamic `sf`                                                     |
| 0xAF  | TREMOLO_32    | single-chord triple tremolo (3 slashes = 32nd); always voice 0   |
| 0xB9  | FINGER_1      | stand-alone fingering digit "1" (size-16 ORN)                    |
| 0xBA  | FINGER_2      | stand-alone fingering digit "2"                                  |
| 0xBB  | FINGER_3      | stand-alone fingering digit "3"                                  |
| 0xBC  | FINGER_4      | stand-alone fingering digit "4"                                  |
| 0xBD  | FINGER_5      | stand-alone fingering digit "5"                                  |
| 0xC4  | UPBOW/ACCENT  | v0xC4: up-bow (V). v0xC2: accent (>), ORN 0xC4 (see below)       |
| 0xC5  | DOWNBOW       | down-bow stroke (П), size-16 ORN                                 |
| 0xC9  | STACCATO      | per-chord staccato dot                                           |
| 0xCC  | FERMATA_ABOVE | standalone fermata above (size-16 ORN; yoffset > 0)              |
| 0xCD  | FERMATA_BELOW | standalone fermata below (size-16 ORN; yoffset < 0)              |
| 0xEF  | TREMOLO_32B   | alternate triple tremolo (ORN at tick == durTicks); maps to R32  |
| 0xA3  | REPEAT_MEAS   | "%" repeat-last-bar glyph (size-16 ORN)                          |
| 0xA7  | CAESURA       | caesura (//) breath element after preceding note                 |
| 0xA8  | BREATH_COMMA  | comma breath mark after preceding note                           |
| 0xBE  | ACCENT        | standalone accent (>), size-16 ORN; v0xC4 only (v0xC2 uses 0xC4) |

**Trill spans and standalone trills.** A TRILL_START (0x36) opens a trill + wavy-line span when a TRILL_END (0x35) or a non-zero alMezuro is present; otherwise it is a plain trill glyph.
TRILL_TR (0xB0) and TRILL_SHORT (0xB6) are standalone 16-byte marks (a plain trill and a short trill), never spanners.
For TRILL_SHORT, Encore also stores a secondary wavy-line-extent element with the same tipo whose `xoffset` sits well to the left (more than about 20 px) of the note at its encoded tick.

**Accent and bow placement (0xBE, 0xC4, 0xC5).** In v0xC2 the accent is stored as an ORN (tipo 0xC4) rather than a NOTE articulation byte, because size-22 notes have no articulation slot and size-24 notes use +22 only for staccato/tenuto.
Tipo 0xC4 (v0xC2) and 0xBE (v0xC4) therefore denote the same accent.

These marks carry a `voice` byte that is always 0 regardless of which voice the annotated note is in, so the target note may be in any voice of the ORN's staff (or the sibling staff).
The position is the raw Encore tick (`measStartTick + orn.tick`); the cumulative placement tick is unreliable here because it stays at 0 when voice 0 has no notes on the staff.
The same applies to DOWNBOW (0xC5) and UPBOW (0xC4).

**Previously undecoded subtypes, now decoded** (confirmed by opening files in Encore 5):

| Tipo       | Name                   | Notation output                                         |
|------------|------------------------|---------------------------------------------------------|
| 0x10       | OTTAVA_ALTA            | 8va line above staff (size-16 ORN); see below           |
| 0x12       | OTTAVA_BASSA           | 8vb line below staff; same rules as 0x10                |
| 0x1C       | GRAPHIC_LINE           | user-drawn graphic line; no musical meaning; skipped    |
| 0x28       | GUITAR_BEND            | guitar bend, curved arrow up (size=28 spanner); skipped |
| 0x29       | GUITAR_BEND_2          | guitar bend, curved arrow (size=28 spanner); skipped    |
| 0x2A       | GUITAR_PREBEND         | guitar prebend (size=28 spanner); skipped               |
| 0x2B       | GUITAR_PREBEND_RELEASE | guitar prebend-release (size=28 spanner); skipped       |
| 0x30       | GUITAR_BEND_V          | guitar V-shape bend (size=28 spanner); skipped          |
| 0xB8       | DOUBLE_MORDENT         | double lower mordent                                    |
| 0xBF       | MARCATO                | marcato accent (^, vertex up); standard marcato         |
| 0xC0       | MARCATO_STACCATO_BELOW | marcato + staccato below (accent with staccato dot)     |
| 0xC6       | MARCATO_BELOW          | marcato below (inverted marcato, vertex down)           |
| 0xC8       | TENUTO                 | tenuto dash above                                       |
| 0xE6, 0xE7 | TREMOLO_8              | 1-slash tremolo (eighth-note speed)                     |
| 0xEE       | TREMOLO_16             | 2-slash tremolo (sixteenth-note speed)                  |
| 0xE9, 0xEA | TREMOLO_64             | 4-slash tremolo (sixty-fourth-note speed)               |

**Ottava endpoints.** Ottava elements store no explicit endpoint.
Byte +14 is the visual right edge of the "8va" text box (a cosmetic constant, around 12 px), and alMezuro (+18) falls outside the element, reading garbage from the next element's typeVoice byte (large values that clamp to the end of the score).
The endpoint is therefore resolved in a post-pass: the start of the next ottava on the same staff, or the end of the score when no successor exists.

### Hairpin direction (speguleco bit 0)

Bit 0 of `speguleco` at +26: 0 = crescendo, 1 = diminuendo.
Encore 5 also sets bit 1 (crescendo = `0x02`, diminuendo = `0x03`); legacy files use `0x00`/`0x01`.
Always test with `speguleco & 0x01`, not `speguleco == 0`.

### Spanner endpoints (hairpins, slurs)

alMezuro (+18) = count of measures forward to the end measure. xoffset2 (+20) = visual x within that target measure.
No separate WEDGESTOP or SLURSTOP element is emitted.

**v0xC2 caveat.** In v0xC2 files the +18 alMezuro slot is unreliable and often holds stale or zero values, so for non-slur ornaments (hairpins, trills) it should be ignored in favor of the xoffset heuristic.
**Slurs are the exception:** in v0xC2 the slur's forward measure-count lives at +16 (altMezuro), not +18, and is reliable (including the value 0, which means a within-measure slur).
The absolute slur xoffset2 in v0xC2 lives in a stale ornament-coordinate origin and must not be matched directly; use the +16 count instead (see Slur endpoint below).

**Hairpin endpoint.** Three-tier resolution:
1. **Next-dynamic** (primary): walk forward for the first Dynamic on the same track within the alMezuro
   window and stop there.
2. **xoffset2 note snap** (fallback): scan the target measure for the last NOTE/REST with
   `xoffset <= xoffset2`. End the hairpin at that note's tick.
3. **Bar-line clamp** (when no note found in step 2): if xoffset2 precedes all notes in the target
   measure, clamp to targetMeasure.tick.
Notes with xoffset == 0 are ignored in steps 2-3 (synthetic fixture guard).

**Slur endpoint.** Resolution depends on the format, because the reliability of the stored coordinates differs:

- **v0xC4 / SCO5 (pixel-span heuristic).** `slurXoffset2 - slurXoffset` equals
  `endNote.xoffset - firstNote.xoffset`. Recover end tick via
  `target = firstNote.xoffset + (slurXoffset2 - slurXoffset)` and snap to the nearest note.
  This applies when alMezuro == 0 (same-measure). For cross-measure slurs (alMezuro > 0), compare
  each note in the target measure against xoffset2 directly and pick the closest. The
  last-note/rest fallback only fires when no note in the target measure can be found.

- **v0xC2 (reliable +16 count).** The absolute xoffset2 is stale here, so it is not matched.
  Instead the +16 measure-count drives resolution:
  - count > 0 (cross-measure): the slur is a note-1 → note-1 arc between bar starts; anchor the
    end to the downbeat (first chord) of measure start+count, with both endpoints set explicitly.
  - count == 0 (within-measure): a tiny pixel span (|slurXoffset2 - slurXoffset| ≤ 2) means a
    short note-to-next-note slur, so the end is the next note on the staff after the start; a
    grace note co-located at the start instead resolves grace-to-main (zero span).

`xoffset` is stored as a signed byte but must be read as unsigned for the pixel-span computation: values > 127 are stored negative (e.g. 0x8A = -118 signed = 138 unsigned).
Using signed arithmetic gives a huge spurious pixel span; unsigned gives the correct 1-2 note span.

**startEncTick formula.** To map the slur's start note position back to an Encore tick (for finding `firstNoteXoff`): use `wt = durTicks × timeSigDen / timeSigNum` (whole-note ticks), NOT `beatTicks × timeSigDen`.
In compound meters (e.g. 6/8 with beatTicks=240, durTicks=720): `beatTicks × timeSigDen = 240 × 8 = 1920 ≠ wt = 720 × 8/6 = 960`.
Using the wrong formula shifts `firstNoteXoff` to the wrong note and causes slurs to end too late.

### Dynamic staff displacement (yoffset > 0)

Normally `yoffset < 0` (below the staff).
When the user drags a dynamic up onto the staff above, `yoffset` becomes positive while `staffByte` still names the lower staff.
Correct target staff = `staffIdx - 1` when `yoffset > 0`.

### End-of-measure ornament ticks

A DYN or STAFFTEXT with `tick > durTicks` is a section-end marker (e.g. a 2/4 measure with volta dynamics at tick 0 and tick 960).
Pin these to the last note/rest of the source measure.

### Snap-back-by-xoffset (DYN and WEDGESTART)

Encore tags an ornament at the chord-rest AT OR AFTER its visual position but stores the visual x in `xoffset`.
When `ornament.xoffset < tagged_chord.xoffset`, Encore visually pulled the glyph back.
Walk backwards on the same (staffIdx, voice) for the latest NOTE/REST with `xoffset <= ornament.xoffset` and re-anchors there.

### Encoding probe (all text-bearing fields)

| Field                    | Probe                                                       |
|--------------------------|-------------------------------------------------------------|
| TK block instrument name | byte 0 printable + byte 1 == 0x00 → UTF-16 LE; else Latin-1 |
| TK fallback (NAME_BASE)  | same probe; falls back to Latin-1                           |
| LYRIC element            | byte 0/1 probe at text start                                |
| TEXT block entries       | byte 14/15 probe; lines split on `0x04 0x00`, ends at null  |
| CHORD-symbol text        | byte 0/1 probe                                              |
| TITL block               | varsize < 5000 → Latin-1; varsize ≥ 10000 → UTF-16          |

### v0xA6 grace note time-borrowing

In v0xA6, grace notes occupy real tick positions (not co-located with the main note).
A 32nd grace at tick=120 pushes subsequent notes forward; the last real note ends up with `rawGap < faceValue`.

**Detection rule:** if `Σ graceFaceValueTicks == faceValueTicks − rawGap`, the note's written duration is `faceValueTicks` (not `rawGap`).
Restore it before computing measure position.

**Inner grace detection.** After a leading grace (grace1 & 0x30 == 0x20), inner graces (grace1 & 0x30 == 0x10) have a strictly larger faceValue (shorter note).
Only applies when `fv > maxFvInQueue`.

**Face-grid snap suppression.** The implicit-silence snap must be suppressed while graces are pending (prevents spurious rests before the grace group) and also for subsequent notes whose apparent gap equals the cumulative grace ticks already borrowed for that staff/voice.

### Grace-note slurs (SLURSTART co-located with appoggiatura)

A SLURSTART ornament (tipo 0x21) can be stored at the same Encore tick as an appoggiatura grace note: both reference the same beat (tick 0 of that beat in Encore), because a grace note shares the written tick of its parent chord and does not advance the cumulative position.
A slur that begins on such a grace therefore has no distinct start tick of its own in the byte stream.

**Serialization order.** In v0xC4, Encore 5 writes the MAIN note BEFORE its grace note at the same beat; in v0xC2 the grace is written first.
A reader that sees a main note immediately followed by a grace at the same tick (the gap `tick − prevTick` is below about 8) is looking at a grace that belongs to the already-written main note, not a prefix to the next note.

### Multi-staff instruments: staffWithin field

For instruments with more than one staff (piano, harp, organ), all notes from all staves of that instrument share the same MEAS element stream.
The destination staff is encoded in bits 6-7 of the staff byte (`staffWithin = staffByte >> 6`):

- `staffWithin = 0`: note belongs to the first (treble) staff.
- `staffWithin = 1`: note belongs to the second (bass) staff.
- `staffWithin = 2` or `3`: third or fourth staff (uncommon).

All notes of a given instrument carry that instrument's sequential index in bits 0-5. For a single piano grand staff (instrument 0), bits 0-5 = 0. For a piano+organ score where organ is instrument 1, all organ notes carry bits 0-5 = 1 regardless of how many LINE slots piano occupies.

For a 2-staff instrument, Encore stores voices 0-1 for the first staff and voices 2-3 for the second staff in the shared stream (voices 2-3 belong to the second staff and are renumbered 0-1 there).

### System-level ornaments (voice = 4)

System-wide ornaments use `voice = 4` AND set the high bit of the staff byte (`0x40`).
This sets `staffWithin = 1` (second staff) on the raw byte.
Readers that support `staffWithin` routing must check `voice >= 4` first to distinguish system ornaments from regular second-staff notes; do not route voice-4 ornaments to the second instrument staff.

Some files store NOTE/REST/BEAM with `voice = 4` WITHOUT a valid staffWithin relationship (seen in v0xC4 SATB scores where voice 4 is an out-of-band grand-staff slot).
The correct interpretation depends on the LINE block's multi-staff configuration.

### Out-of-range voice on regular elements

Some files store NOTE/REST/BEAM with `voice = 4` WITHOUT the `0x40` staff-byte flag.
These are real content; treat them as voice 0 of that staff for attachment purposes.

---

## Lyric element

Type 6. Variable size.
Null-terminated text, NOT fixed-width.

**v0xC4 layout (text at +20):**

| Offset  | Size | Description                                          |
|---------|------|------------------------------------------------------|
| +0      | 2    | within-measure tick                                  |
| +2      | 1    | type/voice byte (high nibble = 6, low = voice)       |
| +3      | 1    | element size (24..36+)                               |
| +4      | 1    | staffIdx & 0x3F                                      |
| +0x0A   | 1    | text anchor (x-offset equivalent)                    |
| +0x14.. | var  | text payload (UTF-16 LE or Latin-1, null-terminated) |

**v0xC2 layout (text at +18, 2 bytes earlier):**

| Offset  | Size | Description                                          |
|---------|------|------------------------------------------------------|
| +0      | 2    | within-measure tick                                  |
| +2      | 1    | type/voice byte (high nibble = 6, low = voice)       |
| +3      | 1    | element size (20..26+)                               |
| +4      | 1    | staffIdx & 0x3F                                      |
| +0x0A   | 1    | text anchor (x-offset equivalent)                    |
| +0x12.. | var  | text payload (UTF-16 LE or Latin-1, null-terminated) |

**v0xA6 layout (compact, text at +6):**

Encore 2.x uses a much smaller lyric element. There is no text-anchor-plus-gap run: a single control
byte sits immediately after the staff byte, then the null-terminated text follows. Like every v0xA6
measure element, the on-disk slot is twice the declared size.

| Offset | Size | Description                                          |
|--------|------|------------------------------------------------------|
| +0     | 2    | within-measure tick                                  |
| +2     | 1    | type/voice byte (high nibble = 6, low = voice)       |
| +3     | 1    | element size (declared; on-disk slot is size x 2)    |
| +4     | 1    | staffIdx & 0x3F                                      |
| +5     | 1    | control byte (hyphen/anchor; not otherwise used)     |
| +6..   | var  | text payload (Latin-1, null-terminated)              |

Text offset: +0x14 (v0xC4), +0x12 (v0xC2), or +6 (v0xA6). Reading the newer offset against a v0xA6
element lands past its tiny slot and yields empty text, so every Encore 2.x lyric would be dropped.

Observed sizes in v0xC4: 24 (`-` dash), 26 (empty word-break), 30 (2 chars), 32 (3 chars), 34 (4 chars).
Observed sizes in v0xC2: 20 (`-` dash), 22 (1-2 chars), 24 (3 chars), 26 (4-5 chars).
Observed sizes in v0xA6: 5-8 (declared), i.e. 10-16 bytes on disk for 2-6 character syllables.

**Encoding.** Detected per element via byte 0/1 probe (same as instrument names).
Portuguese/Spanish scores from older Encore builds use Latin-1.

**Separator tokens.**

| Text       | Role                                      |
|------------|-------------------------------------------|
| `"-"`      | hyphen between syllables of the same word |
| `""` empty | word-break (resets hyphen state)          |
| other      | real syllable                             |

Syllabic role (begin/middle/end/single) derived from hyphen-before / hyphen-after flags.

A `"-"` can open the measure *after* the syllable it follows (the word breaks across a barline, with the first syllable ending one measure and `"-"` plus the next syllable opening the following one).
By then the previous measure's syllable is already placed, so the hyphen must promote that earlier syllable's role (single becomes begin, end becomes middle) for the hyphen to render across the bar.

**Multi-verse.** Verse N uses voice (N−1) on the same staff.
All verses anchor on the voice-0 chord.
Encore writes the first verse (voice 0) with a correct per-syllable tick, but every later verse
stores tick=0 on ALL its syllables; only the x-offset (`textAnchor`, +0x0A) distinguishes their
positions, and it matches the first verse's x-offsets syllable for syllable. A tick-only match
therefore collapses the later verses onto the first notes; the x-offset is the reliable anchor, so
a collapsed verse must be positioned by mapping each syllable's x-offset to the first verse's
x-offset→tick pairs. (The syllables are also not necessarily stored in x-offset order.)

**Lyric-to-note matching.** Each lyric's `textAnchor` (+0x0A) is a visual x-offset, not a tick.
Match each lyric to the nearest note by tick within a half-beat window:

- Simple meters (x/4, x/2): `matchThreshold = beatTicks / 2`
- Compound meters (6/8, 9/8, 12/8): `beatTicks` encodes the dotted-quarter compound beat (360 for 6/8);
  the effective quarter-note tick is `beatTicks × 2/3` (240), so `matchThreshold = beatTicks × 2/3 / 2 = beatTicks / 3`.

Match lyrics in tick order; each lyric claims the nearest unclaimed note (lyrics-first assignment).
If note-first assignment were used, a later syllable could steal the note intended for an earlier one.
A note at or before the syllable tick is preferred over a later note, even if the later note is absolutely closer, so a syllable nudged forward by its layout offset still lands on its own note.
Rests are never counted when assigning a note's reference tick; a measure beginning with a rest would otherwise shift every following note by one position.

A sung syllable always belongs to a note.
If no note matches within the half-beat window and no rest is available to fall back on, attach the syllable to the nearest chord at any distance rather than discarding it.
Continuation syllables (e.g. the second half of "fin-ger" or "soft-ly") can carry a stored tick that sits between notes, more than half a beat from the note they belong to; without this last resort they would be lost.

On a grand staff the lyric must be matched against the notes that share its *resolved* staff, not its raw staff byte.
A bottom-staff note can reach its staff either through `staffWithin` (its raw byte resolves to the lower LINE slot) or through the out-of-band voice (voice value ≥ the voice count routes the note down one staff).
A lyric likewise reaches the lower staff by one of those mechanisms.
Resolve each note's staff with the same routing as the note stream before gathering the candidate ticks; gathering by the raw staff byte instead pulls in another instrument's notes (whose raw staff index collides with the resolved slot) and reverses the syllables.

---

## Note element

### v0xC4 (size = 28)

| Offset | Size | Description                                                                   |
|--------|------|-------------------------------------------------------------------------------|
| +5     | 1    | face value: high nibble = notehead type, low nibble = duration (see below)    |
| +6     | 1    | grace1 (high-nibble flags, see grace section)                                 |
| +7     | 1    | grace2                                                                        |
| +10    | 1    | `xoffset`, layout x-position                                                  |
| +11    | 1    | skipped (high byte of the layout x, ignored)                                  |
| +12    | 1    | staff-relative pitch, diatonic steps from C4 (see below)                      |
| +13    | 1    | tuplet byte, high nibble = actualN, low nibble = normalN                      |
| +14    | 1    | dot count (0/1/2/3)                                                           |
| +15    | 1    | MIDI pitch (0 to 127)                                                         |
| +16    | 2    | playback duration in ticks (recorded MIDI; diverges from notated for tuplets) |
| +19    | 1    | velocity                                                                      |
| +20    | 1    | options                                                                       |
| +21    | 1    | alteration glyph (accidental override)                                        |
| +24    | 1    | articulation byte, above slot                                                 |
| +26    | 1    | articulation byte, below slot                                                 |

The **face value** byte at +5 packs two nibbles.
High nibble = notehead type: 0=normal, 1=diamond, 2=triangle-up, 3=square, 4=cross (X), 5=X-with-circle, 6=plus (+), 7=slash, 8=large open diamond, 9=invisible (no head).
Low nibble = duration: 1=whole, 2=half, 3=quarter, 4=8th, ..., 8=128th.

The **staff-relative pitch** byte at +12 counts diatonic steps from C4 (C4=0, D4=1, E4=2, F4=3, ...
A5=12).
On pitched staves it is a legacy display hint, not used for playback.
On PERC clef staves it instead encodes the visual staff line: convert with `line = max(-4, 10 − position)`.
PERC clef places A4 on the middle line (line=5), so D4→line=9, F4→line=7, A5→line=−2.

### v0xC2 (size = 22 or 24)

The v0xC2 note layout is more compact than v0xC4. **Two pitch-storage sub-variants exist**, distinguished by whether offset +15 holds a plausible MIDI pitch:

- **Sub-variant A** (+15 is empty or a small stray flag): MIDI pitch is at offset +13, the same slot
  where v0xC4 keeps its tuplet byte. Move it before use: the pitch becomes byte +13 and the tuplet
  slot is cleared. These files have no explicit tuplet byte; tuplets are recovered by implied-tuplet
  detection.
- **Sub-variant B** (+15 holds a plausible pitch): MIDI pitch is already at offset +15 (the standard
  pitch slot, like v0xC4). The move must NOT fire. In these files offset +13 carries a genuine tuplet
  ratio (high nibble = actualN, low nibble = normalN, e.g. 0x32 = 3:2) and must be preserved.

The discriminator is whether offset +15 holds a plausible MIDI pitch, not merely whether it is non-zero.
A bare `+15 != 0` test is wrong in both directions.
Treating any non-zero +13 as the pitch imports a sub-variant B triplet at its ratio value (0x32 -> MIDI 50) and drops the tuplet.
But some Encore 3.x/4.x files also leave a small stray value (observed 1 or 3) in the +15 slot of sub-variant A notes; that value is a flag, not a pitch.
Treating it as the pitch imports the note as MIDI 1 (C#-1), several octaves too low, and collapses any chord whose members all carry the flag into a single note once they share that pitch.
Because a value below C0 (MIDI 12) cannot be a real note, +15 counts as the pitch only when it is at least C0; otherwise the pitch comes from +13. Re-saving an ambiguous file in Encore 5 rewrites it to a form that no longer triggers the issue.

**size = 22** (no articulation):

| Offset | Size | Description                                                                     |
|--------|------|---------------------------------------------------------------------------------|
| +5     | 1    | face value (same encoding as v0xC4)                                             |
| +6     | 1    | grace1                                                                          |
| +7     | 1    | grace2                                                                          |
| +10    | 1    | `xoffset`, layout x-position                                                    |
| +11    | 1    | skipped (high byte of the layout x, ignored)                                    |
| +13    | 1    | **MIDI pitch** (sub-variant A) or tuplet ratio (sub-variant B has pitch at +15) |
| +14    | 1    | dotControl, layout/display byte; bit 0 is an unreliable dotted hint (see below) |
| +15    | 1    | **MIDI pitch** (sub-variant B only); in A a stray flag, never a pitch           |
| +16    | 2    | playback duration in ticks                                                      |
| +19    | 1    | velocity                                                                        |
| +20    | 1    | options                                                                         |
| +21    | 1    | alteration glyph                                                                |

**size = 24** (note carries an articulation):

Same layout as size=22, plus:

| Offset | Size | Description                                                             |
|--------|------|-------------------------------------------------------------------------|
| +22    | 1    | articulation byte (encoding as v0xC4 articulationUp; see below)         |
| +23    | 1    | placement/direction flag (0x01 or 0x08); NOT a second articulation byte |

`dotControl = 0xC0` at offset +14 is characteristic of size=24 notes (bits 7 and 6 set as a layout flag; bit 0 is clear, so these notes are not dotted).

### v0xA6 (size = 10, slot = 20 bytes = size × 2)

| Offset | Description                                              |
|--------|----------------------------------------------------------|
| +5     | face value                                               |
| +6     | grace1 (& 0x30: 0x20 = APPOGGIATURA, 0x10 = inner grace) |
| +7     | explicit tuplet byte (3:2 = `0x32`, 5:4 = `0x54`, …)     |
| +9     | staff-position / diatonic line, NOT the MIDI pitch       |
| +11    | MIDI pitch (absolute 0 to 127)                           |

Byte +9 is staff-position (e.g. 11 for B4 in treble clef counting), NOT pitch.
Byte +11 is the playable MIDI value.

---

## REST element

### v0xC4 (size = 18)

| Offset | Size | Description                                                                  |
|--------|------|------------------------------------------------------------------------------|
| +5     | 1    | face value, same encoding as Note element                                    |
| +10    | 1    | `xoffset`, layout x-position                                                 |
| +11    | 1    | skipped (high byte of the layout x, ignored)                                 |
| +13    | 1    | tuplet byte, high nibble = actualN, low nibble = normalN (same as note)      |
| +14    | 1    | dotControl, **bitmask flag, NOT a tick count**. Bit 0 = dotted display hint. |
| +15    | 1    | **mrestCount**, multi-measure rest count (see below)                         |

**Multi-measure rests.** When `mrestCount > 1`, a single MEAS block represents that many consecutive empty display measures (Encore draws one rest symbol with the count above it).
Multi-staff files emit one REST element per staff, so the block can contain several elements, all REST and all carrying the same mrestCount; the count is read from the first element.
Expansion is applied when every element in the block is a REST and `mrestCount > 1`.

The only suppression case is a predecessor MEAS block that is itself a multi-measure rest (all-REST, mrestCount > 1), which prevents cascading in the rare event that Encore writes consecutive mrest blocks.
A predecessor that is a plain single-measure rest (mrestCount == 1) does not suppress expansion, and successor content never affects validity: `mrestCount` is authoritative regardless of what follows.

**dotControl semantics.** dotControl is a **bitmask**, not a sounding tick value:

| Bit    | Meaning                          |
|--------|----------------------------------|
| 0      | dotted display flag (1 = dotted) |
| others | visual/layout hints, ignore      |

Bit 0 is an unreliable dotted indicator: it is sometimes set as a plain layout flag on undotted notes (values such as 0x28, 0x30, 0x39 and 0x60 have been seen on plain 16ths and 8ths in v0xC2 files), so it cannot be trusted on its own when the sounding duration is no longer than the plain face value.
The reliable dot source is the sounding duration relative to the face value.

**v0xC2 dotted-eighth timing quirk.** In v0xC2 files, the MIDI note-on for the sixteenth in a dotted-eighth + sixteenth group is stored at `tick + plain-eighth` (tick + 120), not at `tick + dotted-eighth` (tick + 180).
The dotted eighth therefore reads as a plain eighth (sounding duration 120), and its dotControl byte (typically 0x60) also lacks bit 0, so the dot is not encoded in the bytes at all and must be inferred from the surrounding measure content.

---

### Articulation bytes

Each byte holds one or two glyphs:

Where a single combined SMuFL glyph exists for the combination, use it.
Otherwise produce two separate articulation symbols.

The combined-articulation range 0x22-0x2D is laid out in consecutive (below, above) pairs, one pair per glyph: 0x22/0x23 tenuto+accent, 0x24/0x25 tenuto+staccato, 0x26/0x27 tenuto+heavy-accent (marcato+tenuto), 0x28/0x29 staccatissimo, 0x2A/0x2B heavy-accent+staccatissimo, 0x2C/0x2D tenuto+staccatissimo.
"Heavy accent" is the wedge (∨) Encore writes as marcato.
Verified in Encore 5.

| Value      | Glyphs                                 | Notation output                       |
|------------|----------------------------------------|---------------------------------------|
| 0x04       | trill (plain)                          | `ornamentTrill`                       |
| 0x05       | trill to minor 2nd (flat upper)        | `ornamentTrill` (minor 2nd above)     |
| 0x06       | trill to augmented 2nd (sharp)         | `ornamentTrill` (aug 2nd above)       |
| 0x07       | trill to major 2nd (natural)           | `ornamentTrill` (major 2nd above)     |
| 0x08       | turn                                   | `ornamentTurn`                        |
| 0x01       | flat mark (b)                          | not an articulation, skip             |
| 0x02       | sharp/natural mark (#/♮)               | skip                                  |
| 0x09       | wave mark                              | no notation equivalent, skip          |
| 0x0A       | inverted-mordent (short)               | `ornamentShortTrill`                  |
| 0x0C       | inverted-mordent (long)                | `ornamentTremblement`                 |
| 0x0B       | mordent (simple lower)                 | `ornamentMordent`                     |
| 0x2F       | mordent (double/long lower)            | `ornamentPrallMordent`                |
| 0x12       | accent (`>`)                           | `articAccentAbove`                    |
| 0x13       | marcato (`^`)                          | `articMarcatoAbove`                   |
| 0x14       | staccato + heavy accent (∨)            | `articMarcatoStaccatoBelow`           |
| 0x15       | marcato + staccato                     | `articMarcatoStaccatoAbove`           |
| 0x16       | accent + staccatissimo                 | `articAccent` + `articStaccatissimo`  |
| 0x17       | accent + staccato                      | `articAccentStaccatoAbove`            |
| 0x18       | up bow                                 | `stringsUpBow`                        |
| 0x19       | down bow                               | `stringsDownBow`                      |
| 0x1A       | marcato (variant)                      | `articMarcatoAbove`                   |
| 0x1B       | stopped horn/brass (+)                 | `brassMuteClosed`                     |
| 0x1C       | tenuto (`, `)                          | `articTenutoAbove`                    |
| 0x1D       | staccato (`.`)                         | `articStaccatoAbove`                  |
| 0x1E, 0x1F | harmonic                               | `stringsHarmonic`                     |
| 0x20, 0x21 | fermata (tuplet note = bracket)        | `fermataAbove`                        |
| 0x22, 0x23 | tenuto + accent                        | `articTenutoAccentAbove`              |
| 0x24, 0x25 | tenuto + staccato (portato)            | `articTenutoStaccatoAbove`            |
| 0x26, 0x27 | tenuto + heavy accent (marcato+tenuto) | `articMarcatoTenuto{Below,Above}`     |
| 0x28, 0x29 | staccatissimo                          | `articStaccatissimoAbove`             |
| 0x2A, 0x2B | heavy accent (∨) + staccatissimo       | `articMarcato` + `articStaccatissimo` |
| 0x2C, 0x2D | tenuto + staccatissimo                 | `articTenuto` + `articStaccatissimo`  |
| 0x2E       | inverted turn                          | `ornamentTurnInverted`                |
| 0x30       | half-stopped horn (circle-plus)        | `brassMuteHalfClosed`                 |

### Technical markings (reuse articulation slots)

| Byte         | Meaning                                                             |
|--------------|---------------------------------------------------------------------|
| 0x0D, 0x11   | fingering 1 to 5                                                    |
| 0x1E, 0x1F   | harmonic (see above)                                                |
| 0x44, 0x45   | thumb-position                                                      |
| 0x46         | open-string (plain Fingering "0", not circled)                      |
| 0x47         | "stick" drumstick technique; no SMuFL equivalent, skip              |
| 0x48         | brush; no SMuFL equivalent, skip                                    |
| 0x49         | soft mallet; no SMuFL equivalent, skip                              |
| 0x4A         | hard mallet; no SMuFL equivalent, skip                              |
| 0x39 to 0x40 | scale string numbers 1 to 8 (byte `0x38 + N` = string N); see below |

When at least one scale-string byte (0x39 to 0x40) appears in a measure, every note in that measure with options bit 0 set also displays its scale-degree position as a circled string number.

### Single-note tremolos (articulation slots)

| Byte | Strokes | Notes                                           |
|------|---------|-------------------------------------------------|
| 0x41 | 1       | 8th tremolo                                     |
| 0x42 | 2       | 16th tremolo                                    |
| 0x43 | 3       | 32nd; Encore may render 4 strokes in some files |
| 0x03 | 3       | bare 3-stroke variant (no high-nibble flag)     |

`0x44` and above are technical markings, NOT tremolos.

### Articulation bytes are per-note within a chord

Each NOTE element carries its own `articulationUp` and `articulationDown` bytes.
When several notes in the same chord (same tick, same voice) carry the same artic byte, the byte is simply repeated on each note while Encore draws the glyph once.
A chord therefore denotes at most one copy of each distinct articulation or ornament glyph, no matter how many of its notes carry the byte (e.g. two chord notes both carrying `au=0x04` denote a single trill, not two).

---

## Rhythm encoding

240 ticks per quarter note.
**Whole-note tick count** is always 960 for any time signature and can be computed reliably as `(durTicks * timeSigDen) / timeSigNum`.
Do NOT use `beatTicks * timeSigDen`: in compound meters (e.g. 6/8) `beatTicks` is the compound beat (360 for the dotted quarter), giving 2880 instead of the correct 960.

| Face value | Ticks | Duration |
|------------|-------|----------|
| 1          | 960   | whole    |
| 2          | 480   | half     |
| 3          | 240   | quarter  |
| 4          | 120   | eighth   |
| 5          | 60    | 16th     |
| 6          | 30    | 32nd     |
| 7          | 15    | 64th     |
| 8          | 7     | 128th    |

Notated duration = face value + dot count + tuplet byte.
The playback duration at +16 diverges (live recording, ties, tuplets).

**Tuplets.** Either explicit byte `(actualN << 4) | normalN` or implicit (playback duration ≈ faceTicks × 2/3 or 4/5).
Implicit detection applies only to v0xC2 files.

Supported explicit ratios (each notated as a tuplet bracket):

| Ratio                   | Example            | Constraint                                      |
|-------------------------|--------------------|-------------------------------------------------|
| 2:1                     | dosillo de redonda | normalN × baseLen must be a standard note value |
| 2:3                     | compound duplet    |                                                 |
| 2:4                     | 2 in 4 beats       |                                                 |
| 3:2                     | triplet            |                                                 |
| 4:1, 4:2, 4:3           | quadruplet         |                                                 |
| 5:2, 5:3, 5:4, 5:6, 5:8 | quintuplet         | 5:4 standard; see note below                    |
| 6:4, 6:7, 6:8           | sextuplet          |                                                 |
| 7:4, 7:6, 7:8           | septuplet          |                                                 |
| 8:4, 8:6                | octuplet           |                                                 |
| 9:4, 9:6, 9:8           | nontuplet          | see note below                                  |
| 10:6, 10:8              | decuplet           |                                                 |

Ratios with normalN ∈ {5, 9, 10, 15, ...} produce Tuplet.ticks = normalN × baseLen that cannot be represented as a standard note value (e.g. 9:5 with 8th gives bracket span = 5/8, which is not a valid note value).
Such ratios are left as plain notes without a bracket.

**Beat-relative face values.** In compound and simple meters where one beat equals an eighth (e.g. 6/8, 8/8, 12/8), Encore stores the face value as the number of "beats", not as an absolute note value.
A Q-face note (`fv=3`) in an 8/8 3:2 triplet thus represents one eighth beat, not one quarter note.
The actual written duration is `rdur × (actualN / normalN)`; when that product equals a standard tick count (E=120, Q=240, …), it overrides the face value.
Detection: `rdur == beatTicks × (normalN / actualN)` (one beat per tuplet slot).

**Dotted notes.** Dot count at +14. Can be inferred from `playbackTicks == faceTicks × 3/2` (one dot), `7/4` (two dots), with ±1-tick tolerance.
For rests, dotControl (+14) is a bitmask flag, NOT a tick count; the dot count comes from the sounding duration relative to the face value, not from this byte.

**Ghost rest filter.** When real durations are computed, a REST's rdur is set to `nextTick - tick` (the MIDI gap to the next event).
When a note starts only a few ticks after the rest's MIDI start (MIDI timing slop), rdur ends up far shorter than the face value (e.g. rdur=5 for a 32nd rest with faceTicks=30).
The ghost-rest filter (`rdur > 0 && rdur < 15`) must not drop these real rests.
Rule: if `faceTicks >= 30` (32nd or longer), trust the face value regardless of rdur.
Only drop rests whose face value is also very short (64th or smaller, faceTicks < 30).

---

## BEAM element

Type 4. Explicit beaming per level:

| Size | Byte +5 | Beam level           |
|------|---------|----------------------|
| 30   | 0x01    | 1st (8th flag)       |
| 46   | 0x02    | 2nd (16th extension) |
| 62   | 0x03    | 3rd (32nd extension) |

The importer intentionally does not model BEAM elements: MuseScore auto-beams from note durations and the time signature, so Encore's explicit beam groups are dropped.

---

## TEXT block

Carries text payloads for STAFFTEXT ornaments (subtype `0x1E`).

**Multiple blocks.** A file may contain several TEXT blocks: the first is the score's text table, and each later block is a part-view copy holding the same strings in a different order and count.
The ORN `tind` index is relative to the FIRST (score) block only.
Resolving a `tind` against a later block returns the wrong string (e.g. "Presto" read as "Xilo.").
Use the first non-empty TEXT block and ignore the rest, the same way duplicate TITL blocks are handled.

Block layout (after 8-byte magic + varsize):

| Offset | Size | Description         |
|--------|------|---------------------|
| +0     | 2    | sync (`0x0000`)     |
| +2     | 2    | entry count         |
| +4     | 4    | total content bytes |
| +8…    | var  | entries (see below) |

Each entry:

| Offset | Size | Description                                                 |
|--------|------|-------------------------------------------------------------|
| +0     | 2    | payload size                                                |
| +2     | 14   | header (partially decoded)                                  |
| +16..  | var  | text (UTF-16 LE or Latin-1); lines separated by `0x04 0x00` |
| (end)  | 2+   | `0x00 0x00` null terminator (may be followed by padding)    |

In v0xA6 the entry has no 14-byte header: the text starts immediately after the payload-size field (payload offset 0, i.e. entry offset +2) and is null-terminated Latin-1. Reading at the newer +14 (entry +16) offset lands past a short v0xA6 entry and yields empty text.

**Line separators.** `0x04 0x00` (U+0004) separates lines within a single comment; it is NOT the text terminator.
Each line, including the last, is followed by a `0x04 0x00`, and the whole string ends at a `0x00 0x00` null.
A single-line entry therefore ends with `0x04 0x00 0x00 0x00`.
To recover the text, decode from +16 up to the null, replace each U+0004 with a newline, and drop the resulting trailing newline.
Reading the text as ending at the first `0x04 0x00` truncates a multi-line comment to its first line.

Text length is bounded by the null terminator, **not** by `payload_size - 14 - 4` (some entries carry padding after the terminator).
Dynamic marks use their own ornament subtypes, they are NOT in the TEXT block.

---

## TITL block

Title, 2 subtitles, 3 instructions, 4 authors, 2 headers, 2 footers, 6 copyright lines.
Encoding from varsize: < 5000 → Latin-1 (96 bytes/line); ≥ 10000 → UTF-16 LE (1056 bytes/line).

### UTF-16 line layout (1056 bytes)

| Offset     | Size | Description                                                |
|------------|------|------------------------------------------------------------|
| +0, +29    | 30   | prefix (byte +14 = horizontal alignment for header/footer) |
| +30, +1055 | 1026 | UTF-16 LE text, NUL-terminated, zero-padded                |

Alignment byte: `0x02` = RIGHT, `0x04` = LEFT, `0x06` = CENTER.
Other line types leave it at `0x00`.

### Slot counts

| Category    | Slots |
|-------------|-------|
| title       | 1     |
| subtitle    | 2     |
| instruction | 3     |
| author      | 4     |
| header      | 2     |
| footer      | 2     |
| copyright   | 6     |

Multiple non-empty slots in a category render as stacked lines.
Each slot is independently NUL-terminated; bytes after the NUL are prior-edit debris.

### Replaceable tokens (header/footer only)

| Token | Expanded value |
|-------|----------------|
| `#P`  | page number    |
| `#D`  | date           |
| `#T`  | time           |

### Duplicate TITL blocks

Encore writes one TITL block per page.
Page 2+ blocks are often entirely empty (all content bytes zero).
When multiple TITL blocks are present, use the first one that has non-empty content and ignore subsequent empty ones.
If two blocks both have content (identical copies are common in single-page files), the second replaces the first to avoid duplicating lines.

---

## WINI block (page setup)

Optional block written only when the user explicitly opens and saves Page Setup in Encore. Files that have never been through Page Setup have no WINI block; in that case use default margins.
Present in all files saved by Encore 5.0.2 (`chuVersio = 1056`).

Block layout (after 8-byte magic + varsize header):

| Offset | Size | Type    | Description                                                       |
|--------|------|---------|-------------------------------------------------------------------|
| +0     | 24   | bytes   | screen/window data (not part of the page geometry)                |
| +24    | 4    | int32LE | top margin in typographic points (1/72 in)                        |
| +28    | 4    | int32LE | left margin in pts                                                |
| +32    | 4    | int32LE | bottom edge of printable area (pageHeight_pts - bottomMargin_pts) |
| +36    | 4    | int32LE | right edge of printable area (pageWidth_pts - rightMargin_pts)    |
| +40    | 2    | uint16  | flags (observed: 1)                                               |

Total content size: 42 bytes (`varsize = 42`).
Some older files have `varsize = 40` (the trailing uint16 is absent in some files); both layouts are valid.

**Units, two variants.** Encore 5.x stores values in typographic points (1/72").
Earlier versions (including Encore 4.x and some 3.x) store them in **screen pixels at the monitor DPI** (~84 PPI; the exact value is the screen the file was last saved on, e.g. 83.7).
The displayed margins in those files are still inches; only the stored unit is device pixels.

**Detecting the unit.** The two variants are told apart by the magnitude of the stored edges relative to the page size: when an edge clearly exceeds the page dimension expressed in points (rightEdge above pageWidth × 72, or bottomEdge above pageHeight × 72), the block is in screen pixels; otherwise it is in typographic points.
In the screen-pixel case the pixels-per-inch is recovered from `(rightEdge + left) / pageWidthInches` (≈84).
That ratio is exact only when the left and right margins are equal; with asymmetric margins it is ~2% low (the far edges omit the larger far margin), so the recovered right/bottom margins can be off by a couple of millimetres.

**Derived values (typographic-point variant, rightEdge ≤ pageWidth_pts):**

```
topMargin    = top / 72.0
leftMargin   = left / 72.0
printWidth   = (rightEdge - left) / 72.0
printHeight  = (bottomEdge - top) / 72.0
bottomMargin = pageHeight - topMargin - printHeight
```

**Derived values (screen-pixel variant, rightEdge > pageWidth_pts):**

The page dimensions are NOT stored explicitly; they must be recovered from the symmetry of the margin data.
Assuming the left and right margins are stored with the same pixel value:

```
pageWidth_units  = rightEdge + left   (e.g. 700 for A4 at ~84.7 PPI)
pageHeight_units = bottomEdge + top   (e.g. 990)
```

Match `pageWidth_units / w` against standard paper sizes (ISO A-series, US Letter/Legal, ISO B-series).
The candidate where `|dpiW − dpiH|` is minimised is the paper format.
Compute `scale = pageWidth_units / w_detected`; all four margins follow from the same scale.

**Page-size detection note.** ISO A-series sizes (A0-A10) all share the 1:√2 aspect ratio.
For any AN page, consecutive sizes differ in DPI by √2 ≈ 1.414×, so at most two A-series sizes fall within the plausible DPI range [60, 135] simultaneously.
The correct AN is the one with the smallest `|dpiW − dpiH|` among those in range.
Checking A-series sizes first (before envelope or other sizes) avoids false positives from non-document formats that accidentally satisfy the delta criterion.

**Encoding quirk.** Encore stores `round(inches × 72)`, then displays `floor(pts / 72 × 1000) / 1000`, so a user-entered 0.100" stores as 7 pts and displays back as 0.097".
In screen-pixel files Encore displays margins using 1/72" even though the stored unit is 1/DPI: dividing by 72 reproduces Encore's displayed margins, while dividing by the detected DPI gives the accurate physical margins.

**Zero-margin files.** All four margin values being 0 means no stored margins.

**Encore UI and printer margins.** Encore's Page Setup dialog may display a non-zero margin for a side that has a zero stored value.
The displayed value is the printer driver's hardware non-printable zone, read from the active printer at display time.
It varies by printer and computer, and is not stored in the WINI block.
A document with `rEdge = A4_width_pts` and `rightEdge_margin = 0` may show R = 0.236" on one machine and a different value on another.
The stored WINI value is the authoritative document margin; the UI value is informational only.

---

## Parsing rules

This section documents non-obvious rules that any parser must apply to produce correct output.
These are properties of the Encore binary format, not of any specific implementation.

### Measure completeness

Encore does not require every voice in a measure to be completely filled.
A measure is valid with fewer ticks than `durTicks` in any voice.

**Fill rule:** after reading all elements in a voice, if `placedTicks < durTicks`:
- Add implicit rests for the gap. Whether these should be visible rests or hidden
  depends on context (trailing gap vs internal gap).

**Overflow rule:** if `placedTicks > durTicks`, truncate from the end, removing the smallest elements first until `placedTicks ≤ durTicks`.
A small tolerance of `durTicks / 24` (= 40t in 4/4) is allowed before truncation triggers.

**Notes discarded, not moved.** Elements arriving after `placedTicks = durTicks` for a given voice are silently dropped.
Encore stores multiple MIDI recording passes in the same voice byte; only the first fill is valid notation.

### Anacrusis / pickup measure detection

Two cases:

**Case A, explicit short time signature.** When `timeSig[measure_0] ≠ timeSig[measure_1]`, Encore stored a shorter time signature for the pickup.
Use `timeSig[measure_0]` for display, but the actual duration of measure 0 is `durTicks[measure_0]`.
All subsequent measures start at `durTicks[measure_0]`.

**Case B, implicit (underflowed) pickup.** When `timeSig[measure_0] = timeSig[measure_1]` but the actual placed content (`maxPlacedTicks` across all voices/staves) is `0 < maxPlacedTicks < durTicks`:
1. Shrink measure 0 to `maxPlacedTicks`.
2. Shift all subsequent measures back by `delta = durTicks − maxPlacedTicks`.
3. Any forward-looking spanner endpoints (hairpins, slurs) that pointed past the new
   end of measure 0 must be reduced by the same `delta`.

Guard: do not apply Case B when Case A already set a shorter `durTicks` for measure 0 (that would double-reduce).

### Tuplet compaction

Encore allows encoding more notes in a tuplet run than the stated group size.
For example, 15 notes all marked `tup = 9:5` (nine-in-five).
Standard grouping into `⌊15/9⌋ = 1` full group + 6 leftover notes overflows the measure.

**Rule:** when a contiguous run of N same-voice, same-face-value notes all share the same explicit tuplet byte `tup = an:nn`, and:
1. `N > an`, AND
2. `N` is **not** a multiple of `an`, AND
3. the standard interpretation overflows: `⌊N/an⌋ × fv_ticks × nn + (N%an) × fv_ticks + trailingTicks > durTicks`

Then recompute the ratio to fit the available space: ``` available = durTicks − leadingTicks − trailingTicks m = round(available / fv_ticks) ratio = [N : m/fv] (N actual notes in m normal-value slots) ``` where `leadingTicks` and `trailingTicks` are the durations of elements before/after this run.

| Encore input                         | Available | m         | Result                 |
|--------------------------------------|-----------|-----------|------------------------|
| 15 notes `tup=9:5`, fv=♪, 4/4        | 960t      | 960/120=8 | `[15:8/♪]` = 1 measure |
| 12 notes `tup=9:5`, fv=♪ + 2 plain ♪ | 720t      | 720/120=6 | `[12:6/♪]` + 2♪        |
| 10 notes `tup=9:4`, fv=♩, 4/4        | 960t      | 960/240=4 | `[10:4/♩]` = 1 measure |

**normalN constraint:** `m ∈ {1,2,3,4,6,7,8}` produces standard note-value denominators.
`m = 5` or `m = 10` gives non-standard fractions (5/8, 5/4); in that case round `m` to the nearest safe value (e.g. 5→4 or 5→6, 10→8).

### Tuplet: nested triplets

When an outer 3:2 triplet group closes and the triggering note together with the next `actualN−1` notes form a complete inner triplet of smaller face value:
- Create an outer group (e.g. 3:2/♪, spanning one beat).
- Create an inner group (e.g. 3:2/♬) nested inside the outer.
- Each inner note's position advance uses the doubly-nested ratio `innerRatio × outerRatio`.

### Tuplet: 9:5 without compaction

When exactly 9 notes carry `tup = 9:5` and fit within the measure without compaction, create a single `[9:5/♪]` group.
The duration of the group bracket is `5 × ♪ = 5/8`, which is not a standard note value.
Set the bracket duration after placing all 9 notes (not before), to avoid rejecting the non-standard fraction during group construction.

### Tuplet: incomplete group at measure boundary

When a mixed-duration tuplet group is truncated at the barline (Encore omits the final note because its MIDI tick equals `durTicks`), the face-value sum `Σ fv_ticks < fullFaceSum` even though the count may equal `actualN`.
Detect this condition and insert an invisible rest for `fullFaceSum − Σ fv_ticks` ticks at the end of the group to complete it.
The rest's position advance = `remainingFace_ticks × normalN / actualN`.

### Tuplet: no gap-snap inside active groups

Gap-snap (advancing the position counter to the note's face-value grid when a gap is detected) must be suppressed while a tuplet group is active.
Tuplet note positions are computed from accumulated face-value advances, not from the raw MIDI tick.

### Last note of a measure-spanning tuplet

The last note of a tuplet that ends at the barline often has `realDuration ≪ faceValueTicks` (e.g. rdur=6t at tick=954 in a 960t measure) because Encore truncates playback durations at the barline.
This note is valid notation.
Do not filter it out based on its short `realDuration`; use tuplet group membership to determine legitimacy.

### Voice mapping

| Encore voice byte | Output voice / staff                                          |
|-------------------|---------------------------------------------------------------|
| 0                 | voice 0, same staff                                           |
| 1                 | voice 1, same staff                                           |
| 2                 | voice 2, same staff (or voice 0 of next staff in grand-staff) |
| 3                 | voice 3, same staff                                           |
| ≥ 4 (out-of-band) | voice 0 of the adjacent staff (staffWithin)                   |

**Overflow drop.** Once a voice is full (`placedTicks = durTicks`), additional elements arriving with the same voice byte are dropped, they are never promoted to the next voice.

**Duplicate REST dedup.** When two out-of-band voice bytes both map to the same output voice and both carry an explicit REST at the identical tick, treat the second REST as a no-op (do not advance position for it).
Otherwise the second REST shifts all later elements.

### Chord symbol placement

CHD tick values carry small MIDI offsets from the notated beat (e.g. tick=6 for a beat-1 chord).
Encore renders chord symbols at beat positions.

```
beatStart = floor(chd_tick / beatTicks) * beatTicks
attach to: first note/rest in [beatStart … chd_tick]
fallback:  last note/rest before chd_tick
final:     chd_tick itself
```

**Example:** `beatTicks=240`, CHD@62, notes at tick=0 and tick=60. `beatStart = 0`.
First note in [0,62] = tick=0. Chord goes on beat 1. ✓ (Not tick=60, which is the second 16th of the beat.)

---

## PREC block (page setup / printer DEVMODE)

Magic: `PREC`.
Variable size (132 bytes to several KiB).
The content is the printer/page-setup state, in one of two encodings depending on the platform that wrote the file:

- **Windows** (`SCOW`): a Windows **DEVMODE** structure (see field table below).
- **macOS** (`SCO5`): a macOS **NSPrintInfo XML plist** (begins with `<?xml ... <plist ...`).
  Read the paper from `PMTiogaPaperName` / `PMPaperName` (e.g. `na-letter`, `iso-a4`),
  orientation from `com.apple.print.PageFormat.PMOrientation` (1 = portrait, 2 = landscape),
  and the notation scale from `com.apple.print.PageFormat.PMScaling` (a fraction, 1.2 = 120%).
  The plist carries only the printer's imageable page/paper rects, not the document margins,
  so SCO5 page **margins** are not available from PREC.

PREC is the page-setup source for the score and is present in almost every file across all formats, while the WINI block exists only in some of them.
So the page **size**, **orientation** and **notation scale** come from PREC even for v0xA6/v0xC2 and for files that have no WINI.

**SCO5 document margins.** SCO5 stores no document margins in any importable block: WINI holds only window state, the PREC plist holds only printer rects, and some SCO5 files have no PREC at all.
Observed values vary (some files use a uniform margin, others store 0) and no byte field tracks the difference, so SCO5 document margins cannot be recovered from the file and a default must be supplied.

The DEVMODE begins with a fixed device-name field followed by the standard fixed fields.
Two variants occur and must be distinguished:

- **ANSI** DEVMODE: device name is 32 bytes. Fixed fields start at offset 32. Seen in v0xA6
  and v0xC2 (older printer drivers).
- **Unicode** DEVMODE: device name is 64 bytes (32 UTF-16 code units, so even bytes are `00`
  for an ASCII name). Fixed fields start at offset 64. Seen in v0xC4 (e.g. "Microsoft Print
  to PDF").

Relative to the field base (32 for ANSI, 64 for Unicode), the relevant fields are:

| Offset (from base) | Field         | Meaning                                                    |
|--------------------|---------------|------------------------------------------------------------|
| +12                | dmOrientation | 1 = portrait, 2 = landscape                                |
| +14                | dmPaperSize   | DMPAPER_* enum (see below)                                 |
| +16                | dmPaperLength | tenths of a millimetre (custom sizes only; 0 for standard) |
| +18                | dmPaperWidth  | tenths of a millimetre (custom sizes only; 0 for standard) |
| +20                | dmScale       | score Zoom / notation-size percent (100 = default)         |

`dmPaperSize` holds a Windows DMPAPER_* value: 1=Letter, 5=Legal, 7=Executive, 8=A3, 9=A4, 11=A5, 12=B4, 13=B5.

Detect the variant by reading dmOrientation at both bases and keeping the one that is 1 or 2; range-check the rest.
When dmPaperSize is a standard value, use it directly for the page size; when it is custom/unknown (e.g. 0, 164, 256) fall back to dmPaperLength/Width, then to the WINI geometry.
Validated across the corpus (12,055 files): dmScale shows a clean distribution of round percentages (100 dominant, then 85/80/90/75 …) and dmPaperSize is dominated by 9 (A4) and 1 (Letter), confirming these are real user-set values.

## Known quirks

- **Encore 5.0.2 instrument names.** TK block headers may be absent while the name is still present
  at the formula-derived offset (see Instrument block). Encore 5.0.2 always writes UTF-16 instrument
  names regardless of the offset field value.
- **TITL encoding.** The TITL block's internal version field is unreliable; use `varsize` for
  Latin-1 vs UTF-16 detection (varsize < 5000 → Latin-1; ≥ 10000 → UTF-16).
- **v0xA6 layout differences.** Notes are 10 bytes; MIDI pitch at +11 (not +9, which is staff
  position); element body starts at MEAS offset 0x1A (not 0x36); TK blocks are 64 bytes wide;
  key transposition at TK content +42; header ends at 0xA6; consecutive identical REST pairs
  should deduplicate to one rest.
- **v0xA6 note with one articulation (size 11).** A NOTE that carries a single articulation is
  written as size 11 (a 22-byte slot) instead of size 10. The layout is otherwise identical to
  size 10: the MIDI pitch is still at +11 and the tuplet byte at +7. The one articulation byte
  lives at +18 (size-10 notes always have 0 there); 0x20 at +18 is a fermata above. The pitch
  must be read from +11 just as for size 10; a reader that only special-cases size 10 falls
  through to the generic note layout (pitch at +15) and yields a garbage pitch with no fermata.
- **v0xA6 grace note time-borrowing.** Grace notes occupy real ticks, pushing subsequent notes
  forward. See "v0xA6 grace note time-borrowing" section for the detection rule.
- **Percussion MIDI program.** Percussion tracks always carry MIDI program 1 regardless of the
  actual kit. Identify the kit from the track name.
- **Tempo encoding.** Italian tempo words ("Allegro", "Andante", ...) are stored as STAFFTEXT elements, not as a dedicated tempo element. Numeric tempo marks use the ORN TEMPO subtype (0x32), which carries a BPM at byte +30. Each MEAS header also carries a quarter-note BPM that persists on every measure (so it always equals the active tempo). The ORN TEMPO element's stored tick rarely matches the measure where the tempo actually applies, so the per-measure header BPM is the reliable source for the tempo's position.
- **Lyric voice byte.** Lyric voice = verse index (0-based), not a real voice assignment.
  All verses are anchored to voice-0 notes.
- **Repeat-mark field.** Repeat type is the low byte only: `type = field & 0xFF`.
- **v0xC2 grace1 tie-sender encoding.** In v0xC2, when a grace note is a tie-sender, its `grace1`
  low nibble is set to 1 (`grace1 & 0x0F == 1`). In v0xA6 and v0xC4 this nibble is always 0, so a
  reader must treat the low nibble as the tie-sender flag only in v0xC2.
- **Duplicate NOTE elements.** Some files encode the same pitch twice in the same chord:
  two NOTE elements with identical tick/staff/voice/pitch. Two variants: (a) the second copy
  has bit 0x40 of grace1 set (chord-extension marker); (b) both copies have grace1=0 (v0xC2).
  Either way, discard the second copy to avoid duplicate noteheads.
- **Largest legitimate block.** A MEAS block rarely exceeds 2 KiB. A significantly larger block
  indicates a corrupt file or a format variant not yet documented here.
- **Key=0 and template transposition.** When Encore's Key field is 0 (`0 = sounds as written`),
  store notes at written pitch with no chromatic shift (key semitones = 0).
  If the MIDI program causes a transposing template to be selected (e.g. Bb clarinet for MIDI 72),
  **all** template transpositions are zeroed out (both non-octave and octave) so that the stored
  written pitch is displayed as-is. In particular, if an acoustic-bass or double-bass
  template carries `transposeChromatic = -12` but the enc file has Key=0, the -12 must also be
  cleared; otherwise notes display one octave higher than Encore shows them.
  When Key ≠ 0 and the offset is a pure octave multiple (±12, ±24 …), the sign decides how the
  octave is shown:
  - **Negative** (the instrument sounds lower, e.g. laud, bass guitar): the staff is given a
    matching octave-down clef (8vb), the conventional notation for such instruments. The
    template's existing octave transposition is left intact.
  - **Positive** (the instrument sounds higher, e.g. a tuba "Bajo" with Key=+12): the staff keeps
    a plain clef and the octave is stored as a playback transposition on the instrument, so the
    notes stay at their written height. Octave-up (8va) clefs are not produced, because they are
    rare and Encore itself shows these instruments with a plain clef.
- **Note spelling on transposing staves.** Encore stores only the sounding pitch plus the Key
  offset, never an explicit enharmonic spelling. The written accidental on a transposing staff is
  therefore not in the file; it has to be derived from the sounding pitch and the staff's concert
  key. The bytes fix the audible pitch, not the written spelling.
