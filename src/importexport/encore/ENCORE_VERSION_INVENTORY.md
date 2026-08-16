# Encore version and container inventory

Derived from a census of **20577 real `.enc` files** (everything reachable under `~/Scores`,
following symlinks) and 425 test fixtures, parsed with the production parser via the
`Census.walk_directory` target in `src/importexport/encore/tests/tst_census.cpp`.

A further 160 files carry a `.enc` extension but are not Encore documents: 136 ZIP archives, 15
empty files, 7 with an unrecognised magic (`00 10 00 01`), one JPEG and one text file. The parser
rejects all of them, correctly.

Provenance tags follow `ENCORE_FORMAT.md`: `[verified]` round-tripped or proven by byte diff,
`[observed]` consistent across the corpus, `[assumed]` plausible but unproven, `[external]`
sourced from public release history or from the file author, not from bytes.

---

## 1. The format has three axes, the importer reads one

`EncFormatReader::create(chuMagio, magic)` (`internal/parser/readers.cpp:75`) selects a reader from
the version byte at file offset `0x04`, with `SCO5` matched by magic. The corpus shows three
independent coordinates:

| Axis | Where | What it actually tracks | Read by the importer |
|------|-------|--------------------------|----------------------|
| container magic | `0x00` | plaintext vs encrypted, byte order | yes (SCOW / SCO5 only) |
| version byte | `0x04` | **ornament vocabulary generation** | yes, sole reader selector |
| app version | `0x28` | **element size generation** | no |
| format revision | `0x3E` | see section 4, not a layout selector | no |

The version byte and the app version are not redundant. Both `0xC2` and `0xC4` occur with app 775,
and the two groups differ in ornament vocabulary while sharing element sizes. Choosing the reader
by the version byte is therefore correct; the gap is that nothing reads the app version, which is
what actually predicts element layout.

---

## 2. Observed combinations

| magic | ver `0x04` | app `0x28` | rev `0x3E` | reader used | corpus | fixtures |
|-------|-----------|-----------|-----------|-------------|--------|----------|
| SCOW  | `0xC4`    | 1056      | 4         | v0xC4       | 4879   | 340      |
| ZBOT  | `0xC4`    | 1056      | 1         | v0xC4       | 4253   | **0**    |
| SCOW  | `0xC4`    | 1056      | 1         | v0xC4       | 3155   | **0**    |
| SCOW  | `0xC2`    | 773       | 1         | v0xC2       | 2744   | **0**    |
| SCOW  | `0xC2`    | 775       | 1         | v0xC2       | 1298   | **0**    |
| ZBOT  | `0xC4`    | 1056      | 0         | v0xC4       | 994    | **0**    |
| SCOW  | `0xC4`    | 775       | 1         | v0xC4       | 731    | 2        |
| SCOW  | `0xA6`    | 592       | 1         | v0xA6       | 709    | **0**    |
| SCOW  | `0xC4`    | 1056      | 0         | v0xC4       | 516    | 24       |
| SCOW  | `0xC2`    | 773       | 0         | v0xC2       | 467    | **0**    |
| SCOW  | `0xC2`    | 775       | 0         | v0xC2       | 393    | **0**    |
| SCOW  | `0xC4`    | 775       | 0         | v0xC4       | 243    | **0**    |
| SCOW  | `0xA6`    | 592       | 0         | v0xA6       | 75     | 7        |
| SCOW  | `0xC2`    | 1056      | 4         | v0xC2       | 59     | 30       |
| SCOW  | `0xA6`    | 1056      | 4         | v0xA6       | 22     | 11       |
| SCO5  | none      | 1056      | 2         | SCO5        | 11     | **0**    |
| SCOW  | `0xC2`    | 1056      | 0         | v0xC2       | 10     | 4        |
| SCOW  | `0xC4`    | 775       | 4         | v0xC4       | 4      | 2        |
| ZBOT  | `0xC4`    | 1056      | 4         | v0xC4       | 4      | 2        |
| ZBOT  | `0xC2`    | 773       | 1         | v0xC2       | 4      | **0**    |
| SCO5  | none      | 1056      | 0         | SCO5        | 3      | 2        |
| SCOW  | `0xC4`    | 0         | 0         | v0xC4       | 2      | 1        |
| SCO5  | none      | 1056      | 1         | SCO5        | 1      | **0**    |

