# Encore (.enc) importer - implementation notes

Implementation notes for the MuseScore native importer of Encore
`.enc` files. The binary format itself is documented separately in
[ENCORE_FORMAT.md](ENCORE_FORMAT.md); this document only records
how the importer consumes that format, where the code lives, and
the decisions that map Encore concepts onto MuseScore engraving
elements.

## Code layout

Source tree: `src/importexport/encore/`.

| File                                          | Role |
|-----------------------------------------------|------|
| `internal/encoreelements.{h,cpp}`             | Every `Enc*` binary struct and its `read()` method. Owns `MeasureElemVec` via `std::unique_ptr`. |
| `internal/encorerhythm.{h,cpp}`               | Face-value to ticks conversion, `realDuration2DurationType`, `calcDots / calcDotsSnap`, `detectImpliedTuplet`, and `dottedAdvance` (shared cap path between chords and rests). |
| `internal/encoremapping.{h,cpp}`              | Clef/key conversions, DOM setup (`addTitleFrame`, `addInitialKeySig`, `addInitialTimeSig`, `addInitialClef`, `addRepeatMark`), instrument template matching (`normalizeEncoreInstrName`, `findEncoreInstrumentTemplate`), tempo-term lookup (`encTextToTempoBps`), articulation mapping (`encArticulation2SymIds`). |
| `internal/encoretuplets.{h,cpp}`              | `TupletTracker` and `computeImpliedTupletMembers`. The most subtle pieces and the root cause of every past corruption fix. |
| `internal/importencore.cpp`                   | Top-level `buildScore` orchestration. |
| `tests/tst_encore.cpp`                        | End-to-end integration tests against real and synthetic `.enc` files. |
| `tests/tst_encore_features.cpp`               | Per-feature unit tests with synthetic v0c4 fixtures. |
| `tests/tst_encore_rhythm.cpp`                 | Rhythm helpers (face value, dot, tuplet, dotted advance). |

## Block dispatch and resync

The top-level loop in `importencore.cpp` reads block magics and
dispatches per type. Unknown bytes between known magics are
skipped by `findNextKnownMagic`, which scans byte by byte until
the next recognised magic appears.

**Resync cap.** The largest legitimate Encore block (TKxx) is
around 2 KiB. `findNextKnownMagic` is capped at a 1 MiB resync
window; a longer junk gap indicates a corrupt file and the loop
stops instead of walking the entire payload. This was added in
`0c48ce9c27` after a real corpus file produced a 100+ MB scan.

## Instrument routing

`findEncoreInstrumentTemplate` (in `encoremapping.cpp`) combines
name and MIDI program into a single score over every non-drumset
template:

- diacritics-insensitive name compare so Spanish "Laud" matches
  "Laud".
- substring weights (trackName contains needle: +2) so
  "Guitarra B" reaches "Guitarra clasica".
- bonus when any channel of the template carries the .enc
  `midiProgram` (acoustic-bass ships slap, pop, pizzicato...
  channels; the pizzicato match flips "Bajo" away from the choral
  Bass voice).
- "common" genre tiebreaker so the everyday classical guitar wins
  over the soprano variant when both share GM program 24.

**Percussion shortcut.** Encore percussion tracks always report
`midiProgram = 1` (see ENCORE_FORMAT.md), so a strict MIDI-program
lookup routes percussion to a piano. Before the combined scoring,
`importencore.cpp` checks the lowercased track name for `percus`,
`drum` or `bater` (English, Spanish, Portuguese) and routes
matches directly to the locale-independent "drumset" template.

**Short-name guard.** Instrument names shorter than four characters
(typically SATB choir labels `S` / `A` / `T` / `B` and the Spanish
`C` for Contralto) short-circuit the matcher to `nullptr`. With a
1- to 3-character needle the substring scoring matches almost any
template that contains that letter (e.g. `S` lands on Bass
Clarinet, `C` on Piccolo, `T` on Contrabassoon, `B` on Oboe).
Files that carry such labels also tend to use compact TK blocks
which invalidate the `PRG_BASE + n * PRG_STEP` formula and so make
the MIDI program byte unreliable too, so the importer also skips
the drumset-keyword and the MIDI-program fallback for short
names. The chain falls through to the Grand Piano template and
the original Encore label is preserved as the part's long name;
the user picks the right choir voice from the instrument browser
afterwards.

