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
| 0x28     | 2      | unknown                                 |
| 0x2A     | 2      | unknown                                 |
| 0x2C     | 2      | unknown                                 |
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

**MIDI program.** Fixed-offset table after the TK blocks:
```
base = 2278    (header 194 + first block 120 + intra-data 1964)
step = 2158    (block 120 + data block 2038)
instrument n → file offset  base + n * step
```

**Key transposition.** At `base - 23 + n * step`: signed `int8` semitones matching the Encore Staff Sheet
"Key" dropdown (`0` = sounds as written, `-12` = octave lower, range ±33 semitones).
Encore shifts every note pitch by this value.

**Compact format (no TK blocks).** Single-instrument files store the MIDI program at fixed
offset 390 and the key transposition at `390 - 23 = 367`, using the same relative offset
as the TK-based format. Multi-instrument compact files use a different layout and their
key transpositions are not currently read.

**Percussion quirk.** Percussion tracks always report MIDI program 1 (GM Grand Piano);
infer the actual kit from the track name.

---

## System block (LINE)

21-byte header (start tick, measure count) + N × 30-byte staff entries (N = staves-per-system from header).
Each staff entry: clef, key, page index, staff type, instrument index, visibility (`0x00` = hidden at byte +19).

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
After the 3-byte header every element starts with: 1-byte size + 1-byte staffIdx (mask `0x3F`).

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

**Type 0xB.** Real files emit elements with high nibble 0xB; structure undecoded, payload ignored.

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
| 0x35    | TRILL_END     | end of trill+wavy-line span; no visible glyph                                    |
| 0x36    | TRILL_START   | trill-mark start                                                                 |
| 0x37    | TRILL_ALT     | second trill-span start                                                          |
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
| 0xEF    | TREMOLO_32B   | alternate triple tremolo (ORN at tick == durTicks); also maps to R32             |

**Undecoded subtypes.** `0xBE`, `0xC0`, `0xC4`–`0xC6`, `0xC8`, `0xEE`, plus rare values. Silently ignored.

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

### System-level ornaments (voice = 4)

System-wide ornaments use `voice = 4` AND set the high bit of the staff byte (`0x40`).
Mask `& 0x3F` to get the real staff index.

### Out-of-range voice on regular elements

Some files store NOTE/REST/BEAM with `voice = 4` WITHOUT the `0x40` staff-byte flag.
These are real content; the importer maps them to voice 0 of that staff so LYRIC attachment can find them.

---

## Lyric element

Type 6. Variable size. Null-terminated text, NOT fixed-width.

| Offset    | Size   | Description                                            |
|-----------|--------|--------------------------------------------------------|
| +0        | 2      | within-measure tick                                    |
| +2        | 1      | type/voice byte (high nibble = 6, low = voice)         |
| +3        | 1      | element size (24..36+)                                 |
| +4        | 1      | staffIdx & 0x3F                                        |
| +0x0A     | 1      | text anchor (x-offset equivalent)                      |
| +0x14..   | var    | text payload (UTF-16 LE or Latin-1, null-terminated)   |

Observed sizes: 24 (`-` dash), 26 (empty word-break), 30 (2 chars), 32 (3 chars), 34 (4 chars).

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

---

## Note element

### v0xC4 (size = 28)

| Offset   | Size   | Description                                                                      |
|----------|--------|----------------------------------------------------------------------------------|
| +5       | 1      | face value — low nibble: 1=whole, 2=half, 3=qtr, 4=8th, …, 8=128th               |
| +6       | 1      | grace1 (high-nibble flags, see grace section)                                    |
| +7       | 1      | grace2                                                                           |
| +10      | 2      | layout x-position                                                                |
| +12      | 1      | staff-relative pitch (legacy; not used for playback)                             |
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

### Articulation bytes

Each byte holds one or two glyphs:

| Value        | Glyphs                    |
|--------------|---------------------------|
| 0x04–0x07    | trill-mark                |
| 0x0A, 0x0C   | inverted-mordent          |
| 0x0B, 0x2F   | mordent                   |
| 0x12         | accent (`->`)             |
| 0x13         | marcato (`-^`)            |
| 0x14         | accent + tenuto           |
| 0x15         | marcato + staccato        |
| 0x16         | accent + staccatissimo    |
| 0x17         | accent + staccato         |
| 0x18         | up bow                    |
| 0x19         | down bow                  |
| 0x1C         | tenuto (`--`)             |
| 0x1D         | staccato (`-.`)           |
| 0x20–0x22    | fermata variants          |
| 0x24         | tenuto + staccato         |
| 0x25         | marcato + tenuto          |
| 0x28–0x2D    | staccatissimo combos      |

### Technical markings (reuse articulation slots)

| Byte         | Meaning          |
|--------------|------------------|
| 0x0D–0x11    | fingering 1–5    |
| 0x1E, 0x1F   | harmonic         |
| 0x44, 0x45   | thumb-position   |
| 0x46         | open-string      |

### Single-note tremolos (articulation slots)

| Byte   | Strokes   | Notes                                            |
|--------|----------:|--------------------------------------------------|
| 0x41   | 1         | 8th tremolo                                      |
| 0x42   | 2         | 16th tremolo                                     |
| 0x43   | 3         | 32nd; Encore may render 4 strokes in some files  |
| 0x03   | 3         | bare 3-stroke variant (no high-nibble flag)      |

`0x44` and above are technical markings — NOT tremolos.

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

**Tuplets.** Either explicit byte `(actualN << 4) | normalN` (3:2, 5:4, 6:4) or implicit
(playback duration ≈ faceTicks × 2/3 or 4/5).
The face value is authoritative for notation.

**Dotted notes.** Dot count at +14.
Can be inferred from `playbackTicks == faceTicks × 3/2` (one dot), `7/4` (two dots), with ±1-tick tolerance.

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
