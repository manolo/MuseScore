# Encore importer coverage gaps, ranked

Every item carries its evidence and the thing that would close it. Ranked by corpus impact times
severity, so effort lands where real files exist.

Companion documents: [ENCORE_VERSION_INVENTORY.md](ENCORE_VERSION_INVENTORY.md),
[ENCORE_FORMAT_DIVERGENCE.md](ENCORE_FORMAT_DIVERGENCE.md),
[ENCORE_FORMAT_EVOLUTION.md](ENCORE_FORMAT_EVOLUTION.md).

Corpus: **20577 real files** parsed, everything reachable under `~/Scores` following symlinks.

---

## Summary

| # | Gap | Class | Files | Elements affected | Status |
|---|-----|-------|-------|-------------------|--------|
| 0 | **Every Encore 3.x element body read 2 bytes late** | field | **3215 (15.6%)** | **4.23M notes, 581k rests, 11.5k ornaments** | **FIXED** |
| 1 | Tie arc fields discarded by a size guard | field | ~4000 | **124131 ties** | **Encore 3.x half FIXED with gap 0**; v0xA6 half open, see below |
| 2 | v0xC2 slur measure count read from the wrong offset on Encore 4.x | field | 1760 | **2185 slurs lost of 6691**, plus 83 files losing every count to a stale guard | **FIXED** |
| 3 | v0xA6 ornament y and measure count at v0xC4 offsets | field | 806 | **105 spanners lost of 636 (16.5%)**, plus every ornament y | **FIXED** |
| 4 | SCO5 tie arc read as 8-bit, always zero | field | 15 | 515 ties | **FIXED** |
| 5 | Encrypted containers: 25% of the corpus, 2 fixtures | coverage | 5255 | whole container class | measured |
| 6 | Encore 3.x and 4.0-4.2 have zero fixtures | coverage | 4902 | two whole generations | measured |
| 7 | Encore 4.3 to 4.5 (rev 0/1) has 24 fixtures for 3671 files | coverage | 3671 | whole revision line | measured |
| 8 | Element type `0xA` | field | 14 | 1174 elements | **RESOLVED, drop is correct** |
| 9 | v0xA6 ties, beams, key changes never tested | coverage | 806 | 39772 ties alone | measured |
| 10 | SCO5 is effectively untested | coverage | 15 | 15 of 15 ornament subtypes | measured |
| 11 | Key signature diverges across conversion pairs | field | 6 of 399 pairs | LINE key byte on tab staves | **RESOLVED, not a defect** |
| 12 | Chord symbol reads a fixed 36 byte text slot past the element | field | many | 1098 names corrupted | **FIXED, see gap 22** |
| 13 | Unknown version byte silently parsed as v0xC4 | dispatch | 0 today | any unseen release | **RESOLVED, reads by format version** |
| 14 | `SCOX` / `SCOR` / `SCOS` rejected outright | dispatch | 0 of 21620 | n/a | **RESOLVED, dropped from the spec** |
| 15 | v0xA6 has no multi-measure rest support | field | 806 | none | **RESOLVED, the format has none** |
| 16 | 136 ZIP archives named `.enc` | corpus hygiene | 136 | n/a | not a defect |
| 17 | Dead `ENCORE_IMPORTER.md` links on the PR branch | docs | n/a | 3 links | won't fix, see below |
| 18 | v0xA6 note position, rest tuplet and rest dot control at v0xC4 offsets | field | 806 | 1.72M note positions, 62591 rests | **FIXED** |
| 19 | The four articulations Encore 4.0 renumbered | field | 3216 | 700 marks dropped | **FIXED** |
| 20 | v0xA6 note reads its neighbour's tick as velocity, options and accidental | field | 806 | 1.72M notes, inert but one live path | **FIXED** |
| 21 | `0xC4` remapped to an accent for every v0xC2 generation | field | 2673 + a few | 22 up-bows | **FIXED** |
| 22 | Chord symbol name read as a fixed slot, running into the next element | field | many | **1098 names corrupted** | **FIXED** |
| 23 | Do the version byte and the format version ever disagree | dispatch | 996 crossed | 3 behaviours checked | **RESOLVED, no defect** |

All of these come from **one mechanism**: a field addressed by an absolute offset that does not
hold it in that generation of the format, with no per-generation remap. See
[ENCORE_FORMAT_DIVERGENCE.md](ENCORE_FORMAT_DIVERGENCE.md) section 1.

**What has been fixed.** Gaps 0, 2, 3, 4, 13, 18, 19 and 20 are implemented, each with a regression
test that fails without it. Gap 0 also closes the Encore 3.x half of gap 1, since the tie arc
offsets move with the same shift. Gaps 8 and 11 were closed as non-defects. Gap 1's v0xA6 half is
documented but deliberately not changed, see below. Gaps 5 to 7, 9, 10, 12, 14 and 15 are open.

**Gap 17 is not fixable.** The importer spec lives on the working branch only, so the link is
correct there and dead on the PR branch, and the branch invariant requires the file to be identical
on both. The dead links are a consequence of the branch layout, not an oversight.

