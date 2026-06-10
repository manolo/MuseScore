# Encore (.enc) binary format

Binary format reference for Encore `.enc` files, independent of any implementation.
First documented by Felipe Castro (enc2ly) and Leon Vinken (Enc2MusicXML, GPL v3+).
Extended from a corpus of 5000+ files covering v0xA6 (Enc 2.x), v0xC2 (3.x/4.x), v0xC4 (5.x).

## File structure

| Block                | Description                                        |
|----------------------|----------------------------------------------------|
| header (194 bytes)   | version, line/page/measure counts                  |
| TK00 … TKnn          | one per instrument: name + MIDI program data       |
| PAGE                 | page geometry                                      |
| LINE … LINE          | one per system: staves, clef/key per staff         |
| MEAS … MEAS          | one per measure: notes, rests, ornaments …         |
| PREC *(optional)*    | page-rendering cache                               |
| TITL                 | title / subtitle / author / copyright / …          |
| TEXT                 | free-text annotations for staff text               |

Every block starts with a 4-byte ASCII magic
(`TK00`, `PAGE`, `LINE`, `MEAS`, `TITL`, `TEXT`, `PREC`) followed by a 4-byte varsize.

---

## Format versions

Byte at file offset 4 identifies the version:

| Byte   | Version   | Encore release        |
|--------|-----------|-----------------------|
| 0xA6   | v0xA6     | Encore 2.x (legacy)   |
| 0xC2   | v0xC2     | Encore 3.x / 4.x      |
| 0xC4   | v0xC4     | Encore 5.x            |

File magic at offset 0 (byte order of multi-byte integers follows the magic):

| Magic    | Storage     | Byte order      | Notes                |
|----------|-------------|-----------------|----------------------|
| `SCOW`   | plaintext   | little-endian   | Encore 5.x default   |
| `SCO5`   | plaintext   | big-endian      |                      |
| `SCOX`   | plaintext   |                 | rare variant         |
| `SCOR`   | plaintext   |                 | rare variant         |
| `SCOS`   | plaintext   |                 | rare variant         |
| `ZBOT`   | encrypted   |                 | Encore 4.x default   |
| `ZBOP`   | encrypted   |                 | encrypted variant    |
| `ZBO6`   | encrypted   |                 | encrypted variant    |

**ZBOT encryption.** Only the first 42 bytes decrypt with a known XOR key.
Beyond that the stream is algorithmically generated and unbroken.
The plaintext `SCOW` equivalent is structurally different, so re-saving from Encore 5 is the only practical path.

---

## Header (194 bytes)

| Offset   | Size   | Description                             |
|----------|--------|-----------------------------------------|
| 0x00     | 4      | magic (`SCOW` or `SCO5`)                |
| 0x04     | 1      | format version (see above)              |
| 0x28     | 2      | Encore application version (uint16 LE): 592=4.0 773/775=4.x 1056=5.0 |
| 0x2A     | 2      | purpose unconfirmed (varies per file, possibly total LINE-staff entries) |
| 0x2C     | 2      | default beatTicks for measures (uint16 LE): 240=quarter-note grid (0x00F0), matches MEAS header +0x04 |
| 0x2E     | 2      | number of system blocks                 |
| 0x30     | 2      | number of pages                         |
| 0x32     | 1      | number of instrument blocks             |
| 0x33     | 1      | staves per system                       |
| 0x34     | 2      | **rendered measure count** (see below)  |

Bytes 0x36..0xC1 are padding.

**Rendered measure count.** The field at 0x34 is the number of measures Encore displays.
Files can contain extra "ghost" MEAS blocks from prior edits.
The importer stops after `header.measureCount` blocks (observed: 36 rendered / 56 on disk in one Encore 5 file).

---

## Instrument block (TKnn)

Carries the instrument name as Latin-1 or UTF-16 LE.
Probe: byte 0 printable ASCII + byte 1 == `0x00` → UTF-16 LE; else Latin-1.

**MIDI program.** Layout depends on the TK block size (`varSize`, stored in the 4-byte size
field of each TK block):

- **Large-TK** (`varSize > 250`): fixed-offset table after TK blocks:
  ```
  base = 2278    (header 194 + first block 120 + intra-data 1964)
  step = 2158    (block 120 + data block 2038)
  instrument n → file offset  base + n * step
  ```
- **Small-TK** (`0 < varSize ≤ 250`, e.g. 112): MIDI is in the extra-data region that
  follows each TK block's named content, at a fixed offset of 76 bytes into that region:
  ```
  contentFilePos = TK_block_start + 8        (after 4-byte magic + 4-byte size)
  instrument n MIDI offset = contentFilePos[n] + varSize + 76
  ```
  Key transposition is at the same location minus 23 bytes (`varSize + 53`).

**Key transposition.** At `base - 23 + n * step` (large-TK/no-TK), or `contentFilePos + varSize + 53`
(small-TK): signed `int8` semitones matching the Encore Staff Sheet "Key" dropdown
(`0` = sounds as written, `-12` = octave lower, range ±33 semitones).
Encore shifts every note pitch by this value.

**No-TK-block files.** Some v0xC4 files have no TK blocks at all. These files come in two
sub-layouts determined by where the first PAGE/LINE/MEAS block starts:

- **Compact layout** (first block ≤ offset 2278): MIDI at offset 390, Key at 367.
  Single-instrument only for Key; multi-instrument Key is not read.
- **Large-TK layout** (first block > offset 2278): MIDI at base 2278, Key at 2255,
  using the same offsets as TK-based files. This handles Encore 5 files exported
  without TK blocks but with the standard instrument metadata tables.

**Name recovery for no-TK files.** Instrument names are stored at fixed offsets regardless of
whether TK blocks are present:
```
NAME_BASE = 202     (offset of first instrument name in the file)
NAME_STEP = 2158    (stride between instruments)
instrument n → file offset  NAME_BASE + n * NAME_STEP
```
The name is encoded as UTF-16 LE or Latin-1 (same probe as TK blocks). When no TK blocks are
found, the parser creates instruments with empty names so `recoverMissingNames()` can fill them
from these offsets. "Part N" fallback is applied only after recovery, for names still empty.

**Percussion quirk.** Percussion tracks always report MIDI program 1 (GM Grand Piano);
infer the actual kit from the track name.

---

## System block (LINE)

21-byte header (start tick, measure count) + N × 30-byte staff entries (N = staves-per-system from header).

### LINE header (21 bytes)

| Offset | Size | Description                                                      |
|--------|------|------------------------------------------------------------------|
| +10    | 2    | `start` — 0-based index of the first measure in this system     |
| +12    | 1    | `measureCount` — number of measures in this system              |

### LINE staff entry (30 bytes, repeated N times)

| Offset | Size | Description                                                                    |
|--------|------|--------------------------------------------------------------------------------|
| +14    | 1    | clef type                                                                      |
| +15    | 1    | key signature                                                                  |
| +16    | 1    | page-row counter (varies per system; NOT the page number and NOT a fixed property) |
| +19    | 1    | visibility: `0x00` = hidden; any non-zero = visible                            |
| +20    | 1    | staff type: `0` = MELODY, `1` = TAB, `2` = RHYTHM (single-line percussion)    |
| +21    | 1    | packed instrument/staff index: bits 0-5 = instrument index, bits 6-7 = staff within instrument |

True page count is in `header.pageCount`; page break positions are not yet decoded.

A RHYTHM staff (byte +20 = 2) maps to a single-line percussion template in MuseScore.
The staff type is constant across all LINE blocks for the same staff position.

---

## Measure block (MEAS)

54-byte header + variable element body terminated by `0xFFFF` tick.

### Measure header

