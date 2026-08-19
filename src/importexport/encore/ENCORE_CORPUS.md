# What the corpus says about Encore's files

This is the forensic half of the Encore documentation: not what the bytes mean, which is
[ENCORE_FORMAT.md](ENCORE_FORMAT.md), and not what the importer does with them, which is
[ENCORE_IMPORTER.md](ENCORE_IMPORTER.md), but what a large pile of real files shows about which
programs wrote them, when, and in what shape. Everything here is counted rather than assumed, and
every count is reproducible from the recipe in the last chapter.

The census reads **27824 distinct files** with an `.enc` or `.mus` extension, everything under
`~/Scores` including the download archive, deduplicated by inode because the archive is a web of
symlinks into one blob store. Of those, **26346 are readable Encore or MusicTime documents**. The
encrypted ones are decrypted before their header is read, so this study sees inside them, which is
what makes the chapter on encryption possible at all.

---

# 1. The containers

The first four bytes name the program and the byte order, and nothing else. This is what the corpus
holds.

| Magic | Files | What it is |
|---|---|---|
| `SCOW` | 20391 | Encore for Windows, every generation |
| `ZBOT` | 5823 | an encrypted wrapper, always around a `SCOW` document |
| `MTIW` | 72 | MusicTime for Windows |
| `SCO5` | 50 | Encore 5 for the Macintosh |
| `SCOR` | 8 | Windows Encore, read exactly as `SCOW` |
| `MTIM` | 1 | MusicTime for the Macintosh |

**Every encrypted file in the corpus wraps a `SCOW` document.** Not one wraps MusicTime, and not one
wraps the Macintosh container. `ZBOP` has still never been seen, and the single `ZBO6` in the tree is
a fixture this project generated.

The rest of the pile is not Encore at all, and is worth knowing because these files arrive with the
same two extensions and land on the importer's doorstep.

| Signature | Files | What it is |
|---|---|---|
| `ENIGMA ...` | 1202 | Finale |
| `PK` and `03 04` | 137 | a Zip archive that never unpacked |
| `SOLF` | 66 | Melody Assistant or Harmony Assistant, which used `.mus` until 2003 |
| `00 05 16 07` | 34 | macOS resource-fork sidecars, the `._name` files a copy leaves behind |
| `00 10 00 01` | 8 | one damaged score and its copies, chapter 5 |
| `Finale(R) PC 2.0` | 4 | Finale for PC 2.0 |
| text | 9 | font and symbol tables that happen to end in `.mus` |
| shorter than a header | 15 | truncated or empty |

A `.mus` in the wild is far more likely to be Finale's than Encore's.

---

# 2. The generations, and when each appears

Three fields describe a document and only the first selects the reader: the version byte at `0x04`,
the format version at `0x28` and the revision at `0x3E`. The corpus shows which combinations exist
and, through the oldest surviving modification date of each, roughly when they began.

| Inner magic | ver `0x04` | format `0x28` | Files | Oldest dated file |
|---|---|---|---|---|
| `SCOW` | `0xC4` | 4.20 | 18967 | 1997-11-12 |
| `SCOW` | `0xC2` | 3.05 | 3284 | 1996-05-26 |
| `SCOW` | `0xC2` | 3.07 | 1983 | 1997-08-17 |
| `SCOW` | `0xC4` | 3.07 | 1076 | 1998-07-25 |
| `SCOW` | `0xA6` | 2.50 | 814 | **1992-01-30** |
| `SCO5` | none | 4.20 | 51 | 2008-02-09 |
| `MTIW` | `0xA6` | 2.62 | 43 | 1996-11-23 |
| `MTIW` | `0xC2` | 3.07 | 29 | undated, from the archive |

The dates are a floor, not a birthday: a modification time survives a copy but not every download,
so the oldest file of a generation says only that the generation existed by then. Three of them are
worth trusting because they are the programs' own example scores, shipped and never edited: the
1992 file is Encore 2.5's `SILENT.ENC`, and the November 1997 group is Encore 4's example set.

