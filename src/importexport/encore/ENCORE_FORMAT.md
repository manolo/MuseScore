# Encore (.enc) binary format

Binary format reference for Encore `.enc` files. This document
describes the on-disk layout and semantics of the format itself,
independent of any particular implementation.

The format was first documented by Felipe Castro (enc2ly) and Leon
Vinken (Enc2MusicXML, https://github.com/lvinken/Enc2MusicXML,
GPL v3+). This reference extends their work with details derived
from analyzing a corpus of more than 5000 `.enc` files, covering
the legacy v0xC2 (Encore 3.x/4.x) and v0xA6 (Encore 2.x) variants
in addition to v0xC4 (Encore 5.x).

## File structure

```
+-----------------------+
| header (194 bytes)    |  format version, line/page/measure counts
+-----------------------+
| TK00 ... TKnn blocks  |  one per instrument: name + MIDI program data
+-----------------------+
| PAGE block            |  page geometry
+-----------------------+
| LINE blocks           |  one per system: staves, clef/key per staff
+-----------------------+
| MEAS blocks           |  one per measure: notes, rests, ornaments...
+-----------------------+
| PREC block (optional) |  page-rendering cache
+-----------------------+
| TITL block            |  title / subtitle / author / copyright / etc.
+-----------------------+
| TEXT block            |  free-text annotations
+-----------------------+
```

Every block after the header starts with a 4-byte ASCII magic
(`TK00`, `PAGE`, `LINE`, `MEAS`, `TITL`, `TEXT`, `PREC`) followed by
a 4-byte varsize that gives the block's content length.

## Format versions

The byte at file offset 4 identifies the version:

| Byte | Version | Encore release           |
|------|---------|--------------------------|
| 0xA6 | v0xA6   | Encore 2.x (legacy)      |
| 0xC2 | v0xC2   | Encore 3.x / 4.x         |
| 0xC4 | v0xC4   | Encore 5.x               |

The file magic at offset 0 identifies the storage format and byte
order:

| Magic  | Storage    | Byte order    | Notes                  |
|--------|------------|---------------|------------------------|
| `SCOW` | plaintext  | little-endian | Encore 5.x default     |
| `SCO5` | plaintext  | big-endian    |                        |
| `SCOX` | plaintext  |               | rare variant           |
| `SCOR` | plaintext  |               | rare variant           |
| `SCOS` | plaintext  |               | rare variant           |
| `ZBOT` | encrypted  |               | Encore 4.x default     |
| `ZBOP` | encrypted  |               | encrypted variant      |
| `ZBO6` | encrypted  |               | encrypted variant      |

For plaintext magics, the byte order of multi-byte integers in
the rest of the file follows the magic.

### ZBOT encryption

`ZBOT` (and the related `ZBOP` / `ZBO6` variants) are encrypted
on disk. Only the first 42 bytes can be decrypted with a known
fixed XOR key; from byte 42 onwards the XOR stream is generated
algorithmically and has resisted reverse engineering so far
(non-trivial generator, no obvious cycle or modular pattern).
On top of that, the encrypted body and the plaintext `SCOW`
equivalent are not byte-for-byte the same score, so a full
ZBOT-to-SCOW conversion would also have to reproduce a
structural expansion. The only known practical path is to
re-save the file as `SCOW` from Encore 5.

## Header (194 bytes)

| Offset | Size | Description                                  |
|--------|------|----------------------------------------------|
| 0x00   | 4    | magic (`SCOW` or `SCO5`)                     |
| 0x04   | 1    | format version (see above)                   |
| 0x28   | 2    | (purpose unknown)                            |
| 0x2A   | 2    | (purpose unknown)                            |
| 0x2C   | 2    | (purpose unknown)                            |
| 0x2E   | 2    | number of system blocks                      |
| 0x30   | 2    | number of pages                              |
| 0x32   | 1    | number of instrument blocks expected         |
| 0x33   | 1    | number of staves per system                  |
| 0x34   | 2    | rendered measure count (see notes)           |

Bytes 0x36..0xC1 are padding and known-but-uninteresting.

**Rendered measure count vs MEAS blocks on disk.** The `0x34` field
holds the number of measures Encore actually displays. Real files
can carry additional MEAS blocks past that count -- "ghost" measures
left over from a prior edit or saved truncation. Encore never
renders or plays them, but they remain in the file. The importer
must stop appending after `header.measureCount` MEAS blocks even if
more are present (observed: rendered 36 / on-disk 56 in one Encore 5
file).

## Instrument block