**14272 of 20577 files (69%) sit in a header combination that has no fixture at all.**

Every fixture header combination does occur in real files. There are no synthetic-only headers.

Per reader, the fixture mix is skewed away from the corpus mix:

| reader | corpus | share | fixtures | share |
|--------|--------|-------|----------|-------|
| v0xC4  | 14781  | 71.8% | 371      | 87.3% |
| v0xC2  | 4975   | 24.2% | 34       | 8.0%  |
| v0xA6  | 806    | 3.9%  | 18       | 4.2%  |
| SCO5   | 15     | 0.1%  | 2        | 0.5%  |

Encrypted containers are **5255 files, 25.5% of the corpus**, against 2 fixtures.

---

## 3. Corrections to `ENCORE_FORMAT.md`

| Claim in the spec | Corpus says |
|-------------------|-------------|
| app 775 maps to v0xC2 | app 775 occurs with version byte `0xC2` (1691 files) **and** `0xC4` (978 files) `[observed]` |
| app 1056 maps to v0xC4 | 69 files are `0xC2` with app 1056, and 22 are `0xA6` with app 1056 `[observed]` |
| revision `0x3E` is 0, 1 or 4 | revision **2** occurs, on 11 of the 15 SCO5 files `[observed]` |
| `0x3E` is constant for a given Encore build | **false**, see section 4 |
| ZBOT is an Encore 4.x v0xC4 container | mostly, but 4 ZBOT files decrypt to v0xC2 app 773 `[observed]` |
| instrument entry sizes are 2158, 242, 112 | v0xA6 uses stride **64**, and v0xC4 files of the app-775 generation use **112** `[observed]` |
| `SCOX` / `SCOR` / `SCOS` are observed variants | none occur in 20577 files, and `EncHeader::readMagicAndVersion` rejects them outright, so they cannot import today |
| element type 10 occurs only in Encore 4.0-4.2 | mostly (1092 of 1174 elements), but 82 occur in the 4.3-5.x generation `[observed]` |

---

## 4. What `0x3E` is not

The spec calls `0x3E` a format-revision counter, "constant for a given Encore build regardless of
score content", mapping 0 to pre-4.5 and 1 to Encore 4.5. The corpus refutes this.

One author states he authored his scores in **Encore 4.3** `[external]`. His 399 encrypted
originals split 153 at revision 0 and 246 at revision 1, from the same person and the same build.
Comparing the two groups: identical instrument entry stride (242), identical element size classes,
identical ornament vocabulary, same distribution of measures, instruments, systems and staff sizes.
**Revision 0 and revision 1 are the same format** `[verified]`.

What `0x3E` does track is unresolved. It is not a layout selector, so nothing should branch on it.

The same corpus moves the app-version boundary: app 1056 begins at Encore **4.3**, not 4.5
`[external]`.

---

## 5. Release mapping

Public release history `[external]`: Encore was created by Don Williams and first released in 1984
by Passport Designs. GVOX acquired Passport's intellectual property in 1998. Encore 4.5 was
announced in November 2001. Encore 5 followed roughly ten years after Encore 4, around 2008.
Passport Music Software LLC took over in August 2013, shipping 5.0.4 on Windows and 5.0.7 on
macOS. Sonic Scores acquired the rights in June 2022 and has Encore 6 in development.

