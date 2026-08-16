# How the Encore format evolved, and what that predicts

A format ordering derived from the corpus rather than asserted, then used to predict where the
importer is likely to be wrong in places nobody has looked yet.

Companion documents: [ENCORE_VERSION_INVENTORY.md](ENCORE_VERSION_INVENTORY.md),
[ENCORE_FORMAT_DIVERGENCE.md](ENCORE_FORMAT_DIVERGENCE.md),
[ENCORE_COVERAGE_GAPS.md](ENCORE_COVERAGE_GAPS.md).

---

## 1. Five generations

| Gen | Release | app `0x28` | ver `0x04` | Defining change |
|-----|---------|-----------|-----------|------------------|
| G1 | Encore 2.x | 592 | `0xA6` | compact everything: 166 byte file header, 26 byte measure header, 10 byte note, doubled element slots |
| G2 | Encore 3.x | 773 | `0xC2` | the format is rebuilt: 194 byte file header, 54 byte measure header, 22 byte note, single-width element slots |
| G3 | Encore 4.0 to 4.2 | 775 | `0xC2` then `0xC4` | **every element grows by exactly two bytes**; the ornament vocabulary is renumbered mid-line |
| G4 | Encore 4.5 | 1056 | `0xC4` | note grows to 28; clef becomes an element; chord symbols appear |
| G5 | Encore 5.x | 1056 (rev 4) | `0xC4` | rich-text ornaments (size 86), 2158 byte instrument entries, tab tunings |

macOS Encore 5 is G5 with big-endian storage and the `SCO5` magic, revision 1 or 2.

---

## 2. The evidence for each transition

### G1 to G2: a rewrite, not an extension

Nothing carries over cleanly. The file header end moves from `0xA6` to `0xC2`, the measure header
from `0x1A` to `0x36`, the element slot stops being doubled, the note goes from 10 bytes to 22, the
LINE staff entry from 22 bytes to 30, and the key signature moves from LINE `+14` to LINE `+15`.
The instrument entry stride goes from 64 to 112. This is a from-scratch file format that reuses the
block framing and the element type numbering, nothing else `[observed]`.

That is why v0xA6 needs a genuinely separate reader, and why treating it as "v0xC4 with patches"
(section 5.4 of the divergence document) breaks down.

### G2 to G3: a uniform +2 byte shift

The single strongest signal in the corpus. Every element type grew by exactly two bytes:

```
keysig  12 -> 14      beam  28 -> 30, 44 -> 46
tie     16 -> 18      lyric 20 -> 22
rest    16 -> 18      note  22 -> 24
midicc  10 -> 12      ornament 14 -> 16, 26 -> 28, 32 -> 34
```

No element is exempt. That is one change to a shared element header or preamble, applied
mechanically across the whole writer, exactly what a development team does when it inserts a field
into a common struct `[observed]`.

The consequence for the importer is the one confirmed defect: a field addressed by absolute offset
is at `X` in G2 and at `X + 2` in G3. The slur forward measure count is the case we caught (`+16`
in G2, `+18` in G3, section 5.3 of the divergence document). **Anything else the code addresses by
absolute offset inside a G2 or G3 element has the same exposure.**

### G3 splits on the version byte, not the app version

Encore 4.0 to 4.2 saves carry app 775 with version byte `0xC2` (9 files) or `0xC4` (13 files). The
two groups have identical element sizes but disjoint articulation vocabularies: the `0xC2` group
encodes accent as `0xC4`, the `0xC4` group as `0xBE`, with zero overlap. So the team renumbered the
ornament subtypes inside the 4.0 to 4.2 line and bumped the version byte for it, while leaving the
app version stamp alone `[verified]`.

This is why the version byte is the right reader selector even though it is not the only axis: it
is precisely the ornament-vocabulary axis.

### G3 to G4: note grows to 28, notation gains elements

The note goes 24 to 28, four bytes, which is where `articulationUp` at `+24` and
`articulationDown` at `+26` become addressable. Before G4 there is at most one articulation slot
(v0xC2 keeps it at `+22` on size-24 notes). The clef element (type 1, size 16) appears for the
first time, and so does the chord symbol element (type 7). Element type `0xA`, present only in G3,
disappears `[observed]`.

### G4 to G5: text and instruments get big

Revision `0x3E` goes from 1 to 4. The ornament gains an 86 byte staff-text variant carrying
Encore's rich-text run header, and the instrument entry stride goes from 242 to 2158. Both are the
signature of a release that added text formatting and per-instrument configuration `[observed]`.

---

## 3. What this predicts

The value of an ordering is that it tells you where to look before a bug report does.

### 3.1 Any absolute offset inside a G2 or G3 element is suspect

Confirmed for the slur count. Not yet checked for: the ornament y at `+12`, the hairpin direction
at `+26`, the tempo pair at `+28` and `+30`, the tie `arcX1` at `+10` and `arcX2` at `+12`, the
chord symbol fields. Each should be probed the same way: measure whether the field or the field
minus two produces sane values, per generation.

The v0xC2 reader already carries two symptoms of this without naming the cause: the tempo layout
discrimination ("newer files match v0xC4, older files store the BPM at `+28`") and the
`slurXoffset2Stale` flag. Both are almost certainly the same +2 shift seen from the other side.

### 3.2 The tie is the most exposed unchecked element

`EncTie::read` needs `size >= 18` for `arcX1`, `arcX2` and `sourcePosition`. G2 ties are 16 bytes
and G1 ties are 7, so both get none of that and fall back to the direction bits alone. The fields
are present at `+8` and `+10` in G2, the same minus two. **124131 ties in the corpus are affected**
(84359 at size 16, 39772 at size 7), which makes this the largest single defect found.

### 3.3 A generation that added a field probably added more than one

G3 added two bytes to every element. Only the slur count has been traced to it. The natural
follow-up is to diff a G2 and a G3 save of the same score, which the VM experiments can produce
directly by opening a 3.x file in Encore 4.5.

### 3.4 v0xA6 needs its own ornament layout, not patches

G1 is a different format, not a compact v0xC4. Two fields have been located (`staffTextTindOffset`
`+28`, `staffTextYoffsetOffset` `+8`) and both were found by targeted work. The remaining ornament
fields have never been located, and the corpus proves at least one of them (the slur endpoint) is
being read from the wrong byte. The right shape is a v0xA6 ornament layout, mirroring how the
v0xA6 note already gets its own pitch and tuplet offsets.

### 3.5 Encore 6 will not announce itself

`EncFormatReader::create` falls back to v0xC4 for any unknown version byte, with a `LOGW` the user
never sees. Given the G1 to G2 precedent, a new major version can change everything. The fallback
should be a decision, not an accident.

---

## 4. Where the ordering is unproven

- **Encore 1.x.** No sample. Whether it wrote `.enc`, and with which magic, is unknown.
  `SCOX` / `SCOR` / `SCOS` are candidates but the corpus has none.
- **The G3 version-byte split.** The claim that Encore 4.0 to 4.2 renumbered ornaments mid-line
  rests on 22 files and one clean articulation split. A save from a known 4.0 build would settle
  it, but no such build is available.
- **Revision 2.** It appears only on macOS Encore 5 files. Whether it is a macOS-line counter or a
  point release is untested.
- **The +2 shift as a single change.** It is uniform across ten element types, which is strong, but
  the byte that was inserted has not been identified. A G2 to G3 conversion diff would show it.
