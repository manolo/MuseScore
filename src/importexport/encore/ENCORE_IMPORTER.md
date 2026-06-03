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
| `internal/import/import.cpp`                   | Top-level `buildScore` orchestration. |
| `tests/tst_encore.cpp`                        | End-to-end integration tests against real and synthetic `.enc` files. |
| `tests/tst_encore_features.cpp`               | Per-feature unit tests with synthetic v0c4 fixtures. |
| `tests/tst_encore_rhythm.cpp`                 | Rhythm helpers (face value, dot, tuplet, dotted advance). |

## Block dispatch and resync

The top-level loop in `enc-import.cpp` reads block magics and
dispatches per type. Unknown bytes between known magics are
skipped by `findNextKnownMagic`, which scans byte by byte until
the next recognised magic appears.

**Resync cap.** The largest legitimate Encore block (TKxx) is
around 2 KiB. `findNextKnownMagic` is capped at a 1 MiB resync
window; a longer junk gap indicates a corrupt file and the loop
stops instead of walking the entire payload. This was added in
`0c48ce9c27` after a real corpus file produced a 100+ MB scan.

## Instrument routing

`findEncoreInstrumentTemplate` (in `enc-mapping.cpp`) combines
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

**Percussion detection — four-level chain.** Encore percussion tracks
always report `midiProgram = 1` (see ENCORE_FORMAT.md), so a strict
MIDI-program lookup would route them to Grand Piano. The importer uses
a prioritized chain instead:

1. **PERC clef (primary, language-agnostic).** If the first staff of
   the instrument carries `EncClefType::PERC` in the binary LINE block,
   the instrument is unconditionally routed to the `drumset` template.
   This check runs before any name or MIDI inspection and never produces
   false positives.

2. **Name + MIDI scoring over non-drumset templates** (`findEncoreInstrumentTemplate`).
   This is the main path for melodic instruments and is not restricted to
   non-percussion, so a correctly-named drum instrument still passes
   through here if the clef check was inconclusive.

3. **Name scoring over drumset templates** (`findDrumsetTemplate`). Uses
   the same diacritics-insensitive scoring as step 2 but restricted to
   templates with `useDrumset = true`. MuseScore's own localized template
   names ("Batería", "Batterie", "Drumset", …) drive the match, so no
   hardcoded keyword list is needed and any UI language is supported.

4. **MIDI program lookup** (`searchTemplateForMidiProgram`). Active for
   any instrument that has a non-zero `midiProgram` and has not been
   matched by earlier steps. This is the only available signal when the
   name is absent, so the step has no name-length gate.

**Short-name guard.** Instrument names shorter than four characters
(typically SATB choir labels `S` / `A` / `T` / `B` and the Spanish
`C` for Contralto) skip steps 2 and 3 only. With a 1- to 3-character
needle the substring scoring in those steps matches almost any template
that contains that letter (e.g. `S` lands on Bass Clarinet, `C` on
Piccolo). Step 4 (keyword) and step 5 (MIDI) still fire: when the name
is empty the MIDI program is the sole signal, and suppressing it would
force every un-named instrument to Grand Piano regardless of what
program Encore recorded. The chain falls through to Grand Piano only
when both name and MIDI give no result; the original label is preserved
and the user can reassign from the instrument browser.

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
be wrong. `encTextToTempoBps` in `enc-mapping.cpp` recognises
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

Visual alignment via the staff clef (`pickStaffClef` in `enc-mapping.cpp`).
The clef is derived directly from the **binary Encore clef + Key offset**,
without requiring a matched instrument template:

| Encore clef | Key (semitones) | MuseScore clef |
|-------------|----------------|----------------|
| G (treble)  | -12            | G8_VB          |
| G           | +12            | G8_VA          |
| G           | -24            | G15_MB         |
| G           | +24            | G15_MA         |
| F (bass)    | -12            | F8_VB          |
| F           | +12            | F_8VA          |
| F           | -24            | F15_MB         |
| F           | +24            | F_15MA         |
| any         | 0              | Encore clef    |
| any         | non-octave (e.g. -7) | Encore clef (notes shift, not clef) |

The rule: when `|keyOffsetSemitones|` is a multiple of 12 (one or two
octaves), look for a MuseScore clef in the same glyph family (G or F)
whose `clefOctaveOffset()` equals `keyOffsetSemitones`. If found, use it.
C clefs, percussion and tablature carry no octave variants and always
keep the Encore clef regardless of Key.

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

