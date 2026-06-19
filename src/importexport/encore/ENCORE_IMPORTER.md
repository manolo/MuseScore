# Encore (.enc) importer - implementation notes

Implementation notes for the MuseScore native importer of Encore
`.enc` files. The binary format itself is documented separately in
[ENCORE_FORMAT.md](ENCORE_FORMAT.md); this document only records
how the importer consumes that format, where the code lives, and
the decisions that map Encore concepts onto MuseScore engraving
elements.

## Architecture

The importer is structured in two layers separated by a clean data boundary.
The **parser** reads the binary `.enc` bytes and produces a tree of plain C++ structs.
The **importer** walks that tree and emits MuseScore engraving DOM elements.
Neither layer knows about the other's internals.

### Source tree

```
src/importexport/encore/
├── enc-module.{h,cpp}            Entry point: registers the module with MuseScore
├── internal/
│   ├── notationencreader.{h,cpp} INotationReader adapter; calls importEncore()
│   │
│   ├── parser/                   LAYER 1 — Binary bytes → EncRoot tree
│   │   ├── elem*.h               Parsed data structs (EncRoot, EncNote, EncOrnament, …)
│   │   ├── parsers-*.cpp         Per-block/element parsers
│   │   ├── parsers-encoding.*    Text encoding probe (Latin-1 vs UTF-16 LE)
│   │   ├── readers.{h,cpp}       EncFormatReader base + dispatch; findNextKnownMagic
│   │   ├── readers-v0x*.{h,cpp}  Version-specific readers (v0xC4, v0xC2, v0xA6)
│   │   └── ticks.{h,cpp}         Tick arithmetic: faceValue↔ticks, dots, tuplets
│   │
│   └── importer/                 LAYER 2 — EncRoot tree → MuseScore DOM
│       ├── import.{h,cpp}        importEncore() top-level orchestration
│       ├── import-options.h      EncImportOptions struct (8 user-configurable flags)
│       ├── ctx.h                 BuildCtx: shared mutable state for all passes
│       ├── builders*.{h,cpp}     Score / part / measure setup
│       ├── emitters*.{h,cpp}     Per-type element emitters (notes, rests, ornaments, …)
│       ├── mappers*.{h,cpp}      Encore → MuseScore type conversions
│       └── resolvers*.{h,cpp}    Post-processing resolvers (slurs, hairpins, …)
│
└── tests/
    └── tst_*.cpp                 Per-feature tests (notes, tuplets, ornaments, …)
```

### Data flow

```
.enc file
    │
    ▼  readers-v0x*.cpp + parsers-*.cpp
EncRoot  (EncInstrument[], EncLine[], EncMeasure[], EncTitle)
    │
    ▼  import.cpp: importEncore()
    ├── buildParts()               → Score: parts, staves, instruments
    ├── buildMeasures()            → Score: empty Measure frames
    ├── buildInitialSignatures()   → Score: clef / key / time-sig on measure 0
    └── emitMeasures()             → per measure:
          ├── emitters-*.cpp:        notes, rests, ornaments, dynamics, …
          └── emitters-tuplets.cpp:  tuplet group tracking
    │
    ▼  resolveAll()
    resolvers-*.cpp fix deferred cross-element links (slurs, hairpins, ornaments)
    │
    ▼
MasterScore  (complete)
```

## Import options (Preferences → Import → Encore)

`EncImportOptions` (in `importer/import-options.h`) holds eight user-configurable flags.
`IEncImportConfiguration` / `EncImportConfiguration` (in `ienc-importconfiguration.h` /
`internal/enc-importconfiguration.h`) persist them via `muse::Settings` and expose
`async::Channel<T>` change signals. `NotationEncoreReader` reads the config on every
import and passes the filled struct into `importEncore()`.

| Field | Default | Effect |
|---|---|---|
| `importPageLayout` | true | Apply WINI page margins; false = use MuseScore defaults |
| `importPageBreaks` | true | Insert page breaks from LINE `pageIdx` increments |
| `importSystemLocks` | true | Insert system locks from LINE `showByte` bit 1 |
| `importStaffSize` | true | Apply LINE staff-size hint; false = use MuseScore default |
| `importTempoTextSemantic` | true | Promote Italian tempo terms to TempoText with BPM; false = plain StaffText |
| `importUnsupportedArticulationsAsText` | false | Unknown artic bytes emitted as StaffText; false = silently dropped |
| `instrumentSearchMode` | NameAndMidi | `NameAndMidi` = name+MIDI scoring; `MidiOnly` = skip name steps 2-4; `Piano` = always Grand Piano |
| `underfillMeasureStrategy` | InvisibleRests | How to fill trailing gaps: `InvisibleRests`, `VisibleRests`, `IrregularMeasure` |
| `overfillMeasureStrategy` | Truncate | How to handle overflow: `Truncate`, `IrregularMeasure` |
| `firstMeasureIsPickup` | true | Shorten first measure as pickup if underflowed; false = pad with leading rests |

`EncImportOptions` is stored in `BuildCtx` and consulted throughout `emitters-*.cpp`
and `resolvers-*.cpp`.

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

## v0xC2 compact instrument-table reading

v0xC2 files without TK blocks use a 112-byte-per-entry linear table for instrument
names and MIDI programs. Two sub-layouts exist (see ENCORE_FORMAT.md §No-TK-block files).
The reader (`readers-v0xc4-base.cpp`) auto-detects which variant applies.

**Detection logic (`readMidiProgramsNoTk`, `recoverMissingNames`):**

1. Call `findTildeBlockOffset(ds)`. If it returns a valid offset (≥ 0), Variant A applies;
   otherwise Variant B.

2. If Variant B (no `~~~~`): `noTkBlocks && tildeOff < 0`:
   - Names: `NAME_BASE + n * 112` (= 202, 314, 426, …)
   - MIDI: `262 + n * 112` (= 262, 374, 486, …)
   - No `hasPrimaryBlock` check; all instruments read directly.

3. If Variant A (has `~~~~`):
   - Names: first try `NAME_BASE + n * NAME_STEP` (step=2158) for TK-style instruments;
     then compact fallback at `314 + k * 112` for remaining unnamed instruments.
   - MIDI: compact at `374 + k * 112` for instruments without a primary block.
   - **Primary-block instruments:** `hasPrimaryBlock(n)` probes `202 + n*2158` for
     printable ASCII. If true, MIDI is read from `(202 + n*2158) + 60` instead
     (the "Voz " block style found in some Encore 4.x files).

