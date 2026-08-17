# Encore format divergence matrix

How the four format readers differ, derived from the code rather than assumed, and checked against
the corpus with `Census.walk_directory`.

Companion documents: [ENCORE_VERSION_INVENTORY.md](ENCORE_VERSION_INVENTORY.md) for which versions
exist, [ENCORE_FORMAT_EVOLUTION.md](ENCORE_FORMAT_EVOLUTION.md) for why they differ,
[ENCORE_COVERAGE_GAPS.md](ENCORE_COVERAGE_GAPS.md) for the ranked backlog.

---

## 1. The mechanism that produces silent gaps

`EncNote::read`, `EncOrnament::read`, `EncTie::read` and `EncChordSym::read`
(`internal/parser/parsers-note.cpp`, `parsers-ornament.cpp`, `parsers-lyric-tie.cpp`,
`parsers-chord.cpp`) read **one fixed field layout, the v0xC4 one, for every format**. Per format
correction happens afterwards, in `EncFormatReader::postProcessElement` and in the offset virtuals
declared in `readers.h`.

The consequence is structural, and it is the reason gaps keep surfacing one bug report at a time:

> Any field that is not explicitly remapped silently reads a v0xC4 offset on every other format.
> Nothing warns. The value is simply whatever byte happens to sit there.

So the finite question is: for each field, in each format, is the offset the same, remapped, or
neither. That is the matrix below.

---

## 2. Reference layout: what the readers actually read

Offsets are element relative, `+0` being the first tick byte. `EncMeasureElem::read` consumes
`+3` (size) and `+4` (staff), so every body starts at `+5`.

### `EncNote::read`

| Offset | Field | Offset | Field |
|--------|-------|--------|-------|
| `+5`  | faceValue | `+16..17` | playbackDurTicks |
| `+6`  | grace1    | `+19` | velocity |
| `+7`  | grace2    | `+20` | options |
| `+10` | xoffset   | `+21` | alterationGlyph |
| `+12` | position  | `+24` | articulationUp |
| `+13` | tuplet    | `+26` | articulationDown |
| `+14` | dotControl | | |
| `+15` | semiTonePitch | | |

### `EncOrnament::read`

| Offset | Field | Offset | Field |
|--------|-------|--------|-------|
| `+5`  | tipo (subtype) | `+20` | xoffset2 (end x) |
| `+10` | xoffset        | `+26` | speguleco (hairpin direction) |
| `+12..13` | yoffset (signed) | `+28` | noto (tempo beat unit) |
| `+16` | altMezuro      | `+30` | tempo (BPM) |
| `+18` | alMezuro (forward measure count) | `+32` | tind, only when size >= 33, else `+30` |

### `EncTie::read`

`+5` dirByte (needs size > 5), `+6` startFlag (needs size > 6), then `+10` arcX1, `+12` arcX2,
`+14` sourcePosition, **all three only when size >= 18**.

### `EncChordSym::read`

`+5` toniko, `+6` tipo, `+10` xoffset, `+12` radiko, `+13` baso, then a **fixed 36 byte** text slot
at `+14` when `tipo & 1`, so it reads through `+50` regardless of the element size.

---

## 3. What each reader remaps

`EncFormatReader` declares 24 virtuals. Which ones each reader overrides:

| Virtual | v0xA6 | v0xC2 | v0xC4 | SCO5 |
|---------|-------|-------|-------|------|
| `headerEnd` | `0xA6` | default `0xC2` | default | default |
| `elemBlockOffset` | `0x1A` | `0x36` (base) | `0x36` (base) | `0x36` |
| `elemSpacing` | `size * 2` | default `size` | default | default |
| `scoreSizeOffset` | `0x8D` | default `0x52` | default | default |
| `isMeasureNearEnd` | yes | default false | default | default |
| `deduplicateRest` | yes | default false | default | default |
| `probeInstrumentEncoding` | false | true (base) | true (base) | true |
| `readInstrumentMeta` | yes | yes | yes (base) | base |
| `midiProgramFromEntryEnd` | default 46 | **44** | default 46 | default 46 |
| `readKeyFromTKBlock` | yes | no | no | no |
| `readLineStaffKeys` | yes | no | no | no |
| `hasGraceTimeBorrowing` | true | false | false | false |
| `clustersChordsByXoffset` | **false** | true (base) | true (base) | true |
| `slurXoffset2Stale` | false | **true** | false | false |
| `usesUniformPageMargins` | false | false | false | **true** |
| `lyricPreKieSkip` | **0** | default 5 | default 5 | default 5 |
| `lyricTextGapAfterKie` | **0** | **7** | default 9 | default 9 |
| `textBlockEntryTextOffset` | **0** | default 14 | default 14 | default 14 |
| `textBlockEntryHasRunHeader` | **false** | default true | default true | default true |
| `staffTextTindOffset` | **26** | default -1 | default -1 | default -1 |
| `staffTextYoffsetOffset` | **6** | default -1 | default -1 | default -1 |
| `postProcessElement` | note only | ornament + note | note only | inherits v0xC4 |
| `postProcessVoiceGroup` | inner graces | dotted eighth, implied tuplets | none | none |