The TempoText display is time-signature-aware. For compound meters
(6/8, 9/8, 12/8 where the beat is a dotted quarter) the text is
`♩. = <dotted-quarter-BPM>` where `dotted-quarter-BPM = quarter-BPM * 2/3`.
For simple meters the text is `♩ = <quarter-BPM>`.

### ORN TEMPO subtype 0x32

The ORN `tempo` byte stores the beat-unit BPM displayed in Encore,
which is NOT always the quarter-note BPM:

- Simple meter: `tempo` byte = quarter-note BPM. No conversion needed.
- Compound meter (6/8, 9/8, 12/8): `tempo` byte = dotted-quarter BPM.
  The importer multiplies by 3/2 to obtain quarter-note BPM for the
  tempo map: `quarterBpm = tempo * 3/2`.

The conversion uses the MuseScore measure's nominal timesig (so a
pickup measure with actual 4/8 but nominal 6/8 correctly inherits
the 6/8 compound factor).

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

## v0xA6 grace groups (inner graces and snap suppression)

Encore v0xA6 files can contain grace-note groups with multiple notes:
a LEADING grace (grace1 bit-field & 0x30 == 0x20 = APPOGGIATURA) and
one or more INNER graces (bit-field & 0x30 == 0x10). Inner graces are
always shorter (higher faceValue number) than the leading grace.

Detection rule for inner graces: the note must satisfy

```
en->size == 10                             // v0xA6 note slot
(en->grace1 & 0x30) == 0x10               // inner-grace flag
pendingGraces[trackKey] non-empty          // a leading grace is queued
(en->faceValue & 0x0F) > leadingGraceFv   // shorter than the leader
```

where `leadingGraceFv` is the maximum faceValue seen in the current
grace queue (tracked in `v0xA6LeadingGraceFv` per `trackKey`, cleared
when the queue flushes). Notes with g1=0x10 whose faceValue is LOWER
(= longer duration) than the leader are regular notes following the
group (boda.enc distinguishes: m57 has a 64th inner grace after a 32nd
leader; m75 has regular 16ths after the same 32nd leader).

**Face-grid snap suppression.** The implicit-silence snap that advances
`cumTick` for notes whose Encore tick is on the face grid must be
suppressed when a grace note is pending. In v0xA6, grace notes occupy
real tick positions and push subsequent notes onto the grid, which
would otherwise produce a spurious gap rest (equal to the grace's face
duration) between the regular note and the grace. The condition
`!gracePending` is added to the snap guard.

**Crash implication.** The combination of spurious pre-grace rest,
inner grace as regular note, and the resulting irregular timing
produced a score structure that passed the CLI `-o` export path but
crashed the MuseScore GUI layout engine (SIGSEGV). The `sanityCheck()`
call in `v0xa6_inner_grace_group` test detects this before layout.

## v0xA6 grace note time-borrowing

Encore v0xA6 stores grace notes at their real tick positions. This
shifts subsequent notes forward in the measure timeline. The last real
note in a grace-containing group ends up with a raw gap to the measure
end that is SMALLER than its face value, because the grace notes
"borrowed" that time.

Example (boda.enc m75, 3/8 measure, beatTicks=120, durTicks=360):

```
tick=  0  8th  (regular)     faceValue=120
tick=120  32nd (grace)       faceValue=30  → steals 30 ticks
tick=150  16th (regular)     faceValue=60
tick=210  16th (regular)     faceValue=60
tick=270  8th  (regular)     rawGap=360-270=90  SHOULD be 120
```

`calculateRealDurations` detects this by summing the face values of
all grace notes in the same `(staffIdx, voice)` group that precede
the real note. If `∑ grace_face_values == face_value - rawGap`, the
grace notes collectively stole that time and the real note is restored
to its face value (`realDuration = face_value`). Without this the 8th
at tick=270 maps to a 16th and a rest fills the remaining 30 ticks.

The check only fires for v0xA6 notes (`size == 10`).

## Grace note ordering (multi-grace groups)