---

## 0. Every Encore 3.x element body is read two bytes late  (FIXED)

**The largest defect found, and it subsumes several of the others.**

Fixed by keying the element body offsets on the app version at header `0x28`, which is what
separates the two generations: the version byte is `0xC2` for both and the element sizes overlap,
so neither identifies the layout on its own. Verified end to end against a conversion pair, where
the Encore 3.x original and its Encore 4.5 conversion now import to the same music: 374 of 374
pitches, identical durations, 11 slurs with identical endpoints.

VM experiment B converted an Encore 3.x file (app 773, 18 measures) to v0xC4 in Encore 4.5. The two
element streams align exactly: 634 elements each, 374 notes, 138 lyrics, 67 beams, 32 rests, 11
ornaments, 2 ties, same measures, ticks, staves and voices.

Diffing the paired elements byte for byte gives one answer for every element type: **bytes `+0` to
`+5` are identical, and from `+6` onward the Encore 4.x element carries two extra bytes and then
continues identically.** The v0xC2 reader uses Encore 4.x offsets for both generations, so on an
Encore 3.x file every field at or past `+6` is read from the wrong byte.

Ornaments, verified against the converted file:

| field | code reads | **correct for 3.x** | agreement |
|-------|-----------|---------------------|-----------|
| xoffset | `+10` | **`+8`** | 0/11 versus **11/11** |
| y (s16) | `+12` | **`+10`** | 0/11 versus **11/11** |
| v0xC2 measure count | `+16` | **`+14`** | 0/11 versus **11/11** |
| forward measure count | `+18` | **`+16`** | 0/11 versus **11/11** |
| end x | `+20` | **`+18`** | 0/11 versus **11/11** |
| hairpin direction | `+26` | **`+24`** | 0/11 versus **11/11** |

Notes, over 374 paired notes:

| field | code reads | **correct for size 22** | agreement |
|-------|-----------|--------------------------|-----------|
| xoffset | `+10` | **`+8`** | 9/374 versus **374/374** |
| position | `+12` | **`+10`** | 0/374 versus 228/374 |
| tuplet ratio | `+13` | **`+11`** | 0/374 versus **374/374** |
| dotControl | `+14` | **`+12`** | 1/374 versus **374/374** |
| MIDI pitch | `+15` | **`+13`** | 0/374 versus 228/374 |

(The 228/374 on position and pitch is chord-member ordering inside a tick group, not disagreement.)

**Scope: 3215 files, 15.6% of the corpus, 4231117 size-22 notes, 581269 size-16 rests, 84337
size-16 ties, 11457 ornaments.**

**What this explains.** Three existing pieces of the importer are workarounds for this one shift:

- The "two v0xC2 tempo layouts", where older files supposedly store the BPM at `+28` instead of
  `+30`. They store it at `+28` because `+30` minus two is `+28`.
- The slur measure count being "unreliable for a whole file" (gap 2). It is at `+16` in 3.x and
  `+18` in 4.x.
- `markImpliedTupletMembers`, which infers tuplet membership from a duration mismatch. It exists
  because the explicit tuplet byte is at `+11` in a size-22 note and is never read: the pitch-swap
  heuristic recovers the pitch from `+13` and then sets the tuplet to zero. Encore 3.x scores
  therefore look as though they carry no explicit tuplets at all.

**Fix:** key the element body offsets on the generation, which is readable from the element size
(note 22 versus 24/28, ornament 26 versus 28, tie 16 versus 18, rest 16 versus 18), exactly as gaps
1 and 2 propose for their individual fields. Doing it once at the layout level closes gaps 1 and 2
as special cases.

**Closes with:** fixtures from the Encore 3.x generation, of which there are currently none for
2744 + 467 real files.

## 1. Tie arc fields discarded (124131 ties)  (Encore 3.x half FIXED)

`EncTie::read` reads `arcX1`, `arcX2` and `sourcePosition` only when `size >= 18`. Encore 3.x ties
are 16 bytes and Encore 2.x ties are 7, so **124131 ties fall through** (84359 at size 16, 39772 at
size 7) and the tie direction is decided from the two flag bytes alone.

The fields are present. Per-byte profile of Encore 3.x size-16 ties against size-18 ties, showing
the same uniform +2 shift as every other element:

| field | Encore 3.x (size 16) | Encore 4.x and later (size 18) |
|-------|----------------------|--------------------------------|
| dirByte | `+5`, 100% nonzero | `+5`, 100% nonzero |
| startFlag | `+6` | `+6` |
| padding | `+7`, always 0 | `+7`, `+8`, `+9`, always 0 |
| **arcX1** | **`+8`**, 100% nonzero, `+9` always 0 | `+10`, 100% nonzero, `+11` always 0 |
| **arcX2** | **`+10`**, 100% nonzero, `+11` always 0 | `+12`, 100% nonzero, `+13` always 0 |
| **sourcePosition** | **`+12`**/`+13` duplicated pair | `+14`/`+15` duplicated pair |