## STAFFTEXT placement and tempo promotion

For STAFFTEXT ornaments (subtype `0x1E`):

- `yoffset` (element +12) drives MuseScore's `PlacementV`. A
  negative value (Encore's Cartesian "below" convention) maps to
  `PlacementV::BELOW`; non-negative keeps the default ABOVE.
- The text payload is looked up in the TEXT block by the `tind`
  byte at element offset +32.

**Italian tempo term promotion.** Anonymous `StaffText` would
leave tempo words ("Allegro", "Andante", ...) untracked in
MuseScore's tempo map, so layout spacing and playback speed would
be wrong. `encTextToTempoBps` in `encoremapping.cpp` recognises
the canonical Italian tempo set and promotes those strings to
`TempoText`:

| Term         | BPM | Notes                                  |
|--------------|----:|----------------------------------------|
| Grave        |  35 |                                        |
| Largo        |  50 |                                        |
| Lento        |  52 |                                        |
| Larghetto    |  63 |                                        |
| Adagio       |  71 |                                        |
| Andante      |  92 |                                        |
| Andantino    |  94 |                                        |
| Moderato     | 114 |                                        |
| Allegretto   | 116 |                                        |
| Allegro      | 144 |                                        |
| Vivace       | 172 |                                        |
| Presto       | 187 |                                        |
| Prestissimo  | 200 |                                        |

BPM values mirror MuseScore's tempo palette
(`palettecreator.cpp`).

Relative markings (`a tempo`, `Tempo I`, `Tempo 1`, `tempo
primo`) stay as `TempoText` (so MuseScore treats them as tempo
for layout) but carry no absolute BPS, falling back to the
previous tempo.

Non-tempo strings keep the plain `StaffText` path unchanged.

## Multi-stream voice routing

When more than one MIDI tick stream is encoded inside the same
Encore voice (see ENCORE_FORMAT.md), the importer splits the
overflow into separate MuseScore voices via a per-`(staffIdx,
encVoice)` `streamOffset` counter:

```cpp
auto encVoiceKey = std::make_pair(staffIdx, voice);
int msVoice = voice + streamOffset[encVoiceKey];
```

When a non-chord event arrives and the current MuseScore voice is
already filled to capacity, `streamOffset` increments and the
event is routed to the next MuseScore voice. The switch loops
until a voice with remaining space is found or all four voices
are exhausted (in which case the overflow event is dropped).

A single switch is not enough: the target voice may also be full
(e.g. a prior rest filled it), so the loop continues until either
the event fits or every voice is exhausted.

**Chord-extension guard.** Same MIDI tick within
`CHORD_MIDI_THRESHOLD` (=8) in the same MuseScore voice is
treated as a chord extension. This is only allowed when the
previous event at that voice came from the SAME Encore voice;
otherwise a spill from one encVoice could be mistakenly attached
to a chord belonging to a different encVoice. The importer
tracks `prevEncVoice[trackKey]` for this check.

Without the multi-stream split, the second stream silently
overwrites or merges with the first and the importer emits a
1/3072 tick gap that aborts layout downstream.

## Implicit-silence gap snap

Encore encodes leading and interior silences implicitly via the
element's absolute tick (see ENCORE_FORMAT.md). If the importer
placed every NOTE/REST at `measTick + cumTick[trackKey]` (the
running sum of face-value durations), those silences would
collapse and every subsequent event would shift earlier in the
measure -- changing the song's timing. A common pattern is a
3/4 bar carrying two NOTEs at Encore ticks 240 and 480 with no
preceding REST element; the user-intended music is "quarter
rest, quarter, quarter" but a pure `cumTick` placement would
emit "quarter, quarter, quarter rest".

At the start of `elemTick` computation, the importer compares
the element's absolute Encore tick (converted to a Fraction via
`Fraction(e->tick, wholeTicks)`) against the current
`cumTick[trackKey]`. When the difference is strictly greater
than `CHORD_MIDI_THRESHOLD` (= 8 Encore ticks), `cumTick` is
snapped forward to the Encore tick; `checkMeasure` later inserts
the necessary fill rests during the per-staff gap pass.