MuseScore's `Chord::add(gc)` inserts grace notes at
`m_graceNotes.begin() + gc->graceIndex()`. The default `graceIndex=0`
prepends each new grace, reversing the group order for multi-grace
sequences. The importer sets `gc->setGraceIndex(chord->graceNotes().size())`
before each add so grace chords are appended in tick order (first to
last = left to right in the score, matching Encore's visual order).

## ORN-based single-chord tremolos (tipo 0xAF / 0xEF)

Encore stores single-chord tremolos in two ways:

1. **Articulation-byte encoding** (existing): stroke count packed into the
   `articulationUp` / `articulationDown` bytes of the NOTE element
   (values `0x41` = 1 stroke, `0x42` = 2, `0x43` = 3).

2. **ORN elemento encoding** (added): a size-16 ORN element with
   tipo `0xAF` (standard triple tremolo for plectro string instruments)
   or `0xEF` (alternate encoding when Encore places the ORN at
   `tick == durTicks`, after the last note of a long passage). Both map
   to `TremoloSingleChord` / R32 (3 slashes = 32nd-note speed), the
   standard bandurria / plectro tremolo. Confirmed by 248 occurrences
   in a Beethoven plectro score where tremolo is ubiquitous.

Resolution is deferred via `PendingOrnTremolo` (tick, measTick,
staffIdx, msVoice, tremType). The post-pass:

1. Tries `score->tick2measure(pt.tick)` and `m->findSegment(ChordRest,
   pt.tick)` for the exact-tick case.
2. When the ORN was at `tick == durTicks` the tick falls in the NEXT
   (filler) measure which has only a whole rest. The fallback re-anchors
   to `pt.measTick` (the source measure) and takes the LAST chord-rest
   segment there.

Tipo `0xBE` appears rarely (3 times in Beethoven Plectro) on quarter
notes at measure starts, always with `byte+14 = 0xF4`. Its semantics
are not yet decoded; it is currently silently ignored.

## Articulations, technical markings, tremolos

`encArticulation2SymIds` (in `enc-mapping.cpp`) maps the byte
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

## Stand-alone FINGER and BOWING ORN routing in grand-staff scores

In v0xC4 grand-staff instruments (piano, organ, harp) all elements
share `staffIdx=0`; the 2nd staff's notes use `voice=4`. Stand-alone
FINGER ORNs (tipos 0xB9..0xBD) and BOWING ORNs (0xC4 up-bow, 0xC5
down-bow) use `voice=0` regardless of which staff they belong to.
The importer resolves the ambiguity in a deferred post-pass using two
heuristics computed from a per-measure pre-scan.

**Pre-scan state (computed once per measure before the element loop):**

| Symbol | Meaning |
|--------|---------|
| `voice4NoteTicks` | Set of raw Encore ticks where at least one `voice>=VOICES` note exists |
| `v0NoteCountAtTick[t]` | Count of `voice=0` notes at raw tick `t` |
| `ornFingCountAtTick[t]` | Count of FINGER ORNs at raw tick `t` |
| `maxVoice0Tick` | Largest raw tick carrying a `voice=0` note |

**Pattern A: cross-measure ORN (stored in wrong measure).**

Encore places the fingerings/bowings for the 2nd-staff chord of measure
N+1 at the end of the measure N binary block, at the same raw tick as
the last voice=0 note. Detection:

```
crossMeasure = !voice4NoteTicks.empty()       // grand-staff measure
            && !voice4NoteTicks.count(t)       // no 2nd-staff note at this tick
            && t == maxVoice0Tick              // ORN is at the last 1st-staff note tick
```

Resolution: route to the **first chord of the next measure** on the
sibling track (`track + VOICES`), with fallback to the original track.

**Pattern B: ORN cluster for a multi-note 2nd-staff chord.**

When a voice=4 chord appears at the same tick as a voice=0 note and the
count of FINGER ORNs at that tick exceeds the count of voice=0 notes,
the excess ORNs belong to the 2nd-staff chord. Detection:

```
preferSibling = !crossMeasure
             && voice4NoteTicks.count(t)                // 2nd-staff note at this tick
             && ornFingCountAtTick[t] > v0NoteCountAtTick[t]
```

Resolution: in the resolver, try the **sibling track** (`track + VOICES`)
first; fall back to the original track if no chord is found there.

**Non-grand-staff scores** have an empty `voice4NoteTicks`, so both flags
are `false` and the resolution is identical to the pre-fix behaviour
(exact-tick lookup on the original track with sibling fallback).

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
`enc-mapping.cpp` then routes each EncRepeatType to the right
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

## Text encoding probes (unified table)

Every text-bearing path in the format applies an encoding probe so
both modern (UTF-16 LE) and legacy (Latin-1) files decode
correctly without manual hints:

| Site                              | File / Function                           | Probe |
|-----------------------------------|-------------------------------------------|-------|
| TK block instrument name          | `EncInstrument::read`                     | byte 0 printable + byte 1 == 0x00 -> UTF-16; printable b0 + non-NUL printable b1 -> Latin-1 |
| TK block name recovery (NAME_BASE)| `EncFile::read` formula-offset fallback   | same as TK name; iterates per instrument |
| LYRIC element                     | `EncLyric::read`                          | byte 0/1 probe at payload start |
| TEXT block entry                  | `EncTextBlock::read`                      | byte 14/15 probe (header offset); reads until `0x04 0x00` terminator |
| CHORD-symbol text                 | `EncChordSym::read`                       | byte 0/1 probe across 36-byte slot |
| TITL block (title, subtitle, ...) | `EncTitle::read`                          | varsize gates: <5000 -> ONE_BYTE, >=10000 -> TWO_BYTES |

Every probe is bidirectional: detect UTF-16 LE when seen, fall
back to Latin-1 otherwise (or vice versa for the offset-derived
defaults). Forcing one encoding turns legacy Latin-1 payloads
into Chinese-looking gibberish (two Latin-1 bytes merged into one
BMP code unit) and silently drops the second half of every byte
on a modern UTF-16 file when the heuristic guesses the other
direction.

## TEXT block per-entry encoding probe

The TEXT block carries the payload of every STAFFTEXT ornament.
Modern Encore 5 files write text in UTF-16 LE, but legacy files
(notably Spanish/Portuguese scores) write it as single-byte
Latin-1. Forcing UTF-16 on a Latin-1 entry combines pairs of
single-byte chars into one BMP code unit and produces Chinese-
looking gibberish (sirena.enc m21 "la 1ª vez" becomes
"慬ㄠ₪敶⁺").

`EncTextBlock::read` probes bytes 14 and 15 of each entry: a
printable ASCII byte followed by `0x00` means UTF-16 LE; anything
else (e.g. accented Latin-1 bytes like `0xAA` for `ª`) means
Latin-1. The text byte range is bounded by the first `0x04 0x00`
terminator inside the entry payload, NOT by `payload_size - 14 - 4`;
some entries carry trailing padding after the terminator and the
old length formula clipped the last character of those entries.

## End-of-measure dynamics and staff text

Encore can place a dynamic or staff-text ornament at a tick that
exceeds the measure's `durTicks` (sirena.enc m21 is 2/4 with
durTicks=480 but stores the 1st-volta `pp` + "la 2ª" pair at
tick=960). These are repeat-aware section-end markers Encore
renders just before the bar line of the source measure. The
reader keeps DYN_* and STAFFTEXT ornaments whose tick is past
durTicks (the original `tick > durTicks` filter dropped them and
the user saw only one of two dynamics in MuseScore); the per-
case placement code then clamps `elemTick` to the last existing
ChordRest segment of the current measure so the marker ends up
inside the right bar.