**Fixed for the Encore 3.x half** by gap 0: the arc offsets move with the same two-byte shift, so
keying the body layout on the generation reaches them without a tie-specific rule.

**Still open for the 39772 v0xA6 ties.** Their layout has since been located (the two flag bytes are
swapped relative to every later version, and the source position sits at `+9` duplicated at `+11`),
but the arc pair was not found and the available ground truth is too thin to justify changing the
tie-start decision.

### The v0xA6 tie layout, and why it was not acted on

Measured over all 39772 v0xA6 ties, the byte at `+6` carries a value from the four-way arc-direction
vocabulary (`0x02` / `0x04` / `0xFC` / `0xFE`) in 99.4% of them and the byte at `+5` in none, while
`+5` is `0x80` in 85% and `0x00` in the rest. So the two flag bytes hold the opposite roles from
every later version. The source position at `+9`, duplicated at `+11`, matches a converted file
exactly.

The arc pair was not found. In the file where a conversion gives ground truth the candidate bytes
are constant across every tie while the converted arc positions vary, and x coordinates do not
survive the conversion anyway.

Acting on the swap would change the tie-start decision for 39772 elements. Against the 14 ties that
pair cleanly with a converted twin, reading the direction from `+6` scores 71% correct against the
current 64%, with the same four false positives on both. That is not enough evidence to move that
many ties, so the layout is recorded and the behaviour left alone.

**Closes with:** a v0xA6 file with a converted twin whose ties pair densely enough to separate the
two readings, or a round trip through Encore 2.x.

## 2. v0xC2 slur measure count read from the wrong byte  (FIXED)

`EncFormatReader_V0xC2::postProcessElement` always takes the forward measure count from `+16`
(`orn->alMezuro = orn->altMezuro`). The field moved with the +2 shift, and the corpus separates the
two cases perfectly:

| generation | slur size | slurs | endpoint lost | count at `+16` lands in score | count at `+18` |
|------------|-----------|-------|---------------|-------------------------------|----------------|
| Encore 3.x (app 773) | 26 | 2762 | **0 (0.0%)** | **100%** | 58% |
| Encore 4.0-4.2 `0xC2` (app 775) | 28 | 6663 | **2172 (32.6%)** | 68% | **100%** |
| `0xC2` with app 1056 | 28 | 28 | 13 (46.4%) | | |

Zero failures on the generation the code was tuned for, one in three on the other, same code path.
The ~4500 slurs that do not land outside the score are not therefore correct: they read the wrong
byte and happen to land in range.

**Fixed** by dropping the copy that put the value back on the `+16` field: gap 0 already points the
inline read at the byte the file's generation uses, so the value read is the span in both.

The fix had a second layer. The "unreliable count" guard was calibrated against the garbage `+16`
values and, once the field was correct, was nullifying it. Measured over 887 files with slurs at the
correct offset: the "count points past the last measure" tell fires on **0** files and stays as a
cheap sanity check; the "same multi-measure span repeated" tell fires on **83** files, where a
repeated span is ordinary music, and because it condemns the whole file it was discarding every
count in it. That tell came from a file whose slurs carry 11 and 13 per staff at `+16` and **0 at
`+18` for every one of them**: there was no per-staff constant, only a misread. It was removed,
along with the test and fixture that encoded the phantom.

Measured effect on a 44-measure file whose bytes declare 82 cross-measure slurs: the import goes
from **21 to 82** cross-measure slurs, matching the file exactly.

**Closes with:** one Encore 3.x and one Encore 4.0-4.2 fixture, each with a multi-measure slur.

## 3. v0xA6 ornament fields read at v0xC4 offsets  (FIXED)

The v0xA6 reader overrides only `staffTextTindOffset` and `staffTextYoffsetOffset`, and both apply
only to STAFFTEXT. Every other ornament field kept its v0xC4 offset.

**VM experiment A has been run** and settles it. `SILENT.enc` (v0xA6, 12 measures) was opened in
Encore 4.5 and saved as `SILENT-45.enc` (v0xC4). The two element streams match exactly: 171 notes,
86 lyrics, 9 beams, 7 rests, 6 ties and 6 ornaments, same measures, ticks, staves and subtypes. The
v0xC4 half is therefore ground truth for the v0xA6 half.

Brute-forcing every offset and integer encoding in the v0xA6 slot against that ground truth gives a
unique answer for two fields:

| field | current read | **correct** | evidence |
|-------|--------------|-------------|----------|
| ornament y | s16 LE at `+8`, STAFFTEXT only | **signed byte at `+9`, all subtypes** | 6 of 6 exact: 15, -1, 9, 1, 5, -20 |
| forward measure count | `+18` | **`+14`** | 5 of 5 exact, and see the corpus check below |

Corpus-wide confirmation over all **636 v0xA6 slur and hairpin starts**:

| read at | land inside the score |
|---------|------------------------|
| `+18` (current) | 531 of 636, **105 out of range (16.5%)** |
| `+14` | **636 of 636, 0.0% out of range** |