Two things follow that the ordering by version number does not predict.

**Format 3.07 is not a step between 3.05 and 4.20 in time.** Its files begin in August 1997 and run
to 1999, while 4.20 is already shipping in Encore 4's examples that November. The two overlap, and
3.07 also carries the MusicTime magic, so it reads as a parallel line rather than a rung on the
ladder.

**The version byte and the format version are independent.** Format 3.07 appears under both `0xC2`
and `0xC4`, in comparable numbers, and the two groups differ in ornament vocabulary while sharing
element sizes. Selecting the reader from the version byte is therefore right, and it is also why
nothing can be inferred about layout from the version byte alone.

---

# 3. The encrypted container

This chapter answers the question the container raises on sight: is encryption a property of a
version, of a platform, of a program, or of a save.

## It is not a generation

Of the 4.20 generation, **31% is encrypted**. Every other generation is at zero, give or take a
handful: 3.05 has five encrypted files and 3.07 has one, out of more than five thousand. The
older generations have none at all.

So the encrypted container is not how an era of the format was written. It appears inside one
generation and leaves the rest untouched.

## It is not a platform or a program

No MusicTime document in the corpus is encrypted, and no Macintosh document is either. Every
encrypted file is Windows Encore.

## It is bounded in time, and the revision byte shows where

Within the 4.20 generation the revision at `0x3E` splits the population cleanly.

| revision | plain | encrypted | share encrypted |
|---|---|---|---|
| 0 | 1700 | 1153 | 40% |
| 1 | 6356 | 4660 | 42% |
| 4 | 4835 | 0 | 0% |

**Revision 4 is never encrypted in the wild.** The two exceptions in the tree are fixtures this
project generated. And the dates line up with the split: revisions 0 and 1 begin in 1997 and 1999,
revision 4 begins in **2009**, and no encrypted file of any revision predates **2004**.

## What the programs themselves say

The corpus can only show which files exist. The programs say who could have written them. Dating
each binary by its link timestamp rather than by its filename, and probing each for the cipher's
substitution table and for the literal `ZBOT`:

| Binary | Linked | Cipher table | Names `ZBOT` |
|---|---|---|---|
| Encore 2.5.1 | 1992 | no | no |
| MusicTime 3.5.4 demo | 1999-06-09 | no | no |
| Encore 4.5, retail | **2001-10-19** | **no** | no |
| Encore 4.5.5 demo | 2003-03-10 | no | no |
| MusicTime Deluxe 3.5 | **2003-06-30** | **yes** | no |
| Encore demo | **2003-12-04** | **yes** | no |
| MusicTime 4.0.2 update | 2009-02-13 | no | no |
| Encore 5.0.2 | 2009-10-21 | yes | **yes** |

The cipher enters the product line in the **2003 builds** and not before: the retail Encore 4.5 in
hand, linked in October 2001, cannot read or write it. The first encrypted file in the corpus is
from **2004**, a year behind those builds, which is what an upgrade cycle looks like.

Encore 5.0.2 still carries the table and still names `ZBOT`, in a chain of magic comparisons that
also lists `SCOS`, `SCOX`, `MT3W`, `MTOF`, `JWKW`, `RHPM`, `RHPW` and an XML opener. That chain is a
reading list. Nothing in the corpus suggests Encore 5 writes any of it: not one revision-4 file is
encrypted.

## Nothing records the choice

Three places were searched for the switch that decides how a file is saved, and all three are empty.

- **The document.** Comparing the decrypted headers of encrypted files against the headers of plain
  files of the same revision, byte by byte over the whole header, no byte separates the two
  populations. A decrypted file is indistinguishable from one that was never encrypted.
- **The configuration.** `Encore.ini` of the 4.5 installation holds a recent-file list and nothing
  else. There is no format or protection setting.
- **The manual.** The 4.5 help file has no entry for protection, encryption, passwords or locking,
  and neither string appears in the 5.0.2 binary.