## Dynamic deduplication

Encore occasionally stores the same dynamic twice on the same
`(staff, voice)` at the same tick with slightly differing xoffsets
(observed as duplicate MF ORNs, xoff 37 and 38, in real plectro
band scores where the user dragged a dynamic and left the original
in place). Encore renders only one. Before adding a Dynamic to a
segment, the importer checks whether a Dynamic of the same type
already exists on that `(segment, track)` and drops the duplicate.

## Dynamic staff displacement (yoffset > 0)

A dynamic ORN normally has `yoffset < 0` (below the staff, Encore's
Cartesian convention). When the user drags the glyph upward in Encore
onto the staff above, `yoffset` becomes positive while `staffByte`
still names the lower staff. The importer remaps the dynamic to
`staffIdx - 1` when `yoffset > 0` so it lands on the correct
instrument.

## Cross-measure hairpin snap-start and endpoint

**Snap-start when WEDGE is at the bar line.** A WEDGESTART at
`tick == durTicks` (= measure end / bar line) has no chord-rest
element at that tick. The `snapTickByXoffset` lambda used to return
the default tick (= start of the next measure, m+1.tick) in that
case, giving a zero-span hairpin after endpoint clamping. The fix:
the backwards scan also fires when no chord-rest is found at the
default tick, finding the latest note/rest in the source measure with
`xoffset <= ornament.xoffset` and anchoring the start there.