The signed byte at `+9` spans -4 to 17 across the same 636 elements, a plausible vertical range.
The current s16 read yields values in the thousands (3840 where the truth is 15, -4865 where it is
-20). It preserves the sign, structurally, because `+9` is that halfword's high byte, which is why
staff-text above-versus-below placement works today while everything that uses the magnitude does
not, and why dynamics and the `0xCC` versus `0xCD` fermata direction never get it at all.

**Also validated by the same experiment, no defect:** the v0xA6 note reader is correct. All 85 note
groups keyed by (measure, tick, staff, voice) match on both pitch (`+11`) and face value (`+5`).
The staff-text TEXT index at `+28` is consistent; it reads 0 and 1 where the converted file reads 4
and 5, which is TEXT block re-indexing across the conversion, not a misread.

**Still unlocated:** the v0xA6 x coordinates and the hairpin direction. x does not survive the
conversion at all, because v0xA6 stores screen pixels and v0xC4 stores a different unit, so no
offset in the slot matches the converted values. The sample carried no hairpin.

**Fix:** give v0xA6 a real ornament layout with y at `+9` and the measure count at `+14`, applied
to every subtype, the way the v0xA6 note already gets its own pitch and tuplet offsets.

## 4. SCO5 tie arc always reads zero (515 ties, 15 files)  (FIXED)

`arcX1` and `arcX2` are declared `quint8` and read with `ds >> arcX1` at `+10` and `+12`. The
corpus shows they are **16-bit** fields:

| | `+10` | `+11` | `+12` | `+13` |
|--|-------|-------|-------|-------|
| SCOW size-18 ties | mean 50.6, 100% nonzero | 0.0% nonzero | mean 36.1, 100% nonzero | 0.0% nonzero |
| SCO5 size-18 ties | **0.6% nonzero** | mean 57.7, 100% nonzero | **0.6% nonzero** | mean 38.5, 100% nonzero |

A perfect mirror: little-endian SCOW puts the significant byte first, big-endian SCO5 puts it
second. Reading a single byte at `+10` therefore yields 0 on every SCO5 tie.

Downstream, `EncTie::read` does:

```
if (arcX1 < arcX2)  isTieStart = true;
else if (arcX1 == arcX2 && (startFlag & 0x80) == 0)  isTieStart = false;
```

With `arcX1 == arcX2 == 0` on every SCO5 tie, the second branch fires whenever the start flag's
high bit is clear, forcing `isTieStart = false`. 61% of SCO5 ties have a zero start flag byte.

**Fixed** by reading `arcX1` and `arcX2` as `quint16` through the stream, which already carries the
right byte order per magic. SCOW is unchanged. Comes with the first SCO5 fixture carrying a tie.



## 5 to 7, 9, 10. Coverage

**14272 of 20577 files (69%) sit in a header combination with no fixture at all.**

| Missing combination | Real files | Fixtures |
|---------------------|-----------|----------|
| ZBOT encrypted, rev 1 | 4253 | 0 |
| Encore 4.3-4.5, `0xC4` app 1056 rev 1 | 3155 | 0 |
| Encore 3.x, `0xC2` app 773 rev 1 | 2744 | 0 |
| Encore 4.0-4.2, `0xC2` app 775 rev 1 | 1298 | 0 |
| ZBOT encrypted, rev 0 | 994 | 0 |
| Encore 2.x, app 592 rev 1 | 709 | 0 |
| Encore 3.x, rev 0 | 467 | 0 |
| Encore 4.0-4.2, `0xC2` rev 0 | 393 | 0 |
| Encore 4.0-4.2, `0xC4` rev 0 | 243 | 0 |
| SCO5 rev 2 | 11 | 0 |

**Encrypted containers are 5255 files, 25.5% of the corpus, against 2 fixtures.** That is the
single largest coverage gap by file count.

Per format, features present in the corpus but in no fixture of that format:

- **SCO5**: 15 of 15 ornament subtypes, 17 of 18 element size classes, all 4 ornament sizes, rest
  size 18, tab tuning. Two fixtures cover essentially nothing.
- **v0xA6**: ties (39772), beams, key changes, MIDI CC, slurs, dynamics `0x82`/`0x83`/`0x85`,
  To Coda `0xA5`.
- **v0xC2**: staff text `0x1E`, 47 of 51 ornament subtypes, rest size 20, note sizes 26 and 28.
- **v0xC4**: 17 of 76 ornament subtypes, articulation-down bytes including `0x1D`.

### The fixtures state a generation they do not have

Re-checked against the two axes that matter, the version byte and the format version, **42 fixtures
carry a pair that occurs in no real file**: 31 v0xC2 fixtures and 11 v0xA6 fixtures stamped format
4.20. They were built without a format version and inherited the skeleton's.

| pair | fixtures | real files |
|---|---|---|
| `0xC2` + 4.20 | 31 | 0 |
| `0xA6` + 4.20 | 11 | 0 |
| `0xC2` + 3.05 | 4 | 3220 |
| `0xC2` + 3.07 | 2 | 1718 |
| `0xA6` + 2.50 | 8 | 786 |