`TKnn` (where `nn` is the instrument index) carries the instrument
name as either Latin-1 or UTF-16 LE. The encoding can be
identified by inspecting the first two bytes: if byte 0 is a
printable ASCII character and byte 1 is `0x00`, the name is
UTF-16 LE; otherwise Latin-1.

The per-instrument MIDI program (GM 1-128) lives in a fixed-offset
table that follows the instrument blocks:

```
base = 2278    (= header 194 + first block 120 + intra-data 1964)
step = 2158    (= block 120 + data block 2038)
```

So instrument `n`'s MIDI program byte is at file offset
`base + n * step`. Encore 5.0.2 occasionally omits the instrument
block header for an instrument but still writes the name at the
formula-derived offset.

**Per-instrument "Key" transposition.** 23 bytes BEFORE the MIDI
program (i.e. at file offset `base - 23 + n * step`) Encore stores
a SIGNED `int8` chromatic transposition in semitones. This value
mirrors the Encore Staff Sheet's Key dropdown: `0` = "Sounds as
Written", `-12` = "Octave Lower" (laud, classical guitar, bass
guitar), `+12` = "Octave Higher", and so on up to `±33` semitones
(the dropdown ranges from "2 Octaves Higher" to "Major 20th
Lower"). Encore plays each note shifted by this value relative to
the staff-position MIDI value stored in `EncNote::semiTonePitch`.

**Percussion MIDI program quirk.** Encore percussion tracks
always report MIDI program 1 (GM Grand Piano) regardless of the
actual kit. The actual instrument can only be inferred from the
track name field.

## System block

`LINE` describes one system. Contains a 21-byte header (start
tick, measure count) followed by N 30-byte staff entries (where N
is the number of staves per system from the file header). Each
staff entry carries the clef, key, page index, staff type,
instrument index, and a visibility flag at byte +19 (0x00 =
hidden).

## Measure block

`MEAS`. 54-byte header + variable-size element body, terminated
by a `0xFFFF` tick.

### Measure header (54 bytes after the magic + varsize)

| Offset | Size | Description                                                |
|--------|------|------------------------------------------------------------|
| 0x00   | 2    | BPM (quarter-note beats per minute, applies from this measure forward until the next BPM value differs) |
| 0x02   | 1    | time-signature glyph                                       |
| 0x04   | 2    | ticks per beat                                             |
| 0x06   | 2    | total ticks in the measure                                 |
| 0x08   | 1    | time signature numerator                                   |
| 0x09   | 1    | time signature denominator                                 |
| 0x0C   | 1    | start barline type (see ladder below)                      |
| 0x0D   | 1    | end barline type (same ladder)                             |
| 0x0F   | 1    | repeat-alternative bitmask (see notes)                     |
| 0x1A   | 4    | repeat-mark field. The LOW byte (`value & 0xFF`) carries the repeat type (see ladder below). The remaining three bytes encode position and styling. |
| 0x10..0x35 | 38 | layout/position: measure width, x-offsets, "Writer" UTF-16 tag |

#### Repeat alternative (volta) bitmask

The `0x0F` byte is a bitmask, NOT a number. Bit `n` set means the
measure belongs to ending `n + 1`. A value of `0x01` is "1st
ending", `0x02` is "2nd ending", `0x03` would be "1st + 2nd
ending" (rare). Encore tags **every measure** inside the ending
with the same bitmask, not just the first one (e.g. a 2-measure
1st ending stores `0x01` on both measures, then the next measure
that starts the 2nd ending stores `0x02`). The importer collapses
consecutive measures with the same bitmask into one MuseScore
`Volta` spanning them and sets the begin-text to "1.", "2.",
"1., 2.", ... so the number renders above the bracket.

#### Barline types

| Value | Meaning              |
|-------|----------------------|
| 0     | normal               |
| 2     | repeat start         |
| 3     | double (left)        |
| 4     | repeat end           |
| 5     | final                |
| 6     | double (right)       |
| 8     | dotted               |

#### Repeat-mark ladder

The full set of values that appear in the LOW byte of the
repeat-mark field:

| Byte | Meaning                                                  |
|------|----------------------------------------------------------|
| 0x80 | D.C. al Coda                                             |
| 0x81 | D.S. al Coda                                             |
| 0x82 | D.C. al Fine                                             |
| 0x83 | D.S. al Fine                                             |
| 0x84 | D.S.                                                     |
| 0x85 | "To Coda" source measure (text label, player jumps from) |
| 0x86 | Fine                                                     |
| 0x87 | D.C.                                                     |
| 0x88 | Segno marker                                             |
| 0x89 | Coda destination measure (glyph, player jumps to)        |

Encore pairs the two coda bytes:
- `0x85` (CODA1) is the source measure that displays "To Coda".
- `0x89` (CODA2) is the destination measure that displays the Coda
  glyph. The player jumps from the 0x85 measure to the 0x89 measure
  on the repeat.

Both bytes were historically labelled "Coda marker" in earlier
versions of this spec; mapping both to `MarkerType::CODA` collapsed
the pair and made the importer render two identical Coda glyphs.
The reader now imports `0x85` as `MarkerType::TOCODA` and `0x89` as
`MarkerType::CODA`.

The ornament-based `0xA5` ("To Coda" attached to a measure as an
ornament element) and the repeat-mark `0x85` byte are two parallel
encodings of the same musical idea -- some Encore versions use one,
some the other, some both. Both must produce a `TOCODA` marker.

#### BPM field semantics

The BPM at offset 0 is a quarter-note BPM (beats per minute
counted at the quarter-note unit) regardless of the time
signature. Every measure carries its own BPM value; the playback
tempo at the start of measure N is therefore the BPM stored in
that measure's header. Tempo changes are encoded by repeating
the new BPM in every measure from the change onwards.

In simple-eighth time signatures (3/8, 5/8, ...) Encore's user-
facing UI displays the BPM relative to the beat unit (eighth),
so a 3/8 score stored as quarter-BPM 100 is shown to the user
as eighth-BPM 200. The on-disk value remains quarter-BPM. There
is also an unrelated layout field at offset +0x18 that always
holds 200 in v0xC4 files (do not confuse it with BPM).

### Element body

Element header (3 bytes): 2-byte tick + 1 byte whose high nibble
is the element type and low nibble is the voice. A leading
`0xFFFF` tick terminates the element list.

Element types:

| Value | Type       |
|-------|------------|
| 0     | NONE       |
| 1     | CLEF       |
| 2     | KEYCHANGE  |
| 3     | TIE        |
| 4     | BEAM       |
| 5     | ORNAMENT   |
| 6     | LYRIC      |
| 7     | CHORD      |
| 8     | REST       |
| 9     | NOTE       |

After the 3 element-header bytes, every element starts with a
common 2 bytes: a 1-byte size (the full element span in bytes)
and a 1-byte staff index (masked with `0x3F`; the high bit is
overloaded for system-level ornaments, see below). The remaining
bytes are type-specific.

**Element type `0xB`.** Real Encore files emit a non-trivial
number of elements with the high nibble set to `0xB`. The
structure is currently undocumented and the payload is not
decoded.

### Multi-stream voices

Encore can store more than one MIDI tick stream inside the same
voice slot, typically the result of a live MIDI recording that
produced two or more interleaved time-stamped streams sharing
the same voice nibble in the element header. The streams cannot
be distinguished by the voice byte alone; the secondary streams
are detectable at the tick level: a tick that goes backwards
relative to the previous element of the same voice, or a
non-chord event whose accumulated written duration has already
filled the current voice, indicates a fresh stream.

### Implicit silences via tick offsets

Encore does NOT always emit explicit REST elements for leading or
interior silences. Silence between two events of the same voice
is often encoded only via the tick offset between them: a NOTE at
tick 240 in a 3/4 measure preceded by no REST element represents
a leading quarter rest at beat 1.

Concretely: when the sum of face-value durations of the elements
processed so far in a voice is less than the next element's
absolute tick, the difference is silence the user wrote in
Encore as a rest. A naive cumulative placement (where each new
element lands at the running sum of preceding face values) would
collapse this gap and shift every subsequent event earlier in
the measure, changing the song's timing.

The implicit-silence encoding applies independently to each
`(staffIdx, voice)` pair within a measure, both for the leading
position (first event at tick > 0) and between consecutive
non-chord-extension events.

## KEYCHANGE element

Type 2. Size 6 bytes. The byte at offset +5 from the element
start encodes the new key signature as an index into the fifths
table:

| Value | Key   | Fifths |
|-------|-------|-------:|
| 0     | C     | 0      |
| 1..7  | F..Cb | -1..-7 |
| 8..14 | G..C# | 1..7   |

Value 0 (no accidentals, modulation back to C major / A minor)
is a legitimate change and must emit a key signature so the
staff shows the naturals that cancel the previous accidentals.

## TIE element

Type 3. Size 16 or 18 bytes. Each tie endpoint is described by
two flag bytes.

The arc-direction byte at offset +5:

| Byte | Meaning                                                    |
|------|------------------------------------------------------------|
| 0xFE | tie-start, arc above (note sends tie forward)              |
| 0xFC | tie-start, arc below                                       |
| 0x02 | arc-only endpoint, top-arc-incoming (does NOT start a tie) |
| 0x04 | arc-only endpoint, bottom-arc-incoming                     |

A secondary tie-start flag lives at offset +6: its high bit
(`0x80`) is set whenever the note sends a tie forward, INCLUDING
the cases where the arc-direction byte at +5 happens to be an
arc-only value (`0x02` or `0x04`).

Both bytes must be inspected to identify outgoing ties. An
element is a tie-start if the high bit is set on EITHER byte +5
OR byte +6; elements where neither bit is set mark the receiving
side.

## Ornament element

Type 5. Variable-size element. Field offsets are from the
element start:

| Offset | Size | Description                                              |
|--------|------|----------------------------------------------------------|
| +5     | 1    | ornament subtype (see table below)                       |
| +10    | 2    | start x-position within the start measure                |
| +12    | 2    | signed s16 Cartesian y; negative => placed BELOW         |
| +18    | 1    | number of measures forward to the end measure (for spans)|
| +20    | 2    | end x-position within the end measure (for spans)        |
| +26    | 1    | crescendo/diminuendo flag (`& 0x3`, 0 = crescendo, otherwise diminuendo) |
| +28    | 2    | (purpose varies)                                         |
| +30    | 2    | BPM (for tempo subtype)                                  |
| +32    | 1    | TEXT-block entry index (for staff-text subtype)          |

For staff text (subtype `0x1E`) the signed-y field doubles as a
placement hint: a negative value (Encore's Cartesian "below"
convention) means the text is rendered BELOW the staff;
non-negative means ABOVE.

Ornament subtypes observed on real files:

| Value | Name        | Notes                                              |
|-------|-------------|----------------------------------------------------|
| 0x1D  | WEDGESTART  | hairpin start; endpoint encoded by the +18 measure-count field |
| 0x1E  | STAFFTEXT   | staff text payload via TEXT block + entry index at +32 |
| 0x21  | SLURSTART   | slur start; endpoint encoded by the +18 measure-count field |
| 0x22  | ARPEGGIO    | chord arpeggio; attaches to chord at same tick     |
| 0x32  | TEMPO       | tempo text; BPM in field at +30 (subtype reserved but UNUSED in real files; tempo words travel as staff text instead) |
| 0x35  | TRILL_END   | end of trill+wavy-line span; adds nothing visible  |
| 0x36  | TRILL_START | trill-mark start; chord ornament                   |
| 0x37  | TRILL_ALT   | trill-mark start (second span); chord ornament     |
| 0x41  | SLURSTOP    | reserved -- not emitted by Encore in practice      |
| 0x4D  | WEDGESTOP   | reserved -- not emitted by Encore in practice      |
| 0x80  | DYN_PPP     | size-16 dynamic mark `ppp`                         |
| 0x81  | DYN_PP      | size-16 dynamic mark `pp`                          |
| 0x82  | DYN_P       | size-16 dynamic mark `p`                           |
| 0x83  | DYN_MP      | size-16 dynamic mark `mp`                          |
| 0x84  | DYN_MF      | size-16 dynamic mark `mf`                          |
| 0x85  | DYN_F       | size-16 dynamic mark `f`                           |
| 0x86  | DYN_FF      | size-16 dynamic mark `ff`                          |
| 0x87  | DYN_FFF     | size-16 dynamic mark `fff`                         |
| 0x88  | DYN_SFZ     | size-16 dynamic mark `sfz`                         |
| 0x89  | DYN_SFFZ    | size-16 dynamic mark `sffz`                        |
| 0x8A  | DYN_FP      | size-16 dynamic mark `fp`                          |
| 0xA2  | SEGNO       | segno marker; attaches to the measure              |
| 0xA5  | TO_CODA     | "To Coda" marker; attaches to the measure          |
| 0xA6  | CODA        | coda marker; attaches to the measure               |
| 0xAA  | DYN_FZ      | size-16 dynamic mark `fz`                          |
| 0xAB  | DYN_SF      | size-16 dynamic mark `sf`                          |
| 0xC9  | STACCATO    | per-chord staccato dot                             |

Each size-16 ornament carries a small payload (subtype at +5,
one byte at +10 that varies between samples, and a signed 16-bit
field at +12 that looks like a Y-offset).

**Undecoded subtype values.** Real `.enc` files emit additional
size-16 ornament subtype bytes whose semantics are not yet
known: `0xAF`, `0xC0`, `0xC4`, `0xC5`, `0xC6`, `0xC8`, `0xEE`,
`0xEF`, plus a long tail of rare values (`0xB9`, `0xBA`, `0xBB`,
`0xBE`, ...).

**System-level ornaments (voice = 4).** System-wide ornaments
(dynamics, mordents, tremolos, technical markings, ...) are
written with `voice = 4` on the element header byte AND the
staff byte's high bit (`0x40`) set. The staff byte mask `& 0x3F`
recovers the actual staff index; the voice value sits outside
the normal 0..3 range and identifies the element as system-level
rather than as a real fifth voice.

**Out-of-range voice on regular elements.** A separate quirk: some
Encore files store NOTE, REST, BEAM and similar elements on a
specific staff with `voice = 4` (or higher) on the element header
byte WITHOUT the staff-byte high bit set. The matching LYRIC
elements on the same staff stay on `voice = 0`. The reason is
unclear (possibly an internal "secondary voice" marker the user
never sees), but the elements are real content of the staff and
should be read as voice-0 entries of that staff so the LYRIC
attachment, which anchors on voice-0 chord segments, can pick
them up. Treating values past the 0..3 range as invalid (the
naive read) drops the entire staff's content.

**Spanner endpoints (hairpins, slurs).** A hairpin start or slur
start fully describes its own span: the byte at +18 gives the
count of measures forward to the end measure, and the word at
+20 gives the horizontal position within that end measure.
Encore `.enc` files do not emit a separate hairpin-stop or
slur-stop element in the measure stream.

**Spanner anchored on the bar line.** Encore lets the user place
a hairpin's visible start exactly on the bar line; the binary
stores this with a tick equal to the measure's total ticks (for
example tick=480 in a 2/4 measure with 240 ticks per beat).
Such ornaments are still part of the measure they belong to.