`postProcessElement` coverage in detail:

- **v0xA6**: note pitch `+11`, note tuplet `+7`, articulation `+18` for size 11, zero articulations
  below size 27. **No ornament handling at all.**
- **v0xC2**: ornament accent `0xC4` to `0xBE`, slur count from `altMezuro`, tempo layout
  discrimination; note pitch/tuplet swap, tie-sender nibble, articulation
  `+22` for size 24.
- **v0xC4**: zero articulations below size 27.
- **SCO5**: inherits v0xC4 unchanged.

---

## 4. Element sizes actually observed, per release generation

From the corpus (20577 parsed files). The top three sizes per element type. This is the table that
makes the divergences legible. The file counts below are from an early 262-file sample; the size
classes themselves are unchanged on the full corpus.

| element | 2.x (app 592) | 3.x (app 773) | 4.0-4.2 `0xC2` | 4.0-4.2 `0xC4` | 4.5 / 5.x (app 1056) | SCO5 |
|---------|---------------|---------------|-----------------|-----------------|----------------------|------|
| files   | 9 | 16 | 9 | 13 | 208 | 6 |
| clef    | . | . | . | 16 | 16 | . |
| keysig  | 5 | 12 | 14 | 14 | 14 | 14 |
| tie     | 7 | 16 | 18 | 18 | 18 | 18 |
| beam    | 9, 14, 19 | 28, 44, 76 | 30, 46, 62 | 30, 46, 62 | 30, 46, 62 | 30, 46, 62 |
| ornament| 5, 12, 15 | 14, 26, 32 | 16, 28, 34 | 16, 28, 34 | 16, 28, 86 | 16, 28, 78 |
| lyric   | 5, 6, 7 | 20, 22, 24 | . | 22, 24, 26 | 22, 24, 26 | 22, 24, 26 |
| chordsym| . | . | . | . | 14, 16, 18 | 16 |
| rest    | 7 | 16 | 18, 20 | 18, 26 | 18, 20, 26 | 18 |
| note    | 10, 11 | 22, 24 | 24, 26 | 24, 26 | 18, 28 | 28 |
| type 0xA| . | . | 24 | . | . | . |
| midi cc | 4 | 10 | . | 12 | 12 | . |

Two facts jump out.

**Every element grew by exactly two bytes between Encore 3.x and Encore 4.x.** keysig 12 to 14, tie
16 to 18, beam 28 to 30 and 44 to 46, ornament 14 to 16 and 26 to 28 and 32 to 34, lyric 20 to 22,
rest 16 to 18, note 22 to 24, MIDI CC 10 to 12. That is one coherent format change, not a set of
per element quirks `[observed]`.

**The version byte and the app version split different things.** The two app-775 groups have
identical element sizes but different articulation subtypes (section 5), so the version byte tracks
the ornament vocabulary while the app version tracks element sizes.

---

## 5. Confirmed divergences, and one confirmed defect

### 5.1 The version byte selects the correct reader `[verified]`

Raw subtype byte at `+5` of every ornament, counting the articulation marks:

| bucket | files sampled | `0xBE` accent (4.x era) | `0xC4` accent (3.x era) or up-bow |
|--------|---------------|------------------------|------------------------------------|
| app 773, ver `0xC2` | 16 | 0 | 18 |
| app 775, ver `0xC2` | 9  | 0 | 1 |
| app 775, ver `0xC4` | 13 | 40 | 0 |
| app 1056, ver `0xC4` | 208 | 698 | 125 (up-bow) |