4. **Fallback path (sub-layout a):** if `data[390] >= 1 && 390 < effectiveFirstBlock`,
   MIDI comes from `390 + n * 276` (compact v0xC4 layout, no `~~~~`).

**Template channel matching.** `findTemplateByMidi` only inspects the first channel of
each template (tremolo/secondary channels are skipped) to avoid misrouting instruments
whose secondary channels happen to match a wrong MIDI program (e.g. acoustic-bass channel
44 vs. the main channel 32).

**Trailing-punctuation stripping.** When building word-level needles for name matching,
trailing non-alphanumeric characters are stripped (e.g. "Bandurr." → "Bandurr") before
checking for a substring match. This lets abbreviated names ("Bandurr. I") reach the
correct template ("Bandurria").

**Template bracket clearing.** After `Staff::init(tmpl)` copies bracket data from the
template, the importer explicitly clears brackets/spans on every staff to avoid spurious
cross-part braces (e.g. accordion template carrying a brace that would span unrelated parts
in multi-instrument scores).

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

The TempoText display is time-signature-aware, driven by the MEAS header
`beatTicks` field (beat unit = beatTicks/240 of a quarter note):

| beatTicks | Beat unit | Display symbol | BPS formula |
|-----------|-----------|----------------|-------------|
| 240 | quarter | `♩ = N` | N / 60 |
| 360 | dotted quarter | `♩. = N` | N × 1.5 / 60 |
| 120 | eighth | `♪ = N` | N × 0.5 / 60 |

For compound meters (6/8, 9/8, 12/8 with `beatTicks=360` or legacy
`beatTicks=240`) the display is `♩. = N` and the BPS factor is 1.5 so
that N refers to dotted-quarter BPM. For pieces in non-compound time
with an eighth-note beat (e.g. 5/8, 7/8 with `beatTicks=120`), the
display is `♪ = N` and the BPS factor is 0.5 (eighth to quarter
conversion).

### ORN TEMPO subtype 0x32

The ORN `tempo` byte stores the beat-unit BPM displayed in Encore.
The beat unit is determined by `encMeas.beatTicks`:

- `beatTicks=240` (quarter): `tempo` = quarter-note BPM. BPS = tempo/60.
- `beatTicks=360` (dotted quarter, e.g. 6/8): `tempo` = dotted-quarter BPM.
  BPS = tempo × 1.5 / 60.
- `beatTicks=120` (eighth, e.g. 5/8 felt in eighths): `tempo` = eighth-note BPM.
  BPS = tempo × 0.5 / 60.  Encore displays this as "corchea = N" in Spanish.

**Conflict check**: the MEAS header `bpm` is always stored in quarter-note BPM.
When the ORN's `tempo` disagrees with `encMeas.bpm`, the ORN is normally
suppressed (Encore sometimes places tempo marks one system too early, and the
header BPM is authoritative).  This comparison is only valid when `beatTicks=240`
(same units).  For non-quarter beats (`beatTicks=120`, `360`, etc.) the values
are in different units and cannot be compared: the ORN is used regardless of
the header BPM.

The conversion uses the MuseScore measure's nominal timesig (so a
pickup measure with actual 4/8 but nominal 6/8 correctly inherits
the 6/8 compound factor).

Exercised by:
- `Tst_TempoXmlText.eighth_beat_uses_eighth_sym`
- `Tst_Text.orn_tempo_eighth_beat_not_suppressed_by_header_bpm`

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
3. **Tied-note correction.** After resolving to a chord (via either
   path), if that chord's first note has `tieBack() != null`, the
   tremolo belongs on the tie-START note: the post-pass walks back via
   `tieBack()->startNote()->chord()` and attaches the tremolo there.
   Encore places the tremolo ORN in the stream AFTER the tied-from note;
   the stream cursor (`cumTick`) therefore lands on the continuation
   chord's tick, which resolves via fallback to that continuation chord.
   Without the correction the tremolo would appear on the shorter tied-to
   note instead of the longer tied-from note.

Tipo `0xBE` appears rarely (3 times in Beethoven Plectro) on quarter
notes at measure starts, always with `byte+14 = 0xF4`. Its semantics
are not yet decoded; it is currently silently ignored.

## Hairpin staffIdx, track and xoffset2 snap on grand-staff instruments

Three interrelated issues arise when placing WEDGESTART hairpins on a grand-staff instrument
(e.g. piano) where treble and bass are separate MuseScore staves.

### staffIdx mismatch in xoffset2 snap

`resolveHairpinEndByXoffset` snaps a hairpin's end tick to the last note whose
`xoffset <= ph.hairpinXoffset2`. The filter `em->staffIdx != ph.staffIdx` compares the
note's **raw Encore staffIdx** (`rawStaff & 0x3F`) against `ph.staffIdx`.