## Lyric element

Type 6. One syllable per element, anchored to a chord on the
same staff/voice. The text length is variable: each element
grows as needed for the syllable, and the text payload is
null-terminated rather than fixed-width.

| Offset | Size | Description                                          |
|--------|------|------------------------------------------------------|
| +0     | 2    | within-measure tick                                  |
| +2     | 1    | element-type and voice byte (high nibble = 6, low nibble = voice) |
| +3     | 1    | element size in bytes (24..36+, depends on text length) |
| +4     | 1    | staff index (masked with 0x3F)                       |
| +0x0A  | 1    | text anchor byte (similar to x-offset)               |
| +0x14..| ...  | text payload (UTF-16 LE or Latin-1, see below), null-terminated, zero padded |

Observed sizes: 24 (`-` dash continuation), 26 (empty
placeholder), 30 (2 chars), 32 (3 chars), 34 (4 chars). The
text occupies bytes `[+0x14, size - 4]` followed by a small
zero-padded trailer.

**Per-element text encoding.** v0xC4 lyric text can be either
UTF-16 LE or Latin-1; the encoding is NOT uniform across the
file. Portuguese and Spanish scores written by older Encore
builds keep their lyrics in Latin-1 even inside an otherwise
modern v0xC4 file (e.g. "txã" stored as `74 78 E3 00 ...`). The
encoding can be identified per element with the same probe used
for instrument names: if byte 0 is printable ASCII (0x20..0x7E)
and byte 1 is `0x00`, the text is UTF-16 LE; otherwise Latin-1.