| Release | Platform | magic | ver `0x04` | app `0x28` | rev `0x3E` | corpus | Tag |
|---------|----------|-------|-----------|-----------|-----------|--------|-----|
| Encore 2.x | Windows | SCOW | `0xA6` | 592 | 0, 1 | 784 | `[verified]` |
| Encore 3.x | Windows | SCOW | `0xC2` | 773 | 0, 1 | 3211 | `[verified]` |
| Encore 4.0 to 4.2, early saves | Windows | SCOW | `0xC2` | 775 | 0, 1 | 1691 | `[observed]` |
| Encore 4.0 to 4.2, later saves | Windows | SCOW | `0xC4` | 775 | 0, 1 | 974 | `[observed]` |
| Encore 4.3, encrypted | Windows | ZBOT | `0xC4` | 1056 | 0, 1 | 5247 | `[external]` |
| Encore 4.3 to 4.5 | Windows | SCOW | `0xC4` | 1056 | 0, 1 | 3671 | `[observed]` |
| Encore 5.x | Windows | SCOW | `0xC4` | 1056 | 4 | 4879 | `[verified]` |
| Encore 5.x | macOS | SCO5 | none | 1056 | 0, 1, 2 | 15 | `[observed]` |
| Encore 1.x | Macintosh | unknown | unknown | unknown | unknown | 0 | `[assumed]` |
| Encore 6 | both | unknown | unknown | unknown | unknown | 0 | not yet seen |

Notes:

- The app-775 line splits on the version byte, not the app version. The two groups share every
  element size but use disjoint articulation subtypes: the `0xC2` group encodes accent as `0xC4`,
  the `0xC4` group as `0xBE`, with no overlap `[observed]`. Encore 4.0 to 4.2 therefore renumbered
  its ornament subtypes mid-line while keeping the same app version stamp.
- The 69 files carrying version byte `0xC2` with app 1056, and the 22 carrying `0xA6` with app
  1056, are unexplained. An Encore 5 build saving in a legacy format is the obvious hypothesis and
  is untested.
- No file in the corpus predates Encore 2.x. Whether Encore 1.x for Macintosh wrote `.enc` at all,
  and what magic it used, is untested. `SCOX` / `SCOR` / `SCOS` are the natural suspects but no
  sample exists in 20577 files.
- Encore 6 has not shipped a file into this corpus. When it does, the unknown-version fallback in
  `EncFormatReader::create` will parse it as v0xC4 and emit only a `LOGW`.

---

## 6. The conversion-pair corpus

`~/Scores/downloads/bandurriator` holds **399 matched pairs**: an Encore 4.3 encrypted original in
`backup_zbot/` and the same score converted to Encore 5.0.2 in the parent directory.

Measures, instruments and systems are identical in all 399 pairs, so the pairs are a valid
differential oracle: the music is the same, therefore any difference in what the importer reads is
a defect in one of the two readings. Results are in
[ENCORE_COVERAGE_GAPS.md](ENCORE_COVERAGE_GAPS.md) section "Differential oracle".

The only systematic byte-level change across the conversion is the lyric element growing (sizes 24
and 26 become 30 to 56), consistent with Encore 5 writing lyric text in a wider encoding
`[observed]`. The ornament vocabulary is byte-identical: zero subtypes gained or lost across 399
pairs.

---

## 7. Reproducing this

```
cmake --build builds/Mac-Qtopt-homebrew-Make-Debug --target iex_encore_tests
cd builds/Mac-Qtopt-homebrew-Make-Debug/src/importexport/encore/tests
ENC_CENSUS_DIR=~/Scores ENC_CENSUS_OUT=/tmp/corpus ./iex_encore_tests --gtest_filter='Census.*'
ENC_CENSUS_DIR=src/importexport/encore/tests/data ENC_CENSUS_OUT=/tmp/fixtures ./iex_encore_tests --gtest_filter='Census.*'
```

The full corpus takes about five minutes. Two CSVs per run: `-files.csv` (one scalar row per file)
and `-hist.csv` (long-form `file,kind,key,count`). File keys are paths relative to the scan root,
so an original and its conversion stay distinct rows.

The census follows symlinks. `~/Scores/downloads` is one, which is why an earlier scan saw 347
files instead of 20737.

**Encrypted containers need the branch that carries the decryption.** The census picks it up when
present and otherwise records those files as encrypted without parsing them, so the numbers for the
5255 encrypted files in this document reproduce only where that code exists. Everything else
reproduces anywhere.

The census reports the element list **after** parsing, so it includes the spanner stops synthesized
by `addSpannerEnds` and excludes elements dropped by `deduplicateRest`. For raw byte questions,
probe the file directly.
