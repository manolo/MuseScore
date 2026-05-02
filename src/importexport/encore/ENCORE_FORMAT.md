# Encore (.enc) binary format

This reference for the binary format used by Encore .enc files is
based on the initial work of Felipe Castro (enc2ly) and Leon Vinken
(Enc2MusicXML, https://github.com/lvinken/Enc2MusicXML, GPL v3+),
complemented by analyzing a corpus of more than 4000 .enc song
files.  That corpus analysis added roughly 40% of the coverage
documented here, including the legacy v0xC2 (Encore 3.x/4.x) and
v0xA6 (Encore 2.x) file variants, the per-instrument MIDI program
table at a fixed file offset, UTF-16 probing and formula-offset
name recovery in the TK block, the staff-visibility byte, the TIE
direction byte, implicit tuplet and dot inference from real MIDI
durations, and a chord-cluster heuristic for live-recorded files.

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
a little-endian 4-byte varsize that gives the block's content length.

## Format versions

The byte at file offset 4 (`chuMagio` in the header) identifies the
version:

| Byte | Version | MuseScore Encore release |
|------|---------|--------------------------|
| 0xA6 | v0xA6   | Encore 2.x (legacy)      |
| 0xC2 | v0xC2   | Encore 3.x / 4.x         |
| 0xC4 | v0xC4   | Encore 5.x               |

The file magic at offset 0 is either `SCOW` (little-endian) or `SCO5`
(big-endian); the importer flips its `QDataStream` byte order based
on which one it sees.

Files whose magic at offset 0 is `ZBOT` are **not** covered.  They
are saved by an older Encore release that uses a different on-disk
layout, and can be re-saved as `SCOW` by opening them in Encore 5
first.

## Header (194 bytes)

| Offset | Size | Field            | Notes                              |
|--------|------|------------------|------------------------------------|
| 0x00   | 4    | magic            | `SCOW` or `SCO5`                   |
| 0x04   | 1    | chuMagio         | format version (see above)         |
| 0x28   | 2    | chuVersio        |                                    |
| 0x2A   | 2    | nekon1           |                                    |
| 0x2C   | 2    | fiksa1           |                                    |
| 0x2E   | 2    | lineCount        | number of LINE blocks              |
| 0x30   | 2    | pageCount        |                                    |
| 0x32   | 1    | instrumentCount  | number of TK blocks expected       |
| 0x33   | 1    | staffPerSystem   |                                    |
| 0x34   | 2    | measureCount     | total MEAS blocks                  |

Bytes 0x36..0xC1 are padding and known-but-uninteresting.

## TK block (instrument)

`TKnn` (where `nn` is the instrument index) carries the instrument
name as either Latin-1 or UTF-16 LE. The encoding is detected by
probing the first two bytes: if byte 0 is a printable ASCII character
and byte 1 is `0x00`, the name is UTF-16 LE; otherwise Latin-1.

The per-instrument MIDI program (GM 1-128) lives in a fixed-offset
table that follows the TK blocks:

```
PRG_BASE = 2278    (= header 194 + TK00 block 120 + intra-data 1964)
PRG_STEP = 2158    (= TK block 120 + data block 2038)
```

So instrument `n`'s MIDI program byte is at file offset
`PRG_BASE + n * PRG_STEP`. Encore 5.0.2 occasionally omits the TK
block header for an instrument but still writes the name at the
formula-derived offset; the importer recovers the name by scanning
that position.

## LINE block

Describes one system. Contains a 21-byte header (start tick, measure
count) followed by `staffPerSystem` 30-byte `EncLineStaffData` entries
with the clef, key, page index, staff type, instrument index, and the
visibility flag at byte +19 (0x00 = hidden).

## MEAS block

54-byte header + variable-size element body, terminated by `0xFFFF`.

### MEAS header (54 bytes from after `MEAS` + varsize)

| Offset | Size | Field            |
|--------|------|------------------|
| 0x00   | 2    | bpm              |
| 0x02   | 1    | timeSigGlyph     |
| 0x04   | 2    | beatTicks        |
| 0x06   | 2    | durTicks         |
| 0x08   | 1    | timeSigNum       |
| 0x09   | 1    | timeSigDen       |
| 0x0C   | 1    | barTypeStart     | EncBarlineType (NORMAL=0, REPEATSTART=2, DOUBLEL=3, REPEATEND=4, FINAL=5, DOUBLER=6, DOTTED=8) |
| 0x0D   | 1    | barTypeEnd       | same enum as barTypeStart |
| 0x0F   | 1    | repeatAlternative|
| 0x1A   | 4    | coda             | low byte (offset +0x1A) carries the EncRepeatType: 0x80 DCALCODA, 0x81 DSALCODA, 0x82 DCALFINE, 0x86 FINE, 0x87 DC, etc. The remaining bytes encode position/styling. |
| 0x10..0x35 | 38 | layout/position | measure width, x-offsets, "Writer" UTF-16 tag |

### Element body

Element header (3 bytes): 2-byte tick (little-endian) + 1 byte where
the high nibble is `EncElemType` and the low nibble is the voice.
A leading `0xFFFF` tick terminates the element list.

`EncElemType` values:

| Value | Type       | Element struct  |
|-------|------------|-----------------|
| 0     | NONE       | -               |
| 1     | CLEF       | EncGenericElem  |
| 2     | KEYCHANGE  | EncKeyChange    |
| 3     | TIE        | EncTie          |
| 4     | BEAM       | EncGenericElem  |
| 5     | ORNAMENT   | EncOrnament     |
| 6     | LYRIC      | EncLyric        |
| 7     | CHORD      | EncChordSym     |
| 8     | REST       | EncRest         |
| 9     | NOTE       | EncNote         |

After the 3 element-header bytes, every struct starts with a common
2 bytes: 1-byte `size` (the full element span in bytes) and 1-byte
`staffIdx` (masked with `0x3F`). The remaining bytes are
type-specific; see `encoreelements.h` for the exact layout.

**Element type `0xB`.** Real Encore files emit a non-trivial number of
elements with the high nibble set to `0xB` (e.g. ~450 in the Beethoven
Sinfonia 7 II arrangement). The struct is currently undocumented; the
importer treats unknown high-nibble types as no-ops. The payload is not
yet decoded.

## KEYCHANGE element

`EncKeyChange` (type 2). Size 6. The byte at elemStart+5 encodes the new
key signature as an index into the fifths table:

| tipo | Key  | Fifths |
|------|------|-------:|
| 0    | C    | 0      |
| 1..7 | F..Cb | -1..-7 |
| 8..14| G..C# | 1..7   |

`tipo=0` (no accidentals, modulation back to C major / A minor) is a
legitimate change and must emit a key signature so the staff shows the
naturals that cancel the previous accidentals. The Beethoven Sinfonia 7
Plectro corpus uses tipo=0 in 24 of 40 KEYCHANGE elements.

## TIE element

`EncTie` (type 3). Size 16 or 18. The arc-direction byte at elemStart+5
encodes which side of the tie this element represents:

| Byte | Meaning                                                    |
|------|------------------------------------------------------------|
| 0xFE | tie-start, arc above (note sends tie forward)              |
| 0xFC | tie-start, arc below                                       |
| 0x02 | arc-only endpoint, top-arc-incoming (does NOT start a tie) |
| 0x04 | arc-only endpoint, bottom-arc-incoming                     |

Encore additionally writes a second tie-start flag at elemStart+6 whose
high bit (`0x80`) is set whenever the note sends a tie forward, including
the cases where the arc-direction byte at +5 happens to be an arc-only
value (`0x02` or `0x04`). About a third of outgoing ties in the
Beethoven Sinfonia 7 II Allegretto Plectro corpus use this encoding (50
of 157), and the LaMorenaDeMiCopla m20 dotted-quarter B exhibits it.

Importer rule: any element with the high bit set on EITHER byte +5 OR
byte +6 is treated as a tie-start. Elements where neither bit is set
mark the receiving side and are dropped because the importer matches the
receiving note by (staffIdx, voice, pitch) when it is placed.

Beethoven Plectro distribution by `(+5, +6)` pair:

| (+5, +6)     | Count | Role         |
|--------------|------:|--------------|
| (0xFC, 0x80) |    72 | tie-start    |
| (0xFC, 0x00) |     2 | tie-start    |
| (0xFE, 0x00) |    33 | tie-start    |
| (0x04, 0x80) |    50 | tie-start    |
| (0x04, 0x00) |     1 | arc-only end |
| (0x02, 0x00) |    36 | arc-only end |

## Ornament element

`EncOrnament` (type 5). Variable-size element starting at elemStart+5
(after the 3-byte element header and the 2-byte size + staffIdx common
prefix). Field offsets are from the element start:

| Offset | Field      | Notes                                              |
|--------|------------|----------------------------------------------------|
| +5     | tipo       | ornament subtype (see `EncOrnamentType` below)     |
| +10    | xoffset    | start x-position within the start measure          |
| +12    | yoffset    | signed s16 Cartesian y; negative => placed BELOW   |
| +18    | alMezuro   | count of measures forward to the end measure       |
| +20    | xoffset2   | end x-position within the end measure              |
| +26    | speguleco  | `& 0x3` -- 0 = crescendo, otherwise diminuendo     |
| +28    | noto       |                                                    |
| +30    | tempo      | BPM for `TEMPO` subtype                            |
| +32    | tind       |                                                    |

For STAFFTEXT (`0x1E`) the importer uses `yoffset` to pick MuseScore's
`PlacementV`: a negative value (Encore's Cartesian "below" convention,
e.g. the "ten" markers in Beethoven Sinfonia 7 II Allegretto Plectro
m3) maps to `PlacementV::BELOW`; non-negative keeps the default ABOVE.

`EncOrnamentType` values seen on real files:

| Value | Name        | Notes                                              |
|-------|-------------|----------------------------------------------------|
| 0x1D  | WEDGESTART  | hairpin start; endpoint encoded by alMezuro        |
| 0x1E  | STAFFTEXT   | staff text payload via TEXT block + `tind` index   |
| 0x21  | SLURSTART   | slur start; endpoint encoded by alMezuro           |
| 0x22  | ARPEGGIO    | chord arpeggio; attaches to chord at same tick     |
| 0x35  | TRILL_END   | end of trill+wavy-line span; adds nothing visible  |
| 0x36  | TRILL_START | trill-mark start; chord ornament                   |
| 0x37  | TRILL_ALT   | trill-mark start (second span); chord ornament     |
| 0xA2  | SEGNO       | segno marker; attaches to the measure              |
| 0xA5  | TO_CODA     | "To Coda" marker; attaches to the measure          |
| 0xA6  | CODA        | coda marker; attaches to the measure               |
| 0xC9  | STACCATO    | per-chord staccato dot (Encore's own MusicXML exporter drops it; the importer recovers the visible glyph) |
| 0x32  | TEMPO       | tempo text; BPM in `tempo` field                   |
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
| 0xAA  | DYN_FZ      | size-16 dynamic mark `fz`                          |
| 0xAB  | DYN_SF      | size-16 dynamic mark `sf`                          |

**Undecoded tipo values.** Real `.enc` files (Beethoven Sinfonia 7 II
Allegretto Plectro, May 2026 inventory) emit additional ornament tipo
bytes whose semantics are not yet known. All have element size 16 (vs
size 28 for the canonical ornaments and 86 for STAFFTEXT). Counts in
that single file:

| tipo | count | tipo | count |
|-----:|------:|-----:|------:|
| 0xAB |     1 | 0xC4 |    13 |
| 0xAF |   236 | 0xC5 |    28 |
| 0xB9 |     1 | 0xC6 |     9 |
| 0xBA |     1 | 0xC8 |     1 |
| 0xBB |     2 | 0xEE |    51 |
| 0xBE |     3 |      |       |
| 0xC0 |     2 | 0xEF |     6 |

Dynamic markings live in this cluster and the importer decodes the
contiguous 0x80..0x8A ladder: `ppp pp p mp mf f ff fff sfz sffz fp` in
order. The mapping was confirmed against `encore-symbols.enc` (one of
every dynamic, one tipo per byte) and cross-checked against Beethoven
Sinfonia 7 II Allegretto Plectro (subset usage of pp/p/f/ff only).
Coverage of the encore-symbols reference: 11 of 13 dynamics; the two
unmapped ones (`fz`, `sf`) sit at tipos this corpus never exhibited.
On Beethoven Plectro the recovered count rose from 165 to 208 of 221
(94 %).

Each size-16 ornament carries a small payload (tipo at +5, one byte at
+10 that varies between samples, and a signed 16-bit field at +12 that
looks like a Y-offset).

**System-level ornaments (voice = 4).** Encore writes its system-wide
ornaments (dynamics, mordents, tremolos, technical markings, ...) with
`voice = 4` on the element's typeVoice byte AND the staffByte high bit
(`0x40`) set. The staffByte mask already strips the high bit
(`staffIdx &= 0x3F`), but the voice value would otherwise be dropped
by the normal `voice >= VOICES` filter. The importer accepts voice=4
ORN elements as a special case and routes them to voice 0 of the same
staff. Without this exception every dynamic in `encore-symbols.enc`
was silently lost.

**Spanner endpoints (hairpins, slurs).** A WEDGESTART or SLURSTART fully
describes its own span: `alMezuro` is the count of measures forward to
the end measure and `xoffset2` is the horizontal position within that
measure. Encore `.enc` files do not emit a separate WEDGESTOP or
SLURSTOP element in the measure stream, so the importer must synthesize
the endpoint from `alMezuro` at WEDGESTART/SLURSTART time. Other
implementations achieve the same result by cloning each START as a STOP
into the destination measure during a post-pass (see
`Enc2MusicXML/src/encfile.cpp::addSpannerEnds`).

**WEDGESTART at measure end.** Encore lets the user place a hairpin's
visible start exactly on the bar line; the binary stores this as
`tick == durTicks` (e.g. tick=480 in a 2/4 measure with beatTicks=240).
Such ornaments must NOT be discarded by the "tick >= durTicks" filter
that drops out-of-measure notes; the importer keeps every ORNAMENT up
to and including `tick == durTicks` and only excludes ones strictly
beyond it.

## Lyric element

`EncLyric` (type 6). One syllable per element, anchored to a chord on
the same staff/voice. The text length is variable -- each element grows
as needed for the syllable -- so the importer parses UTF-16 LE code
units one by one until a null terminator (rather than reading a fixed
6-character window).

| Offset       | Field      | Notes                                       |
|--------------|------------|---------------------------------------------|
| +0x00..+0x01 | tick       | within-measure tick                         |
| +0x02        | typeVoice  | high nibble = 6 (LYRIC), low nibble = voice |
| +0x03        | size       | 24..36+ (depends on text length)            |
| +0x04        | staffIdx   | masked with 0x3F                            |
| +0x0A        | kie        | text anchor byte (similar to xoffset)       |
| +0x14..      | text       | UTF-16 LE, null-terminated, zero padded     |

Observed sizes (LaMorenaDeMiCopla.enc): 24 (`-` dash continuation), 26
(empty placeholder), 30 (`JU`, 2 chars), 32 (`LIO`, `PIN`, 3 chars), 34
(`RRES`, 4 chars). The text occupies bytes `[+0x14, size - 4]` followed
by a small zero-padded trailer.

**Separator tokens in the LYRIC stream.** Encore writes hyphenation and
word-break markers as their own LYRIC elements interleaved with the
real syllables:

| Element text | Meaning                                                  |
| ------------ | -------------------------------------------------------- |
| `"-"`        | hyphen-continuation marker between two syllables of the same word |
| `""` (empty) | word-break marker (resets the hyphen state)              |
| anything else | a real syllable                                         |

These markers DO NOT consume a chord-rest slot. Treating them as plain
syllables shifts every following syllable by one note position. The
importer therefore filters separator tokens out of the per-track queue
and uses them only to drive each surviving syllable's
`LyricsSyllabic`:

| hyphenBefore | hyphenAfter | LyricsSyllabic |
| ------------ | ----------- | -------------- |
| no           | no          | `SINGLE`       |
| no           | yes         | `BEGIN`        |
| yes          | no          | `END`          |
| yes          | yes         | `MIDDLE`       |

**Tick-anchored attachment.** Each remaining syllable carries the raw
Encore tick from the binary. At the end of the measure pass the
importer walks the measure's chord-rest segments and assigns each
chord the syllable whose tick is closest to the chord's measure-
relative tick, within a half-beat threshold. This anchors lyrics on
the chord Encore intended (`JU` on the quarter, `LIO` on the eighth,
`RO` on the second eighth in the LaMorenaDeMiCopla m18 case) instead
of consuming queue entries in order regardless of position.

**Multi-verse lyrics.** Encore encodes additional verses by placing
LYRIC elements on different VOICES of the same staff: verse 1 lives on
voice 0, verse 2 on voice 1, verse 3 on voice 2, and so on. All verses
visually anchor on the same chord (the voice-0 ChordRest). MuseScore
distinguishes verses with `Lyrics::verse()`. The importer therefore
maps each LYRIC element's `voice` field to the resulting `Lyrics::verse()`
value (0-indexed) and attaches every verse to the host voice-0 chord.

## Note element (v0xC4: size = 28)

Field offsets from the element start:

| Offset | Field            | Notes                                 |
|--------|------------------|---------------------------------------|
| +5     | faceValue        | low nibble: 1=whole..8=128th          |
| +6     | grace1           | low nibble of grace-flag pair         |
| +7     | grace2           |                                       |
| +10    | xoffset          | layout x-position                     |
| +12    | position         | staff-relative pitch (legacy)         |
| +13    | tuplet           | high nibble = actualN, low = normalN  |
| +14    | dotControl       | 0/1/2/3 dots                          |
| +15    | semiTonePitch    | MIDI pitch                            |
| +16    | playbackDurTicks | real (recorded) duration              |
| +19    | velocity         |                                       |
| +20    | options          |                                       |
| +21    | alterationGlyph  |                                       |
| +24    | articulationUp   |                                       |
| +26    | articulationDown |                                       |

v0xA6 notes are 10 bytes long and store the pitch at offset +9
(signed offset from C4=60).

### Articulation bytes

`articulationUp` (+24) and `articulationDown` (+26) each hold a glyph
index. Encore packs more than one glyph into a single byte: bytes in
the 0x12..0x2D range represent either a single articulation OR a combo
of two (e.g. 0x24 = tenuto + staccato). The Above/Below pair is encoded
by which slot holds the byte; the index itself does not distinguish
direction. Mapping derived from `encore-symbols.enc` m8-m13 where every
combo byte was cross-referenced against Encore's MusicXML export
note-by-note.

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

The importer maps these via `encArticulation2SymIds` (returns a list of
SymIds); unmapped values are silently dropped. SymIds in the ornament
family (`ornamentTrill`, `ornamentMordent`, `ornamentShortTrill`) are
wrapped in MuseScore's `Ornament` element (an `Articulation` subclass)
so the MusicXML export emits them under `<ornaments>` instead of
`<articulations>`. Fermatas (`fermataAbove/Below`,
`fermataShortAbove/Below`) are emitted as `Fermata` elements attached
to the ChordRest's `Segment` so the export produces `<fermata>` instead
of `<other-articulation smufl="..."/>`; the upright/inverted variant
follows the artic slot (articUp -> above/upright, articDown ->
below/inverted) via the Fermata's `PlacementV`.

### Technical markings

Encore reuses the artic byte slot for per-note technical markings:

| Byte           | Meaning              | MuseScore element |
| -------------- | -------------------- | ----------------- |
| 0x0D..0x11     | fingering 1..5       | `Fingering` text "1".."5" |
| 0x1E, 0x1F     | harmonic             | `Articulation` `SymId::stringsHarmonic` |
| 0x44, 0x45     | thumb-position       | `Articulation` `SymId::stringsThumbPosition` |
| 0x46           | open-string          | `Fingering` STRING_NUMBER text "0" -- the MusicXML exporter writes `<open-string/>` |

The fingering and open-string variants attach to the `Note`; the
remaining technicals attach to the `Chord` as articulations and are
rendered as ornaments under MusicXML's `<technical>` block.

### Single-note tremolos

Encore also reuses the artic byte slot to encode single-note tremolos.
The low nibble carries the stroke count (number of slashes on the
stem); the high nibble flags the byte as "tremolo":

| Byte | Stroke count | Notes                                       |
| ---- | ------------:| ------------------------------------------- |
| 0x41 | 1            | eighth tremolo                              |
| 0x42 | 2            | sixteenth tremolo                           |
| 0x43 | 3            | thirty-second; Encore renders 4 strokes in some cases (`encore-symbols.enc` m2 tick=480) |
| 0x03 | 3            | bare 3-stroke variant (no high-nibble flag) |

`0x44` and higher belong to technical markings (fingering,
thumb-position, harmonic, open-string) and must not be treated as
tremolos. The importer creates a `TremoloSingleChord` with
`TremoloType::R8/R16/R32` matching the stroke count.

## Rhythm encoding

Encore uses 240 ticks per quarter note (MuseScore uses 480; the
importer scales 2x).

```
faceValue  ticks (Encore)  duration
   1            960        whole
   2            480        half
   3            240        quarter
   4            120        eighth
   5             60        16th
   6             30        32nd
   7             15        64th
   8              7        128th
```

**Tuplets.** Notes inside a tuplet group either:

- carry an explicit `tuplet` byte (`(actualN << 4) | normalN`) — used
  for 3:2, 5:4 and 6:4 ratios; or
- have no tuplet byte but a `realDuration` that scales the face value
  by 2/3 (triplet) or 4/5 (quintuplet) — the importer detects this
  pattern by comparing `realDuration` against `faceValue * 2/3` etc.

**Partial tuplet ticks.** When fewer notes than `actualN` are placed
inside an open tuplet group (e.g. a writer-error 3:2 quarter triplet
with only two of the three notes), the placed duration is
`N * baseLen * normalN / actualN`. For ratios whose denominator is not
a power of two (3:2, 5:4, 7:4, ...) this fraction is not representable
as a `TDuration` (the canonical MuseScore "duration class with up to 4
dots" abstraction). Setting the tuplet's ticks to such a value would
later abort `Beam::calcBeamBreaks`, which constructs
`TDuration(tuplet->ticks(), /*truncate*/false)` with the strict-fit
assertion enabled. The importer detects this case with a
`TDuration(placedTicks, /*truncate*/true)` snap check and falls back
to the canonical `baseLen * normalN` instead.

**Dotted notes.** The `dotControl` byte gives the dot count (1/2/3).
The legacy heuristic uses `realDuration == faceValue * 3/2` (and
7/4, 15/8 for two/three dots) with a ±1-tick snap tolerance.

## BEAM element

`EncGenericElem` with high nibble 4 (type 4). Encore stores explicit
beaming decisions per beam level:

| Size | Byte at +5 | Meaning                                       |
|------|------------|-----------------------------------------------|
| 30   | 0x01       | level-1 beam (8th-note flag/beam)             |
| 46   | 0x02       | level-2 beam (16th-note beam extension)       |
| 62   | 0x03       | level-3 beam (32nd-note beam extension)       |

The Beethoven Sinfonia 7 Plectro arrangement has 1042 + 333 + 39 = 1414
BEAM elements. The importer currently relies on MuseScore's auto-beam,
which produces ~30% more beam segments than Encore's explicit decisions.
Honoring the explicit BEAM elements would require pairing each one with
the chord range it covers and setting BeamMode::BEGIN / MID / END on
those chords; left as future work.

## TEXT block (free-text annotations)

`TEXT` is the 4-byte magic introducing a variable-size block whose
4-byte little-endian `varSize` follows immediately. It carries the
text payload of STAFFTEXT 0x1E ornaments.

Block layout (after the 8 bytes magic + varSize already consumed by
the block dispatcher):

| Offset | Field        | Notes                                       |
|--------|--------------|---------------------------------------------|
| +0..+1 | sync         | 0x0000                                      |
| +2..+3 | count        | number of entries (matches STAFFTEXT count) |
| +4..+7 | contentSize  | total bytes in all entries (sum of +0..+1) for each entry below |
| +8...  | entries      | `count` entries, each formatted as below   |

Each entry:

| Offset      | Field      | Notes                                  |
|-------------|------------|----------------------------------------|
| +0..+1      | payloadSize| size of the rest of this entry         |
| +2..+15     | header     | 14 bytes of fields (not fully decoded) |
| +16..end-4  | text       | UTF-16 LE characters                   |
| end-3..end  | terminator | 0x04 0x00 0x00 0x00                    |

`EncOrnament` of subtype STAFFTEXT (0x1E) carries no inline text; its
`tind` byte (element offset +32) indexes directly into the TEXT block's
entry list. The Beethoven Sinfonia 7 Plectro corpus uses 247 entries
including `Allegretto`, `cresc.`, `dimin.`, `ten.`, `pizz.`, `dolce`,
`sempre piano`, and section markers (`II`, `III`, `IV`, `V`).

Dynamic markings (`p`, `pp`, `ff`, `mf`, ...) are NOT in the TEXT
block. They are likely encoded as a different ornament subtype not yet
decoded; the importer therefore does not yet emit `<dynamics>` markings.

## TITL block

Variable-length text block containing title, two subtitles, three
instructions, four authors, two headers, two footers and six
copyright lines. Each line is 96 bytes (Latin-1) or 1056 bytes
(UTF-16 LE). The encoding is determined from the block's own
varsize: a varsize >= 10000 unambiguously indicates UTF-16.

### TITL line layout (UTF-16 variant, 1056 bytes)

| Offset      | Field              | Notes                                         |
| ----------- | ------------------ | --------------------------------------------- |
| +0..+29     | 30-byte prefix     | mostly zero. byte at +14 = horizontal align   |
| +30..+1055  | text payload       | UTF-16 LE, NUL-terminated, padded with 0x00   |

The alignment byte at prefix offset +14 only matters for the four
header/footer slots. Encore writes:

| Value  | Alignment | Mapped MuseScore Sid                  |
| ------ | --------- | ------------------------------------- |
| `0x02` | RIGHT     | `oddHeaderR` / `evenHeaderR` (resp. footer) |
| `0x04` | LEFT      | `oddHeaderL` / `evenHeaderL`          |
| `0x06` | CENTER    | `oddHeaderC` / `evenHeaderC`          |

Other line kinds (title, subtitle, instruction, author, copyright)
leave this byte at `0x00` and the alignment metadata is ignored.

The importer maps each non-empty header/footer line into both the
odd and even Sid for the same corner, so the text shows on every
page regardless of page parity.

## Known quirks

- Encore 5.0.2 v0xC4 files can have fewer TK blocks than the header's
  `instrumentCount` (instruments without a TK still have their name
  at the formula offset). The importer pads the instrument vector to
  the declared count and recovers names.
- v0xC4 files written by Encore 5.0.2 always use UTF-16 instrument
  names even when the TK offset is <= 250; the importer probes for
  this.
- The TITL block's own `chuVersio` is unreliable — the importer
  detects UTF-16 from the block varsize instead.
- v0xA6 (Encore 2.x) is a different layout: MEAS elements are 10
  bytes, the pitch is at +9, and the element offset within MEAS is
  0x1A instead of 0x36.

## Where this is implemented

- `internal/encoreelements.{h,cpp}` — every Enc* binary struct and
  its `read()` method.
- `internal/encorerhythm.{h,cpp}` — face value / real duration /
  dot / tuplet conversion helpers.
- `internal/encoremapping.{h,cpp}` — clef/key conversions, DOM setup,
  instrument template matching.
- `internal/encoretuplets.{h,cpp}` — tuplet state machine.
- `internal/importencore.cpp` — top-level `buildScore` orchestration.