**Separator tokens.** Encore writes hyphenation and word-break
markers as their own lyric elements interleaved with the real
syllables. These markers do NOT consume a chord-rest slot; they
only carry hyphenation state for adjacent syllables:

| Element text  | Meaning                                                  |
| ------------- | -------------------------------------------------------- |
| `"-"`         | hyphen-continuation marker between two syllables of the same word |
| `""` (empty)  | word-break marker (resets the hyphen state)              |
| anything else | a real syllable                                          |

A syllable's hyphenation role is determined by the presence or
absence of a hyphen marker before and after it:

| hyphen before | hyphen after | syllabic role |
| ------------- | ------------ | ------------- |
| no            | no           | single        |
| no            | yes          | begin         |
| yes           | no           | end           |
| yes           | yes          | middle        |

**Multi-verse lyrics.** Encore encodes additional verses by
placing lyric elements on different VOICES of the same staff:
verse 1 lives on voice 0, verse 2 on voice 1, verse 3 on voice
2, and so on. All verses visually anchor on the same chord (the
voice-0 chord); the voice field is used only as a verse index,
not as an actual voice assignment for the lyric.

## Note element (v0xC4: size = 28)

Field offsets from the element start:

| Offset | Size | Description                                          |
|--------|------|------------------------------------------------------|
| +5     | 1    | face value (low nibble: 1=whole, 2=half, 3=quarter, 4=eighth, ..., 8=128th) |
| +6     | 1    | grace-flag low nibble                                |
| +7     | 1    | grace-flag (continued)                               |
| +10    | 2    | layout x-position                                    |
| +12    | 1    | staff-relative pitch (legacy)                        |
| +13    | 1    | tuplet byte (high nibble = actual N, low nibble = normal N) |
| +14    | 1    | dot count (0/1/2/3)                                  |
| +15    | 1    | MIDI pitch                                           |
| +16    | 2    | playback duration in ticks (real recorded duration)  |
| +19    | 1    | velocity                                             |
| +20    | 1    | options                                              |
| +21    | 1    | alteration glyph (accidental override)               |
| +24    | 1    | articulation byte (above slot)                       |
| +26    | 1    | articulation byte (below slot)                       |