This matters more than a wrong number in a header, because every behaviour keyed off the format
version is invisible to those 42: the two byte element body shift, and the articulation
renumbering. A v0xC2 fixture stamped 4.20 exercises the v0xC2 reader against the newest geometry, a
combination Encore never wrote.

Re-stamping is not a one line change. A fixture moved to format 3.05 gets a body shift of -2, so
its bytes have to be rebuilt with the geometry that goes with the stamp. It is fixture work, not a
patch, which is why it belongs with the coverage fixtures rather than with the defect fixes.

**Correction to two earlier drafts.** The first pass ran before the census followed symlinks, saw
347 of the available files, and concluded that 48 fixtures carried header combinations no Encore
build writes. The second over-corrected, and said every fixture header combination occurs in real
files. The table above is the measured answer: most do, and 42 do not. Beyond that the fixture
problem is proportion: v0xC2 is 24% of the corpus and 8% of the fixtures, and encrypted files are
25% of the corpus and 0.5% of the fixtures.

**Closed with** six fixtures, weighted toward what the defects taught: every one of them came from
a field addressed at an offset the generation does not use, so the fixtures that matter are the
ones that state a real generation and hold the geometry to match.

| fixture | covers | pair | corpus |
|---|---|---|---|
| `structure_family_3x.enc` | note, rest, tie, staccato, MIDI CC in the pre-4.0 geometry | `0xC2` + 3.05 | 3220 files |
| `structure_family_40x_c2.enc` | the same music in the shifted geometry | `0xC2` + 3.07 | 1718 files |
| `structure_family_40x_c4.enc` | the same music again, on the pair that crosses the two version axes | `0xC4` + 3.07 | 996 files, none before |
| `importer_v0xa6_tie_and_key_change.enc` | the compact tie and the compact key change | `0xA6` + 2.50 | 39772 ties, none before |
| `ornaments_sco5_bigendian.enc` | articulation, dynamic, fermata and a rest, big-endian | SCO5 | 16 files, no ornament before |
| `zbot_family_40x.enc` | the element family through the decryption | encrypted | 5257 files, 2 fixtures before |

The first three share **one** assertion set, which is the point: Encore moved the whole element
family in one release, so a reader that gets the generation wrong fails on all of them at once. The
test was checked by forcing the body shift back to zero, and it fails on the 3.05 file.

Every fixture is built by `tests/data/gen_enc_test_files.py`, which lives on this branch alongside
this document. The 42 mis-stamped fixtures described above are a separate job: re-stamping one
changes how its bytes are read, so each has to be rebuilt with the geometry that matches its
stamp.

## 8. Element type `0xA`: RESOLVED, and the current drop is correct

Type `0xA` carries **pitched events Encore plays but does not notate**. It uses the note layout of
its own generation byte for byte (face value `+5`, xoffset `+10`, staff position `+12`, MIDI pitch
`+15`, playback duration `+16`, velocity `+19`), and decoding it as a note yields coherent music:
in `zustrara.enc` a stepwise melody with accidentals, in `RONDAA~1.ENC` a run of constant
middle-line quarters.

Opening `RONDAA~1.ENC` in Encore 4.5 confirms what it is. The carrying staff is **hidden** in the
Staff Sheet; revealing it shows stemless noteheads piled up, with an ordinary **whole-measure rest
in every affected measure**. Encore therefore considers those measures notationally empty and keeps
the events for playback only.

Supporting evidence from the corpus:

- The events never share a tick with a type-9 note on the same staff (0 of 166 in `RONDAA~1.ENC`).
- The staff switches from type 9 to type 10 at a measure boundary and never switches back
  (measures 0-20 notes, 21-31 type 10, zero overlap).
- Saving from Encore 4.5 and from Encore 5.0 preserves all 166 elements; 146 of them change by
  exactly one byte, `xoffset` at `+10`, by plus or minus one. That is re-layout, not conversion. So
  this is a live element type, not a legacy artefact.

**Correction to an earlier draft.** This document previously implied the importer loses music here.
It does not: Encore renders those measures as whole rests too. Emitting the events as notes would
add music Encore does not display and would collide with the accompanying rests.

**What to change:** nothing behavioural. The drop should become a named, deliberate skip rather
than falling through the "unknown element" counter in `debug-dump.cpp`, and the type belongs in the
element table in `ENCORE_FORMAT.md`, where it now is.

## 11. Key signature diverges across conversion pairs  (RESOLVED, not a defect)

The differential oracle flagged `line_key` as differing in 6 of 399 pairs. All six turn out to be
files with a **tablature staff**, and in every one the differing entries sit on the tab staff and on
systems after the first. Only 6 of the 27 pairs that have a tab staff diverge at all.

Opened in Encore, both halves of a pair render identically: a tab staff draws fret numbers and no
key signature, so the byte is never rendered there and Encore does not keep it consistent across
saves `[verified]`.

It does not reach the importer either. The initial key signature comes from the first system's LINE
entries only, and later systems are never read for it; mid-score changes come from KEYCHANGE
elements. Importing both halves of two such pairs gives byte-identical key signatures.