Corroborated independently on the full corpus: across 399 Encore 4.3 to 5.0.2 conversion pairs,
**zero ornament subtypes are gained or lost**, so the vocabulary is stable once past the app-775
renumbering.

The split is exactly on the version byte, inside the same app version. The single-axis dispatch is
correct on this axis. Suspicion closed.

### 5.2 Note pitch slot is self-calibrating `[verified]`

Raw probe of bytes `+11`, `+13`, `+15` on every note, measuring the share of values inside MIDI
24 to 100:

| bucket, note size | `+11` | `+13` | `+15` |
|-------------------|-------|-------|-------|
| app 773, size 22  | 2%  | **100%** | 0% |
| app 775, size 24 (both version bytes) | 0% | 3 to 9% | **100%** |
| app 1056, size 28 | 0% | 5% | **100%** |

The pitch slot follows the element size, which is exactly what the v0xC2 reader's existing swap
(`if tuplet > 0 && semiTonePitch < 12`) keys off. Correct as written. Suspicion closed.

### 5.3 CONFIRMED DEFECT: the v0xC2 slur forward measure count is read from a fixed offset

`EncFormatReader_V0xC2::postProcessElement` does `orn->alMezuro = orn->altMezuro` unconditionally,
that is, it always takes the count from `+16`. The corpus says the offset moved with the same +2
shift as everything else:

| bucket | slur size | count at `+16` lands in score | count at `+18` lands in score |
|--------|-----------|-------------------------------|-------------------------------|
| app 773, ver `0xC2` (Encore 3.x) | 26 | **100%** (values 0, 1, 3) | 58% |
| app 775, ver `0xC2` (Encore 4.x) | 28 | 68% (values include 255) | **100%** (values 0, 1, 2) |
| app 775, ver `0xC4` | 28 | 77% | **100%** |
| app 1056, ver `0xC4` | 28 | 65% | **100%** |
| SCO5 | 28 | 85% | **100%** (all zero) |

So the v0xC2 reader reads `+16` for Encore 4.x `0xC2` files, where the real field is at `+18`. The
full corpus separates the two cases perfectly, same reader, same code path:

| generation | slurs | endpoint lost |
|------------|-------|---------------|
| Encore 3.x (app 773), slur size 26 | 2762 | **0, 0.0%** |
| Encore 4.0-4.2 `0xC2` (app 775), slur size 28 | 6663 | **2172, 32.6%** |
| `0xC2` with app 1056 | 28 | 13, 46.4% |

The ~4500 app-775 slurs that do land in range are not thereby correct: they read the wrong byte and
happen to fall inside the score.

The discriminator needs no version lookup. **Slur ornament size 26 means the count is at `+16`,
size 28 means `+18`**, which is the same +2 shift as every other element. The existing spec note
("unreliable for a whole file when any slur's `+16` points past the last measure") is a symptom
level workaround for this root cause.

### 5.4 CONFIRMED DEFECT, NOW RESOLVED: v0xA6 ornaments are never remapped

**Update.** A v0xA6 to v0xC4 conversion in Encore 4.5 has since located the two fields: the
ornament y is a **signed byte at `+9`** (not an s16 at `+8`, and it applies to every subtype, not
only STAFFTEXT), and the forward measure count is at **`+14`** (not `+18`). Read at `+14`, all 636
v0xA6 spanner starts in the corpus land inside their score, against 105 out of range at `+18`. The
same experiment validated the v0xA6 note reader as correct. Details in
[ENCORE_COVERAGE_GAPS.md](ENCORE_COVERAGE_GAPS.md) gap 3. The analysis below is what pointed at it.


The v0xA6 ornament is compact: sizes 5, 12 and 15, in a slot of `size * 2`. The v0xA6 reader
overrides only `staffTextTindOffset` (`+28`) and `staffTextYoffsetOffset` (`+8`), and both apply
**only to STAFFTEXT**. Every other ornament field keeps its v0xC4 offset:

| field | offset read | inside a size-12 slur? | remapped for v0xA6 |
|-------|-------------|------------------------|--------------------|
| yoffset `+12` | dynamics placement, standalone fermata above vs below (`0xCC` and `0xCD` differ only in y sign) | no | only for STAFFTEXT |
| altMezuro `+16` | v0xC2 slur span | no | no |
| alMezuro `+18` | forward measure count | no | no |
| xoffset2 `+20` | spanner end x | no | no |
| speguleco `+26` | hairpin crescendo vs diminuendo | no | no |
| noto `+28`, tempo `+30` | tempo beat unit and BPM | no | no |