v0xA6 notes are 10 bytes long. The MIDI pitch (absolute,
0..127) lives at element offset +11 -- the first byte of the
slot's padding region. Byte +9 in real Encore 2.x files holds a
related but distinct field (staff-position / diatonic line
count): for example a B4 chord on a treble-clef staff resolves
to byte +9 = 11 (= six diatonic steps above middle C in
treble-clef-position counting) while byte +11 = 71 (the
playable MIDI value).

The explicit tuplet byte (0x32 = 3:2, 0x54 = 5:4, etc.) lives
at byte +7 in v0xA6 NOTE slots -- the position v0xC4 uses for
`grace2`. v0xC4 stores the tuplet byte at +13 instead, which
lands in v0xA6's padding region and reads as 0; the importer
must override the field for size==10 NOTE elements so explicit
triplets / quintuplets are recognised.

### Articulation bytes

The articulation bytes at offset +24 (above slot) and +26
(below slot) each hold a glyph index. Encore packs more than
one glyph into a single byte: bytes in the 0x12..0x2D range
represent either a single articulation OR a combo of two (e.g.
0x24 = tenuto + staccato). The Above/Below pair is encoded by
which slot holds the byte; the index itself does not
distinguish direction.

| Value | Glyphs                                  |
|-------|-----------------------------------------|
| 0x04..0x07 | trill-mark (single-note ornament)  |
| 0x0A, 0x0C | inverted-mordent                   |
| 0x0B, 0x2F | mordent                            |
| 0x12  | accent (`->`)                           |
| 0x13  | marcato (`-^`)                          |
| 0x14  | accent + tenuto                         |
| 0x15  | marcato + staccato                      |
| 0x16  | accent + staccatissimo                  |
| 0x17  | accent + staccato                       |
| 0x18  | up bow                                  |
| 0x19  | down bow                                |
| 0x1A  | marcato (alt.)                          |
| 0x1C  | tenuto (`--`)                           |
| 0x1D  | staccato (`-.`)                         |
| 0x20  | fermata                                 |
| 0x21  | fermata (alt.)                          |
| 0x22  | fermata (short / square)                |
| 0x23  | accent + tenuto                         |
| 0x24  | tenuto + staccato                       |
| 0x25  | marcato + tenuto                        |
| 0x26  | marcato + staccatissimo                 |
| 0x27  | marcato + tenuto                        |
| 0x28  | staccatissimo                           |
| 0x29  | staccatissimo (alt.)                    |
| 0x2A  | staccatissimo + staccato                |
| 0x2B  | accent + staccatissimo                  |
| 0x2C  | staccatissimo                           |
| 0x2D  | tenuto + staccatissimo                  |