`wholeTicks` is the number of Encore ticks in a whole note. The
file header stores `beatTicks` (the duration of one beat in the
current time signature: 240 for x/4 meters, 120 for x/8 meters,
480 for x/2 meters, etc.), so
`wholeTicks = beatTicks * timeSigDen` (= 960 across the corpus).
An earlier version of this code used `4 * beatTicks` on the
implicit assumption that `beatTicks` always equals 240. That
held for x/4 meters but produced a denominator half the correct
value in x/8 meters: every gap-snap fire then pushed `cumTick`
twice as far as intended and the measure overflowed. v0xA6
scores in 3/8 / 6/8 (where `beatTicks = 120` always) tripped this
on every measure.

The 8-tick threshold matches the same constant used for chord-
extension detection, so the snap behaves consistently with the
"same-cluster" timing tolerance: drift inside a chord cluster
remains absorbed by `cumTick`, while anything beyond it is
treated as an intentional silence the user notated. The smallest
face value with non-degenerate ticks is the 64th (15 Encore
ticks); any real silence is strictly above the threshold.

The snap only applies to NOTE/REST elements in the non-chord-
extension branch. Chord extensions (same tick, same Encore
voice) reuse `lastChordPos[trackKey]` as before; ornaments,
ties, and other annotations follow their own per-element tick
anchoring.

## Per-instrument Key transposition

Encore's Staff Sheet exposes a per-instrument "Key" dropdown that
adds a chromatic transposition at playback time (see
ENCORE_FORMAT.md). The value is stored as a signed `int8` in
semitones, 23 bytes before the MIDI program byte in the same
fixed-offset table (`PRG_BASE - 23 + n * PRG_STEP`), and
`EncFile::read` populates `EncInstrument::keyTransposeSemitones`
right next to the MIDI-program read.