Measured consequence: **39 of the 61 v0xA6 slurs in the corpus lose their endpoint, 64%**, against
0.1% for the v0xC4 generation and 0.0% for SCO5. The v0xA6 corpus contains no hairpins or tempo
ornaments, so those paths are unexercised rather than proven wrong, but they read the same
out-of-element bytes.

### 5.5 Size thresholds that no format guards

| Guard | Where | Sizes observed that fall on the wrong side |
|-------|-------|--------------------------------------------|
| `size >= 18` for tie arcX / arcX2 / sourcePosition | `EncTie::read` | **124131 ties fall through**: size 16 (84359, Encore 3.x) and size 7 (39772, v0xA6) get direction from `+5` and `+6` only |
| `size > 15` for the multi-measure rest count | `EncRest::read` | v0xA6 rest size 7, so **v0xA6 has no multi-measure rest support at all** |
| `size == 24` for the v0xC2 articulation at `+22` | v0xC2 `postProcessElement` | v0xC2 note sizes 26 and 28 get articulations zeroed; also `+22` is zero in all sampled app-775 size-24 notes, so the rule is a no-op on this corpus |
| `size < 27` zeroes articulations | v0xC4 and v0xA6 | v0xC4 note sizes 18 and 24. Verified harmless: byte `+22` is zero in all 19606 sampled size-24 notes |
| fixed 36 byte text slot at `+14` | `EncChordSym::read` | chord symbol sizes 14, 16, 18, 20, 22 all read past the element end when the text bit is set |
| `size >= 33` for the ornament text index | `EncOrnament::read` | correct for every observed size. Staff-text index landed in range in 100% of corpus files, all formats |

### 5.6 Element type 10 is dropped

Type `0xA`, size 24, occurs 1174 times. 1092 of those are in Encore 4.0-4.2 files, under both
version bytes, and 82 in the 4.3-5.x generation. It is absent from Encore 3.x and from v0xA6, so it
is essentially an artefact of the 4.0-4.2 line that occasionally survives a later re-save. The
importer drops it and `debug-dump.cpp` counts it as "Unknown elements (type 0xA, dropped)".

### 5.7 Container handling is narrower than the spec

`EncHeader::readMagicAndVersion` (`internal/parser/parsers-text.cpp:40`) accepts `SCOW` and `SCO5`
only and returns false otherwise. `SCOX`, `SCOR` and `SCOS`, listed in the spec as observed
variants, therefore cannot import, and `encoreLoadErrorMessage` reports them as "not a recognized
Encore file" rather than as an unsupported variant. None of the three occurs in 20577 files.
`isZbotMagic` treats `ZBOP` and `ZBO6` as sharing the `ZBOT` keystream; neither appears in the
corpus either, so that is untested. Plain `ZBOT` is validated at scale: 5255 encrypted files
decrypt and parse, and 399 of them match an Encore 5.0.2 conversion of the same score across
thirteen musical invariants.

`EncFormatReader::create` falls back to the v0xC4 reader for any unknown version byte with only a
`LOGW`. When Encore 6 ships a new version byte, that is the path it will take.

---

## 6. Open questions

Each needs bytes we do not have, or an experiment. See
[ENCORE_COVERAGE_GAPS.md](ENCORE_COVERAGE_GAPS.md) section 4 for the recipes.

1. Where does v0xA6 keep the ornament y, the spanner endpoint, the hairpin direction and the tempo
   fields, given the compact layout? The staff-text overrides prove the compact ornament carries
   real data past its declared size, so these fields exist somewhere in the 30 byte slot.
2. Does v0xA6 store multi-measure rests, and where?
3. Why do all 47 SCO5 slurs have a forward measure count of exactly 0, and all 47 end-x values of
   0? Either macOS Encore never writes multi-measure slurs, or it keeps the endpoint elsewhere.
4. What is element type `0xA` in Encore 4.0 to 4.2?
5. Is the chord-symbol text slot really a fixed 36 bytes, or does it end at the element boundary?
6. Do `ZBOP` and `ZBO6` really share the `ZBOT` keystream?