For a single-instrument piano, all notes (both treble and bass) have raw staffIdx=0 because
the staffWithin bit (`rawStaff >> 6`) distinguishes the staves, not the lower 6 bits.
`ph.staffIdx` must therefore be the raw Encore value (`e->staffIdx` of the WEDGESTART ORN),
not the MuseScore-mapped slot from `lineSlotByRawByte` (which would be 1 for the bass, never
matching any note's em->staffIdx=0, effectively disabling xoffset2 snap for all bass hairpins).

### Track mismatch: ORN voice vs. note voice

WEDGESTART ORNs always carry Encore voice=0, but the notes they span may use a different
Encore voice. On a grand-staff instrument with `staffWithin=1`, the note voice is not
remapped down (voice < vBase=2 stays unchanged), so bass notes in Encore voice=1 end up in
MuseScore voice=1 (track=5 for bass staff 1), while the WEDGESTART's voice=0 produces
track=4 (voice=0 of bass staff). Voice=0 contains only a whole-measure rest; attaching both
hairpins there pins them both to beat 1 regardless of their intended start tick.

The fix: in `handleWedgeStart`, when `e->staffWithin > 0`, scan `encMeas.elements` for the
first note on the same sub-staff (same staffIdx + staffWithin) and use that note's Encore
voice to compute the MuseScore track. This ensures the hairpin is placed in the voice that
has actual notes, so its startTick anchors to a real note segment instead of a measure rest.

### Voice filter removed in xoffset2 snap

The xoffset2 snap also filtered by `ph.encVoice` against `em->voice`. Because the ORN and
notes can be in different Encore voices, this filter was removed: any note on the same raw
staffIdx with `xoffset <= xoffset2` is a valid snap candidate.

### Track assignment for WEDGESTART on grand-staff instruments

WEDGESTART ORN elements always carry Encore voice=0, but the actual notes they span may be in
a different Encore voice. On a grand-staff instrument with `staffWithin=1`, notes in Encore
voice=1 remap to MuseScore voice=1 (track=5 for bass staff 1), while the WEDGESTART's voice=0
maps to track=4 (voice=0, a measure-rest-only voice). The hairpin must be on the same track
as the notes so its startTick can anchor to a real note segment.

Fix: scan `encMeas.elements` for the first note on the same sub-staff (staffIdx + staffWithin)
and use its Encore voice after remapping to derive the correct MuseScore track.

### Start-tick computation for WEDGESTART on grand-staff instruments

`ec.elemTick` is cumTick-based and reflects the accumulated position for the WEDGESTART's
own (staffIdx, voice) trackKey. Because ORNs always use voice=0 and the bass notes may be
in voice=1+, cumTick for (staffIdx=1, voice=0) stays at 0 for the entire measure. Both
WEDGESTART elements in a same-measure swell pair would therefore get `elemTick = measTick`,
making the second hairpin also appear to start at beat 1.

Fix: for grand-staff instruments (`e->staffWithin > 0`), compute the start tick directly from
the raw Encore element tick: `rawElemTick = measTick + Fraction(e->tick, wholeTicks2)`.

### Same-measure swell pair: midpoint split

Two consecutive WEDGESTART elements in the same Encore measure (the < > swell pattern) should
each cover approximately half the measure.  The xoffset2 pixel coordinates do not map cleanly
to MuseScore ticks in measures that have empty beats (no notes in the second half), producing
short hairpins confined to the first 40-50% of the visual measure.

Fix (pre-pass in `resolveHairpins`): for each CRESC+DIM pair that both start in the same
MuseScore measure (`score->tick2measure()` returns the same `Measure*`), compute the measure
midpoint (`measure->tick() + measure->ticks() / 2`) and assign:
- CRESC end → midpoint
- DIM start → midpoint
- DIM end → barline (`measure->tick() + measure->ticks()`)

The xoffset2 snap and dynamic-clipping steps (1) and (2) are skipped for swell-pair
hairpins; the midpoint override takes precedence.  Cross-measure hairpins sharing the same
track are unaffected (different `tick2measure()` result → no override).

Exercised by `Tst_Importer.v0c4_swell_pair_splits_at_measure_midpoint`.
A dedicated 2-staff regression test covering the grand-staff track/staffIdx fixes is planned.

## Trill spans (TRILL_START / TRILL_END)

Encore encodes trill spans with three ORN subtypes:

| Subtype | Value | Role |
|---------|-------|------|
| `TRILL_START` | `0x36` | Start of trill span; `alMezuro` = measures forward to end |
| `TRILL_ALT`   | `0x37` | Secondary trill mark within the span (not a span start)    |
| `TRILL_END`   | `0x35` | End of span (no visible glyph); ignored by Encore's MusicXML exporter |

**Resolution (in `resolvers-ornaments.cpp`):**

For each `TRILL_START` the importer determines whether the span endpoint is
known:

1. **Same-measure span:** a `TRILL_END` (0x35) on the same track with a tick
   greater than the trill start → creates a `Trill` spanner from
   `startTick` to the `TRILL_END` tick.
2. **Cross-measure span:** `alMezuro > 0` → creates a `Trill` spanner to the
   `endTick()` of `ctx.measuresByIdx[measIdx + alMezuro]`.
3. **No span info** (`alMezuro == 0` and no matching `TRILL_END`) → falls back
   to an `Ornament` glyph (`ornamentTrill`) — the single-beat `tr` mark.

`TRILL_ALT` (0x37) always produces an `Ornament` glyph, never a spanner; it
marks a note within the span that Encore annotates with a redundant `tr`.

`TRILL_END` ticks are stored in `ctx.pendingTrillEnds` (keyed by track) and
cleared by the resolver.

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
  **Exception:** bytes 0x20 and 0x21 on a note with `tuplet != 0`
  encode "tuplet bracket placement above/below" (as exported by Encore
  as `<tuplet type="stop" placement="above/below"/>` in MusicXML), not
  a fermata. The importer skips fermata creation in that case.

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

## Multi-staff routing: staffWithin and out-of-range voice

Encore encodes which staff of a multi-staff instrument an element belongs to using
the high 2 bits of the element's staff byte (`staffWithin = staffByte >> 6`).
See ENCORE_FORMAT.md §Multi-staff instruments for the format details.

The importer handles this in two paths:

### Path A: staffWithin > 0 (high bits of staff byte)

Piano, harp, and similar grand-staff instruments use this encoding. All notes
share `systemStaffIdx = 0` in the element stream; `staffWithin` selects the
destination staff. The importer:

1. Reads `staffWithin = rawStaffByte >> 6` into `EncMeasureElem::staffWithin`.
2. In the note-loop element dispatch, when `staffWithin > 0`:
   - `staffIdx += staffWithin` — route to the correct staff.
   - `voice -= staffWithin * (VOICES / 2)` — remap voice to 0-based within that staff.
3. The TIE-start pre-pass applies the same routing so tie keys are consistent.

For a 2-staff piano: voices 0/1 stay on staff 0, voices 2/3 (staffWithin=1) route
to staff 1 as voices 0/1.

### Path B: voice >= VOICES (voice nibble out of range)

Two distinct cases require mapping voice >= 4 down to voice 0:

- **System-level ornaments.** Dynamics and technical marks are written with
  `voice = 4` plus `staffWithin = 1` (0x40 bit set). They anchor on voice 0
  of the target staff.
- **Bass-staff SATB elements.** Some v0xC4 choir scores carry bass-staff
  NOTE/REST/BEAM with `voice = 4` and no valid staffWithin. Notes are real
  content; dropping them leaves the bass staff empty.

Path B fires first (before the staffWithin check), ensuring system ornaments
are not accidentally routed to a second instrument staff.

## Chord symbol (harmony) import

Encore stores chord symbols as CHORD elements (type 7) in the measure element stream.
The importer (`handleChordSym` in `emitters.cpp`) creates a `Harmony` element and calls
`setHarmony()` for each one.

### Two encoding modes

**Text mode** (`tipo & 0x01 == 1`): the chord name is stored verbatim in `teksto`
(36-byte UTF-16 LE or Latin-1 slot, same encoding probe as lyrics). The text is passed
directly to `setHarmony()`. Example: `teksto="Am"` → `Am`.

**Numeric mode** (`tipo & 0x01 == 0`): the chord is encoded as three bytes:

| Field   | Meaning |
|---------|---------|
| `radiko`| Root note: lower nibble = note name (0=C..6=B), upper nibble = accidental (0=natural, 0x10=sharp, 0x20=flat) |
| `toniko`| Chord quality index 0-62 into the `kChordQuality[]` table (`elements.cpp`). Index 0 = major (no suffix), 1 = "m", 4 = "7", 12 = "maj7", etc. See `ENCORE_FORMAT.md §CHORD element`. |
| `baso`  | Slash bass note, same encoding as `radiko`. Active only when `tipo & 0x02`. |

`EncChordSym::chordName()` constructs the string `root + quality [+ "/" + bass]` and
passes it to `setHarmony()`. Examples: `radiko=0x26 + toniko=0` → `Bb`;
`radiko=0x03 + toniko=24` → `F7`; `radiko=0x00 + toniko=1 + baso=0x04, tipo=2` → `Cm/G`.

### Fallback for undefined quality indices

Several `toniko` indices (16, 20, 23, 28-31, 39) are undefined in the Encore format.
`kChordQuality[i] == ""` for those entries, so the chord degrades to just the root note
(treated as major). This is a safe fallback for files that use undocumented chord types.

### MuseScore parser normalization

After `setHarmony()`, `harmonyName()` returns the MuseScore-canonical form, which may
differ from the raw input string (e.g. `"Cmaj7"` → `"CMaj7"`, `"F7"` → `"F7"`).
Test assertions on `harmonyName()` must use the normalized form.

### Tests

- `tst_parser_chord.cpp`: unit tests for `EncChordSym::chordName()` in isolation.
  Covers all natural roots, sharps, flats, major/minor/dom7/aug/dim/sus4/slash, and edge cases (invalid radiko, out-of-range toniko, text mode passthrough).
- `tst_importer.cpp` → `numeric_chord_symbols`: integration test loading `chord_parsing.enc`
  (64 measures, one numeric chord each, all toniko 0-63). Verifies C, Cm, C+, C7, Cdim, CMaj7.
- `tst_importer.cpp` → `numeric_chord_with_bass_note`: integration test loading `akordo.enc`,
  verifies slash chord `Ab13sus4/F#` (tipo=2, bass note present).

## Duplicate NOTE elements in chord clusters

Some Encore files encode the same note pitch twice in the same chord cluster. The two NOTE
elements are identical in tick/staff/voice/pitch but differ in `grace1 bit 0x40`: one copy
has the bit clear (the "root" chord note), the other has it set (a chord-extension marker).
When both are added to MuseScore's Chord, the result is two noteheads at the same stem
position — visually a double-headed notehead the user must delete manually.

**Detection:** `grace1 & 0x40` marks the chord-extension copy. The importer suppresses it
when the concert pitch is already present in the chord (`chord->findNote(concertPitch)`).
This guard is scoped to `grace1 & 0x40` to avoid false positives on v0xC2 chord clusters,
where the `semiTonePitch` field holds an unreliable value before the v0xA6 pitch fix and
multiple cluster notes can share the same raw byte value.

**Source:** `handleNote` in `emitters-note.cpp` (~line 490).

### Tests

- `tst_notes.cpp` → `duplicate_pitch_in_chord_cluster_suppressed`: integration test loading
  `notes_chord_duplicate.enc` (a synthetic fixture with two identical NOTE elements at tick=0,
  pitch=60, grace1=0x00 and grace1=0x40). Verifies the chord has exactly one note.

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

**Tick-anchored attachment.** Each remaining syllable carries the raw Encore tick (the
lyric element's `tick` field, which may be visually offset by ~30-80 ticks from the
associated note's tick due to Encore's layout).  At the end of the measure pass the
importer walks the measure's chord-rest segments and assigns each chord the syllable
whose encTick is closest, within a half-beat threshold (`beatTicks / 2`).

**segEncTick from Encore NOTE elements (not from MuseScore cumTick).** Each
ChordRest segment's reference tick (`segEncTick`) is taken positionally from the
Encore NOTE elements in `encMeas.elements`, not derived from the MuseScore cumTick.
The previous approach (`cumTick × encTicksPerQuarter × 4`) was unreliable because:
- The note loop uses accumulated durations (cumTick), not Encore tick proportions.
- In 6/8 with `beatTicks=240` (quarter as beat, v0xC4), the formula applied a ×2/3
  compound correction, halving all segEncTick values and causing a systematic shift
  of one note (e.g. `John` attached to the second note in "When Johnny Comes Marching
  Home", and `ing` lost entirely because no note fell within the threshold).
- Using Encore NOTE encTicks directly avoids the conversion step: the kth MuseScore
  ChordRest corresponds to the kth NOTE element in `encMeas.elements`.

Exercised by `Tst_Text.lyrics_offset_ticks_still_attach_correctly` (lyric ticks
offset by +50 from their note ticks) and `Tst_Text.lyrics_compound_meter_all_syllables_matched`
(v0xC2 6/8, `beatTicks=360`).

**Multi-verse.** Each LYRIC element's `voice` field maps to the
resulting `Lyrics::verse()` value (0-indexed). Every verse
attaches to the host voice-0 chord on the same tick.

## Rhythm: face value, dots, tuplets

The face value nibble is authoritative for the notated duration.
`playbackDurTicks` is NEVER used to upgrade a note's visible
duration; it is consulted only by `detectImpliedTuplet` to flag
the note as a tuplet member.

**v0xC2 dotControl interpretation and the bit-0 fallback guard.**
In v0xC4, `dotControl` at note byte +14 is a dot COUNT (0, 1, 2, 3).
In v0xC2, the same byte is a layout/display field whose bit meanings are
less precise: bit 0 is sometimes set as a "dotted" indicator but also
appears coincidentally on undotted notes (observed with values 0x28, 0x39,
0x60 in tapada.enc where the notes are plain).

`computeDotCount` resolves dots in priority order:
1. `calcDots(dotControl, fv)` — treats `dotControl` as a tick value.
2. `calcDotsSnap(realDuration, fv)` — MIDI tick value within ±1 snap.
3. Bit-0 fallback (`useBit0Fallback=true`) — forces 1 dot when bit 0 is set.

**Bit-0 fallback guard (ticks.cpp):** the fallback only fires when
`realDuration > faceValue2ticks(fv)` (rdur exceeds the plain face value).
When `rdur ≤ faceTicks` the note is plain (exact match) or shortened by
multi-stream overlap; bit 0 in dotControl is then a spurious layout flag.
This guard prevents false dotted notes on v0xC2 plain 16ths and 8ths whose
`dotControl` happens to have bit 0 set (tapada.enc m28 staff 2: five plain
notes were incorrectly promoted to dotted, overflowing the measure).

**v0xC2 dotted-eighth anomaly (`fixDottedEighthPattern`, readers-v0xc2.cpp):**
Encore stores the 16th companion of a dotted-8th+16th group at `tick+120`
(= tick + faceTicks(8th)) instead of `tick+180` (= tick + dotted-8th).
Detection: 8th with `rdur=120` has a 16th at `tick+120` with `rdur=60`.
When detected, `EncNote::forceDotted` is set on the 8th; the emitter
then forces `dots=1` directly, bypassing `computeDotCount` entirely.

**faceSum guard in `fixDottedEighthPattern`:** the 8th+16th@tick+120 binary
pattern is ambiguous — it also appears in a genuine 8th followed by a 16th
inside a fully-filled measure. Guard: only apply when `faceSum + 60 == durTicks`
(the voice group is exactly 60t short — the amount the anomaly steals).
When `faceSum == durTicks` (full measure) the fix is blocked.

`EncNote::forceDotted` (elem-note.h): bool field set exclusively by
`fixDottedEighthPattern`. In `emitters-note.cpp`, when `forceDotted=true`
`dots=1` is assigned before `computeDotCount`, so the bit-0 fallback never
runs for these notes.

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

**Fractional dotted values must not match integer rdur.** For
some face values, the theoretical dotted duration is non-integer
in the 960-tick system.  The triple-dotted 16th is `60×15/8 =
112.5 ticks`; C++ integer division truncates this to 112.  A
live-recorded note whose `calculateRealDurations` tick-diff
happens to equal 112 would falsely match the triple-dot check
without a guard.  `calcDots` and `calcDotsSnap` therefore skip
each threshold when `(base × n) % d != 0` (i.e. when the dotted
value is not exactly representable as an integer), preventing
spurious dot counts that cause cumTick to overshoot and cap
subsequent notes to shorter values.  Affected face values:
16th (3-dot), 32nd (2-dot and 3-dot), 64th (all three), 128th
(all three).  Exercised by `notes_triplet_orphan_missing_tup.enc`
(the Salome compas 19 scenario) and the dedicated unit tests in
`Tst_EncoreRhythm.dotCalculation_noFalsePositiveForFractionalDottedValues`
and the integration test `Tst_Notes.rdur112_16th_note_not_triple_dotted`.

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

**"Coda" word label not imported.** Encore renders its first CODA
marker as "⊕ Coda" (symbol + the word). This is Encore's own
display convention — the word "Coda" is not stored in the ENC
file as a data element (it does not appear in the TEXT block and
there is no accompanying STAFFTEXT ornament element). MuseScore's
`MarkerType::CODA` renders as the ⊕ symbol only, which is the
standard music-engraving convention. The omission is therefore
intentional and correct.

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

**Overlapping bitmask filtering (`usedVoltaBits`).** Encore sometimes sets bits in a
later bracket that were already shown in an earlier bracket.  For example, "1.-3."
followed by a measure with raw bits `{2,4}` (bitmask `0x0A`): ending 2 was already
shown in the "1.-3." bracket, so only the NEW ending (4) should appear on the second
bracket ("4."), not "2, 4.".

`BuildCtx::usedVoltaBits` accumulates all bitmasks emitted so far in the current
repeat block.  Each new volta bracket filters its bitmask against `~usedVoltaBits`
to obtain the display mask.  Both `Volta::setEndings` and the displayed text use
the filtered mask.  When `repeatAlternative` drops to 0 the counter resets.

Exercised by `Tst_Importer.v0c4_volta_overlapping_bits_filtered`.

## Gap-snap wholeTicks must always be 960

The gap-snap logic converts an Encore MIDI tick to a fraction of the measure
using `encTickFrac = Fraction(e->tick, wholeTicks)`.  The formula
`wholeTicks = beatTicks * timeSigDen` gives 960 only when `beatTicks` is the
raw note unit (240 for x/4, 120 for x/8).  Files that store a non-standard
value — e.g. 2/2 with `beatTicks=240` instead of the correct 480 — produce
`wholeTicks=480`.  A note at Encore tick=360 would then have
`encTickFrac = 360/480 = 3/4`, exceeding `cumTick = 3/8` after a rest+quarter,
causing gap-snap to fire and jump cumTick from 3/8 to 3/4.  All notes in the
second half of the measure (ticks 480-840) are dropped (measure overflows).

Encore always uses 960 ticks per whole note regardless of time signature or
beatTicks encoding.  `wholeTicks` is now a compile-time constant 960.  The
same fix applies to the chord-symbol placement formula.  Exercised by
`Tst_Importer.v0c4_2_2_beatticks240_gap_snap_no_false_fire`.

## Percussion clef for drumset staves

When `applyBestInstrument` assigns a drumset template (via PERC clef, GM
range, or drumset name), the LINE block clef for that staff (often C3L, C4L
or F in band files) would otherwise override the percussion clef in
`buildInitialSignatures`.  After instrument assignment, `buildInitialSignatures`
now checks whether the staff carries a drumset and, if so, forces
`ClefType::PERC` instead of calling `pickStaffClef` on the enc clef.

## MIDI artifact filter bypass for chord roots and chord extensions

The MIDI artifact filter (lines 152–174 in `emitters-note.cpp`) drops notes
whose `realDuration` falls in the range 5–14 ticks when the face value is an
eighth note or longer.  Two valid cases were incorrectly caught:

1. **First note on staff in a measure** (`savedPrevMidiTick < 0`): can never
   be a tie-continuation artifact because there is no prior note in this
   measure to generate one from.  Its short `realDuration` comes from the
   *next* chord note starting a few ticks later.

2. **Chord extensions** (`isChordExt = true`): notes within
   `CHORD_MIDI_THRESHOLD` (8 ticks) of the previous note are real chord tones
   recorded with tight MIDI timing; they are not artifacts.

Both are now bypassed, so all notes in a simultaneous chord group survive even
when `calculateRealDurations` assigns a very short tick-diff rdur.

## Instruments in the GM Percussive range (MIDI programs 113–128)

General MIDI programs 113–128 are the "Percussive" section (Agogo, Steel
Drums, Woodblock, Taiko Drum, Melodic Tom, Synth Drum, …).  Encore files
sometimes name percussion parts with performer credits or catalog numbers
(e.g. "A. Marazuela 335", "Hermenegildo Lerma") that match no instrument
template.  Without a special case, the importer would reach the Grand Piano
fallback and ignore the MIDI program entirely.

`applyBestInstrument()` now checks for `instr.midiProgram >= 113` (Step 1b)
immediately after the PERC-clef check (Step 1) and before any name search.
When the program is in this range the instrument is routed to the drumset
template.  Exercised by
`Tst_Instruments.gm_perc_range_midi_program_routes_to_drumset`.

## Tuplet group with one note missing the tup byte (sandwich orphan)

Live-recorded v0xC4 files occasionally have `tup=0x00` on one note in the
middle of a triplet run, surrounded by notes with the correct explicit ratio
(e.g. `tup=0x32, tup=0x00, tup=0x32`).  Without a fix, the group breaks at
the orphan, the surrounding explicit notes are treated as isolated single-note
groups, all three are placed as regular 8ths, the measure overflows, and the
last triplet note is dropped entirely.

Two-part fix:

1. **`computeImpliedTupletMembers` — sandwich heuristic**: when the main
   explicit-group loop breaks (next note has wrong/no tup byte) but the group
   is still incomplete, check whether:
   - the orphan has the same face value as the group's base note;
   - the note after the orphan resumes the same explicit ratio;
   - the orphan's binary tick is within `max(4, advTicks/4)` ticks of the
     expected advance from the last included note.
   If all three hold, set `a2/n2` to the group's ratio so the loop continues
   and marks the orphan as a group member.

2. **`handleNote` — active-tuplet bypass**: even after the orphan is in
   `validTupletGroupMember`, `handleNote` recomputes `actualN/normalN` from
   `en->actualNotes()/normalNotes()`, which returns 0 for `tup=0x00`.  The
   else-branch then closes the active group.  A guard after the implied-tuplet
   block: if `actualN == 0` and the tuplet is active and not yet full and the
   note is in `validTupletGroupMember`, borrow the active tuplet's ratio so
   the note is added to the bracket rather than closing it.  Exercised by
`Tst_Notes.triplet_orphan_middle_note_missing_tup_byte` (single orphan,
no prior complete group) and
`Tst_Notes.triplet_orphan_with_prior_complete_group` (seenCompleteGroup=true
path).

## Time signature changes between metrically-equivalent meters

`buildInitialSignatures` emits a `TimeSig` segment at each measure
where the time signature differs from the previous one.  The detection
used `Fraction::operator==`, which compares by cross-multiplication:
`Fraction(6,8) == Fraction(3,4)` because `6×4 == 3×8 = 24`.  A score
that changes from 6/8 to 3/4 (or back) has the same total tick duration
in both meters (durTicks=720 in both), so the change was silently
skipped and no visual time signature indicator was written.

The fix uses `Fraction::identical()`, which compares numerator and
denominator directly (`6 != 3`), so distinct time signatures with equal
mathematical values are correctly detected as changes.  This applies to
all pairs of metrically-equivalent but visually-distinct signatures:
6/8 vs 3/4, 2/2 vs 4/4, 3/8 vs 6/16, etc.  Exercised by
`Tst_Structure.time_sig_change_6_8_to_3_4_and_back` and
`Tst_Structure.time_sig_change_2_2_to_4_4_and_back`.

## Common time "C" symbol (timeSigGlyph)

The MEAS header byte at offset 0x02 (`timeSigGlyph`) encodes the
visual form of the time signature.  Two values denote common time:

- `0x43` ('C', uppercase ASCII) — produced by Encore 3.x / 4.x
- `0x63` ('c', lowercase ASCII) — produced by Encore 5.x

Both map to `TimeSigType::FOUR_FOUR` (MuseScore's common-time C symbol).
When `timeSigGlyph == 0x00` the normal numeric display is used.

`buildMeasures` populates `ctx.nominalTimeSigType` and
`ctx.measTickToTimeSigType` (a tick-to-type map for change points) via
`encGlyphToTimeSigType()`.  `addInitialTimeSig` and the change-detection
loop in `buildInitialSignatures` call `tsig->setSig(ts, tsType)` with the
resolved type so that the "C" symbol survives the round-trip through MSCX.

Exercised by `Tst_Structure.timesig_v0c2_common_time_glyph_preserved`
(glyph=0x63) and `Tst_Structure.timesig_v0c2_common_time_glyph_uppercase_preserved`
(glyph=0x43).

## Parser normalization ("fat parse, thin import")

All format-specific interpretation is resolved in the parser layer before `EncRoot` is handed
to the importer. `postProcessElement()` in each `EncFormatReader` subclass is the single hook
where raw binary quirks are normalized into semantic fields. The importer (`BuildCtx` and all
emitters/resolvers) has no knowledge of which format version produced the data.

The three v0xC2 normalizations performed in `EncFormatReader_V0xC2::postProcessElement`:

| Quirk | Raw binary encoding | Normalized field |
|---|---|---|
| Ornament tipo 0xC4 = accent | ORN tipo byte = 0xC4 | remapped to ACCENT (0xBE) so all formats share a single tipo value |
| grace1 tie-sender flag | `grace1 & 0x0F == 1` | `EncNote.isTieSender = true` (false for all other formats) |
| alMezuro unreliable | alMezuro may hold stale values | `EncOrnament.alMezuroValid = false`; copied to `PendingSlur.alMezuroValid` at enqueue |

The importer uses `en->isTieSender` and `en->isImpliedTupletMember` directly (no format flags)
and `ps.alMezuroValid` (per-slur, not a global context flag). Adding a new Encore format version
requires only a new `EncFormatReader` subclass and its `postProcessElement` (for the three
ornament/note quirks) plus a `calculateRealDurations` phase when tuplet detection semantics
differ.

## v0xC2 size=24 pitch sub-variants

The v0xC2 pitch-swap (`semiTonePitch = byte[+13]; byte[+13] = 0`) was
introduced to handle notes where Encore stores pitch in the tuplet slot
(+13).  A second sub-variant exists in some Encore 4.x files where the
pitch is already in the standard `semiTonePitch` slot (+15) and the
tuplet slot contains 0.

The importer guards the swap with `if (en->tuplet > 0)` in
`postProcessElement` (`readers-v0xc2.cpp`): when `tuplet == 0` the swap
is skipped and `semiTonePitch` is preserved as-is.

Without this guard all notes in sub-variant B files (e.g. TUVEHAMB.ENC)
imported as MIDI note 0 (C-1) — same pitch, far below the staff.

Exercised by `Tst_Notes.notes_v0c2_size24_semitone_pitch`.

## Multi-measure rest expansion when successor is not a note measure

A single MEAS block whose lone REST element has `mrestCount > 1` is
expanded to that many MuseScore measures (`buildMeasures` and the
emitters's `measDisplayCount` lambda both track this).

The original code guarded expansion with two conditions:

1. Predecessor must not also be a single-REST block (prevents cascading).
2. **Successor must contain pitched notes** (`hasPitchedNotes(*next)`).

Condition 2 was the bug source: it collapsed a legitimate mrest when
followed by a rest measure (e.g. a dotted-quarter rest in the measure
after a 3-measure multi-measure rest). The successor content is
irrelevant — Encore's `mrestCount` byte is authoritative.

The fix removes condition 2 from `encMeasDisplayCount` (builders.cpp)
and from the identical `measDisplayCount` lambda in emitters.cpp. Both
must agree or `buildMeasures` creates the right number of MuseScore
measures but the emitters places notes in the wrong ones.

Exercised by `Tst_Importer.mrest_single_block_expands_when_successor_is_rest`.

## Sid::createMultiMeasureRests set only when file uses mrest blocks

`buildScore` (`import.cpp`) sets `Sid::createMultiMeasureRests` only when the
Encore file contains at least one MEAS block whose lone REST element has
`mrestCount > 1`. When no such block exists the flag stays at its MuseScore
default (false), so individual rest measures are rendered as individual whole
rests — matching what Encore displays — rather than being collapsed into
multi-measure rest objects.

Detection: `std::any_of` over `enc.measures`, checking `elements.size() == 1`,
element type `REST`, and `mrestCount > 1`.

Exercised by:
- `Tst_Importer.mmrest_flag_on_when_file_has_mrest_block` (flag true when mrest present)
- `Tst_Importer.mmrest_flag_off_when_file_has_no_mrest_blocks` (flag false otherwise)

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

**Endpoint priority.** Three-tier resolution:

1. **Next-dynamic** (primary): first Dynamic annotation on the same track
   after start tick and within the `alMezuro` upper bound. Handles `mf<f>mf`
   chains where each hairpin terminates at the next visible dynamic glyph.

2. **`xoffset2` note snap** (fallback when no Dynamic): scan the target
   measure for the last NOTE/REST with `xoffset <= xoffset2`. End the
   hairpin at that note's tick. Mirrors the `snapTickByXoffset` start-snap
   logic in `emitters-orn.cpp`.

3. **Bar-line clamp** (when no note found in step 2): if `xoffset2`
   precedes all notes with positive xoffsets in the target measure, clamp
   to `targetMeasure.tick`.

Steps 2-3 are skipped for notes with `xoffset == 0` (synthetic fixture
guard) and when `xoffset2 == 0` (no endpoint hint).

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

## Grace-to-main and grace-to-later slurs

When a SLURSTART (tipo 0x21) is co-located with an appoggiatura (same Encore tick),
the pixel-span heuristic fails because grace notes and their parent chord share the
same written tick — there is no note at the proportional written tick derived from
the slur's xoffset2. Two resolution cases:

**Grace-to-main** (`startTick == endTick` after snapping): create the slur with
`startElement = graceChord` and `endElement = mainChord`. Skip both
`computeStartElement()` and `computeEndElement()` so neither auto-resolver overrides
the explicitly-set elements.

**Grace-to-later** (`startTick < endTick`): find the chord AT or AFTER `startTick`,
read its `graceNotesBefore()`, set `startElement = graceChord`. Skip only
`computeStartElement()`; let `computeEndElement()` run normally.

**Co-located grace+regular notes (both orderings):**

When a grace and its principal note share the same Encore tick, three additional rules apply:

1. **firstNoteXoff = grace xoffset.** Use the GRACE note's xoffset as the reference for
   `targetEndXoff = startXoff + pixelSpan`. The co-located regular note has a larger
   xoffset; using it inflates the target and selects a later note as endpoint. Stop the
   iteration at `startTick` as soon as a grace note is found (v0xC4 serialises regular-first
   at the same tick, so continue past regular notes until hitting the grace).

2. **Integrated shortcut.** After scanning, if the co-located regular note matches
   `targetEndXoff` better than any later note (`regularDist < bestDist`), resolve
   grace-to-main. If a later note matches better, use the heuristic endpoint (grace-to-later).

3. **Zero-span invariant.** If no endpoint note is found, set `tick2 = tick` (same as start).
   A post-pass detects this condition and treats the slur as grace-to-main, skipping the
   general end-element resolver. Without this, the resolver finds a rest or next-measure note.

**Attaching grace-to-main slurs.** Use `addSpanner(slur, /*computeStartElement=*/false)`.
The `computeStartElement()` call in the regular path would replace the explicitly-set grace
with the main chord.

**v0xC4 binary ordering.** Encore 5 serialises the MAIN note BEFORE its ACCIACCATURA
grace at the same beat — opposite of v0xC2 (grace first). When the main note arrives first
and a grace follows at the same tick (`tick − prevTick < 8`), it is a retroactive
chord-extension of the already-placed main chord. Attach it directly to that chord instead
of queuing it as a prefix for the next note.

**Regression fixtures:**

| Fixture | Format | Pattern |
|---------|--------|---------|
| `ornaments_v0c2_grace_slur_to_main_coloc.enc` | v0xC2 | Grace before main; note@600 "bait" |
| `ornaments_v0c4_grace_slur_to_main_coloc.enc` | v0xC4 | Grace before main (coloc) |
| `ornaments_v0c4_grace_after_main_in_binary.enc` | v0xC4 | Regular FIRST, grace SECOND at tick=0 |
| `ornaments_v0c4_grace_after_main_grace_to_later.enc` | v0xC4 | Regular FIRST at tick=240; note@480 |
| `ornaments_v0c4_grace_after_main_preceding_notes.enc` | v0xC4 | Preceding quarter + regular@240 + grace@240 |
| `ornaments_v0c4_grace_after_main_slur_to_main.enc` | v0xC4 | Regular xoff=20, grace xoff=10; slur at grace |

---

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

## Staff scale

The file header byte at 0x52 holds a staff-size selector (1-4, default 4).
`applyStaffScale` maps it to `Pid::MAG` on every staff before `resolveAll`.
Global spatium is not changed.

| Header value | Staff scale |
|---|---|
| 1 | 60% |
| 2 | 75% |
| 3 | 100% |
| 4 | 130% |

## Page margins

Page margins are stored in an optional WINI block near the end of the file.
The block is written only when the user explicitly opens and saves Page Setup
in Encore; files that were never touched through that dialog have no WINI block.

### WINI block layout

Magic: `WINI`. Size field: 42 bytes (21 × uint16 LE).

The four margin-related values are stored as int32 LE (pairs of adjacent uint16,
high word always zero) at byte offsets 24-39 within the block content:

| Offset | Field | Meaning |
|---|---|---|
| +24 | top | top margin |
| +28 | left | left margin |
| +32 | bottomEdge | bottom boundary of printable area (from page top) |
| +36 | rightEdge | right boundary of printable area (from page left) |

### WINI unit variants

Encore 5.x stores these values in **typographic points (1/72")**.
Earlier versions (~4.x, some 3.x) store them in **screen pixels at the monitor
DPI** (~84-85 PPI). Detection in `applyPageMargins` (`import.cpp`):

```
if rightEdge > pageWidth × 72:   # coordinate exceeds A4 in pts → screen-pixel format
    scale = (rightEdge + left) / pageWidthIn   # ≈ 84.67 for A4
else:
    scale = 72                                 # typographic points
```

### Page-size detection (`detectWiniPageSize`)

In screen-pixel format, the paper size is not stored explicitly. It is recovered
by matching `pageWidth_units = rightEdge + left` against all standard
`QPageSize` sizes in two passes:

**Pass 1 — ISO A-series (A0..A10) only.** All AN sizes share the 1:√2 aspect
ratio, so within any AN file exactly one AN size falls in the plausible DPI
range [60, 135] (consecutive sizes differ by √2 ≈ 1.414× in DPI). The in-range
AN candidate with smallest `|dpiW − dpiH|` is selected. Checking A-series first
prevents non-document sizes (envelopes, 12"×18" sheet, …) from winning over the
correct AN via accidentally smaller delta.

**Pass 2 — all other sizes (Letter, Legal, B-series, …).** Reached only when no
A-series candidate qualifies. Picks smallest `|dpiW − dpiH|` among in-range
candidates. If neither pass finds a match, returns false and the caller keeps the
current MuseScore page dimensions (custom page).

When the page is detected, `Sid::pageWidth` and `Sid::pageHeight` are updated
to the detected dimensions before computing margins, so right/bottom margins
are always derived from the correct paper size.

### Encoding quirks

Encore rounds when storing: `round(inches × 72)`. The display in Page Setup
shows `floor(pts / 72 × 1000) / 1000` so values may differ slightly from what
the user typed (e.g. 0.100 in stores as 7 pts and displays as 0.097 in). In
screen-pixel files Encore also uses 1/72" for its own margin display, so the
top/left values it shows (e.g. 0.39") differ slightly from the physical margin
computed at the correct DPI (e.g. 0.33").

### Files with no WINI block

Files that were never saved through Page Setup have no WINI block.
`EncPageSetup::hasData` will be false and `applyPageMargins` is a no-op;
MuseScore defaults (15 mm per side) remain.

## Import options (EncImportOptions)

`importEncore()` accepts an optional `EncImportOptions` struct (`import-options.h`).
All fields default to the current behaviour so existing callers are unaffected.
The Preferences UI (Phase 2) will read these values from `IEncoreImportConfiguration`.

### Layout group

| Field | Default | Effect when changed |
|---|---|---|
| `importPageLayout` | `true` | When `false`, `applyPageMargins()` is skipped; MuseScore default margins apply. |
| `importPageBreaks` | `true` | When `true`, a `LayoutBreak::PAGE` is inserted after the last measure of each system where the LINE block `pageIdx` resets (row-on-page counter decreases or stays the same), indicating a new page. When `false`, no page breaks are added and MuseScore paginates freely. |
| `importSystemLocks` | `true` | When `false`, `applySystemLocksFromLines()` is skipped; MuseScore freely reflows measures across systems. |
| `importStaffSize` | `true` | When `false`, `applyStaffScale()` is skipped; all staves keep MAG 1.0. |

### Text / content group

| Field | Default | Effect when changed |
|---|---|---|
| `importTempoTextSemantic` | `true` | When `false`, Italian tempo terms in STAFFTEXT ornaments stay as `StaffText` instead of being promoted to `TempoText` with BPM. Numeric BPM marks from MEAS headers are unaffected. |
| `importUnsupportedArticulationsAsText` | `false` | When `true`, articulation bytes with no MuseScore equivalent (0x01, 0x02, 0x09, 0x47-0x4A) are emitted as `StaffText` instead of being silently dropped. |

### Measure correction group

| Field | Type | Default | Options |
|---|---|---|---|
| `underfillMeasureStrategy` | `UnderfillStrategy` | `InvisibleRests` | `InvisibleRests`: gap rests (invisible). `VisibleRests`: normal visible rests. `IrregularMeasure`: reserved. |
| `overfillMeasureStrategy` | `OverfillStrategy` | `Truncate` | `Truncate`: remove trailing notes (current). `StretchLastNote`: reserved. `IrregularMeasure`: reserved. |
| `firstMeasureIsPickup` | `bool` | `true` | When `false`, Case A and Case B pickup detection are bypassed; the first measure keeps its nominal full duration and leading beats are left as rests. |

#### firstMeasureIsPickup=false + IrregularMeasure invariant

When `firstMeasureIsPickup=false` and the first Encore measure is a Case A pickup (its
declared time signature differs from the nominal), `buildMeasures` sets the first MuseScore
measure's ticks to `nominalTimeSig` and must also advance `currentTick` by the same
`nominalTimeSig` value — not by the shorter Encore pickup duration `ts`.

If these two values diverge, subsequent measures are placed at inconsistent positions.
When `IrregularMeasure` then fires during `fillTrailingGaps` (because the measure's actual
content is shorter than its nominal duration), it computes a shift delta based on
`nominalTimeSig` but applies it to positions anchored at `ts`, moving all later measure
internal ticks away from their implied barlines. Spanners such as volta brackets that are
placed using `measure->tick()` therefore land mid-measure instead of at barlines.

Exercised by `Tst_Options.firstMeasure_not_pickup_irregular_volta_at_barline`.

### Pending lyric fix (not an option)

`attachPendingLyrics()` now falls back to attaching unmatched lyrics to the nearest
`Rest` in the measure instead of discarding them silently.

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