### Technical markings

Encore reuses the articulation byte slots for per-note technical
markings:

| Byte           | Meaning              |
| -------------- | -------------------- |
| 0x0D..0x11     | fingering 1..5       |
| 0x1E, 0x1F     | harmonic             |
| 0x44, 0x45     | thumb-position       |
| 0x46           | open-string          |

### Single-note tremolos

Encore also reuses the articulation byte slots to encode
single-note tremolos. The low nibble carries the stroke count
(number of slashes on the stem); the high nibble flags the byte
as "tremolo":

| Byte | Stroke count | Notes                                       |
| ---- | ------------:| ------------------------------------------- |
| 0x41 | 1            | eighth tremolo                              |
| 0x42 | 2            | sixteenth tremolo                           |
| 0x43 | 3            | thirty-second; Encore renders 4 strokes in some cases |
| 0x03 | 3            | bare 3-stroke variant (no high-nibble flag) |

`0x44` and higher belong to technical markings (fingering,
thumb-position, harmonic, open-string) and must not be treated
as tremolos.

## Rhythm encoding

Encore uses 240 ticks per quarter note.

```
face value  ticks (Encore)  duration
   1            960        whole
   2            480        half
   3            240        quarter
   4            120        eighth
   5             60        16th
   6             30        32nd
   7             15        64th
   8              7        128th
```