**Endpoint priority.** Two possible endpoint signals exist:

1. **Next-dynamic**: first Dynamic annotation on the same track after
   the start tick and within the `alMezuro` upper bound. This is the
   primary resolver and handles `mf<f>mf` chains where each hairpin
   terminates at the next visible glyph.

2. **`xoffset2` bar-line clamp** (fallback only). When `xoffset2`
   is smaller than the first NOTE's `xoffset` in the target measure
   and NO Dynamic was found via step 1, Encore drew the hairpin tip
   right before any note content (= at the bar line). The importer
   clamps the endpoint to the target measure's start tick in that
   case.

The clamp fires only when no Dynamic is found so a cross-measure dim
that ends at a `mf` dynamic is not incorrectly pinned to the bar line.
The clamp is also guarded against synthetic fixtures where notes have
`xoffset == 0` (generator default), which would always trigger it.

## Slur endpoint resolution (pixel-span heuristic)

## Slur endpoint resolution (pixel-span heuristic)

SLURSTART carries two layout-x fields: `xoffset` at the start and
`xoffset2` at the end. Each one is offset from the underlying
note's xoffset by a per-element drawing constant, so neither one
matches a note xoffset directly. Their DIFFERENCE, however, is
the pixel distance between the first and last covered notes:

```
slurXoffset2 - slurXoffset == endNote.xoffset - firstNote.xoffset
```

`PendingSlur` therefore captures `startTick`, `startMeasIdx`,
`alMezuro`, `slurXoffset`, `slurXoffset2`, plus `staffIdx` and
`encVoice`. The post-pass:

1. Finds the first NOTE in the start measure at the slur start
   tick on the same (staffIdx, encVoice) and reads its
   `xoffset`.
2. Computes `target = firstNote.xoffset + (slurXoffset2 -
   slurXoffset)`.
3. Walks the same measure's NOTEs on the same (staffIdx,
   encVoice) and picks the one whose xoffset is closest to
   `target`.
4. Anchors the slur's `tick2` on that note.

If `alMezuro > 0` (cross-measure span) the heuristic is skipped:
xoffsets reset at the bar line and a per-measure pixel
calibration would be needed to bridge them, so the importer falls
back to the last existing ChordRest on the same track in the
alMezuro target measure (the previous behaviour).

Without the heuristic the importer extended every slur to the
last note in the alMezuro target measure, which on real legacy
files (e.g. a 3-note slur in instrument 2 of a Spanish plectro
score) grew to cover every remaining note in the bar.

## Snap-back-by-xoffset for attached ornaments

Encore tags an attached ornament (dynamic or hairpin start) at the
CHORD-REST AT OR AFTER its visible position but records the
rendered layout x in the ornament's `xoffset` field. When the
ornament's xoffset is SMALLER than the tagged chord-rest's
xoffset, Encore visually pulls the glyph back to the previous
chord-rest. The importer used to plant the glyph on the tagged
tick, so users saw it one chord later than in Encore.

The fix lives in a `snapTickByXoffset` lambda shared by the
`DYN_*` and `WEDGESTART` cases:

1. If the default tick is the measure start, skip the snap so
   bar-line dynamics stay at the bar (their xoffset is layout
   padding, not a real position).
2. Locate the NOTE/REST at the default tick on the same
   `(staffIdx, voice)` and read its xoffset.
3. If `ornament.xoffset >= note.xoffset`, keep the default tick.
4. Otherwise walk the EncMeasure elements backward on the same
   `(staffIdx, voice)` and return the largest tick whose NOTE/REST
   has `xoffset <= ornament.xoffset`. Fall back to the default
   when nothing qualifies.

The same convention applies to STAFFTEXT but is not snapped
because text positions stayed accurate in the corpus; revisit if
a real file shows the same off-by-one chord drift.

## Hairpin direction and endpoint resolution

WEDGESTART direction is bit 0 of `speguleco`: 0 = crescendo,
1 = diminuendo. Encore 5 also sets bit 1 on the same byte
(crescendo reads as `0x02`, diminuendo as `0x03`); the legacy
`0x00` / `0x01` pair still appears on older files. Testing
`speguleco == 0` treats every Encore 5 hairpin as diminuendo and
flips every cresc/dim pair on disk. The importer uses
`(speguleco & 0x01) == 0` so both encodings agree.