What the corpus does show is that the choice was made file by file. Both forms live side by side in
**28% of the folders** that hold 4.20 documents, and **571 score titles appear in both forms**. In
one collection the split is 891 plain against 1258 encrypted. No source in the archive is entirely
encrypted, so it is not a publisher's practice either.

## The reading

Encryption was available to a build of Encore from late 2003, was used on roughly two files in five
by the people who had that build, was never applied to MusicTime or to the Macintosh, and stopped
being written by the time revision 4 appeared in 2009, though Encore 5 kept the ability to read it.
Which command or checkbox produced it is not recoverable from the files, the settings or the
manual, and would need the running program to answer.

---

# 4. What the corpus does not cover

Coverage is measured against the fixtures the test suite carries, because an untested combination is
where the next defect will be found. The gap is concentrated and easy to state: the combinations
that hold the most real files have the fewest fixtures, and the encrypted container, a quarter of
the corpus, has none that came from a real save.

| Combination | Real files | Fixtures |
|---|---|---|
| encrypted, revision 1 | 4253 | 0 |
| `0xC4` 4.20 revision 1 | 3155 | 0 |
| `0xC2` 3.05 revision 1 | 2744 | 0 |
| `0xC2` 3.07 revision 1 | 1298 | 0 |
| encrypted, revision 0 | 994 | 0 |
| `0xA6` 2.50 revision 1 | 709 | 0 |
| `SCO5` revision 2 | 11 | 0 |

Per generation, the features that exist in real files and in no fixture of that generation:

- **`SCO5`**: nearly every ornament subtype, nearly every element size class, tab tuning. The two
  fixtures cover almost nothing.
- **`0xA6`**: ties, beams, key changes, MIDI control change, slurs, three dynamics and the To Coda
  ornament.
- **`0xC2`**: staff text, most of the ornament vocabulary, two note sizes.
- **`0xC4`**: a fifth of the ornament subtypes and the articulation-below byte.

There is also a subtler gap: a group of fixtures state a version byte and a format version that
occur together in no real file, because they were built without a format version and inherited the
skeleton's. They test the reader against a document Encore would never write.

---

# 5. Files that are damaged

Four scores in the archive are broken in ways worth recognising, because each produces a different
symptom and none of them is an importer defect.

- **A header overwritten.** One file has its first few thousand bytes replaced by unrelated data, so
  the magic is gone and the first block header lands in the middle of nothing. Its copies carry the
  `00 10 00 01` signature counted in chapter 1. The right behaviour is to decline it, which is what
  the magic test does.
- **A block sequence repeated.** Another holds its whole block run three times over. A reader that
  trusts the header's measure count stops at the right place; one that reads to the end of the file
  triples the score.
- **Two truncations.** Two files end inside a block. The size fields point past the end of the file,
  which is why every block skip is clamped to the device length.

Not damage, though it looks like it: the resource-fork sidecars, the Zip archives and the JPEG named
`.enc` are simply not scores.

---

# 6. Reproducing this

Everything above comes from reading the first `0x60` bytes of every candidate file, decrypting that
prefix when the magic is an encrypted one, and tabulating. The cipher is small enough to reimplement
outside the importer: the substitution table can be read straight out of
`internal/parser/zbot_table.cpp`, unpacked from its nibbles and patched from the outlier list in
`zbot.cpp`, and the keystream walks it with the seventeen jump deltas of `kTableA` from a starting
row of `0xAB`, advancing one row every four bytes.

The binary dating uses the PE link timestamp at the COFF header, which is four bytes eight into the
`PE\0\0` signature that `e_lfanew` at `0x3C` points to. Filenames and version strings inside these
binaries are not reliable: one file labelled as a 4.5 demo carries an "Encore 5.0" string, and the
retail 4.5 carries an "Encore 3.0" one.

The substitution table sits in the shipped binaries in its natural column order, while the importer
stores it permuted for access speed, so a probe has to undo the permutation before searching.