Compact-TK files (TK varsize <= 250, e.g. SATB choir scores saved
by Encore 5.0.2 with `offset = 112`) do NOT follow the
`PRG_BASE + n * PRG_STEP` layout: the formula reads garbage and
any non-zero byte would mis-shift every pitch on that staff. The
reader skips the Key lookup entirely for those files (the staff-
sheet "Key" feature is absent there anyway) and falls back to a
sanity bound (`-33..+24`, Encore's UI range) on regular-TK files
where the formula offset still happens to land on unrelated data.

v0xA6 files (Encore 2.x, e.g. files saved by Encore 2 before
Encore 3 / 4 / 5 added the longer TK block layout) store the same
Key field, but its location is different. v0xA6 uses 64-byte TK
blocks (8-byte header + 56-byte content); the Key byte sits at
TK content offset +42 (= file offset `TK_start + 8 + 42`). Two
adjustments are made for these files:

- `EncHeader::read` ends at file offset `0xA6` (174 bytes) for
  v0xA6, not `0xC2` (194). Skipping past `0xC2` would consume the
  first TK block (whose magic sits at `0xA6` in real v0xA6 files)
  and shift every per-instrument metadata field by one slot.
- The per-instrument loop in `EncFile::read` reads the v0xA6 Key
  byte directly from the in-flight TK block content, BEFORE
  delegating to `EncInstrument::read`. The byte is sanity-bound
  to `-33..+24` and stored on the same `EncInstrument::
  keyTransposeSemitones` field used for v0xC4. Downstream
  `applyConcertPitch` then shifts m_pitch per staff regardless of
  source format.

Encore puts the WRITTEN staff-position MIDI value into
`EncNote::semiTonePitch` and shifts the audible pitch by the Key
on playback; MuseScore plays at `Note::m_pitch` directly. The
importer therefore captures one `staffPitchOffset` per staff
(from `EncInstrument::keyTransposeSemitones`) while building the
parts and adds it to every NOTE pitch at the two
`applyConcertPitch` call sites (regular notes + grace notes):

```cpp
applyConcertPitch(note, en->semiTonePitch + staffPitchOffset[staffIdx]);
```

Visual alignment via the staff clef. With the pitch offset alone
the noteheads land at the SOUNDING staff position, which for
`Key = -12` staves is one octave lower than the user saw in
Encore. The importer recovers the original visual by also picking
the staff clef from the matched MuseScore instrument template
when its octave decoration matches the Key (see `pickStaffClef`
in `encoremapping.cpp`):

- Encore byte: plain G / F clef (no decoration option in Encore's UI)
- Template `concertClef`: `G8_VB` / `F8_VB` (octave bassa) when
  the instrument is octave-transposing (laud, classical guitar,
  electric bass, ...)
- Override fires only when the template concert clef's octave
  decoration matches `keyTransposeSemitones` (-12 for `G8_VB` /
  `F8_VB`, +12 for `G8_VA` / `F_8VA`, etc.) AND both clefs share
  the same glyph family (G / F)
- When the override fires AND the template has a distinct
  `transposingClef` with no octave decoration (bass-guitar,
  double-bass: `concertClef = F8_VB`, `transposingClef = F`,
  `transposeChromatic = -12`), the importer picks the
  `transposingClef`. The instrument's `transposeChromatic` then
  places the noteheads at the same staff position the concert
  clef would render them at, but the GLYPH stays identical to
  what Encore stored (plain F).
- When the template has a single clef (laud, classical guitar:
  `concertClef = transposingClef = G8_VB`), there is no choice;
  the concert clef wins.

Different-glyph mismatches, exact matches, and staves without a
matched template keep Encore's stored clef.

## Per-measure tempo (MEAS header BPM)

The 54-byte MEAS header carries a quarter-note BPM at offset 0
(see ENCORE_FORMAT.md). A post-measure pass walks the measure
list once after every measure has been built and emits a
`TempoText` at the start of:

- the first measure (initial tempo), and
- every measure whose BPM differs from the previous applied
  value (back-to-back identical measures get no extra mark).

For each emitted mark the importer also calls
`Score::setTempo(measTick, BeatsPerSecond(bpm / 60))` so the
score's tempo map drives playback.

The pass skips both the visible mark AND the tempo map update
when a `TempoText` already lives at the target ChordRest
segment. That covers:

- ORN TEMPO (subtype 0x32) - the element-specific tempo mark
  has already populated both the visible text and the tempo
  map at the same tick.
- STAFFTEXT promoted to TempoText by the Italian-tempo-term
  lookup ("Allegro", "Andante", ...) - the promoted mark
  already provides the right BPS for the term and a visible
  label.

For now the TempoText is always rendered as `♩ = <quarter-BPM>`,
matching the existing ORN TEMPO and Italian-term outputs.
Time-signature-aware display (e.g. `♪ = N` in 3/8 instead of
`♩ = N/2`) is a follow-up.

## TIE element handling

Both the arc-direction byte (+5) and the secondary tie-start flag
(+6) are inspected. Any element with the high bit set on EITHER
+5 OR +6 is treated as a tie-start. Elements where neither bit
is set mark the receiving side and are dropped from the tie
queue; the receiving note is matched by `(staffIdx, voice,
pitch)` when it is placed.

Observed distribution by `(+5, +6)` pair on the Beethoven
Sinfonia 7 II Allegretto Plectro corpus (`Beethoven_S7M2_Plectro.enc`):

| (+5, +6)     | Count | Role         |
|--------------|------:|--------------|
| (0xFC, 0x80) |    72 | tie-start    |
| (0xFC, 0x00) |     2 | tie-start    |
| (0xFE, 0x00) |    33 | tie-start    |
| (0x04, 0x80) |    50 | tie-start    |
| (0x04, 0x00) |     1 | arc-only end |
| (0x02, 0x00) |    36 | arc-only end |

About a third of outgoing ties (50 of 157) use the secondary +6
flag with arc-only +5. Ignoring the +6 byte loses those ties.

## Articulations, technical markings, tremolos

`encArticulation2SymIds` (in `encoremapping.cpp`) maps the byte
to a vector of `SymId`s (combo bytes return more than one);
unmapped values are silently dropped.

- SymIds in the ornament family (`ornamentTrill`,
  `ornamentMordent`, `ornamentShortTrill`) are wrapped in
  MuseScore's `Ornament` element (an `Articulation` subclass) so
  the MusicXML export emits them under `<ornaments>` instead of
  `<articulations>`.
- Fermatas (`fermataAbove/Below`, `fermataShortAbove/Below`) are
  emitted as `Fermata` elements attached to the ChordRest's
  `Segment` so the export produces `<fermata>` instead of
  `<other-articulation smufl="..."/>`. The upright/inverted
  variant follows the artic slot: `articUp` -> above/upright,
  `articDown` -> below/inverted (via `Fermata::placementV`).

**Technical markings (per-note artic byte):**

| Byte           | MuseScore element                               |
| -------------- | ----------------------------------------------- |
| 0x0D..0x11     | `Fingering` text "1".."5"                       |
| 0x1E, 0x1F     | `Articulation` `SymId::stringsHarmonic`         |
| 0x44, 0x45     | `Articulation` `SymId::stringsThumbPosition`    |
| 0x46           | `Fingering` STRING_NUMBER text "0" (the MusicXML exporter emits `<open-string/>`) |

Fingerings and open-string attach to the `Note`. The remaining
technicals attach to the `Chord` as articulations and render as
ornaments under MusicXML's `<technical>` block.

**Single-note tremolos.** A `TremoloSingleChord` element is
created with `TremoloType::R8/R16/R32` matching the stroke count
(0x41 -> R8, 0x42 -> R16, 0x43 / 0x03 -> R32).

**Per-chord staccato from ORN tipo 0xC9.** Encore stores chord-
level staccato as a separate size-16 ORN at the chord's tick.
Encore's own MusicXML exporter drops `0xC9` entirely (the
Beethoven reference XML shows only 1 `<staccato/>` while the
score visually displays staccato dots on hundreds of notes). The
importer attaches `SymId::articStaccatoAbove` and dedups against
the per-note artic byte `0x1D`. Recovered count on Beethoven
Plectro: 1 -> 1864 staccatos.

## Spanner endpoints

Encore `.enc` files do not emit a separate WEDGESTOP or SLURSTOP
element. The endpoint is synthesised from `alMezuro` (count of
measures forward) and `xoffset2` (horizontal position within the
end measure) at WEDGESTART/SLURSTART time, in a post-pass over
the measure list. Compare with `Enc2MusicXML/src/encfile.cpp::addSpannerEnds`,
which clones each START as a STOP into the destination measure.

**Zero-length hairpins.** A hairpin whose computed end falls on
the same tick as the start would assert during layout. The
importer drops degenerate hairpins cleanly instead.

**WEDGESTART at tick == durTicks.** Encore lets the user place a
hairpin's visible start exactly on the bar line. The importer
keeps every ORNAMENT up to and including `tick == durTicks` and
only excludes ones strictly beyond it. The chord/note filter
remains a strict `>= durTicks`.

## Out-of-range voice (voice >= VOICES)

A naive `voice >= VOICES` filter would drop every element whose
encoded voice nibble lies outside 0..3 (MuseScore's voice range).
Two distinct cases need the value mapped down to voice 0 of the
same staff instead of dropped:

- **System-level ornaments.** Dynamics, mordents, tremolos and
  technical markings are written with `voice = 4` plus the staff
  byte's `0x40` bit set. They are system-wide marks that anchor
  visually on voice 0.
- **Bass-staff regular elements.** A separate Encore quirk
  observed on at least one v0xC4 SATB choir score: the bass
  staff's NOTE / REST / BEAM elements carry `voice = 4` on the
  element header byte (no staff-byte high bit set), while the
  matching LYRIC elements on the same staff use `voice = 0`. The
  notes are real content; dropping them imports the bass staff
  empty.

The importer therefore maps EVERY out-of-range voice value down
to 0 for all element types and lets the multi-stream /
chord-extension machinery handle conflicts with existing voice-0
content on the same staff, the same way it does for normal
voices.

## Lyric attachment

**Per-element encoding probe.** Each LYRIC element is decoded
independently (UTF-16 LE vs Latin-1) using the same probe as
instrument names: byte 0 printable ASCII and byte 1 `0x00` =>
UTF-16; otherwise Latin-1. Reading a Latin-1 lyric as UTF-16
would pair adjacent bytes into garbage CJK code units (e.g.
"txã" -> `U+7874 U+00E3`).

**Separator filtering.** The hyphen (`-`) and word-break
(empty-string) LYRIC elements are filtered out of the per-track
queue and consumed only to drive each surviving syllable's
`LyricsSyllabic` (`SINGLE`, `BEGIN`, `END`, `MIDDLE`).

**Tick-anchored attachment.** Each remaining syllable carries
the raw Encore tick. At the end of the measure pass the importer
walks the measure's chord-rest segments and assigns each chord
the syllable whose tick is closest to the chord's measure-
relative tick, within a half-beat threshold. This anchors lyrics
on the chord Encore intended (e.g. `JU` on the quarter, `LIO` on
the eighth, `RO` on the second eighth in the LaMorenaDeMiCopla
m18 case) instead of consuming queue entries in order regardless
of position.

**Multi-verse.** Each LYRIC element's `voice` field maps to the
resulting `Lyrics::verse()` value (0-indexed). Every verse
attaches to the host voice-0 chord on the same tick.

## Rhythm: face value, dots, tuplets

The face value nibble is authoritative for the notated duration.
`playbackDurTicks` is NEVER used to upgrade a note's visible
duration; it is consulted only by `detectImpliedTuplet` to flag
the note as a tuplet member.

**Triplet `playbackDurTicks` does not override face value.** A
`playbackDurTicks = 80` (triplet 8th in 240 tpqn) on a notated
16th must stay a 16th. The earlier code in
`realDuration2DurationType` upgraded rdur=80 to `V_EIGHTH`
regardless of the face value nibble; for a notated 16th with
rdur=80 this misclassified the note as longer and pushed the
remainder of the measure into a spurious second voice. Verified
on `bandurriator/Tie a Yellow Ribbon Guitarra B.enc` m1 (single
voice with 14 events, previously 10 + 4 spurious voice-2).

**Inflated dotted rdur does not promote face value.** Real-world
case: a voice that carries a single chord with no following
events triggers `EncMeasure::calculateRealDurations` to inflate
`rdur` to the gap-to-measure-end. In a 3/4 bar with a quarter
chord at tick 0, the inflated rdur=720 lands exactly on the
"dotted half" mapping bucket in `realDuration2DurationType`
(720 = 480 * 3/2). Without a guard, the chord would render as a
dotted half instead of the quarter the binary actually encodes.
`realDuration2DurationType` therefore rejects the dotted mapping
when both:

1. `rdur > faceTicks` (inflated by `calculateRealDurations`, not
   truncated by a following event); AND
2. `calcDots(rdur, fv) == 0` (rdur is NOT a real dotted multiple
   of the face's tick count).

When either condition fails (truncated rdur, or a genuine dotted
note) the dotted mapping still applies. Exercised by
`synthetic_v0c4_inflated_rdur_quarter_chord.enc` (chord pitches
64+73, face=quarter, rdur after inflation=720, previously
rendered as a dotted half before the guard).

**Partial tuplet ticks.** For ratios whose denominator is not a
power of two (3:2, 5:4, 7:4, ...), the placed duration
`N * baseLen * normalN / actualN` is not representable as a
`TDuration`. Setting the tuplet's ticks to such a value would
later abort `Beam::calcBeamBreaks` (which constructs
`TDuration(tuplet->ticks(), /*truncate*/false)` with the strict-
fit assertion enabled). The importer detects this case with a
`TDuration(placedTicks, /*truncate*/true)` snap check and falls
back to the canonical `baseLen * normalN`, filling unused
positions with invisible rests.

**Chord/rest tick consistency.** When the remaining measure
space cannot fit any standard `TDuration`, the note is dropped
rather than created with a non-standard `TDuration(advance)`
that would yield chord ticks with garbage values (124/16, etc.).
When a second cap fires on a chord-extension, chord ticks are
always updated to match the `cumTick` advance regardless of
tuplet membership.

## Grace notes

A grace chord must be parented under its main `Chord`
(`Chord::add`), not under a `Segment`. Parenting under a Segment
crashes `pagePos()` during beam layout when
`toChord(explicitParent())` is dereferenced. The importer queues
pending grace chords until the next main chord is created in the
same trackKey, then attaches them with `Chord::add`.

## MEAS coda field

`EncMeasure::repeatMark()` returns the LOW byte of the 4-byte
`coda` field:

```cpp
EncRepeatType repeatMark() const {
    return static_cast<EncRepeatType>(coda & 0xFF);
}
```

The prior `(coda >> 8) & 0xFF` accessor silently dropped every
D.C./D.S./Fine on every Encore file. `addRepeatMark` in
`encoremapping.cpp` then routes each EncRepeatType to the right
`Jump` or `Marker`.

**CODA1 vs CODA2.** Encore distinguishes the source measure of
"To Coda" from the destination measure carrying the Coda glyph by
two different repeat-mark bytes: `0x85` (CODA1) is the source and
`0x89` (CODA2) is the destination. The importer maps `0x85` to
`MarkerType::TOCODA` and `0x89` to `MarkerType::CODA`. Mapping
both to CODA collapsed the pair and made MuseScore render two
Coda glyphs where Encore showed "To Coda" + Coda. The ornament-
based `0xA5` ("To Coda" attached as an ornament element) is the
parallel encoding for the same direction and also routes to
TOCODA via the pending-markers post-pass.

## Volta coalescing and numbered text

Encore stores the `repeatAlternative` bitmask on every measure
inside a volta (`0x01` on each measure of the 1st ending, `0x02`
on each measure of the 2nd ending, ...). A naive 1-Volta-per-
measure import produces N voltas of 1 measure each, none of which
shows a number above the bracket because MuseScore reads the
visible label from `Volta::beginText`, not from the endings list.

The importer keeps an `activeVolta` pointer across the measure
loop. When the current measure shares its bitmask with the
previous one it extends the active Volta's `tick2` to cover the
new measure. When the bitmask changes (or drops to 0) the active
Volta is closed and a new one is opened on the next non-zero
measure. The `beginText` is set from the endings list ("1.",
"2.", "1., 2.", ...) so the bracket label renders.

## Ghost MEAS blocks past header.measureCount

Encore 5 occasionally leaves trailing MEAS blocks in the file
from prior edits that the user truncated; the file header's
`measureCount` field at offset 0x34 is authoritative and reflects
what Encore actually displays. `EncFile::read` stops appending
once `measures.size() == header.measureCount` so the imported
score matches what the user saw in Encore. Without this cap an
Encore 5 file with rendered count 36 and 56 MEAS blocks on disk
produces a 56-measure MuseScore score with 20 measures of stale
content past the real end of the piece.

## Barlines per staff

`Measure::setEndBarLineType` takes a `track_idx_t`. Passing `false`
converts to `track = 0` and leaves DOUBLE / END barlines visible
on the first instrument only. The importer iterates over every
staff so multi-instrument scores get the barline on every system
line.

## Multi-slot text joining

Each TITL category that reserves multiple slots (subtitle 1-2,
instruction 1-3, author 1-4, copyright 1-6, header 1-2, footer
1-2) can carry up to that many stacked visible lines, with one
non-empty slot per line (see ENCORE_FORMAT.md). The importer
joins all non-empty slots of the same category with `\n` before
writing the result to:

| Category    | VBox text (`TextStyleType`) | Score Properties metaTag |
|-------------|-----------------------------|--------------------------|
| title       | `TITLE`                     | `workTitle`              |
| subtitle    | `SUBTITLE`                  | `subtitle`               |
| instruction | `LYRICIST`                  | `lyricist`               |
| author      | `COMPOSER`                  | `composer`               |
| copyright   | (not on VBox)               | `copyright`              |

For Encore's `Mamae_eu_quero-Bateria.enc`, the three non-empty
author slots become a single `composer` text:

```
Vicente Paiva e Jararáca
Adapt.: Sgt Solano
Banda de Música do CRPO/VRS
```

This matches what Encore's own MusicXML exporter writes as a
single `<creator type="composer">` with newline separators.

## TITL header/footer mapping

Each non-empty header/footer line is mapped into both the odd and
even `Sid` for the same corner so the text shows on every page
regardless of page parity:

| Alignment byte +14 | Sids (header)                       |
| ------------------ | ----------------------------------- |
| `0x02` (RIGHT)     | `oddHeaderR`, `evenHeaderR`         |
| `0x04` (LEFT)      | `oddHeaderL`, `evenHeaderL`         |
| `0x06` (CENTER)    | `oddHeaderC`, `evenHeaderC`         |

Footers use the analogous `oddFooterX` / `evenFooterX` Sids.

When multiple header (or footer) slots share the same alignment
byte their texts join with `\n` into a single Sid value, so the
two lines render stacked at that page corner. Slots with
different alignments stay on their own Sids.

### Duplicate TITL blocks

`EncTitle::read()` clears the slot vectors (`subtitle`,
`instruction`, `author`, `header`, `footer`, `copyright`) at the
start of every pass. Some Encore files (e.g. `Mamae_eu_quero-
Bateria.enc`) save the TITL block twice; the reset makes the
second block replace the first instead of appending its content,
which would otherwise double every line in the resulting score.

### Token translation (Encore `#X` -> MuseScore `$X`)

Encore embeds `#`-prefixed tokens in header and footer text
(see ENCORE_FORMAT.md). The text would otherwise reach the
`Sid` verbatim and MuseScore would print the literal characters
`#P` on every page instead of the page number. Before assigning
the text to the style slot, the importer rewrites each known
token to its MuseScore macro equivalent:

| Encore | MuseScore | Meaning                                          |
|--------|-----------|--------------------------------------------------|
| `#P`   | `$P`      | page number on every page (Encore shows it on page 1 too, so `$P` rather than `$p`) |
| `#D`   | `$D`      | creation date                                    |
| `#T`   | `$m`      | time (mapped to MuseScore's last-modification time, the closest available macro) |

Unknown `#X` tokens are left untouched so the original text is
preserved when a user typed something that happens to start with
`#`.

## TK block name recovery

For v0xC4 files: Encore 5.0.2 always uses UTF-16 instrument names
even when the TK offset is <= 250, so the importer probes for
UTF-16 unconditionally. When a TK header is missing entirely
(Encore 5.0.2 occasionally omits one) the name is recovered by
scanning the formula-derived offset `PRG_BASE + n * PRG_STEP - K`.

## BEAM elements

The importer currently relies on MuseScore's auto-beam, which
produces ~30% more beam segments than Encore's explicit
decisions. Honoring the explicit BEAM elements would require
pairing each one with the chord range it covers and setting
`BeamMode::BEGIN / MID / END` on those chords. Left as future
work; for the Beethoven Sinfonia 7 Plectro arrangement (1042 +
333 + 39 = 1414 BEAM elements) the visual difference is small.

## Dynamic ladder coverage

The contiguous 0x80..0x8A ladder is fully decoded: `ppp pp p mp
mf f ff fff sfz sffz fp`. The two outliers (0xAA -> fz, 0xAB ->
sf) cover the dynamics that live outside the contiguous range.

Mapping was confirmed against `encore-symbols.enc` (one of every
dynamic, one tipo per byte) and cross-checked against the
Beethoven Sinfonia 7 II Allegretto Plectro corpus (subset usage
of pp/p/f/ff only). Coverage of the encore-symbols reference:
13 of 13 dynamics. On Beethoven Plectro the recovered count
rose from 165 to 208 of 221 (94%).

## Reference fixtures

Test fixtures under `tests/data/`:

- `encore_symbols.enc` -- user-contributed 24-measure demo
  exhibiting one of every symbol family. Used as the canonical
  reference for tipo byte mappings.
- `synthetic_v0c4_*.enc` -- targeted single-feature fixtures
  generated programmatically. Each covers exactly one of the
  decisions in this document.
- Real-world corpus files (Beethoven Sinfonia 7, La Morena de
  mi Copla, bandurriator/Tie a Yellow Ribbon, etc.) referenced
  by the integration tests in `tst_encore.cpp`.

## Current corpus status

5124 `.enc` files in `c-download-enc/downloads`:

- 4963 OK
- 161 non-Encore (136 ZIP archives, 23 HTML pages, all expected)
- 0 CORRUPTED
- 0 CRASHED

Six importer fixes landed in `feature/enc-importer` brought the
CORRUPTED count from 32 to 0. All ported to `4.6.5-tmp`; the
rest-anchor analog has also been ported to Enc2MusicXML
(`d420e98`).