WEDGESTART endpoints are not resolved at parse time. Encore
renders a `mf<f>mf` chain with each hairpin terminating exactly
at the next Dynamic glyph on the same track, even though
`alMezuro` nominally points at a whole measure. The importer
collects every WEDGESTART into a `PendingHairpin` (start tick,
upper bound = end of `alMezuro` target measure, track, direction)
and resolves the tick2 in a post-pass once every Dynamic has
been placed: walk forward from the start tick on the same track,
stop at the first Dynamic at or before the upper bound, and use
that tick as the hairpin's end. If no Dynamic is found inside
the window, fall back to the upper bound so a lone trailing
hairpin still spans its measure.

Without the post-pass two adjacent hairpins (e.g. `mf<f` and
`f>mf` on the same beat) overlapped visually because both
extended to the bar line of their measure.

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

## System layout fitting

After the note loop and before resolvers (which place system breaks), the
importer runs `fitSpatiumToLineBreaks` to reduce MuseScore's spatium (staff
space) until the score's auto-layout produces at least as many measures per
system as Encore's LINE blocks specify.

### Why spatium adjustment is needed

Encore stores exactly how many measures belong on each printed line in the
LINE block's `measureCount` field (1 byte, immediately after `start`). After
import the system breaks from `resolveAll` force the same boundaries. If
MuseScore's default spatium (1.750 mm at A4) is too large for the page width,
the layout engine auto-breaks a system before its forced break, moving one
measure to the next line and violating the Encore structure.

### Algorithm

```
1. Collect targets: enc.lines[0..3].measureCount (first 4 lines, skip zeros).
2. Loop up to 20 iterations:
   a. score->style().setSpatium(spatium); score->doLayout();
   b. Collect actual music-system measure counts in document order.
   c. For each j in [0, min(4, targets.size()) - 1]:
        if sysCounts[j] < targets[j]: allFit = false; break.
   d. If allFit: done.  Else: spatium *= 0.9.
3. Safety floor: stop if spatium < 0.01.
```

Only the first 4 lines are checked: beyond that the lines in real scores
are representative of the whole document and a density misfit in line 5+
would be caught by the line 1-4 checks (the piece is usually consistent).
The 10 % reduction per step converges in at most 20 steps (factor 0.9^20
≈ 0.12 of the original value, well below any real score's practical minimum).

The function runs BEFORE `resolveAll`, so no system breaks are in place
during the doLayout calls; the reduced spatium is stored and respects the
breaks placed later.

### What is NOT adjusted

Page margins: files that have never had Page Setup explicitly saved in Encore
contain no WINI block and keep MuseScore defaults (15 mm per side). When the
WINI block is present the margins are applied exactly as stored.

## Page margins

Page margins are stored in an optional WINI block near the end of the file.
The block is written only when the user explicitly opens and saves Page Setup
in Encore; files that were never touched through that dialog have no WINI block.

### WINI block layout

Magic: `WINI`. Size field: 42 bytes (21 x uint16 LE).

The four margin-related values are stored as int32 LE (pairs of adjacent uint16,
high word always zero) at byte offsets 24-39 within the block content:

| Offset | Field | Meaning |
|---|---|---|
| +24 | top | top margin in typographic points (1/72 in) |
| +28 | left | left margin in pts |
| +32 | bottomEdge | page_height_pts - bottom_margin_pts |
| +36 | rightEdge | page_width_pts - right_margin_pts |

Derived values applied to MuseScore style:

```
topMargin    = top / 72.0                        (inches)
leftMargin   = left / 72.0
printWidth   = (rightEdge - left) / 72.0
printHeight  = (bottomEdge - top) / 72.0
bottomMargin = pageHeight - topMargin - printHeight
```

`printWidth` is set via `Sid::pagePrintableWidth`; `bottomMargin` is computed
from the current score page height (defaults to A4 = 297 mm / INCH).

### Encoding quirks

Encore rounds when storing: `round(inches * 72)`. The display in Page Setup
shows `floor(pts / 72 * 1000) / 1000` so values may differ slightly from what
the user typed (e.g. 0.100 in stores as 7 pts and displays as 0.097 in).

### Files with no WINI block

Files that were never saved through Page Setup have no WINI block.
`EncPageSetup::hasData` will be false and `applyPageMargins` is a no-op;
MuseScore defaults (15 mm per side) remain.

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