Recorded in `ENCORE_FORMAT.md` under the LINE staff entry: on a tablature staff the key field means
nothing, so it should be read from the notation staff.

## 12 to 15. Remaining

- **Chord symbol text slot.** RESOLVED, and it was a defect. See gap 22 below.
- **Unknown version byte.** RESOLVED. `EncFormatReader::create` now picks the layout of the highest
  known format version at or below the file's own, and logs both numbers. The header carries no
  date and no build stamp, so the format version at `0x28` is the only thing to go on.
- **`SCOX` / `SCOR` / `SCOS`.** RESOLVED. Dropped from the magic table and reduced to one
  paragraph recording that they were looked for and not found. They occur in none of the 21620
  files, no byte order or layout was ever established, and no line of the importer acts on them, so
  a table row described nothing. The paragraph keeps the trace for anyone who does meet one.

  `ZBOP` and `ZBO6` are a different case and stay in the table: the importer branches on them, in
  the error message and in the decryption. They keep their `[assumed]` mark, with the reason the
  assumption is safe to hold, that a wrong keystream fails the header check and the file is
  rejected rather than imported as wrong music.
- **v0xA6 multi-measure rests.** RESOLVED, nothing to support. The compact rest states its own
  duration at `+12`, and across 806 files and 249589 rests the largest value is 960, one whole
  note, with the rest of the distribution being 120, 240, 480, 360, 180, 60, 720, 80, 30 and 90.
  Not one rest spans more than a single measure, and the 14 byte element has no room for a count.

## 18. The v0xA6 note and rest read three fields from the wrong place  (FIXED)

Found by the cross-generation oracle: `rest_tuplet` diverged in every v0xA6 pair, and the counts
gave it away, 98 zeros plus 6 ones plus 26 threes on one side against 130 zeros on the other. Same
rests, different reading.

The compact rest is 14 bytes and the compact note 20, and neither carries the fields the later
generations keep past `+12`:

| field | was read at | what is there |
|---|---|---|
| note staff position | `+12` | the first byte of the playback duration, a constant `0x80`, so every note reported position -128 |
| rest tuplet | `+13` | the high byte of the rest's own duration |
| rest dot control | `+14` | the first byte of the element behind it |

The real staff position is at `+9`, signed, counted in diatonic steps from middle C. Across 1.72
million notes each pitch falls on exactly one position and the alterations share their natural's,
which is what confirmed it.

62591 rests in 570 files carried a tuplet descriptor the file never stated. It turned out to be
**inert**: with values 1 and 3 the actual-notes nibble is 0 and no tuplet forms. The position
mattered more, since it feeds the percussion line mapping and the tablature fingering.

Verified by re-running the oracle: `rest_tuplet` went from 12 diverging pairs to 0, and
`note_position`, added to the census for this, diverges in exactly the 38 pairs where the pitches
also differ, with no v0xA6 pair among them.

## 19. Encore 4.0 renumbered four articulations  (FIXED)

Encore 4.0 moved tenuto, staccato and the two fermatas down by six, in the same release that
shifted every element body by two bytes. Files older than format 3.07 state them at the higher
codes, which the emitters dropped as unrecognised.

| format 3.05 and older | 3.07 and later | meaning |
|---|---|---|
| `0xCE` | `0xC8` | tenuto |
| `0xCF` | `0xC9` | staccato |
| `0xD2` | `0xCC` | fermata above |
| `0xD3` | `0xCD` | fermata below |

One conversion pair holds 26 ornaments in each half, identical one for one except `0xCE` against
`0xC8`: the accent, the breath, the tempo mark, the staff text and the slur all keep their codes,
so the vocabulary did not move as a block. Corpus-wide, 3216 files of format 3.05 contain not one
staccato at `0xC9`, the most common articulation in every other generation.

`0xC0`, `0xC1`, `0xC2` and `0xCA` are probably the same block shifted, which would make them the
fingerings and the up-bow, but no pair covers them and they are left as stated.

## 20. The v0xA6 note reads its neighbour's tick as data  (FIXED)

The compact note body ends at `+19`. The velocity, option and accidental-glyph slots the later
generations keep past that point fall on the following element: the option byte reads as its tick
low byte and the accidental byte as its high byte, which is why the accidental histogram took only
the values 0, 1, 2, 3 and 255, the tick pages of a measure plus the end marker.

All three are inert today, since nothing in the importer reads the velocity or the accidental
glyph. The option byte is the exception: the tablature fingering fallback tests its low bit
together with the staff position, and fixing gap 18 made that position plausible, so the two
together would have turned a neighbour's tick into a string number.

## 21. Heuristics measured against the corpus

With the body offsets following the generation, the five v0xC2 heuristics were instrumented and the
corpus run to see which still fire, split by format version (773 is 3.05, 775 is 3.07, 1056 is
4.20 with a `0xC2` version byte).