| Offset      | Size   | Description                                                                            |
|-------------|--------|----------------------------------------------------------------------------------------|
| 0x00        | 2      | BPM (quarter-note beats-per-minute; applies forward until next change)                 |
| 0x02        | 1      | time-signature glyph                                                                   |
| 0x04        | 2      | ticks per beat (beatTicks); for compound meters e.g. 6/8 this is the compound beat (dotted quarter = 360), not the simple beat |
| 0x06        | 2      | total ticks in measure (durTicks)                                                      |
| 0x08        | 1      | time-signature numerator                                                               |
| 0x09        | 1      | time-signature denominator                                                             |
| 0x0C        | 1      | start barline type (see table)                                                         |
| 0x0D        | 1      | end barline type (same table)                                                          |
| 0x0F        | 1      | repeat-alternative bitmask (see Volta section)                                         |
| 0x1A        | 4      | repeat-mark field — LOW byte = repeat type (see table); upper 3 bytes = position/style |
| 0x10–0x35   | 38     | layout data: measure width, x-offsets, "Writer" UTF-16 tag                             |

#### Barline types

| Value   | Meaning          |
|---------|------------------|
| 0       | normal           |
| 2       | repeat start     |
| 3       | double (left)    |
| 4       | repeat end       |
| 5       | final            |
| 6       | double (right)   |
| 8       | dotted           |

#### Volta (repeat-alternative) bitmask

Byte 0x0F is a bitmask — bit `n` set means the measure belongs to ending `n+1`.
Encore sets the same bitmask on **every** measure inside the ending (not just the first).
The importer collapses consecutive equal-bitmask runs into one `Volta` with begin-text "1.", "2.", etc.

#### Repeat-mark ladder (LOW byte of 0x1A)

| Byte   | Meaning                                                        |
|--------|----------------------------------------------------------------|
| 0x80   | D.C. al Coda                                                   |
| 0x81   | D.S. al Coda                                                   |
| 0x82   | D.C. al Fine                                                   |
| 0x83   | D.S. al Fine                                                   |
| 0x84   | D.S.                                                           |
| 0x85   | "To Coda" source — displays "To Coda", player jumps from here  |
| 0x86   | Fine                                                           |
| 0x87   | D.C.                                                           |
| 0x88   | Segno marker                                                   |
| 0x89   | Coda destination — displays Coda glyph, player jumps to here   |

`0x85` (CODA1) and `0x89` (CODA2) are paired: `0x85` → TOCODA marker, `0x89` → CODA marker.
Mapping both to CODA was a bug.
ORN subtype `0xA5` and repeat-mark `0x85` are parallel encodings of "To Coda"; both produce a TOCODA marker.

#### BPM semantics

Quarter-note BPM regardless of time signature.
In 3/8, 5/8, etc., Encore's UI shows eighth-BPM (= 2× on-disk value), but the binary always stores quarter-BPM.
An unrelated layout field at +0x18 always holds 200 in v0xC4 files — do not confuse with BPM.

---

### Element body

Each element: 2-byte tick + 1 type/voice byte (high nibble = type, low nibble = voice). `0xFFFF` tick terminates.
After the 3-byte header every element starts with: 1-byte size + 1-byte **staff byte**.

**Staff byte encoding** (same format as `instrStaffIdx` in the LINE block):

| Bits  | Mask   | Meaning                                                                 |
|-------|--------|-------------------------------------------------------------------------|
| 0-5   | `0x3F` | System staff index: which row in the current LINE staffData array.      |
| 6-7   | `0xC0` | Staff-within-instrument (`staffWithin`): which staff of the instrument. |

`staffWithin = staffByte >> 6`. Values: 0 = first staff, 1 = second (bass), 2 or 3 = further staves.

For a piano grand staff, notes on the treble staff use `staffWithin = 0`; notes on the bass staff use
`staffWithin = 1`. All notes in the MEAS stream share `systemStaffIdx = 0` and the `staffWithin` field
distinguishes the destination. The voice field (low nibble of the type/voice byte) is distributed across
staves: voices 0-1 belong to `staffWithin=0`, voices 2-3 to `staffWithin=1`.

| Type   | Name        |
|--------|-------------|
| 0      | NONE        |
| 1      | CLEF        |
| 2      | KEYCHANGE   |
| 3      | TIE         |
| 4      | BEAM        |
| 5      | ORNAMENT    |
| 6      | LYRIC       |
| 7      | CHORD       |
| 8      | REST        |
| 9      | NOTE        |

**Type 0xB (MIDI CC events).** Elements with high nibble 0xB (`EncElemType::UNKNOWN2`) are MIDI
Control Change events stored inline for playback-only use; they have no visual representation.
Observed always with size=12 (total 12 bytes from element start). Byte layout:
```
d[0..1]  tick (uint16 LE)
d[2]     typeVoice = 0xBn (type=11, voice=n)
d[3]     size = 12
d[4]     MIDI channel / track index
d[5]     MIDI CC event marker (0xB0 = CC channel-0)
d[6..9]  zeros
d[10]    MIDI CC controller number (64=sustain pedal, 7=volume, 1=modulation)
d[11]    MIDI CC value (127=max/on, 0=off)
```
Examples observed: `40 7F` (sustain pedal ON), `40 00` (sustain off), `07 6A` (volume 106).
The importer reads these as `EncGenericElem` and skips them; MuseScore playback uses its own
expression/velocity system.

### Multi-stream voices

One Encore voice slot can contain multiple interleaved MIDI tick streams (e.g. from live recording).
Secondary streams are detectable at tick level: a backwards tick, or a non-chord event arriving after
the voice is already full, signals a fresh stream.

### Implicit silences

Encore does **not** always emit explicit REST elements.
A gap between two events of the same voice (ticks advance more than the cumulative face values) represents
silence the user wrote as a rest.
Naive cumulative placement collapses this gap; the importer must detect on-grid tick offsets and advance cumTick.

---

## CHORD element

Type 7. Variable size. Encodes a chord symbol (harmony marking) above the staff.

### Byte layout (content bytes, after the 5-byte element header)

| Offset | Size | Field    | Description |
|--------|------|----------|-------------|
| +0     | 1    | `toniko` | Chord quality type (index 0-62 into the quality table below) |
| +1     | 1    | `tipo`   | Flags: bit 0 = text present, bit 1 = bass note present |
| +2-4   | 3    | —        | skipped |
| +5     | 1    | `xoffset`| Horizontal display offset |
| +6     | 1    | —        | skipped |
| +7     | 1    | `radiko` | Root note (see note encoding below) |
| +8     | 1    | `baso`   | Bass note for slash chords (same encoding as `radiko`; valid only when `tipo & 0x02`) |
| +9     | 36   | `teksto` | Chord text slot (only present when `tipo & 0x01`; UTF-16 LE or Latin-1, byte 0/1 probe) |

Trailing bytes (beyond `+9` when no text, beyond `+45` when text is present) are skipped using the element `size` field.

### Root note encoding (`radiko` / `baso`)

`lower nibble (bits 0-3)` = note name: 0=C, 1=D, 2=E, 3=F, 4=G, 5=A, 6=B

`upper nibble (bits 4-7)` = accidental: 0x0=natural, 0x1=sharp, 0x2=flat

Examples: `0x05`=A, `0x26`=Bb, `0x13`=F#, `0x21`=Db.

### Chord quality table (`toniko`)

When `tipo & 0x01` is set, `teksto` overrides `toniko` and `radiko` (the chord name is taken directly from the text field). When `tipo & 0x01` is clear, the chord name is constructed as `root + quality` from the table below.

