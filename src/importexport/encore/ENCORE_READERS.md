# The readers: what each generation needs, and where the reading is still thin

Companion to [ENCORE_CORPUS.md](ENCORE_CORPUS.md), which counts what the files are. This one is
about the four readers that consume them: how one is chosen, what each has to move, what has been
proven right, and what is still untested. The format itself is
[ENCORE_FORMAT.md](ENCORE_FORMAT.md) and the importer above it is
[ENCORE_IMPORTER.md](ENCORE_IMPORTER.md).

---

# 1. Choosing a reader

`EncFormatReader::create` picks from the version byte at `0x04`, with the Macintosh container
matched by magic because it has no version byte where the others keep one. Three fields describe a
document and only that one selects:

| Field | Where | What it actually tracks |
|---|---|---|
| container magic | `0x00` | the program, the byte order, and whether the file is encrypted |
| version byte | `0x04` | the **ornament vocabulary** generation |
| format version | `0x28` | the **element size** generation |
| revision | `0x3E` | not a layout selector; see the corpus notes |

The version byte and the format version are not redundant, and the corpus proves the split is real:
format 3.07 occurs under both version bytes, the two groups share every element size, and they
differ in the articulation numbering. Selecting on the version byte is therefore correct for the
vocabulary, and it is also why nothing about element sizes may be inferred from it.

---

# 2. What each reader reads

Offsets are element relative, `+0` being the first tick byte. The five framing bytes are consumed by
the base, so every body starts at `+5`. These are the fields the readers actually take, which is a
smaller set than the format describes.

**Note.** `+5` face value, `+6` and `+7` the grace flags, `+10` x-offset, `+12` staff position,
`+13` tuplet, `+14` the layout byte whose low two bits are the dot count, `+15` MIDI pitch,
`+16` playback duration, `+19` velocity, `+20` options, `+21` accidental glyph, `+24` and `+26` the
two articulation slots.

**Ornament.** `+5` subtype, `+10` x-offset, `+12` signed y, `+16` and `+18` the two forward measure
counts, `+20` end x, `+26` hairpin direction, `+28` tempo beat unit, `+30` tempo, and the staff-text
index at `+32` when the element is long enough, at `+30` when it is not.

**Tie.** `+5` arc direction and `+6` the secondary start flag, both size-guarded, then the arc
coordinates and source position at `+10`, `+12` and `+14`, which only exist from size 18 up.

**Chord symbol.** `+5` quality, `+6` type, `+10` x-offset, `+12` root, `+13` bass, and, when the
type says the name is written out, a text field from `+14` that runs to the end of the element,
which is why the element grows with the name.

---

# 3. Element sizes, generation by generation

The table that makes every divergence legible. Top sizes per element type, from the corpus.

| element | 2.50 | 3.05 | 3.07 `0xC2` | 3.07 `0xC4` | 4.20 | SCO5 |
|---------|------|------|------------|------------|------|------|
| clef    | . | . | . | 16 | 16 | . |
| key     | 5 | 12 | 14 | 14 | 14 | 14 |
| tie     | 7 | 16 | 18 | 18 | 18 | 18 |
| beam    | 9, 14, 19 | 28, 44, 76 | 30, 46, 62 | 30, 46, 62 | 30, 46, 62 | 30, 46, 62 |
| ornament| 5, 12, 15 | 14, 26, 32 | 16, 28, 34 | 16, 28, 34 | 16, 28, 86 | 16, 28, 78 |
| lyric   | 5, 6, 7 | 20, 22, 24 | . | 22, 24, 26 | 22, 24, 26 | 22, 24, 26 |
| chord symbol | . | . | . | . | 14, 16, 18 | 16 |
| rest    | 7 | 16 | 18, 20 | 18, 26 | 18, 20, 26 | 18 |
| note    | 10, 11 | 22, 24 | 24, 26 | 24, 26 | 18, 28 | 28 |
| MIDI CC | 4 | 10 | . | 12 | 12 | . |

**Every element grew by exactly two bytes between 3.05 and 3.07.** Key 12 to 14, tie 16 to 18, beam
28 to 30 and 44 to 46, ornament 14 to 16 and 26 to 28, lyric 20 to 22, rest 16 to 18, note 22 to 24,
MIDI control change 10 to 12. One coherent change to the element body, not a set of per-element
quirks, which is why a single body shift is enough to read the older generation.

---

# 4. What has been proven, and closed

Three suspicions about the single-axis dispatch were tested against the corpus and none survived as
a defect.

**The version byte selects the right vocabulary.** Counting the raw ornament subtype of every
ornament, the articulation numbering splits exactly on the version byte and inside a single format
version. Independently, across the conversion pairs of chapter 6, no ornament subtype is gained or
lost, so the vocabulary is stable once past the renumbering.

**The note pitch slot calibrates itself.** Probing the candidate pitch bytes and measuring how many
values land in a plausible MIDI range, one slot reads as pitch in essentially every note of a
generation and the others read as noise. The reader needs no heuristic here, only the right offset
per generation.

**The revision byte is not a build stamp.** Encore 4.5 was watched writing revision 0 in a
conversion, and revisions 0 and 1 both appear across a decade of files. It is not a layout selector
and nothing reads it.

---

# 5. Where the reading is still thin

Coverage is measured against the fixtures, since an untested combination is where the next defect
appears. The full table is in the corpus notes; the shape of it is that **the combinations holding
the most real files have the fewest fixtures**, and the encrypted container, a quarter of the
corpus, has no fixture that came from a real save.

What is missing per generation, in features that exist in real files and in no fixture of that
generation:

- **`SCO5`** is the worst covered: nearly every ornament subtype, nearly every element size class
  and the tab tuning are untested. Two fixtures cover almost nothing, and the generation reverses
  the byte order, so a mistake there is invisible everywhere else.
- **`0xA6`** has no fixture for ties, beams, key changes, MIDI control change, slurs, three of the
  dynamics or the To Coda ornament.
- **`0xC2`** has none for staff text, most of the ornament vocabulary, or two of the note sizes.
- **`0xC4`** is the best covered and still misses a fifth of the ornament subtypes and the
  articulation-below byte.

A subtler gap: a group of fixtures carries a version byte and a format version that occur together
in no real file, because they were generated without a format version and inherited the skeleton's.
They exercise the reader against a document Encore would never write, which is worse than not
testing it, since a passing test implies a coverage it does not have.

---

# 6. The differential oracle

The strongest verification available does not need Encore running. One collection in the archive
holds hundreds of matched pairs: an encrypted Encore 4.3 original and the same score converted and
saved by Encore 5.0.2. Measures, instruments and systems are identical across every pair, so any
difference between the two readings is a defect in one of them.

Thirteen musical invariants come out identical across every pair: note and rest face values,
tuplets, clefs and clef changes, time signatures, staff types, tab tunings, key changes, tempo marks
and articulations. That is positive validation of the modern reader across all three revisions and,
because half of every pair is encrypted, of the decryption at scale.

The residue is three files, under half a percent, differing in an ornament subtype, a staff-text
index or an element type, plus a key mismatch in the system block of a handful of pairs. Those are
the places to look first when this reading is next questioned.

Two conversions done by hand in a virtual machine fill the gap the pairs cannot reach, since neither
Encore 4.5 nor 5.0.2 writes the two oldest generations: an Encore 2.5 score opened and re-saved in
4.5, and an Encore 3.x score likewise. They are how the older element geometries were confirmed, and
they are the method to repeat for anything the pairs leave open.