The notated duration is always determined by face value + dot
count + tuplet byte. The playback duration at note offset +16 is
the recorded MIDI duration, which generally diverges from the
notated duration (tuplets, ties, dotted notes, live recording
timing).

**Tuplets.** Notes inside a tuplet group either:

- carry an explicit tuplet byte (`(actualN << 4) | normalN`),
  used for 3:2, 5:4 and 6:4 ratios; or
- have no tuplet byte but a playback duration that scales the
  face value by 2/3 (triplet) or 4/5 (quintuplet). In this case
  the tuplet is implicit and must be inferred by comparing the
  recorded duration against `faceTicks * 2/3`, `faceTicks * 4/5`,
  etc. The face value is still authoritative for the notated
  duration; the playback duration is only a tuplet-membership
  hint.

**Partial tuplet ticks.** When fewer notes than `actualN` are
placed inside an open tuplet group (e.g. a writer-error 3:2
quarter triplet with only two of the three notes), the placed
duration is `N * baseLen * normalN / actualN`. For ratios whose
denominator is not a power of two (3:2, 5:4, 7:4, ...) this
fraction is not representable as a standard "duration class with
up to 4 dots".

**Dotted notes.** The dot-count byte gives the dot count
(1/2/3). For files where the dot byte is left at zero, the dot
count can still be inferred from `playbackTicks == faceTicks *
3/2` (and 7/4, 15/8 for two/three dots), with a ±1-tick snap
tolerance.

## BEAM element

Type 4. Encore stores explicit beaming decisions per beam level:

| Size | Byte at +5 | Meaning                                       |
|------|------------|-----------------------------------------------|
| 30   | 0x01       | level-1 beam (8th-note flag/beam)             |
| 46   | 0x02       | level-2 beam (16th-note beam extension)       |
| 62   | 0x03       | level-3 beam (32nd-note beam extension)       |

Each beam element covers a chord range.

## TEXT block (free-text annotations)

`TEXT` is the 4-byte magic introducing a variable-size block
whose 4-byte varsize follows immediately. It carries the text
payload of staff-text ornaments (subtype 0x1E).

Block layout (after the 8 bytes magic + varsize):

| Offset | Size | Description                                          |
|--------|------|------------------------------------------------------|
| +0     | 2    | sync (always `0x0000`)                               |
| +2     | 2    | entry count (matches the number of staff-text ornaments) |
| +4     | 4    | total bytes in all entries                           |
| +8...  | ...  | entries, each formatted as below                     |

Each entry:

| Offset       | Size | Description                                  |
|--------------|------|----------------------------------------------|
| +0           | 2    | payload size (size of the rest of this entry)|
| +2           | 14   | header (14 bytes of fields, not fully decoded)|
| +16..end-4   | ...  | text (UTF-16 LE characters)                  |
| end-3..end   | 4    | terminator (`0x04 0x00 0x00 0x00`)           |

A staff-text ornament (subtype 0x1E) carries no inline text; the
byte at element offset +32 indexes directly into this entry list.

Dynamic markings (`p`, `pp`, `ff`, `mf`, ...) are NOT in the TEXT
block; they are encoded as their own ornament subtypes in the
0x80..0x8A and 0xAA..0xAB ranges.

## TITL block

Variable-length text block containing title, two subtitles,
three instructions, four authors, two headers, two footers and
six copyright lines. Each line is 96 bytes (Latin-1) or 1056
bytes (UTF-16 LE). The encoding is determined from the block's
own varsize: a varsize >= 10000 unambiguously indicates UTF-16.
(The block's internal version field is unreliable for this
purpose.)

### Title line layout (UTF-16 variant, 1056 bytes)

| Offset      | Size  | Description                                         |
| ----------- | ----- | --------------------------------------------------- |
| +0..+29     | 30    | prefix (mostly zero; byte at +14 = horizontal alignment) |
| +30..+1055  | 1026  | text payload (UTF-16 LE, NUL-terminated, padded with 0x00) |

The alignment byte at prefix offset +14 only matters for the
four header/footer slots:

| Value  | Alignment |
| ------ | --------- |
| `0x02` | RIGHT     |
| `0x04` | LEFT      |
| `0x06` | CENTER    |

Other line kinds (title, subtitle, instruction, author,
copyright) leave this byte at `0x00` and the alignment metadata
is ignored.

### Multi-line content via slot stacking

Each TITL category (subtitle, instruction, author, copyright,
header, footer) reserves several consecutive slots:

