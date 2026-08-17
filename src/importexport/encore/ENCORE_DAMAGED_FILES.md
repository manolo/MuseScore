# Damaged Encore files in the corpus

Four documents out of 20673 are damaged rather than unsupported. They are written up here because
each one looked, at first, like a gap in the importer, and each one turned out not to be. Nothing in
this document asks for a code change.

The distinction matters when triaging: a file the importer cannot read is a defect only if Encore
itself can read it. For all four of these, it cannot.

---

## Summary

| Document | Copies | Declares | Recovers | Damage |
|---|---|---|---|---|
| `bajoelci.enc` | 7 | 90 measures | **90**, complete | first 4658 bytes overwritten |
| `leyen.enc` | 7 | 87 measures | 14 | block sequence repeated three times, no closing blocks |
| `flec-neg.enc` | 1 | 237 measures | 168 | truncated at exactly 131072 bytes |
| `Getsemany_Triana` | 2 | 60 measures | 24 | truncated at exactly 24576 bytes |

The first two come from one machine, a computer used by a group in Alcalá whose files were known to
have been corrupted at some point. The other two are from unrelated collections and their damage is
a different, more ordinary kind.

## How to find them again

The census records what each header declares and what the parser found. Any file where the second is
smaller than the first is a candidate:

```
got_measures < hdr_measures, or got_lines < hdr_lines, or got_instruments < hdr_instruments
```

Over the corpus that yields 47 files, of which 36 are this suite's own fixtures, 4 are the documents
above, and the rest are off-by-one differences with intact closing blocks, which are not damage.

---

## 1. `bajoelci.enc`: the first 4658 bytes overwritten

Seven byte-identical copies, md5 `5d700332`, in the folders of one machine. One copy keeps its
original timestamp, 31 December 1994, inside an ARJ backup; another sits in a folder named
`Salva_04_95`.

**What it looks like.** The file begins `00 10 00 01` instead of `SCOW`, so nothing recognises it.
The importer rejects it, and so do Encore 4.5 and Encore 5.0.

**What is actually wrong.** A healthy Encore 2.x file is laid out

```
[header 166] [TK instrument blocks] [PAGE] [LINE systems] [MEAS measures] [PREC TITL TEXT FONT]
```

and this one is

```
[4658 bytes of foreign data] [LINE x2] [MEAS x90] [PREC TITL TEXT FONT]
```

The foreign data is a font table of 66-byte entries followed by the printer configuration of the
computer that saved the file, naming its installed devices: `bupsml2 (LPT2:)`,
`Genigraphics Driver on COM1:`, `HP LaserJet Plus on \\gbulaser\gbuljml2 (LPT1:)`,
`PostScript (MGXPS24) on None`, `TeleGrafx - on None`. None of that belongs to the format: a healthy
file from the same folder and the same year contains no font name, no `LPT1:` and no printer entry
outside its own `PREC` block, and this file has its `PREC` and `FONT` blocks intact at the end, so
the information is duplicated.

**What survived.** Everything from the first surviving block onward is untouched, and because the
measure blocks come after the system blocks, the overwrite stopped before reaching any music.
Grafting the block region under a synthetic Encore 2.x header and running the production parser over
it gives 90 measures, 4 staves, 1377 notes, 70 ties, 107 rests, in 2/4, and the score passes
`sanityCheck` and reads as the piece it is meant to be.

Lost with the header: the instrument blocks, so the four parts have no names and no MIDI programs;
the page setup; and all but the last two of the roughly thirteen system definitions, so the original
layout is gone. The music itself is complete.

**Why the search for other victims found nothing.** The five distinctive strings from that printer
list occur in the seven copies of this file and nowhere else in 21620 files, so no other document
received the same injected content.

**Why it is not supported.** A reader for it was written and worked: it located the block start by
walking every candidate and accepting only the one whose chain both consumed the file and matched the
counts the header stated, then handed the body to the existing Encore 2.x reader unchanged. It was
dropped because the file is damaged rather than written in an unknown format, and Encore does not
read it either. Supporting it would mean carrying a recovery path for one document.

Two conclusions drawn earlier and later withdrawn, kept here so they are not drawn again:

- The header was read as a big-endian record with the system count at `0x0A` and the measure count
  at `0x36`, because those offsets hold 2 and 90 and the file has 2 systems and 90 measures. That was
  a coincidence found by searching for the values. A real header would state the **original** system
  count, around thirteen, not the two that happen to survive.
- The container was taken for a Macintosh-derived variant, since a big-endian header over a
  little-endian body is what a byte-swapped Mac format looks like. The printer entries are Windows
  ports and drivers, and the whole preamble is foreign to the file.

## 2. `leyen.enc`: the block sequence repeated three times

Seven copies, 68398 bytes, from the same machine and the same folders. The header survives and is
read normally: `SCOW`, version byte `0xA6`, format 2.50, declaring 14 systems, 4 instruments and 87
measures.

The body does not match it. The block sequence

```
TK00 TK01 TK02 TK03  PAGE x4  LINE x4  MEAS x7
```

appears **three times over**, and the file has no `PREC`, `TITL`, `TEXT` or `FONT` block to close it.
The whole file contains 14 measure blocks against the 87 the header declares, so 73 measures are
simply not present. Cross-linked cluster chains produce exactly this shape.

It imports today, silently, as a 14-measure score. Nothing can recover the rest: the data is not in
the file.

## 3 and 4. `flec-neg.enc` and `Getsemany_Triana`: truncation

Both stop mid-file with no closing blocks, and both stop at a round binary size:

- `flec-neg.enc`, 131072 bytes exactly, which is 128 KiB. Declares 237 measures, holds 168.
- `Getsemany_Triana_1580.enc`, 24581 bytes, and its twin `Getsemany_Triana_.enc`, 24576 bytes
  exactly, which is 24 KiB. Declare 60 measures, hold 24.

Sizes landing on a power of two or a cluster multiple point at an interrupted copy or transfer, not
at corruption on the machine that wrote them. These come from collections unrelated to the two above.

## Not damage

`So_joga_conversa_fora.enc` declares 98 measures and holds 97, but its closing blocks are all
present. A declared count one higher than the block count is an ordinary difference, not a truncated
file, which is why the test above needs the closing blocks to be checked before a file is called
damaged.

---

## The one improvement this suggests

Three of the four import silently with most of their music missing. The header states how many
measures, systems and instruments the file should have, and the parser knows how many it found, so
the importer could say so instead of presenting a truncated score as if it were whole. That is a
different change from supporting any of these files and it is not made here.