| heuristic | 3.05 | 3.07 | 4.20 | verdict |
|---|---|---|---|---|
| implied tuplet from a duration mismatch | 41095 | 11711 | 12 | keep, still the main source of tuplets in those files |
| dotted-eighth anomaly | 1198 | 504 | 2 | keep |
| `0xC4` remapped to an accent | 745 | 6 | 16 | **was a defect**, see below |
| pitch and tuplet slots swapped | 0 | 0 | 45 | keep, already scoped to the post-4.0 layout |
| old TEMPO layout | 0 | 0 | 3 | keep, three tempo marks depend on it |

None was dead, so nothing was removed for being unused. The distribution did expose one thing: the
`0xC4` remap fires almost only on format 3.05, which is the signature of the renumbering in gap 19
rather than of a heuristic. It is the fifth member of that block, `0xC4` before Encore 4.0 being
the accent that later releases spell `0xBE`, and it was being applied to every v0xC2 file
regardless of generation, so the 22 genuine up-bows in the newer two generations imported as
accents. It now lives with the other four, scoped by format version.

The fixture behind the original test was stamped format 4.20, the generation in which `0xC4` is an
up-bow, so it asserted the opposite of what it claimed. It carries its own generation now.

## 22. Chord symbol names run into the element behind them  (FIXED)

`EncChordSym::read` read a fixed 36 byte text slot at `+14`, which needs a 50 byte element. The
corpus says the slot is not fixed at all: chord symbol sizes run 14, 16, 18 and up to 54, two bytes
at a time, because the element grows with the name. Only 14 elements in 21620 files are big enough
for the fixed read.

Instrumenting the parser and running the corpus: of 4221 chord symbols carrying an explicit text,
**1098 import with a name that continues past the element**, for example `C䔯bːṀč` where the score
shows `C`. The remainder are saved by luck, their neighbour's first byte happening to be zero.

The fix reads to the element end instead, `size - (14 + bodyShift)`, which is what the size steps
say the slot is. The regression test needed the symbol placed on beat 2: on beat 1 the element
behind it is a note at tick 0, whose first byte terminates the string by accident, so the defect
does not show.

## 23. The two version axes agree in real files

The reader is chosen by the version byte at `0x04` while the element geometry follows the format
version at `0x28`, so it is worth knowing whether the two ever disagree. Across the corpus:

| version byte | format | files | element base size |
|---|---|---|---|
| `0xA6` | 2.50 | 786 | 10 |
| `0xC2` | 3.05 | 3220 | 22 |
| `0xC2` | 3.07 | 1718 | 24 |
| `0xC4` | 3.07 | 996 | 24 |
| `0xC4` | 4.20 | 14633 | 28 |
| SCO5 | 4.20 | 16 | 28 |

The note size ladder confirms that the body geometry belongs to the format version and not to the
version byte: format 3.07 gives a 24 byte note whether the file says `0xC2` or `0xC4`.

**One combination genuinely crosses the two axes**, `0xC4` with format 3.07, 996 files. Each
behaviour the two readers disagree about was checked against those files rather than assumed:

| behaviour | verdict |
|---|---|
| element body shift | correct: 3.07 and 4.20 share the shifted layout, and both readers return 0 |
| lyric text offset | correct, and it follows the **version byte**: 99.2% of 26043 syllables read as text at the v0xC4 offset, against 0.3% at the v0xC2 one |
| MIDI program from the entry end | correct: forcing the v0xC2 value on these files raises the share of unassigned programs from 43.1% to 44.4%, so it is not better |

Nothing is misaligned in real files. The only crossed pairs left are in the fixtures, see gaps 5 to
7 above.

## 16. Corpus hygiene, not a defect

160 files carry a `.enc` extension but are not Encore documents: 136 ZIP archives (multi-megabyte,
all under `downloads/brasilsonoro`), 15 empty files, 7 with magic `00 10 00 01`, one JPEG, one text
file. The parser rejects all of them. The ZIPs presumably need extracting before they contribute
anything. The 7 files with magic `00 10 00 01` are worth one look in case they are an unrecognised
Encore variant.

---

## Differential oracle

`~/Scores/downloads/bandurriator` holds **399 matched pairs**: an Encore 4.3 encrypted original in
`backup_zbot/` and the same score converted to Encore 5.0.2 in the parent directory. Measures,
instruments and systems are identical in all 399 pairs, so any difference in what the importer
reads is a defect in one of the two readings.

| invariant | pairs | differ | |
|-----------|-------|--------|--|
| note face values | 399 | 0 | 0.0% |
| note tuplets | 399 | 0 | 0.0% |
| rest face values | 396 | 0 | 0.0% |
| clefs | 399 | 0 | 0.0% |
| time signatures | 399 | 0 | 0.0% |
| staff types | 399 | 0 | 0.0% |
| tab tunings | 399 | 0 | 0.0% |
| key changes | 144 | 0 | 0.0% |
| tempo marks | 82 | 0 | 0.0% |
| articulations | 27 | 0 | 0.0% |
| clef changes | 10 | 0 | 0.0% |
| ornament subtypes | 387 | 1 | 0.3% |
| staff text indices | 336 | 1 | 0.3% |
| element types | 399 | 1 | 0.3% |
| **LINE key** | **399** | **6** | **1.5%** |