| Category    | Slots | Use                                                     |
|-------------|------:|---------------------------------------------------------|
| title       |   1   | work title                                              |
| subtitle    |   2   | one or two subtitle lines                               |
| instruction |   3   | up to three "lyricist" / instruction lines              |
| author      |   4   | up to four composer lines                               |
| header      |   2   | one or two page-header lines                            |
| footer      |   2   | one or two page-footer lines                            |
| copyright   |   6   | up to six copyright lines                               |

When more than one slot in a category is non-empty, Encore
renders the slots as stacked lines at the same position. Each
slot stores one visible line; the text in every slot is
independently NUL-terminated within its own 1026-byte payload,
so any bytes after the NUL terminator are leftover from prior
edits and are NOT additional content.

For header and footer slots that share the same alignment byte
the stacking happens within the same page corner; slots with
different alignment bytes go to different page corners.

### Duplicate TITL blocks

Some Encore files write the TITL block twice, with identical
content in both copies (observed in `Mamae_eu_quero-Bateria.enc`
and similar Brasilsonoro library files). Both blocks parse the
same way; readers should treat them idempotently rather than
concatenating their slot content.

### Replaceable tokens in header / footer text

Header and footer lines can carry `#`-prefixed tokens that
Encore expands at render time. The text is stored verbatim in
the TITL line; only at print/display time does Encore replace
each token with the corresponding runtime value:

| Token | Meaning                |
|-------|------------------------|
| `#P`  | current page number    |
| `#D`  | date                   |
| `#T`  | time                   |

Empirical frequencies over a corpus of 5000+ `.enc` files:
`#P` dominates (hundreds of occurrences, often as `Página: #P`
or equivalent), `#D` appears in a few dozen files (typically
as `City, #D`), and `#T` is documented but was not observed in
the analysed corpus.

## Known quirks

- Encore 5.0.2 v0xC4 files can have fewer instrument blocks than
  the header's instrument count; the missing instruments still
  have their name written at the formula-derived offset.
- v0xC4 files written by Encore 5.0.2 always use UTF-16
  instrument names even when the offset within the block is
  <= 250.
- The TITL block's own version field is unreliable; the actual
  encoding (Latin-1 vs UTF-16) follows the block varsize.
- v0xA6 (Encore 2.x) is a different layout: notes are 10 bytes,
  the absolute MIDI pitch is at byte +11 (NOT +9, which holds a
  staff-position field), and the element offset within a measure
  is 0x1A instead of 0x36. v0xA6 occasionally stores two byte-
  identical REST elements back-to-back at the same tick / staff /
  voice / faceValue; Encore renders the pair as a single rest, so
  the importer dedupes consecutive duplicates. The file header
  also ends at 0xA6
  (right where TK00 begins) instead of 0xC2, and TK blocks are
  64 bytes wide (8-byte header + 56-byte content) instead of
  2158 bytes. The per-instrument Key transposition byte still
  exists -- as a signed int8 in semitones (-33..+24, same UI
  range as later formats) -- but its location moves to TK
  content offset +42 (i.e. file offset TK_start + 8 + 42),
  unrelated to the v0xC4 PRG_BASE + n * PRG_STEP formula.
- Encore percussion tracks always report MIDI program 1 (GM
  Grand Piano) regardless of the actual kit; the instrument is
  identifiable only from the track name.
- Italian tempo terms (Allegro, Andante, "a tempo", ...) travel
  as staff text (subtype 0x1E). The dedicated TEMPO subtype
  (0x32) is reserved but unused in real files.
- Lyric text in v0xC4 is not uniformly UTF-16: lyrics of older
  Portuguese / Spanish scores stay in Latin-1. Encoding is
  detectable per element via the same probe used for instrument
  names.
- The measure's repeat-mark field is a 4-byte word but the
  repeat type lives in the LOW byte only (`value & 0xFF`).
- "To Coda" is NOT in the repeat-mark ladder; it is encoded as a
  size-16 ornament with subtype `0xA5` attached to the source
  measure.
- A single Encore voice can carry two or more interleaved MIDI
  tick streams sharing the same voice nibble; they are
  distinguishable only at the tick level.
- Several glyphs the binary records are dropped by Encore's own
  MusicXML exporter (e.g. per-chord staccato dots in ornament
  subtype `0xC9`, trill-line ends in ornament subtype `0x35`).
  Native readers can recover them directly from the `.enc`
  binary.
- The largest legitimate block (an instrument block) is around
  2 KiB. A run of unrecognised bytes much longer than that
  suggests a corrupt file rather than a long block.