| Index | Quality suffix | Index | Quality suffix |
|-------|---------------|-------|---------------|
|  0 | (major, no suffix) | 32 | 9 |
|  1 | m | 33 | 9(b5) |
|  2 | + | 34 | 11 |
|  3 | dim | 35 | 13 |
|  4 | 7 | 36 | 13(b5) |
|  5 | 5 | 37 | 13(b9) |
|  6 | 6 | 38 | 13(#9) |
|  7 | 6/9 | 39 | (undefined) |
|  8 | (add2) | 40 | +7 |
|  9 | (add9) | 41 | +7(b9) |
| 10 | (omit3) | 42 | +7(#9) |
| 11 | (omit5) | 43 | +9 |
| 12 | maj7 | 44 | sus2 |
| 13 | maj7(b5) | 45 | sus2sus4 |
| 14 | maj7(6/9) | 46 | sus4 |
| 15 | maj7(#5) | 47 | 7sus4 |
| 16 | (undefined) | 48 | 9sus4 |
| 17 | maj9 | 49 | 13sus4 |
| 18 | maj9(b5) | 50 | m(add2) |
| 19 | maj9(#5) | 51 | m(add9) |
| 20 | (undefined) | 52 | m6 |
| 21 | maj13 | 53 | m6/9 |
| 22 | maj13(b5) | 54 | m7 |
| 23 | (undefined) | 55 | m(maj7) |
| 24 | 7 (alternate) | 56 | m7(b5) |
| 25 | 7(b5) | 57 | m7(add4) |
| 26 | 7(b9) | 58 | m7(add11) |
| 27 | 7(#9) | 59 | m9 |
| 28-31 | (undefined) | 60 | m(maj9) |
|  | | 61 | m11 |
|  | | 62 | m13 |

Indices 16, 20, 23, 28-31, 39 are undefined in the Encore format; the importer treats them as major (empty suffix). Index 45 in the original Encore encoding is "sus2,sus4"; the comma is removed to avoid conflicts with MuseScore's chord parser.

---

## KEYCHANGE element

Type 2. Size 6 bytes. Byte at +5 = key index into the fifths table:
- 0 = C / 0 fifths
- 1–7 = F..Cb / −1..−7 fifths
- 8–14 = G..C# / +1..+7 fifths

Value 0 is a legitimate change (naturals cancel prior accidentals).

---

## TIE element

Type 3. Size 16 or 18 bytes.

Byte +5 encodes arc direction and outgoing status; byte +6 is an additional tie-start flag.

| Byte +5        | Arc direction   | Tie-start?                         |
|----------------|-----------------|------------------------------------|
| `0xFE` / `0x80`+ | arc above     | yes (bit 7 set)                    |
| `0x02`         | arc below       | yes (bit 1 set, no high bit)       |
| `0x03`         | arc below       | yes (bits 0+1: incoming + outgoing)|
| `0x01`         | incoming only   | no                                 |
| `0x04`         | incoming only   | no                                 |

**Tie-start rule:** an element is a tie-start when `(+5 & 0x80) || (+5 & 0x02) || (+6 & 0x80)`.
Values `0x02` and `0x03` encode the arc-below outgoing direction; they do NOT set bit 7
but bit 1 carries the same "outgoing" semantic.

**Arc x-positions (18-byte elements only).** For size ≥ 18, two additional bytes encode the
visual x-positions of the arc endpoints:

| Offset   | Size   | Description                                                         |
|----------|--------|---------------------------------------------------------------------|
| +10      | 1      | `arcX1` — x-position of arc start (measure-relative pixel units)   |
| +12      | 1      | `arcX2` — x-position of arc end                                    |
| +14      | 1      | `sourcePosition` — staff position of the source note (matches `EncNote::position`); disambiguates which note in a multi-note chord carries the tie |

**Intra-chord arc detection.** When `arcX1 == arcX2`, both endpoints are at the same visual
column — the arc connects two notes of the same chord vertically with zero horizontal extent.
This is a decorative mark in Encore (not a forward tie). The tie-start flag is overridden to
`false` so no MuseScore Tie is created. Such elements often appear in groups of 2–4 identical
copies at the same tick (one per chord note) and always carry `dirByte = 0x02`.

When `arcX1 != arcX2`, the arc spans notes at genuinely different time positions: the
tie-start flag stands, and a normal forward tie is created.

---

## Ornament element

Type 5. Variable size. Offsets from element start:

| Offset   | Size   | Description                                                |
|----------|--------|------------------------------------------------------------|
| +5       | 1      | ornament subtype (see table)                               |
| +10      | 2      | start x-position within the measure (layout units)         |
| +12      | 2      | signed s16 Cartesian y (negative = below staff)            |
| +18      | 1      | alMezuro — measures forward to the end measure             |
| +20      | 1      | xoffset2 — end x-position in the target measure            |
| +26      | 1      | speguleco — bit 0: 0 = crescendo, 1 = diminuendo           |
| +30      | 2      | BPM (TEMPO subtype only)                                   |
| +32      | 1      | TEXT block entry index (STAFFTEXT subtype)                 |

### Ornament subtypes

| Value   | Name          | Notes                                                                            |
|---------|---------------|----------------------------------------------------------------------------------|
| 0x1D    | WEDGESTART    | hairpin; end encoded by alMezuro (+18) and xoffset2 (+20)                        |
| 0x1E    | STAFFTEXT     | text from TEXT block at entry index +32                                          |
| 0x21    | SLURSTART     | slur; endpoint encoded by alMezuro and xoffset2                                  |
| 0x22    | ARPEGGIO      | chord arpeggio                                                                   |
| 0x32    | TEMPO         | tempo; BPM at +30 (reserved, unused — tempo travels as STAFFTEXT)                |
| 0x35    | TRILL_END     | end of trill+wavy-line span; no visible glyph. Consumed as span endpoint.        |
| 0x36    | TRILL_START   | trill span start → MuseScore `Trill` spanner (tr + wavy line) when 0x35 or      |
|         |               | `alMezuro>0` is present; otherwise falls back to Ornament glyph.                |
| 0x37    | TRILL_ALT     | secondary trill mark within a span → always Ornament glyph (not a spanner).     |
| 0xB0    | TRILL_TR      | standalone 16-byte "tr" mark → ornamentTrill glyph; never a spanner.            |
|         |               | Placement: same rules as TRILL_SHORT (REST-forward-snap + dedup).                |
| 0xB6    | TRILL_SHORT   | standalone 16-byte short-trill mark → ornamentShortTrill glyph; never a spanner. |
|         |               | Placed at the note's tick. If tick falls on a REST, snaps forward to the next   |
|         |               | chord in the same measure.                                                       |
|         |               | Encore also stores a secondary "wavy-line extent" element with the same tipo:    |
|         |               | the element's `xoffset` is well to the left (> 20px) of the note at its encoded |
|         |               | tick. Rule: if `ornXoff < noteXoff - 20`, snap backward via xoffset heuristic.  |
|         |               | Dedup prevents a duplicate glyph when primary and secondary resolve to the same  |
|         |               | chord.                                                                           |
| 0x41    | SLURSTOP      | reserved, not emitted in practice                                                |
| 0x4D    | WEDGESTOP     | reserved, not emitted in practice                                                |
| 0x80    | DYN_PPP       | dynamic `ppp` (size-16)                                                          |
| 0x81    | DYN_PP        | dynamic `pp`                                                                     |
| 0x82    | DYN_P         | dynamic `p`                                                                      |
| 0x83    | DYN_MP        | dynamic `mp`                                                                     |
| 0x84    | DYN_MF        | dynamic `mf`                                                                     |
| 0x85    | DYN_F         | dynamic `f`                                                                      |
| 0x86    | DYN_FF        | dynamic `ff`                                                                     |
| 0x87    | DYN_FFF       | dynamic `fff`                                                                    |
| 0x88    | DYN_SFZ       | dynamic `sfz`                                                                    |
| 0x89    | DYN_SFFZ      | dynamic `sffz`                                                                   |
| 0x8A    | DYN_FP        | dynamic `fp`                                                                     |
| 0xA2    | SEGNO         | segno marker on the measure                                                      |
| 0xA5    | TO_CODA       | "To Coda" marker (→ TOCODA)                                                      |
| 0xA6    | CODA          | Coda glyph marker                                                                |
| 0xAA    | DYN_FZ        | dynamic `fz`                                                                     |
| 0xAB    | DYN_SF        | dynamic `sf`                                                                     |
| 0xAF    | TREMOLO_32    | single-chord triple tremolo (3 slashes = 32nd speed); always at voice 0 regardless of note voice |
| 0xB9    | FINGER_1      | stand-alone fingering digit "1" (size-16 ORN; attached to top note of chord)    |
| 0xBA    | FINGER_2      | stand-alone fingering digit "2"                                                  |
| 0xBB    | FINGER_3      | stand-alone fingering digit "3"                                                  |
| 0xBC    | FINGER_4      | stand-alone fingering digit "4"                                                  |
| 0xBD    | FINGER_5      | stand-alone fingering digit "5"                                                  |
| 0xC4    | UPBOW         | up-bow stroke (V) as size-16 ORN; maps to Articulation stringsUpBow             |
| 0xC5    | DOWNBOW       | down-bow stroke (П) as size-16 ORN; maps to Articulation stringsDownBow         |
| 0xC9    | STACCATO      | per-chord staccato dot                                                           |
| 0xCC    | FERMATA_ABOVE | standalone fermata above (size-16 ORN; yoffset > 0)                             |
| 0xCD    | FERMATA_BELOW | standalone fermata below (size-16 ORN; yoffset < 0)                             |
| 0xEF    | TREMOLO_32B   | alternate triple tremolo (ORN at tick == durTicks); also maps to R32             |
| 0xA3    | REPEAT_MEAS   | "%" repeat-last-bar glyph (size-16 ORN); replaces measure content with MeasureRepeat |
| 0xA7    | CAESURA       | caesura (//) breath element placed after preceding note (size-16 ORN)           |
| 0xA8    | BREATH_COMMA  | comma breath mark placed after preceding note (size-16 ORN)                     |

**Undecoded subtypes.** Silently ignored; observed in corpus:

| Tipo  | Count | Files | Hypothesis |
|-------|------:|-------|------------|
| 0xBE  |  12+  | TieYellow | unknown; appears at tick=0, possibly a mark/flag |
| 0xC0  |   3   | Boda-LA, Beethoven | unknown |
| 0xB0  |  11   | TieYellow | decoded as TRILL_TR (standalone ornamentTrill glyph) |
| 0xC6, 0xC8, 0xEE | rare | various | unknown |

### Hairpin direction (speguleco bit 0)

Bit 0 of `speguleco` at +26: 0 = crescendo, 1 = diminuendo.
Encore 5 also sets bit 1 (crescendo = `0x02`, diminuendo = `0x03`); legacy files use `0x00`/`0x01`.
Always test with `speguleco & 0x01`, not `speguleco == 0`.

### Spanner endpoints (hairpins, slurs)

alMezuro (+18) = count of measures forward to the end measure.
xoffset2 (+20) = visual x within that target measure.
No separate WEDGESTOP or SLURSTOP element is emitted.

**Hairpin endpoint.** Primary: walk forward from WEDGESTART for the first Dynamic on the same track within
the alMezuro window and stop there.
Fallback (no Dynamic found): use xoffset2 bar-line clamp — when xoffset2 < first NOTE's xoffset in the
target measure, Encore drew the hairpin ending at the bar line; clamp to targetMeasure.tick.

**Slur endpoint (pixel-span heuristic).** `slurXoffset2 - slurXoffset` equals `endNote.xoffset - firstNote.xoffset`.
Recover end tick via `target = firstNote.xoffset + (slurXoffset2 - slurXoffset)` and snap to the nearest note.
Only applies when alMezuro == 0; cross-measure slurs fall back to the last ChordRest in the target measure.

`xoffset` is stored as `qint8` but must be treated as `quint8` (unsigned) for the pixel-span computation:
values > 127 are stored negative (e.g. 0x8A = -118 signed = 138 unsigned). Using signed arithmetic
gives a huge spurious pixel span; unsigned gives the correct 1-2 note span.

**startEncTick formula.** To reverse-map the slur's MuseScore start tick to an Encore tick (for finding
`firstNoteXoff`): use `wt = durTicks × timeSigDen / timeSigNum` (whole-note ticks), NOT
`beatTicks × timeSigDen`. In compound meters (e.g. 6/8 with beatTicks=240, durTicks=720):
`beatTicks × timeSigDen = 240 × 8 = 1920 ≠ wt = 720 × 8/6 = 960`. Using the wrong formula shifts
`firstNoteXoff` to the wrong note and causes slurs to end too late.

### Dynamic staff displacement (yoffset > 0)

Normally `yoffset < 0` (below the staff).
When the user drags a dynamic up onto the staff above, `yoffset` becomes positive while `staffByte` still
names the lower staff.
Correct target staff = `staffIdx - 1` when `yoffset > 0`.

### End-of-measure ornament ticks

A DYN or STAFFTEXT with `tick > durTicks` is a section-end marker
(e.g. a 2/4 measure with volta dynamics at tick 0 and tick 960).
The importer pins these to the last ChordRest segment of the source measure.

### Snap-back-by-xoffset (DYN and WEDGESTART)

Encore tags an ornament at the chord-rest AT OR AFTER its visual position but stores the visual x in `xoffset`.
When `ornament.xoffset < tagged_chord.xoffset`, Encore visually pulled the glyph back.
The importer walks backwards on the same `(staffIdx, voice)` for the latest NOTE/REST with
`xoffset <= ornament.xoffset` and re-anchors there.

### Encoding probe (all text-bearing fields)

| Field                      | Probe                                                       |
|----------------------------|-------------------------------------------------------------|
| TK block instrument name   | byte 0 printable + byte 1 == 0x00 → UTF-16                  |
| TK fallback (NAME_BASE)    | same probe; falls back to Latin-1                           |
| LYRIC element              | byte 0/1 probe at text start                                |
| TEXT block entries         | byte 14/15 probe; length bounded by `0x04 0x00` terminator  |
| CHORD-symbol text          | byte 0/1 probe                                              |
| TITL block                 | varsize < 5000 → Latin-1; varsize ≥ 10000 → UTF-16          |

### v0xA6 grace note time-borrowing

In v0xA6, grace notes occupy real tick positions (not co-located with the main note).
A 32nd grace at tick=120 pushes subsequent notes forward; the last real note ends up with `rawGap < faceValue`.
The importer restores this: check `Σ graceFaceValues == faceValue − rawGap` and set `realDuration = faceValue`.

**Inner grace detection.** After a leading grace (grace1 & 0x30 == 0x20), inner graces
(grace1 & 0x30 == 0x10) have a strictly larger faceValue (shorter note).
Only applies when `fv > maxFvInQueue`.

**Face-grid snap suppression.** The implicit-silence snap must be suppressed while graces are pending
(prevents spurious rests before the grace group) and also for subsequent notes whose apparent gap equals
the stolen grace ticks (`stolenTicks` accumulated per trackKey).

### Grace-note slurs (SLURSTART co-located with appoggiatura)

When a SLURSTART ornament (tipo 0x21) is at the same Encore tick as an appoggiatura
grace note, both elements reference the same beat in the measure (tick=0 of that beat
in Encore). In MuseScore, the grace note and its parent chord both land at cumTick=0
because grace notes steal ticks without advancing cumTick.

**Problem**: the same-measure xoffset heuristic converts the end note's Encore tick
(e.g. tick=15) to a proportional MuseScore tick (`Fraction(15, 960)`). No chord exists
there — the parent chord is at cumTick=0. The slur's `computeEndElement()` call finds
nothing and returns null → slur removed.

**Fix**: two cases are handled:

1. **Grace-to-main** (`ps.startTick == endTick` after snapping, e.g. grace and parent both at
   measure beat 0): a slur is created with explicit `startElement = graceChord` and
   `endElement = mainChord`. Both `computeStartElement()` and `computeEndElement()` are
   skipped in the validation loop.

2. **Grace-to-later** (`ps.startTick < endTick`, e.g. SLURSTART at Encore tick=450, grace at
   450, regular note at 480, half note at 0): `tick2rightSegment(ps.startTick)` finds the
   chord AT or AFTER startTick (the regular quarter note), reads its `graceNotesBefore()`,
   and sets `slur->setStartElement(graceChord)`. Only `computeStartElement()` is skipped in
   the validation loop; `computeEndElement()` runs normally and anchors the slur end to the
   correct later chord.

### Multi-staff instruments: staffWithin field

For instruments with more than one staff (piano, harp, organ), all notes from all staves share
the same MEAS element stream with `systemStaffIdx = 0`. The destination staff is encoded in bits
6-7 of the staff byte (`staffWithin = staffByte >> 6`):

- `staffWithin = 0`: note belongs to the first (treble) staff.
- `staffWithin = 1`: note belongs to the second (bass) staff.
- `staffWithin = 2` or `3`: third or fourth staff (uncommon).

Within each destination staff, voices are re-indexed from 0. For a 2-staff instrument, Encore
stores voices 0-1 for the first staff and voices 2-3 for the second staff in the stream; the
importer remaps voice by subtracting `staffWithin * 2` after routing.

### System-level ornaments (voice = 4)

System-wide ornaments use `voice = 4` AND set the high bit of the staff byte (`0x40`).
This sets `staffWithin = 1` (second staff) on the raw byte. Readers that support `staffWithin`
routing must check `voice >= 4` first to distinguish system ornaments from regular second-staff
notes; do not route voice-4 ornaments to the second instrument staff.

Some files store NOTE/REST/BEAM with `voice = 4` WITHOUT a valid staffWithin relationship
(seen in v0xC4 SATB scores where voice 4 is an out-of-band grand-staff slot).
The correct interpretation depends on the LINE block's multi-staff configuration.

### Out-of-range voice on regular elements

Some files store NOTE/REST/BEAM with `voice = 4` WITHOUT the `0x40` staff-byte flag.
These are real content; the importer maps them to voice 0 of that staff so LYRIC attachment can find them.

---

## Lyric element

Type 6. Variable size. Null-terminated text, NOT fixed-width.

**v0xC4 layout (text at +20):**

| Offset    | Size   | Description                                            |
|-----------|--------|--------------------------------------------------------|
| +0        | 2      | within-measure tick                                    |
| +2        | 1      | type/voice byte (high nibble = 6, low = voice)         |
| +3        | 1      | element size (24..36+)                                 |
| +4        | 1      | staffIdx & 0x3F                                        |
| +0x0A     | 1      | text anchor (x-offset equivalent)                      |
| +0x14..   | var    | text payload (UTF-16 LE or Latin-1, null-terminated)   |

**v0xC2 layout (text at +18, 2 bytes earlier):**

| Offset    | Size   | Description                                            |
|-----------|--------|--------------------------------------------------------|
| +0        | 2      | within-measure tick                                    |
| +2        | 1      | type/voice byte (high nibble = 6, low = voice)         |
| +3        | 1      | element size (20..26+)                                 |
| +4        | 1      | staffIdx & 0x3F                                        |
| +0x0A     | 1      | text anchor (x-offset equivalent)                      |
| +0x12..   | var    | text payload (UTF-16 LE or Latin-1, null-terminated)   |

The 2-byte difference is handled via `EncFormatReader::lyricTextGapAfterKie()` (returns 9 for v0xC4, 7 for v0xC2).

Observed sizes in v0xC4: 24 (`-` dash), 26 (empty word-break), 30 (2 chars), 32 (3 chars), 34 (4 chars).
Observed sizes in v0xC2: 20 (`-` dash), 22 (1-2 chars), 24 (3 chars), 26 (4-5 chars).

**Encoding.** Detected per element via byte 0/1 probe (same as instrument names).
Portuguese/Spanish scores from older Encore builds use Latin-1.

**Separator tokens.**

| Text         | Role                                            |
|--------------|-------------------------------------------------|
| `"-"`        | hyphen between syllables of the same word       |
| `""` empty   | word-break (resets hyphen state)                |
| other        | real syllable                                   |

Syllabic role (begin/middle/end/single) derived from hyphen-before / hyphen-after flags.

**Multi-verse.** Verse N uses voice (N−1) on the same staff. All verses anchor on the voice-0 chord.

**Lyric-to-note matching.** The importer matches each lyric to the nearest chord segment within a half-beat window (`matchThreshold = beatTicks / 2`). For compound meters (6/8, 9/8, 12/8) the segEncTick formula uses `encTicksPerQuarter = beatTicks * 2/3` (since `beatTicks` represents a dotted-quarter beat in those meters, not a plain quarter). The match window uses the wider value `beatTicks * 2/3` to accommodate Encore's visual pre-positioning of lyrics. Assignment is lyrics-first: each lyric (in tick order) claims the nearest unclaimed note, avoiding the note-first greedy issue where a later syllable can steal the note from an earlier one.

---

## Note element

### v0xC4 (size = 28)

| Offset   | Size   | Description                                                                      |
|----------|--------|----------------------------------------------------------------------------------|
| +5       | 1      | face value — high nibble: 0=normal, 3=square notehead, 5=cross/X notehead (cymbal/triangle on PERC staves); low nibble: 1=whole, 2=half, 3=qtr, 4=8th, …, 8=128th |
| +6       | 1      | grace1 (high-nibble flags, see grace section)                                    |
| +7       | 1      | grace2                                                                           |
| +10      | 2      | layout x-position                                                                |
| +12      | 1      | staff-relative pitch — diatonic steps from C4 (C4=0, D4=1, E4=2, F4=3, … A5=12). On PERC clef staves this byte encodes the visual staff line in Encore; the importer converts it to a MuseScore drumset line: `line = max(-4, 10 − position)`. MuseScore's PERC clef places A4 on the middle line (line=5), so D4→line=9, F4→line=7, A5→line=−2. On pitched staves: legacy display hint, not used for playback. |
| +13      | 1      | tuplet byte — high nibble = actualN, low nibble = normalN                        |
| +14      | 1      | dot count (0/1/2/3)                                                              |
| +15      | 1      | MIDI pitch (0–127)                                                               |
| +16      | 2      | playback duration in ticks (recorded MIDI; diverges from notated for tuplets)    |
| +19      | 1      | velocity                                                                         |
| +20      | 1      | options                                                                          |
| +21      | 1      | alteration glyph (accidental override)                                           |
| +24      | 1      | articulation byte — above slot                                                   |
| +26      | 1      | articulation byte — below slot                                                   |

### v0xA6 (size = 10, slot = 20 bytes = size × 2)

| Offset   | Description                                                    |
|----------|----------------------------------------------------------------|
| +5       | face value                                                     |
| +6       | grace1 (& 0x30: 0x20 = APPOGGIATURA, 0x10 = inner grace)       |
| +7       | explicit tuplet byte (3:2 = `0x32`, 5:4 = `0x54`, …)           |
| +9       | staff-position / diatonic line — NOT the MIDI pitch            |
| +11      | MIDI pitch (absolute 0–127)                                    |

Byte +9 is staff-position (e.g. 11 for B4 in treble clef counting), NOT pitch.
Byte +11 is the playable MIDI value.

---

## REST element

### v0xC4 (size = 18)

| Offset   | Size   | Description                                                                      |
|----------|--------|----------------------------------------------------------------------------------|
| +5       | 1      | face value — same encoding as Note element                                       |
| +10      | 1      | layout x-position                                                                |
| +13      | 1      | tuplet byte — high nibble = actualN, low nibble = normalN (same as note)         |
| +14      | 1      | dotControl — **bitmask flag, NOT a tick count**. Bit 0 = dotted display hint.   |

**dotControl semantics.** dotControl is a **bitmask**, not a sounding tick value:

| Bit | Meaning                        |
|-----|--------------------------------|
| 0   | dotted display flag (1 = dotted) |
| others | visual/layout hints, ignore  |

Do NOT pass dotControl as a raw tick count to `calcDots()` — it will return 0 in most
cases. Instead:
1. Try `calcDots(dotControl, fv)` (works when dotControl happens to equal a dotted tick count).
2. Fallback to `calcDotsSnap(realDuration, fv)` (handles exact or ±1-tick-accurate rdur).
3. If both return 0 AND `dotControl & 1`, force 1 dot. This handles MIDI timing drift
   where rdur is 10–20 ticks off from the theoretical dotted value.

---

### Articulation bytes

Each byte holds one or two glyphs:

| Value        | Glyphs                    |
|--------------|---------------------------|
| 0x04         | trill (plain; no accidental on upper neighbor)                     |
| 0x05         | trill to minor second (flat upper neighbor; `intervalAbove=MINOR`) |
| 0x06         | trill to augmented second (sharp; `intervalAbove=AUGMENTED`)       |
| 0x07         | trill to major second (natural; `intervalAbove=MAJOR`)             |
| 0x08         | turn                                                               |
| 0x09         | inverted turn                                                      |
| 0x0A, 0x0C   | inverted-mordent                                                   |
| 0x0B, 0x2F   | mordent                                                            |
| 0x12         | accent (`->`)                                                      |
| 0x13         | marcato (`-^`)                                                     |
| 0x14         | accent + tenuto                                                    |
| 0x15         | marcato + staccato                                                 |
| 0x16         | accent + staccatissimo                                             |
| 0x17         | accent + staccato                                                  |
| 0x18         | up bow                                                             |
| 0x19         | down bow                                                           |
| 0x1B         | stopped horn/brass (+)                                             |
| 0x1C         | tenuto (`--`)                                                      |
| 0x1D         | staccato (`-.`)                                                    |
| 0x1E, 0x1F   | harmonic                                                           |
| 0x20–0x22    | fermata variants; **but 0x20/0x21 on a note with tuplet != 0 means "tuplet bracket above/below" (not a fermata)** |
| 0x24         | tenuto + staccato                                                  |
| 0x25         | marcato + tenuto                                                   |
| 0x28–0x2D    | staccatissimo combos                                               |
| 0x2E         | inverted turn                                                      |
| 0x30         | half-stopped horn (circle-plus)                                    |

### Technical markings (reuse articulation slots)

| Byte         | Meaning                                                        |
|--------------|----------------------------------------------------------------|
| 0x0D–0x11    | fingering 1–5                                                  |
| 0x1E, 0x1F   | harmonic (see above)                                           |
| 0x44, 0x45   | thumb-position                                                 |
| 0x46         | open-string (plain Fingering "0", not circled)                |
| 0x47         | "stick" technique; no standard SMuFL equivalent (not imported) |
| 0x39–0x40    | scale string numbers 1–8 (byte `0x38 + N` = string N); when at least one such byte appears in a measure, all notes in that measure with options bit 0 set also display their scale-degree position as a circled string number |

### Single-note tremolos (articulation slots)

| Byte   | Strokes   | Notes                                            |
|--------|----------:|--------------------------------------------------|
| 0x41   | 1         | 8th tremolo                                      |
| 0x42   | 2         | 16th tremolo                                     |
| 0x43   | 3         | 32nd; Encore may render 4 strokes in some files  |
| 0x03   | 3         | bare 3-stroke variant (no high-nibble flag)      |

`0x44` and above are technical markings — NOT tremolos.

### Deduplication of artic-byte markings on chords

Each NOTE element carries its own `articulationUp` and `articulationDown` bytes.
When multiple notes in the same chord (same tick, same voice) carry the same
artic byte, each would independently produce the same ornament or articulation
glyph on the chord — resulting in duplicate visual marks (e.g. two "tr" symbols).

**Importer rule:** before adding an ornament or articulation to a chord, the
importer checks `chord->articulations()` for an existing element with the same
SymId and skips the new one if found. Since `Ornament` extends `Articulation`
and is stored in the same list, this dedup covers both types.

**Example:** two notes at tick=0 (forming a chord) both have `au=0x04` (trill).
Without dedup, the chord would receive two `ornamentTrill` elements; the rule
ensures exactly one is added.

---

## Rhythm encoding

240 ticks per quarter note. **Whole-note tick count** is always 960 for any time signature
and can be computed reliably as `(durTicks * timeSigDen) / timeSigNum`. Do NOT use
`beatTicks * timeSigDen`: in compound meters (e.g. 6/8) `beatTicks` is the compound beat
(360 for the dotted quarter), giving 2880 instead of the correct 960.

| Face value   | Ticks   | Duration   |
|-------------:|--------:|------------|
| 1            | 960     | whole      |
| 2            | 480     | half       |
| 3            | 240     | quarter    |
| 4            | 120     | eighth     |
| 5            | 60      | 16th       |
| 6            | 30      | 32nd       |
| 7            | 15      | 64th       |
| 8            | 7       | 128th      |

Notated duration = face value + dot count + tuplet byte.
The playback duration at +16 diverges (live recording, ties, tuplets).

**Tuplets.** Either explicit byte `(actualN << 4) | normalN` or implicit (playback duration
≈ faceTicks × 2/3 or 4/5). Implicit detection applies only to v0xC2 files.

Supported explicit ratios (importer creates a Tuplet bracket):

| Ratio  | Example               | Constraint |
|--------|-----------------------|------------|
| 2:1    | dosillo de redonda    | normalN × baseLen must be TDuration-aligned |
| 2:3    | compound duplet       | |
| 2:4    | 2 in 4 beats          | |
| 3:2    | triplet               | |
| 4:1, 4:2, 4:3 | quadruplet  | |
| 5:2, 5:3, 5:4, 5:6, 5:8 | quintuplet | 5:4 is the standard; others need normalN × baseLen ∈ TDuration |
| 6:4, 6:7, 6:8 | sextuplet | |
| 7:4, 7:6, 7:8 | septuplet | |
| 8:4, 8:6 | octuplet | |
| 9:4, 9:6, 9:8 | nontuplet | 9:5 is NOT supported (5/8 is not a TDuration-aligned fraction) |
| 10:6, 10:8 | decuplet | |

Ratios with normalN ∈ {5, 9, 10, 15, ...} produce Tuplet.ticks = normalN × baseLen that
cannot be represented as a standard TDuration (e.g. 9:5 with 8th gives ticks=5/8, which
is not a valid note value). Such ratios are left as plain notes without a bracket.

**Beat-relative face values.** In compound and simple meters where one beat equals an eighth
(e.g. 6/8, 8/8, 12/8), Encore stores the face value as the number of "beats", not as an
absolute note value. A Q-face note (`fv=3`) in an 8/8 3:2 triplet thus represents one eighth
beat, not one quarter note. The actual written duration is `rdur × (actualN / normalN)`; when
that product equals a standard tick count (E=120, Q=240, …), it overrides the face value.
Detection: `rdur == beatTicks × (normalN / actualN)` (one beat per tuplet slot).

**Dotted notes.** Dot count at +14.
Can be inferred from `playbackTicks == faceTicks × 3/2` (one dot), `7/4` (two dots), with ±1-tick
tolerance. For rests, dotControl (+10) is a bitmask flag, NOT a tick count; use
`calcDotsSnap(realDuration, fv)` as the authoritative dot source.

**Ghost rest filter.** `calculateRealDurations` sets a REST's rdur to `nextTick - tick` (the
MIDI gap to the next event). When a note starts only a few ticks after the rest's MIDI start
(MIDI timing slop), rdur ends up far shorter than the face value (e.g. rdur=5 for a 32nd rest
with faceTicks=30). The ghost-rest filter (`rdur > 0 && rdur < 15`) must not drop these real
rests. Rule: if `faceTicks >= 30` (32nd or longer), trust the face value regardless of rdur. Only
drop rests whose face value is also very short (64th or smaller, faceTicks < 30).

---

## BEAM element

Type 4. Explicit beaming per level:

| Size   | Byte +5   | Beam level            |
|--------|-----------|-----------------------|
| 30     | 0x01      | 1st (8th flag)        |
| 46     | 0x02      | 2nd (16th extension)  |
| 62     | 0x03      | 3rd (32nd extension)  |

---

## TEXT block

Carries text payloads for STAFFTEXT ornaments (subtype `0x1E`).
Block layout (after 8-byte magic + varsize):

| Offset   | Size   | Description           |
|----------|--------|-----------------------|
| +0       | 2      | sync (`0x0000`)       |
| +2       | 2      | entry count           |
| +4       | 4      | total content bytes   |
| +8…      | var    | entries (see below)   |

Each entry:

| Offset      | Size   | Description                                                       |
|-------------|--------|-------------------------------------------------------------------|
| +0          | 2      | payload size                                                      |
| +2          | 14     | header (partially decoded)                                        |
| +16..term   | var    | text (UTF-16 LE or Latin-1); ends at first `0x04 0x00` terminator |
| term..+3    | 4      | `0x04 0x00 0x00 0x00` terminator (may be followed by padding)     |

Text length is bounded by the terminator, **not** by `payload_size - 14 - 4`
(some entries carry padding after the terminator).
Dynamic marks use their own ornament subtypes — they are NOT in the TEXT block.

---

## TITL block

Title, 2 subtitles, 3 instructions, 4 authors, 2 headers, 2 footers, 6 copyright lines.
Encoding from varsize: < 5000 → Latin-1 (96 bytes/line); ≥ 10000 → UTF-16 LE (1056 bytes/line).

### UTF-16 line layout (1056 bytes)

| Offset      | Size   | Description                                                |
|-------------|--------|------------------------------------------------------------|
| +0–+29      | 30     | prefix (byte +14 = horizontal alignment for header/footer) |
| +30–+1055   | 1026   | UTF-16 LE text, NUL-terminated, zero-padded                |

Alignment byte: `0x02` = RIGHT, `0x04` = LEFT, `0x06` = CENTER. Other line types leave it at `0x00`.

### Slot counts

| Category      | Slots   |
|---------------|--------:|
| title         | 1       |
| subtitle      | 2       |
| instruction   | 3       |
| author        | 4       |
| header        | 2       |
| footer        | 2       |
| copyright     | 6       |

Multiple non-empty slots in a category render as stacked lines.
Each slot is independently NUL-terminated; bytes after the NUL are prior-edit debris.

### Replaceable tokens (header/footer only)

| Token   | Expanded value   |
|---------|------------------|
| `#P`    | page number      |
| `#D`    | date             |
| `#T`    | time             |

### Duplicate TITL blocks

Some files write TITL twice (identical content). Treat idempotently — do not concatenate slots.

---

## WINI block (page setup)

Optional block written only when the user explicitly opens and saves Page Setup in Encore.
Files that have never been through Page Setup have no WINI block; the importer uses
MuseScore defaults in that case. Present in all files saved by Encore 5.0.2 (`chuVersio = 1056`).

Block layout (after 8-byte magic + varsize header):

| Offset | Size | Type    | Description |
|--------|------|---------|-------------|
| +0     | 24   | bytes   | screen/window data (not used by importer) |
| +24    | 4    | int32LE | top margin in typographic points (1/72 in) |
| +28    | 4    | int32LE | left margin in pts |
| +32    | 4    | int32LE | bottom edge of printable area (pageHeight_pts - bottomMargin_pts) |
| +36    | 4    | int32LE | right edge of printable area (pageWidth_pts - rightMargin_pts) |
| +40    | 2    | uint16  | flags (observed: 1) |

Total content size: 42 bytes (`varsize = 42`). Some older files have `varsize = 40`
(the trailing uint16 is absent); the importer handles both.

Derived values:

```
topMargin    = top / 72.0                        (inches)
leftMargin   = left / 72.0
printWidth   = (rightEdge - left) / 72.0
printHeight  = (bottomEdge - top) / 72.0
bottomMargin = pageHeight - topMargin - printHeight   (pageHeight from style, default A4)
```

**Encoding quirk.** Encore stores `round(inches × 72)`, then displays
`floor(pts / 72 × 1000) / 1000`. A user-entered 0.100" stores as 7 pts and
displays back as 0.097".

**Zero-margin files.** When all four margins are 0, `top = left = 0` and
`bottomEdge = pageHeight_pts`, `rightEdge = pageWidth_pts`. The importer
accepts this (guard requires `bottomEdge > 0 && rightEdge > 0`).

---

## Importer: measure and tuplet rules

This section documents the decisions the importer makes when Encore content does not map
cleanly to standard MuseScore notation.

### Measure length: fill and discard rules

Encore does not require measures to be completely filled with notes and rests. The importer
enforces correct measure length using the following rules, applied in order:

1. **Fill with rests.** After all notes are placed, MuseScore's `checkMeasure` fills any
   gaps with invisible gap rests so that every voice sums to exactly the measure duration.

2. **Overshoot removal (small).** If the voice total exceeds the measure length by ≤ 1/24
   of a whole note after gap-fill, the importer removes the smallest trailing gap rests
   until the sum is correct.

3. **Undershoot fill (small).** If the voice total falls short by ≤ 1/24, a single
   invisible V_MEASURE gap rest is added for the deficit.

4. **Hard nuclear cap.** If the voice still overflows after steps 1–3 (by any amount),
   trailing ChordRest elements are removed from the end of the voice, smallest-first, until
   the sum is ≤ measure length. Any remaining deficit is filled with a gap rest.
   This guarantees the importer never produces a corrupt measure (`sanityCheck` always passes).

5. **Notes discarded, not moved.** Notes that arrive after the voice is already full
   (`cumTick ≥ measure->ticks()`) are **silently dropped**. They are never routed to a
   second MuseScore voice (voice 1). Encore sometimes stores multiple MIDI recording passes
   in the same voice slot; the importer treats only the first fill as valid and discards the
   rest.

6. **Anacrusis / pickup measure.** When the first measure's time signature differs from the
   score's nominal time signature, it is treated as a pickup measure (`isIrregular = true`).
   No fill rests are added beyond the pickup duration.

### Tuplets: compaction into available space

Encore freely allows writing more tuplet notes than the nominal group size. For example,
a phrase encoded as 12 notes all carrying `tup = 9:5` (nine-in-five), or 15 notes all
carrying `tup = 9:5`, cannot be directly represented by standard groups without overflowing
the measure.

The importer detects this situation and **re-computes the tuplet ratio to fit exactly in
the available space**. The algorithm, applied before the regular explicit-group logic:

For each contiguous run of N notes in a voice, all sharing the same explicit tuplet byte
`tup = an:nn` and the same face value `fv`:

1. N must be > an (more notes than one stated group).
2. N must **not** be an exact multiple of an (otherwise the regular logic creates the
   correct number of standard groups: e.g. N=6, an=3 → two `[3:2]` groups, no override).
3. The "non-override interpretation" must **overflow** the remaining measure space:
   `floor(N/an) × fv × nn + (N%an) × fv + trailingDur > durTicks`.
   If the standard interpretation already fits, no override is needed.
4. Compute:
   ```
   available = durTicks − leadingDur − trailingDur
   m = round(available / fv_ticks)
   ```
   where `leadingDur` = actual duration of notes before this run (accounting for their own
   tuplet ratios), and `trailingDur` = actual duration of notes after this run.
5. If `m > 0` and `m × fv` is a standard TDuration-aligned fraction (e.g. half, dotted
   half, whole), create a single `[N:m/fv]` bracket for all N notes.

**Examples:**

| Encore input | Available space | Computed [N:m] | Result |
|---|---|---|---|
| 15 notes `tup=9:5`, fv=8th, 4/4 alone | 960t | m=960/120=8 | `[15:8/eighth]` fills 1 measure |
| 12 notes `tup=9:5`, fv=8th + 2 plain 8ths at end | 720t | m=720/120=6 | `[12:6/eighth]` (3/4) + 2×(1/8) = 1 |
| 10 notes `tup=9:4`, fv=quarter, 4/4 alone | 960t | m=960/240=4 | `[10:4/quarter]` fills 1 measure |

The rule works at any position: leading notes before the run reduce `available`, trailing
notes after the run also reduce it.

**Accepted normalN values for Tuplet.ticks safety:**
`normalN ∈ {1, 2, 3, 4, 6, 7, 8}` always produce a TDuration-aligned `normalN × baseLen`.
normalN=5 and 10 give non-standard fractions (e.g. 5/8, 5/4) and cannot be stored in
MuseScore's Tuplet.ticks without crashing beam layout. As a special case, ratios with
normalN=5 that trigger the compaction rule are overridden to a safe normalN instead
(e.g. 9:5 → 15:8, 12:9-5 → 12:6).

### Tuplets: nested triplets

When a measure contains a 3:2 triplet group that closes via the no-downdate rule (the
second note has a smaller face value than the first, downdating the threshold) and the
triggering note together with the next `actualN-1` notes of the same face value form a
complete inner triplet, a **nested Tuplet** is created:

- Outer 3:2/eighth: spans one beat. Elements = {note_a, inner-triplet, note_b}.
- Inner 3:2/sixteenth: nested inside outer. Elements = 3 sixteenth notes.

The inner Tuplet's `setTuplet(outerTuplet)` links them; MuseScore renders the standard
nested bracket notation. Advances use the doubly-nested ratio (inner × outer) to keep
cumTick exact so subsequent plain notes are placed correctly.

### Tuplets: 9:5 nontuplet

9:5 is supported when the compaction rule fires (see above). When 9:5 notes do NOT trigger
the compaction rule (i.e. exactly 9 notes at the beginning of the measure), the group is
created as a standard `[9:5/eighth]`:
- `Tuplet.ticks = 5/8` is non-standard but is set AFTER all 9 notes are placed (mirroring
  MuseScore's `sanitizeTuplet()` path, which avoids the debug-build assertion in
  `TDuration(Fraction, truncate=false)`).
- `beam.cpp` uses `TDuration(tuplet->ticks(), true/*truncate*/)` to avoid asserting on
  non-standard spans during beam-break calculation.

### Last note of a measure-spanning tuplet

The last note of a tuplet that spans to the very end of a measure often has a very short
`realDuration` (rdur) — e.g. a note at tick 954 in a 960t measure has rdur≈6 — because
Encore truncates playback durations at the barline. The importer's MIDI-artifact filter
(`rdur > CHORD_CLUSTER_THRESHOLD=4 && rdur < 15 → skip`) would otherwise drop this note.

**Fix:** notes in `validTupletGroupMember` bypass the rdur-based artifact filter. Their
group membership already guarantees they are legitimate notation notes.

### Mixed-duration tuplet group truncated at measure boundary

When a tuplet group contains mixed note values (e.g. `{Q, Q, 8th, 8th}` in a 3:2 bracket
summing to face value 3Q = `fullFaceSum`), Encore omits the final note(s) when their MIDI
start tick equals `durTicks` (measure boundary). The written file therefore has an incomplete
group: face-value sum < `fullFaceSum`, even though element count may already equal `actualN`.

The importer detects this via `faceTicks < fullFaceSum` in `closeTupletWithFill` and adds an
invisible fill rest with the remaining face value, completing the group so `checkMeasure` sees
a full measure. The fill rest's duration is derived from `fullFaceSum − faceTicks` (e.g. 1/8
for the case above), and its advance = `remainingFace × normalN/actualN` (e.g. 1/12).

### Tuplet advances inside active groups

Gap-snap (advancing cumTick to the note's face-value grid position when a gap is detected)
is **suppressed** while a tuplet group is active (`inActiveTuplet = true`). Tuplet notes
are placed by cumTick advance, not by their MIDI tick. Firing gap-snap inside a tuplet
would create spurious rests and misalign subsequent notes.

### Voice assignment rules

| Encore voice byte | MuseScore voice |
|---|---|
| 0 | 0 |
| 1 | 1 |
| 2 | 2 (or staff 2 voice 0 for grand-staff instruments) |
| 3 | 3 |
| ≥ 4 (out-of-band) | 0 of the adjacent staff |
| `staffWithin > 0` | staffIdx + staffWithin, voice remapped |

**No multi-stream overflow.** If cumTick fills the measure for a given voice, any
additional notes with the same Encore voice byte are dropped. They are never routed to
the next MuseScore voice.

### Chord symbol placement

CHD elements (type 7) contain harmony markings. Their encoded tick often carries a
small MIDI offset from the note they annotate — for example, a chord symbol that
logically belongs to beat 1 may have tick=6 while the note is at tick=0.

**Placement rule:** Encore renders chord symbols at BEAT positions, not at
individual note ticks. The importer applies the same principle:

1. Compute `beatStart = floor(chd_tick / beatTicks) * beatTicks` — the start of
   the beat that contains the CHD.
2. Find the **first** existing `ChordRest` segment in the measure whose relative
   tick is in the range `[beatStart, chd_tick]`.
3. Attach the harmony there as a segment annotation.
4. Fallback (no segment in that range): scan backwards for any segment before
   the CHD tick. If still none, use `elemTick` (cumTick-based).

This handles both small drift (CHD@6 for a beat-1 chord, 6t from note=0) and
the subtle near-miss case: a CHD@62 in a measure with notes at tick=0 **and**
tick=60 must snap to tick=0 (beat start), not to tick=60 (which is only 2t
away but is the SECOND 16th note of the beat).

**Example:** M15 of a 2/4 piece. `beatTicks=240`. CHD@62. Notes at tick=0 and
tick=60. `beatStart = floor(62/240)*240 = 0`. First segment in [0, 62] =
tick=0 → harmony on beat 1, first 16th note. ✓

---

## Known quirks

- Encore 5.0.2 can omit instrument block headers while still writing the name at the formula-derived offset.
- Encore 5.0.2 always uses UTF-16 instrument names even when the offset field implies Latin-1.
- TITL block's internal version field is unreliable; use varsize for encoding detection.
- v0xA6: notes are 10 bytes; MIDI pitch at +11 (NOT +9 = staff-position); element offset in MEAS is 0x1A
  instead of 0x36; TK blocks are 64 bytes wide; Key transposition at TK content +42 (not the PRG_BASE
  formula); file header ends at 0xA6; occasional back-to-back identical REST pairs dedupe to one rest.
- v0xA6: grace notes stored at real tick positions — push subsequent notes forward and leave the last real
  note with `rawGap < faceValue`. Restored via the grace time-borrowing fix.
- Percussion tracks always report MIDI program 1; identify kit from track name.
- Italian tempo terms travel as STAFFTEXT; TEMPO subtype (0x32) is reserved but unused.
- Lyric voice = verse index, not a real voice assignment.
- Repeat-mark field: repeat type is LOW byte only (`value & 0xFF`).
- Glyphs dropped by Encore's own MusicXML exporter (per-chord staccato `0xC9`, trill-end `0x35`)
  are recoverable directly from the `.enc` binary.
- Largest legitimate block ≈ 2 KiB; a longer run of unrecognised bytes suggests a corrupt file.
- **Duplicate NOTE elements in chord clusters.** Some Encore files encode the same pitch twice in
  the same chord cluster: two NOTE elements with identical tick/staff/voice/pitch. Two variants:
  (a) the second copy has `grace1 bit 0x40` set (chord-extension marker), the first does not; or
  (b) both copies have `grace1 = 0` (seen in some v0xC2 files). In both cases adding both creates
  two noteheads at the same stem position. The importer suppresses any note whose pitch is already
  present in the current chord, regardless of the `grace1` value.
- **Key=0 and template transposition.** When Encore's Key field is 0 (`0 = sounds as written`),
  the importer stores notes at written pitch with no chromatic shift (`staffPitchOffset = 0`).
  If the MIDI program causes a transposing template to be selected (e.g. Bb clarinet for MIDI 72),
  the template's non-octave transposition is zeroed out so that the stored written pitch is
  displayed as-is. Octave transpositions from the template (e.g. guitar, bass) are preserved.