Thirteen musical invariants are identical across all 399 pairs. That is strong positive validation
of the v0xC4 reading across revisions 0, 1 and 4, and of the ZBOT decryption at scale. The single
ornament, staff-text and element-type difference is one file (`Kalinka Orquesta Plectro.enc`) that
differs by one element, most likely an edit between saves rather than a reading defect.

The one systematic signal is `line_key`, gap 11 above.

**This is the harness worth keeping.** It answers "did we leave something out this time" without
needing a ground-truth score: run the oracle before and after any importer change, and any
invariant that moves is a regression.

### The cross-generation run

The 399 pairs above are one generation compared with itself. Widening the corpus to pairs that
cross generations, 104 distinct pairs after deduplicating by file name, produced three more
defects: 18, 19 and 20 below. Two lessons about the measurement itself came with them.

**Pair on the music, not on the name.** Of the 104 pairs only **64 are true twins**, meaning their
pitch multisets are identical. The other 40 are different arrangements of the same piece, or the
same piece transposed, and they were the source of nearly every false signal: the whole
`keychange_type` signal turned out to be pairs whose pitches also differ. Comparing anything else
before checking the pitches wastes the run.

**Three classes of divergence are not defects**, and comparing them measures the wrong thing:

| class | invariants | why |
|---|---|---|
| the format does not have the field | `line_clef`, `line_stafftype`, `line_key`, `tab_strings` (v0xA6 has no `staffData`), and the note fields the compact v0xA6 body does not carry, which the importer now deliberately zeroes | an empty histogram against a populated one |
| Encore rewrites it on save | `meas_barend` (a final barline appears in the newer file in 32 of 64 twins, including v0xC4 to v0xC4), `wini_present`, `prec_scale`, `prec_papersize`, `line_staffsize`, `line_hidden`, `instr_midiprog`, `instr_nstaves`, `instr_keytranspose` | the file changed, not the reading |
| the field's meaning depends on the subtype | `orn_almezuro`, `orn_noto`, `orn_speguleco`, `orn_altmezuro` | comparing a slur's measure count against a tempo mark's beat unit. The subtype-scoped versions (`slur_almezuro`, `slur_end_in_range`) diverge in 2 twins out of 64 |

What survives all three filters is small and each item is explained: the residual `orn_subtype` and
`elem_type` differences are four files that lost a slur or ten staff texts between saves, which the
matching `elem_type` counts confirm.

---

## VM experiments

Encore 4.5 and 5.0.2 are available by hand in a VM. Neither writes v0xA6 or v0xC2, so **conversion
saves are the high-value experiments**. Two of the originally planned experiments are now
unnecessary: the bandurriator corpus already supplies 399 Encore 4.3 to 5.0.2 pairs, which covers
the rev-0/1 against rev-4 question, and the ZBOT decryption is validated at scale by those pairs.

What remains, ranked.

### A. v0xA6 to v0xC4 conversion: DONE

`SILENT.enc` opened in Encore 4.5 and saved as `SILENT-45.enc`, both in `~/Scores/demos/2.5/`.
Encore 4.5 asked twice to substitute a missing font (TimesNewRomanPS) and accepted the file. Note
that Encore 4.5 wrote **format revision 0**, which is further evidence that `0x3E` is not a build
stamp.

Results in gap 3 above. The v0xA6 tie arc fields could not be located from it: like the ornament x
coordinates, the tie arc x does not survive the unit change across the conversion.

### B. Encore 3.x to Encore 4.5 conversion: DONE

`SALVEDOL.ENC` was opened in Encore 4.5, which showed a legacy-conversion dialog: score titles are
no longer stored now that Windows supports long filenames, so the title would be lost and the file
must be renamed. Pressing **Rename** renamed it to `Salve Dolorosa.ENC` without converting; a
later explicit save produced the v0xC4 version in place (app 1056, revision 0, 25392 bytes against
the original 20332).

The Encore 3.x original survives as `~/Downloads/SALVEDOL.ENC`. The staged pair is in the session
scratchpad as `SALVE-orig.ENC` and `SALVE-45.enc`.

Results in gap 0 above. The dialog is itself a format fact: before Encore 4.5 the score title
doubled as the document name `[external]`.

Element type `0xA` did not appear in this file, so it remains unidentified. A conversion of an
Encore 4.0-4.2 file that does contain it would settle that.

### C. Key signature check (closes gap 11)

Open both halves of one diverging pair, for example `Vals sobre las olas.Bandurria 1.enc` from
`backup_zbot/` and from the parent directory, and read the key signatures off the score in Encore.
That decides between "we misalign staff entries" and "Encore rewrites the key on re-save".

### D. Tie probe (gives gap 1 and 4 their fixtures)

One score with four ties: one short, one long, one on a chord, one across a barline. Save from
Encore 5.0.2. If a macOS Encore is available, save it there too for the first real SCO5 fixture.
