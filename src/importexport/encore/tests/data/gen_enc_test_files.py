"""
Generates synthetic Encore .enc test files that are openable in Encore 5.

Uses bazo.enc as skeleton: its header, TK00, PAGE and LINE blocks are taken
verbatim, ensuring chuVersio/lineCount/pageCount are correct.  Only MEAS
block content is custom-crafted per test.

v0xA6 files are Encore 2.x format; Encore 5 cannot open them (expected).

Run from repo root:
    python3 src/importexport/encore/tests/data/gen_enc_test_files.py
"""
import struct, os

OUT_DIR = os.path.dirname(os.path.abspath(__file__))

# ---------------------------------------------------------------------------
# Skeleton: bytes before / after the MEAS blocks in bazo.enc.
# SKELETON_PRE  = header(194) + TK00 + PAGE + LINE x2  (2514 bytes total)
# SKELETON_POST = PREC + TITL + TEXT                   (23014 bytes)
# ---------------------------------------------------------------------------
SKELETON_PRE = bytes.fromhex(
    '53434f57c4' +
    '00' * 35 +
    '20041800f0000200010001010600000000000001000104ee40d19e0320000000000028000000000000000300000001' +
    '00' * 9 +
    '18479e030000000002' +
    '00' * 23 +
    'a80b4403020064000000f000c0030404' +
    '00' * 14 +
    'c80000000000ffff10ea3d03ffff381b2600ffff08c93600ffffa8ae4403ffff789f4903544b30306e08' +
    '00' * 2051 +
    '010001' +
    '00' * 12 +
    '02020202' +
    '00' * 24 +
    'ffffffffffffffff' +
    '00' * 16 +
    '20202020202020204040404040404040000fff0000000000060034393e43474c5858504147451a000000000002' +
    '00' * 9 +
    '02' +
    '00' * 13 +
    '4c494e45380000000000200028000000000000000300000001000000000060d19e030200000013000300000006006e0094003c0076008e00fcf8f4f0ece8e4e04c494e453800000000000000640000000000030003' +
    '00' * 9 +
    '78cb9e03020000010e0003000000060020003e02aa0120023802fcf8f4f0ece8e4e0'
)
SKELETON_POST = bytes.fromhex(
    '50524543340400004d006900630072006f0073006f00660074002000580050005300200044006f00630075006d0065006e007400200057007200690074006500720000000000000001040006dc00580303ff0000010009009a0b3408640001000f00580202000100580202000000410034' +
    '00' * 75 +
    '0100000000000000010000000200000001000000ffffffff' +
    '00' * 16 +
    '44494e55220010014c030c00cad2f672' +
    '00' * 28 +
    '09' +
    '00' * 9 +
    '01' +
    '00' * 505 +
    '01' +
    '00' * 11 +
    '10010000534d544a00000000100000014d006900630072006f0073006f00660074002000580050005300200044006f00630075006d0065006e00740020005700720069007400650072000000496e70757442696e00464f524d534f5552434500524553444c4c00556e69726573444c4c00496e7465726c656176696e67004f464600496d61676554797065004a5045474d6564004f7269656e746174696f6e00504f52545241495400436f6c6c617465004f4646005265736f6c7574696f6e004f7074696f6e3100506170657253697a65004c455454455200436f6c6f724d6f6465003234627070' +
    '00' * 40 +
    '0c0000004d584457010100005449544cfa52' +
    '00' * 12 +
    'b1001800000000001c' +
    '00' * 1047 +
    'b1001800000000001c' +
    '00' * 1047 +
    'b1001800000000001c' +
    '00' * 1047 +
    'b1000c000000000010' +
    '00' * 1047 +
    'b1000c000000000010' +
    '00' * 1047 +
    'b1000c000000000010' +
    '00' * 1047 +
    'b1000c000000000010' +
    '00' * 1047 +
    'b1000c000000000010' +
    '00' * 1047 +
    'b1000c000000000010' +
    '00' * 1047 +
    'b1000c000000000010' +
    '00' * 1039 +
    '0c00000000000000b1000c000000020010000001' +
    '00' * 10 +
    '230050' +
    '00' * 1031 +
    'b1000c000000000010' +
    '00' * 1047 +
    'b1000c000000020010000001' +
    '00' * 1044 +
    'b1000c000000000010' +
    '00' * 1047 +
    'b8000900000000000b' +
    '00' * 1047 +
    'b8000900000000000b' +
    '00' * 1047 +
    'b8000900000000000b' +
    '00' * 1047 +
    'b8000900000000000b' +
    '00' * 1047 +
    'b8000900000000000b' +
    '00' * 1047 +
    'b8000900000000000b' +
    '00' * 1037 +
    'b1000900000000000c00b1000c00000000001000b1000c00000000001000b8000900000000000b00b1000c00020000001000b100100001000000140033001a00000000004a000700640000000300500000000200020002000200b8000a00000000000a0000001f2b37434f5b6773284028145f4b32ff000000005445585408' +
    '00' * 11 +
    '464f4e54c001' +
    '00' * 104 +
    '41006e0061007300740061007300690061' +
    '00' * 253 +
    '540069006d006500730020004e0065007700200052006f006d0061006e' +
    '00' * 15 +
    '5400610068006f006d0061' +
    '00' * 21 +
    '434f4c528e000000084010020001' +
    '00' * 76 +
    'ffffff00c0c0c0007f7f7f00ff00ff007f007f00ffff00007f7f000000ff0000007f000000ffff00007f7f000000ff0000007f00ff0000007f0000004452554d0000000057494e492a000000020000000000c800e2fffffff8ffffff3b02000053050000120000001200000038030000410200000100'
)
# LINE measureCount byte sits at PRE offset 8+12=20 from each LINE block.
# The first LINE block starts at offset 2386 in bazo.enc => 2386 in PRE.
LINE1_MCOUNT = 2386 + 8 + 12   # = 2406

def set_chumagio(chuMagio):
    s = bytearray(SKELETON_PRE)
    s[4] = chuMagio
    return bytes(s)

# ---------------------------------------------------------------------------
# Measure helpers
# ---------------------------------------------------------------------------
def meas_hdr(timeSigNum, timeSigDen, bpm=100, barTypeEnd=0, beatTicks=None, durTicks=None, glyph=0):
    """54-byte MEAS header, with layout bytes copied from bazo.enc MEAS 0.

    Bytes 0x10..0x35 carry layout/position data (measure width, x offsets,
    text strings like "Writer" in UTF-16) that Encore needs to render the
    measure.  Leaving these zero causes Encore to reject the file with a
    "Program Error" dialog on open.

    `barTypeEnd` sets the end-of-measure barline byte at offset 0x0D
    (0=NORMAL, 2=REPEATSTART, 3=DOUBLEL, 4=REPEATEND, 5=FINAL, 6=DOUBLER).

    `beatTicks` overrides the default (240, the quarter-note count).
    Encore stores beatTicks as the duration of one beat of the time
    signature: 240 for x/4 meters, 120 for x/8 meters, 480 for x/2.
    Use 120 to reproduce x/8 scores faithfully.
    """
    h = bytearray(0x36)
    struct.pack_into('<H', h, 0, bpm)
    h[2] = glyph & 0xFF  # time-signature glyph (0x00=numeric, 0x43/'C'=common time, 0x63/'c'=common time v5)
    if beatTicks is None:
        # Legacy default: hard-code 240 (quarter) regardless of denominator
        # and back-compute durTicks via *4/den. Existing fixtures rely on
        # this. Real Encore files instead set beatTicks per denominator
        # (240 for x/4, 120 for x/8, 480 for x/2) -- pass that value
        # explicitly to reproduce the on-disk layout faithfully.
        beatTicks = 240
        computed_dur  = beatTicks * timeSigNum * 4 // timeSigDen
    else:
        # Whole-note tick count is always 960 in Encore's convention
        # (beatTicks * timeSigDen). durTicks = whole * timeSigNum / timeSigDen.
        computed_dur = beatTicks * timeSigNum
    if durTicks is None:
        durTicks = computed_dur
    struct.pack_into('<HH', h, 4, beatTicks, durTicks)
    h[8], h[9] = timeSigNum, timeSigDen
    # 0x0C = barTypeStart, 0x0D = barTypeEnd (see EncMeasure::read).
    h[0x0D] = barTypeEnd & 0xFF
    # Layout/position bytes (0x10..0x35) lifted verbatim from bazo.enc MEAS 0
    # so Encore accepts the file.  These are 38 bytes covering measure width,
    # x-position, and a trailing UTF-16 "Writer" tag.  All 6 measures of every
    # synthetic file reuse the same block; Encore re-flows on open.
    layout = bytes.fromhex(
        'cc00000034000000c8000000000000009f000000000020005700720069007400650072000000'
    )
    assert len(layout) == 0x26, len(layout)
    h[0x10:0x36] = layout
    return bytes(h)

def meas_block(hdr54, elems):
    return b'MEAS' + struct.pack('<I', len(elems)) + hdr54 + elems

def empty_meas(tsNum=4, tsDen=4):
    return meas_block(meas_hdr(tsNum, tsDen), b'\xff\xff')

def end_marker():
    return b'\xff\xff'

def assemble(chuMagio, custom_list, fill_ts=(4,4), text_override=None):
    """Wrap custom MEAS blocks in the bazo.enc skeleton.

    The skeleton declares measureCount = 6 in its header; we patch that
    field at offset 0x34 to match the total MEAS blocks actually emitted
    so the importer (which now honours measureCount) keeps every block.
    Less than 6 blocks: pad with empty measures. More than 6: write them
    all and bump measureCount accordingly.

    `text_override` appends a custom TEXT block AFTER the skeleton's TEXT,
    which the reader overrides (entries.clear() runs on each TEXT block read).
    """
    pre = bytearray(set_chumagio(chuMagio))
    total_meas = max(len(custom_list), 6)
    struct.pack_into('<h', pre, 0x34, total_meas)
    body = b''.join(meas_block(h, e) for h,e in custom_list)
    if len(custom_list) < 6:
        body += b''.join(empty_meas(*fill_ts) for _ in range(6-len(custom_list)))
    out = bytes(pre) + body + SKELETON_POST
    if text_override is not None:
        out += text_override
    return out

# ---------------------------------------------------------------------------
# Note / rest / ornament element builders
# ---------------------------------------------------------------------------
def note_v0c4(tick, voice, staffIdx, fv, pitch, tuplet=0, layout=0):
    """28-byte v0xC4 note (size=28 from elemStart).

    `layout` writes the layout byte at element +14 (d[11]); its low two bits are the
    dot count (see ENCORE_FORMAT.md 6.3 and 7.3). 0x1d is the value real files carry
    on a dotted eighth."""
    d = bytearray(25)
    d[0]=28; d[1]=staffIdx&0x3F; d[2]=fv; d[10]=tuplet; d[11]=layout&0xFF; d[12]=pitch
    return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

def note_v0c4_xoff(tick, voice, staffIdx, fv, pitch, xoff):
    """v0xC4 28-byte note with explicit layout x-position at element+10..+11 (LE uint16).
    d[7..8] = element bytes +10..+11 (xoffset field)."""
    d = bytearray(25)
    d[0]=28; d[1]=staffIdx&0x3F; d[2]=fv; d[12]=pitch
    struct.pack_into('<H', d, 7, xoff & 0xFFFF)
    return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

def note_v0c4_xoff_tup(tick, voice, staffIdx, fv, pitch, xoff, tuplet=0):
    """28-byte v0xC4 note stating both its notated column (+10) and a tuplet ratio (+13)."""
    d = bytearray(25)
    d[0]=28; d[1]=staffIdx&0x3F; d[2]=fv; d[10]=tuplet; d[12]=pitch
    d[7]=xoff & 0xFF; d[8]=(xoff >> 8) & 0xFF
    return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

def note_v0c4_perc(tick, voice, staffIdx, fv, pitch, position=0):
    """28-byte v0xC4 note with percussion position byte (element+12 = d[9])."""
    d = bytearray(25)
    d[0]=28; d[1]=staffIdx&0x3F; d[2]=fv; d[9]=position&0xFF; d[12]=pitch
    return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

def set_score_size(data, sz):
    """Patch the scoreSize byte (header offset 0x52) in assembled data."""
    d = bytearray(data)
    if 0x52 < len(d):
        d[0x52] = sz & 0xFF
    return bytes(d)

ENC_FORMAT_2_50 = 0x0250
ENC_FORMAT_3_05 = 0x0305
ENC_FORMAT_3_07 = 0x0307
ENC_FORMAT_4_20 = 0x0420

def set_version(data, ver):
    """Patch chuVersio uint16 LE at header offset 0x28."""
    d = bytearray(data)
    struct.pack_into('<H', d, 0x28, ver)
    return bytes(d)

def set_line_staff_size_hint(data, sz0indexed):
    """Patch LINE staff entry byte[13] (staffSizeHint, 0-indexed) in ALL LINE blocks."""
    d = bytearray(data)
    pos = 0
    while pos + 8 < len(d):
        if d[pos:pos+4] == b'LINE':
            blk_size = int.from_bytes(d[pos+4:pos+8], 'little')
            # LINE content: 10-byte skip + 2-byte start + 1-byte count = 13 bytes before staff entries
            for si in range(8):  # up to 8 staves per system
                off = pos + 8 + 13 + si * 30 + 13  # entry_start + 13 = byte[13]
                if off >= pos + 8 + blk_size:
                    break
                d[off] = sz0indexed & 0xFF
            pos += 8 + blk_size
        else:
            pos += 1
    return bytes(d)

def set_staff_clef(data, staff_idx=0, clef=7):
    """Patch the clef byte for staff_idx in ALL LINE blocks of assembled data.
    LINE layout: 4-byte magic + 4-byte size + 13-byte header + N*30-byte staff entries.
    Clef byte is at entry-start + 14."""
    d = bytearray(data)
    pos = 0
    while pos + 8 < len(d):
        if d[pos:pos+4] == b'LINE':
            entry_offset = pos + 8 + 13 + staff_idx * 30 + 14  # +14 = clef byte
            if entry_offset < len(d):
                d[entry_offset] = clef & 0xFF
            size = int.from_bytes(d[pos+4:pos+8], 'little')
            pos += 8 + size
        else:
            pos += 1
    return bytes(d)

def note_v0c4_dotctrl(tick, voice, staffIdx, fv, pitch, dotControl):
    """28-byte v0xC4 note with dotControl set at elemStart+11."""
    d = bytearray(25)
    d[0]=28; d[1]=staffIdx&0x3F; d[2]=fv; d[11]=dotControl; d[12]=pitch
    return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

def note_v0c4_grace(tick, voice, staffIdx, fv, pitch, grace1, grace2):
    """v0xC4 note with grace bytes set. grace1 at d[3], grace2 at d[4]."""
    d = bytearray(25)
    d[0]=28; d[1]=staffIdx&0x3F; d[2]=fv
    d[3]=grace1; d[4]=grace2   # grace1 at elemStart+6, grace2 at elemStart+7
    d[12]=pitch
    return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

def note_v0c4_grace_xoff(tick, voice, staffIdx, fv, pitch, grace1, grace2, xoff=0):
    """v0xC4 grace note with explicit xoffset (d[7]) for slur heuristic tests."""
    d = bytearray(25)
    d[0]=28; d[1]=staffIdx&0x3F; d[2]=fv
    d[3]=grace1; d[4]=grace2
    d[7]=xoff & 0xFF   # xoffset at element+10
    d[12]=pitch
    return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

def note_v0c4_voice4(tick, staffIdx, fv, pitch):
    """v0xC4 note with voice=4 (>= VOICES=4), should be skipped."""
    d = bytearray(25)
    d[0]=28; d[1]=staffIdx&0x3F; d[2]=fv; d[12]=pitch
    return struct.pack('<H',tick)+bytes([0x94])+bytes(d)  # 0x94=(9<<4)|4

def rest_v0c4(tick, voice, staffIdx, fv):
    """18-byte v0xC4 rest."""
    d = bytearray(15)
    d[0]=18; d[1]=staffIdx&0x3F; d[2]=fv
    return struct.pack('<H',tick)+bytes([(8<<4)|(voice&0xF)])+bytes(d)

def rest_v0c4_tup(tick, voice, staffIdx, fv, tuplet=0):
    """18-byte v0xC4 rest with explicit tuplet byte. d[10] = tuplet at +13 from elemStart."""
    d = bytearray(15)
    d[0]=18; d[1]=staffIdx&0x3F; d[2]=fv; d[10]=tuplet
    return struct.pack('<H',tick)+bytes([(8<<4)|(voice&0xF)])+bytes(d)

def rest_v0c4_dotted(tick, voice, staffIdx, fv, dotControl):
    """18-byte v0xC4 rest with dotControl set (actual sounding duration in Encore ticks)."""
    d = bytearray(15)
    d[0] = 18; d[1] = staffIdx & 0x3F; d[2] = fv
    d[10] = 0          # tuplet = none
    d[11] = dotControl # actual sounding duration (encodes dots)
    return struct.pack('<H', tick) + bytes([(8<<4)|(voice&0xF)]) + bytes(d)

def rest_v0c4_mrest(tick, voice, staffIdx, fv, mrestCount):
    """18-byte v0xC4 rest with mrestCount at rawElemStart+15 (d[12]).
    mrestCount > 1 means this single MEAS block represents that many empty measures."""
    d = bytearray(15)
    d[0] = 18; d[1] = staffIdx & 0x3F; d[2] = fv
    d[12] = mrestCount & 0xFF
    return struct.pack('<H', tick) + bytes([(8<<4)|(voice&0xF)]) + bytes(d)

def tie_v0c4(tick, voice, staffIdx, direction=0xfe, startFlag=0):
    """16-byte TIE element (type=3). Marks note(s) at (staffIdx, voice, tick)
    as tie-start when EITHER the arc-direction byte at +5 (`direction`) OR
    the tie-start flag at +6 (`startFlag`) has the high bit set. Real Encore
    files often store the arc-direction as 0x04 (arc-only) while the high
    bit lives on byte +6 (0x80); the importer must accept both forms."""
    d = bytearray(13)
    d[0] = 16          # size field (total from tick start)
    d[1] = staffIdx & 0x3F
    d[2] = direction   # +5: arc direction (0xfc/0xfe outgoing, 0x02/0x04 arc-only)
    d[3] = startFlag   # +6: tie-start flag (0x80 = outgoing tie)
    # bytes d[4..12] = padding zeros
    return struct.pack('<H', tick) + bytes([(3<<4)|(voice&0xF)]) + bytes(d)

def ornament_v0c4_voice4_staffhi(tick, staffIdx, tipo):
    """16-byte size-16 ORN with voice=4 and staffByte high bit set (0x40 ORed
    onto the masked staffIdx). This is how Encore encodes system-level
    ornaments such as dynamics and tremolos -- the importer must accept
    voice=4 ORN elements and route them to voice 0 of the same staff.
    Total 16 bytes: tick(2) + typeVoice(1) + size(1) + staffByte(1) +
    tipo(1) + 10 bytes of payload zeros.
    """
    d = bytearray(16)
    struct.pack_into('<H', d, 0, tick)
    d[2] = (5 << 4) | 4               # typeVoice: ORN | voice=4
    d[3] = 16                          # size
    d[4] = (staffIdx & 0x3F) | 0x40    # staffByte with high bit set
    d[5] = tipo
    return bytes(d)


def ornament_v0c4(tick, voice, staffIdx, tipo, xoffset=0, alMezuro=0, xoffset2=0, speguleco=0, yoffset=0, altMezuro=0):
    """33-byte ORNAMENT element (type=5).
    Layout from elemStart (= tick offset 0):
      d[0:2]=tick, d[2]=typeVoice, d[3]=size, d[4]=staffIdx,
      d[5]=tipo, d[6..9]=skip, d[10]=xoffset, d[11]=skip,
      d[12:14]=yoffset s16, d[14..15]=skip,
      d[16]=altMezuro (v0xC2 spanning measure-count), d[17]=skip,
      d[18]=alMezuro (v0xC4 spanning measure-count), d[19]=skip,
      d[20]=xoffset2, d[21..25]=skip,
      d[26]=speguleco, d[27..32]=zeros.
    """
    d = bytearray(33)
    struct.pack_into('<H', d, 0, tick)
    d[2] = (5<<4)|(voice&0xF)
    d[3] = 33
    d[4] = staffIdx&0x3F
    d[5] = tipo
    d[10] = xoffset
    struct.pack_into('<h', d, 12, yoffset)
    d[16] = altMezuro
    d[18] = alMezuro
    d[20] = xoffset2
    d[26] = speguleco & 3
    return bytes(d)

def rest_v0c2(tick, voice, staffIdx, fv):
    """16-byte v0xC2 rest (same layout as EncRest, size field = 16)."""
    d = bytearray(13)
    d[0] = 16; d[1] = staffIdx & 0x3F; d[2] = fv
    return struct.pack('<H', tick) + bytes([(8 << 4) | (voice & 0xF)]) + bytes(d)


def note_v0c2(tick, voice, staffIdx, fv, pitch, layout=0):
    """22-byte v0xC2 note. Pitch at d[10]=elemStart+13 (tuplet field).

    `layout` writes the layout byte, which sits at element +12 (d[9]) in this
    generation; its low two bits are the dot count (see ENCORE_FORMAT.md 6.3 and 7.3)."""
    d = bytearray(19)
    d[0]=22; d[1]=staffIdx&0x3F; d[2]=fv; d[9]=layout&0xFF; d[10]=pitch
    return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

def note_v0c2_size24(tick, voice, staffIdx, fv, pitch, artic):
    """24-byte v0xC2 note with articulation. Pitch at d[10]=elemStart+13;
    articulation byte at d[19]=elemStart+22; direction flag at d[20]=elemStart+23.
    dotControl=0xC0 is the characteristic value for size-24 notes."""
    d = bytearray(21)
    d[0]=24; d[1]=staffIdx&0x3F; d[2]=fv
    d[9]=0x80              # position flag (characteristic for size=24)
    d[10]=pitch            # MIDI pitch in tuplet slot (+13)
    d[11]=0xC0             # dotControl (characteristic for size=24)
    struct.pack_into('<H', d, 13, 480)  # playback duration (+16)
    d[19]=artic            # articulation byte (+22): 0x1c=tenuto, 0x1d=staccato
    d[20]=0x08             # direction flag (+23)
    return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

def note_v0c2_ext(tick, voice, staffIdx, fv, pitch, grace1=0, dotControl=0):
    """22-byte v0xC2 note with grace1 and dotControl fields."""
    d = bytearray(19)
    d[0]=22; d[1]=staffIdx&0x3F; d[2]=fv
    d[3]=grace1               # elemStart+6 = grace1
    d[10]=pitch               # elemStart+13 = pitch (tuplet field in v0xC2)
    d[11]=dotControl          # elemStart+14 = dotControl
    return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

def note_v0c2_tuplet_pitch(tick, voice, staffIdx, fv, pitch, tup):
    """22-byte v0xC2 note for Encore 4.x files that store the MIDI pitch directly
    in semiTonePitch (+15) instead of the tuplet slot (+13). The tuplet slot then
    carries a genuine tuplet ratio (e.g. 0x32 = 3:2), which must NOT be mistaken
    for the pitch."""
    d = bytearray(19)
    d[0]=22; d[1]=staffIdx&0x3F; d[2]=fv
    d[10]=tup                 # elemStart+13 = tuplet ratio (3:2 = 0x32)
    d[12]=pitch               # elemStart+15 = semiTonePitch (real MIDI pitch)
    return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

def note_v0c2_spurious_semitone(tick, voice, staffIdx, fv, pitch, flag=1):
    """22-byte v0xC2 sub-variant A note (pitch at +13) where the semiTonePitch
    slot (+15) carries a small non-zero flag (1 or 3) instead of 0. Some Encore
    3.x/4.x files leave a stray value there; it is NOT a MIDI pitch. The importer
    must still read the pitch from +13 and must not mistake the flag for the pitch
    (which would import the note as MIDI 1 = C#-1, several octaves too low)."""
    d = bytearray(19)
    d[0]=22; d[1]=staffIdx&0x3F; d[2]=fv
    d[10]=pitch               # elemStart+13 = real MIDI pitch
    d[12]=flag                # elemStart+15 = spurious flag, not a pitch
    return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

def note_v0xa6(tick, voice, staffIdx, fv, pitch_offset, position=None, end='<'):
    """v0xA6 note: size=10, slot=20 bytes. MIDI pitch lives at elemStart+11
    (= file offset within the 20-byte slot, NOT within the 10-byte d
    array). The slot layout is 3 header bytes + 7 d bytes + 10 padding
    bytes; we put the absolute MIDI pitch (60 + pitch_offset, signed
    offset accepted for callers used to the C4-based convention) at
    offset 11 by overwriting the first padding byte. Real Encore 2.x
    files store the same field at the same offset.

    position: staff position at elemStart+9, a signed count of diatonic steps from middle C
    (0 = C4, 5 = A4, -1 = B3). Left unwritten when None so existing fixtures keep their bytes.
    """
    d = bytearray(7)
    d[0]=10; d[1]=staffIdx&0x3F; d[2]=fv
    if position is not None:
        d[6] = position & 0xFF      # = file offset +9
    pad = bytearray(10)
    midi = 60 + (pitch_offset if pitch_offset >= -128 and pitch_offset <= 127 else 0)
    pad[11 - 3 - 7] = midi & 0xFF   # = pad[1] = file offset +11
    return struct.pack(end+'H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)+bytes(pad)

def rest_v0xa6(tick, voice, staffIdx, fv, dur_ticks=0):
    """v0xA6 rest: size=7, slot=14 bytes. faceValue at +5 and the rest's own duration in ticks
    as a uint16 at +12. The layout carries neither a tuplet descriptor nor a dot control: the
    two slots the later generations keep at +13 and +14 are, here, the duration's high byte and
    the first byte of whatever element follows."""
    b = bytearray(14)
    struct.pack_into('<H', b, 0, tick)
    b[2] = (8 << 4) | (voice & 0xF)   # type 8 = REST
    b[3] = 7                          # size, in 2-byte units
    b[4] = staffIdx & 0x3F
    b[5] = fv
    struct.pack_into('<H', b, 12, dur_ticks)
    return bytes(b)

def tie_v0xa6(tick, voice, staffIdx, startFlag=0x80):
    """v0xA6 tie: size 7, a 14-byte slot. The arc pair the later generations carry does not fit in
    it, so the two flag bytes at +5 and +6 are the whole of the tie signal. 39772 of these sit in
    the corpus. See ENCORE_FORMAT.md §TIE element."""
    b = bytearray(14)
    struct.pack_into('<H', b, 0, tick)
    b[2] = (3 << 4) | (voice & 0xF)   # type 3 = TIE
    b[3] = 7
    b[4] = staffIdx & 0x3F
    b[5] = startFlag & 0xFF
    return bytes(b)


def keychange_v0xa6(tick, voice, staffIdx, tipo):
    """v0xA6 mid-score key change: size 5, a 10-byte slot, the Encore key index at +5."""
    b = bytearray(10)
    struct.pack_into('<H', b, 0, tick)
    b[2] = (2 << 4) | (voice & 0xF)   # type 2 = KEYCHANGE
    b[3] = 5
    b[4] = staffIdx & 0x3F
    b[5] = tipo & 0xFF
    return bytes(b)


def gen_v0xa6_tie_and_key_change():
    """Two measures of Encore 2.x: a tie across the bar line, and a key change opening the second.
    Neither had a fixture, though the corpus holds 39772 v0xA6 ties."""
    m1  = note_v0xa6(0, 0, 0, 2, 0, position=0)      # middle C, half
    m1 += tie_v0xa6(0, 0, 0)
    m1 += note_v0xa6(480, 0, 0, 2, 0, position=0)    # same pitch, the receiver
    m1 += end_marker()
    m2  = keychange_v0xa6(0, 0, 0, 9)                # 9 = D major, two sharps
    m2 += note_v0xa6(0, 0, 0, 1, 2, position=1)
    m2 += end_marker()
    meas = [b'MEAS' + struct.pack('<I', len(e)) + _mhdr_a6(4, 4) + e for e in (m1, m2)]
    return build_v0xa6([('Voz', 1, 0)], meas, staff_size=1)


def lyric_v0xa6(tick, voice, staffIdx, text, kie=0):
    """v0xA6 compact lyric: tick(2)+tv(1)+size(1)+rawStaff(1)+control/anchor byte (kie = the
    horizontal x-offset), then null-terminated Latin-1 text within the size*2 slot (text at +6)."""
    tb = text.encode('latin1') + b'\x00'
    need = 6 + len(tb)
    size = (need + 1) // 2
    if size * 2 < need:
        size += 1
    b = bytearray(size * 2)
    struct.pack_into('<H', b, 0, tick)
    b[2] = (6 << 4) | (voice & 0xF)   # type 6 = LYRIC
    b[3] = size
    b[4] = staffIdx & 0x3F
    b[5] = kie & 0xFF                 # control/anchor byte = x-offset
    b[6:6 + len(tb)] = tb
    return bytes(b)

def stafftext_orn_v0xa6(tick, voice, staffIdx, tind, yoffset=0):
    """v0xA6 compact STAFFTEXT ornament: size=15 (30-byte slot), tipo 0x1E at +5, and the
    TEXT-block entry index at elemStart+28 (= +26 from the type/voice byte). The vertical
    placement value (signed Cartesian y, positive = above, negative = below) lives at
    elemStart+8 (= +6 from the type/voice byte), not at the v0xC4 offset."""
    size = 15
    b = bytearray(size * 2)
    struct.pack_into('<H', b, 0, tick)
    b[2] = (5 << 4) | (voice & 0xF)   # type 5 = ORNAMENT
    b[3] = size
    b[4] = staffIdx & 0x3F
    b[5] = 0x1E                       # tipo = STAFFTEXT
    struct.pack_into('<h', b, 8, yoffset)   # placement y at +6 from the type/voice byte
    b[28] = tind & 0xFF               # tind
    return bytes(b)

def text_block_v0xa6(entries):
    """v0xA6 TEXT block: entries have no 14-byte header; text starts at payload offset 0."""
    body = struct.pack('<HHI', 0, len(entries), 0)   # sync, count, contentSize
    for t in entries:
        tb = t.encode('latin1') + b'\x00'
        body += struct.pack('<H', len(tb)) + tb
    return b'TEXT' + struct.pack('<I', len(body)) + bytes(body)

# ---------------------------------------------------------------------------
# File generators, original set
# ---------------------------------------------------------------------------
def gen_v0c2_pitches():
    e  = note_v0c2(0,  0,0,3,60)
    e += note_v0c2(240,0,0,3,64)
    e += note_v0c2(480,0,0,3,67)
    e += note_v0c2(720,0,0,3,72)
    e += end_marker()
    return assemble(0xC2,[(meas_hdr(4,4),e)])

def note_v0c2_pre4(tick, voice, staffIdx, fv, pitch, playbackLo):
    """22-byte note as written by Encore 3.x (app version 773).

    Encore 4.0 inserted two bytes into every element body at offset +8, so in a
    pre-4.0 file the pitch is at +13 and +15 is the low byte of the playback
    duration, not a pitch slot. `playbackLo` fills +15 with a plausible value so
    the file exercises the case a reader using the post-4.0 offsets gets wrong:
    it would take +15 for the pitch and import the note several semitones off.
    """
    d = bytearray(19)
    d[0] = 22; d[1] = staffIdx & 0x3F; d[2] = fv
    d[10] = pitch        # +13: MIDI pitch
    d[12] = playbackLo   # +15: low byte of the playback duration, NOT a pitch
    return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)


def tie_v0c2_pre4(tick, voice, staffIdx, arcX1, arcX2):
    """16-byte TIE as written by Encore 3.x (app version 773).

    The arc endpoints sit at +8 and +10, two bytes below the post-4.0 layout.
    Both flag bytes (+5 direction, +6 start flag) are left clear, so the arc span
    is the only thing that marks this as a forward tie: a reader that requires
    the post-4.0 element length before reading the arc finds no tie at all.
    """
    d = bytearray(13)
    d[0] = 16; d[1] = staffIdx & 0x3F
    d[2] = 0             # +5: no arc-direction bit
    d[3] = 0             # +6: no tie-start flag
    d[5] = arcX1         # +8
    d[7] = arcX2         # +10
    return struct.pack('<H', tick) + bytes([(3 << 4) | (voice & 0xF)]) + bytes(d)


def gen_v0c2_pre4_element_offsets():
    """Encore 3.x file: pitch at +13 with a decoy at +15, and a 16-byte tie whose
    arc span at +8/+10 is the only tie-start signal.

    Reading it with the post-4.0 offsets imports both notes a fourth too high and
    drops the tie entirely.
    """
    e  = note_v0c2_pre4(0,   0, 0, 2, 60, 72)   # C4, decoy 72 (C5) in the +15 slot
    e += tie_v0c2_pre4(0,    0, 0, 10, 90)      # forward tie, arc span 10 -> 90
    e += note_v0c2_pre4(480, 0, 0, 2, 60, 72)   # C4 again, the tie receiver
    e += end_marker()
    # App version 773 is what selects the pre-4.0 element body layout.
    return set_version(assemble(0xC2, [(meas_hdr(4, 4), e)]), 773)


def gen_v0c2_pre4_articulation_codes():
    """Encore 3.x file whose articulations use the numbering that release predates: tenuto 0xCE,
    staccato 0xCF and fermata above 0xD2, each six above the code every later generation uses.
    An accent (0xBE) rides along to show the rest of the vocabulary did not move.

    Read with the later numbering, the first three are codes nothing recognises and the marks
    never reach the score.
    """
    e  = orn16_v0c4(0,   0, 0, tipo=0xCE)       # tenuto, 0xC8 from Encore 4.0 on
    e += note_v0c2_pre4(0,   0, 0, 3, 60, 72)
    e += orn16_v0c4(240, 0, 0, tipo=0xCF)       # staccato, later 0xC9
    e += note_v0c2_pre4(240, 0, 0, 3, 62, 72)
    e += orn16_v0c4(480, 0, 0, tipo=0xD2)       # fermata above, later 0xCC
    e += note_v0c2_pre4(480, 0, 0, 3, 64, 72)
    e += orn16_v0c4(720, 0, 0, tipo=0xC4)       # accent here, an up-bow from Encore 4.0 on
    e += note_v0c2_pre4(720, 0, 0, 3, 65, 72)
    e += end_marker()
    return set_version(assemble(0xC2, [(meas_hdr(4, 4), e)]), 773)


def gen_v0c2_post40_articulation_codes():
    """The same file stamped format 3.07, where the vocabulary is already the current one: 0xC4 is
    an up-bow and must stay one, and the codes six higher mean nothing.

    The note bodies use the post-4.0 layout, since format 3.07 is what moved them.
    """
    e  = orn16_v0c4(0, 0, 0, tipo=0xC4)         # up-bow
    e += note_v0c2(0, 0, 0, 3, 60)
    e += orn16_v0c4(240, 0, 0, tipo=0xC9)       # staccato, at its own code
    e += note_v0c2(240, 0, 0, 3, 62)
    e += end_marker()
    return set_version(assemble(0xC2, [(meas_hdr(4, 4), e)]), 775)


# ===========================================================================
# One measure of the same music, written in the geometry of a chosen generation.
#
# Encore 4.0 inserted two bytes into every element body at +8, so the whole element family moves
# together: the corpus shows a note of 22 bytes before that release and 24 after, a rest of 16 then
# 18, a tie of 16 then 18, a MIDI CC of 10 then 12. The articulation vocabulary moved in the same
# release, so a staccato is 0xCF before it and 0xC9 after.
#
# `shift` is -2 for the pre-4.0 geometry (format 3.05) and 0 for the later one (3.07 and 4.20).
# Everything reads from one place, so the three fixtures below differ only in their stated
# generation and in the bytes that generation implies.
# ===========================================================================
def _family_note(tick, voice, staffIdx, fv, pitch, shift):
    size = 24 + shift
    d = bytearray(size - 3)
    d[0] = size
    d[1] = staffIdx & 0x3F
    d[2] = fv                     # +5 face value
    d[12 + shift] = pitch         # +15 with the later layout, +13 before it
    return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)


def _family_rest(tick, voice, staffIdx, fv, shift):
    size = 18 + shift
    d = bytearray(size - 3)
    d[0] = size
    d[1] = staffIdx & 0x3F
    d[2] = fv                     # +5 face value
    return struct.pack('<H', tick) + bytes([(8 << 4) | (voice & 0xF)]) + bytes(d)


def _family_tie(tick, voice, staffIdx, arcX1, arcX2, shift):
    size = 18 + shift
    d = bytearray(size - 3)
    d[0] = size
    d[1] = staffIdx & 0x3F
    # +5 direction and +6 start flag stay put; only the arc pair moves with the generation.
    struct.pack_into('<H', d, 7 + shift, arcX1)    # +10 later, +8 before
    struct.pack_into('<H', d, 9 + shift, arcX2)    # +12 later, +10 before
    return struct.pack('<H', tick) + bytes([(3 << 4) | (voice & 0xF)]) + bytes(d)


def _family_midicc(tick, voice, staffIdx, controller, value, shift):
    size = 12 + shift
    d = bytearray(size - 3)
    d[0] = size
    d[1] = staffIdx & 0x3F
    d[7 + shift] = controller     # +10 later, +8 before
    d[8 + shift] = value
    return struct.pack('<H', tick) + bytes([(11 << 4) | (voice & 0xF)]) + bytes(d)


def _family_staccato(tick, voice, staffIdx, shift):
    """Size 16 ornament. The subtype itself carries the generation: before Encore 4.0 a staccato is
    0xCF, from that release on it is 0xC9."""
    d = bytearray(13)
    d[0] = 16
    d[1] = staffIdx & 0x3F
    d[2] = 0xCF if shift else 0xC9
    return struct.pack('<H', tick) + bytes([(5 << 4) | (voice & 0xF)]) + bytes(d)


def _family_measure(shift):
    e  = _family_note(0, 0, 0, 3, 60, shift)
    e += _family_staccato(0, 0, 0, shift)
    e += _family_tie(0, 0, 0, 10, 90, shift)
    e += _family_note(240, 0, 0, 3, 60, shift)      # the tie receiver, same pitch
    e += _family_note(480, 0, 0, 3, 62, shift)
    e += _family_rest(720, 0, 0, 3, shift)
    e += _family_midicc(720, 0, 0, 64, 127, shift)
    return e + end_marker()


def gen_family_3x():
    """Encore 3.x: version byte 0xC2, format 3.05, the pre-4.0 geometry. 3220 files in the corpus."""
    return set_version(assemble(0xC2, [(meas_hdr(4, 4), _family_measure(-2))]), ENC_FORMAT_3_05)


def gen_family_40x_c2():
    """Encore 4.0 to 4.2: version byte 0xC2, format 3.07, the shifted geometry. 1718 files."""
    return set_version(assemble(0xC2, [(meas_hdr(4, 4), _family_measure(0))]), ENC_FORMAT_3_07)


def gen_family_40x_c4():
    """The one combination that genuinely crosses the two version axes: version byte 0xC4 with
    format 3.07, 996 files in the corpus and no fixture before this one. The reader comes from the
    version byte and the geometry from the format, so this is where the two must agree."""
    return set_version(assemble(0xC4, [(meas_hdr(4, 4), _family_measure(0))]), ENC_FORMAT_3_07)


def note_v0c2_artic_4x(tick, voice, staffIdx, fv, pitch, articUp=0, articDown=0):
    """Encore 4.x note (base 24 bytes) that grows to carry its articulations.

    The two slots sit immediately past the base note: +24 for the mark above and
    +26 for the one below, the same places v0xC4 uses. A note with only the mark
    above is 26 bytes; one with both is 28.
    """
    size = 28 if articDown else (26 if articUp else 24)
    d = bytearray(size - 3)
    d[0] = size; d[1] = staffIdx & 0x3F; d[2] = fv
    d[12] = pitch              # +15
    if articUp:
        d[21] = articUp        # +24
    if articDown:
        d[23] = articDown      # +26
    return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)


def gen_v0c2_artic_grows_note():
    """Encore 4.x file whose notes grow to carry their articulations.

    Reading it as if every note were the 24-byte base drops all three marks: the
    slots only exist because the element is longer.
    """
    e  = note_v0c2_artic_4x(0,   0, 0, fv=3, pitch=67, articUp=0x1d)                  # 26 bytes
    e += note_v0c2_artic_4x(240, 0, 0, fv=3, pitch=64, articUp=0x12, articDown=0x1d)  # 28 bytes
    e += note_v0c2_artic_4x(480, 0, 0, fv=3, pitch=60)                                # 24, sin marca
    e += note_v0c2_artic_4x(720, 0, 0, fv=3, pitch=62, articUp=0x1c)                  # 26 bytes
    e += end_marker()
    return set_version(assemble(0xC2, [(meas_hdr(4, 4), e)]), 775)


def gen_v0c2_size24_artic_pitch():
    """Encore 3.x notes that carry an articulation: G4+staccato then E4+tenuto.

    A v0xC2 note grows past its base length to hold an articulation, so in this
    generation, whose base note is 22 bytes, an articulated note is 24 bytes with
    the mark at +22. The pitch is at +13, as it is for every note of that base.
    App version 773 is what selects the pre-4.0 body layout.
    """
    e  = note_v0c2_size24(0,   0, 0, fv=3, pitch=67, artic=0x1d)  # G4 staccato
    e += note_v0c2_size24(480, 0, 0, fv=3, pitch=64, artic=0x1c)  # E4 tenuto
    e += end_marker()
    return set_version(assemble(0xC2, [(meas_hdr(4, 4), e)]), 773)

def note_v0c2_size24_semitone(tick, voice, staffIdx, fv, pitch):
    """24-byte v0xC2 note with pitch at semiTonePitch slot (d[12]=rawElemStart+15),
    tuplet=0. Matches the layout found in some Encore 4.x v0xC2 files (e.g. TUVEHAMB.ENC)
    where the pitch is already in the correct field and the tuplet swap must NOT fire."""
    d = bytearray(21)
    d[0]=24; d[1]=staffIdx&0x3F; d[2]=fv
    # d[10] (tuplet, rawElemStart+13) = 0: pitch is NOT here.
    d[12]=pitch          # semiTonePitch at rawElemStart+15: the real pitch.
    struct.pack_into('<H', d, 13, 480)  # playback duration
    return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

def gen_v0c2_size24_semitonepitch():
    # Two v0xC2 size=24 notes with tuplet=0 and pitch at semiTonePitch (not tuplet).
    # Regression: the pitch-swap (semiTonePitch=tuplet) must be skipped when tuplet==0.
    e  = note_v0c2_size24_semitone(0,   0, 0, fv=3, pitch=60)  # C4
    e += note_v0c2_size24_semitone(240, 0, 0, fv=3, pitch=64)  # E4
    e += end_marker()
    return assemble(0xC2,[(meas_hdr(4,4),e)])

def gen_v0c2_common_time_glyph():
    # v0xC2 4/4 with timeSigGlyph=0x63 ('c' = common time "C" symbol in Encore 5.x).
    # Regression: the importer must preserve TimeSigType::FOUR_FOUR, not silently
    # downgrade to NORMAL (numeric 4/4).
    e  = note_v0c2(0, 0, 0, fv=3, pitch=60)    # C4
    e += note_v0c2(240, 0, 0, fv=3, pitch=64)  # E4
    e += end_marker()
    return assemble(0xC2, [(meas_hdr(4, 4, glyph=0x63), e)])

def gen_v0c2_common_time_glyph_uppercase():
    # v0xC2 4/4 with timeSigGlyph=0x43 ('C' = common time "C" symbol in older Encore).
    # Same semantic as 0x63 but produced by different Encore versions.
    e  = note_v0c2(0, 0, 0, fv=3, pitch=60)    # C4
    e += note_v0c2(240, 0, 0, fv=3, pitch=64)  # E4
    e += end_marker()
    return assemble(0xC2, [(meas_hdr(4, 4, glyph=0x43), e)])

def gen_v0c2_triplets():
    e = b''.join(note_v0c2(i*80,0,0,4,p) for i,p in enumerate([67,69,71,67,64,62]))
    e += end_marker()
    return assemble(0xC2,[(meas_hdr(2,4),e)],fill_ts=(2,4))

def gen_v0c2_triplet_pitch_in_semitone():
    # Encore 4.x file that stores pitch in semiTonePitch (+15) with a genuine 3:2
    # tuplet byte (0x32) in the tuplet slot (+13). A 2/4 bar: one triplet of three
    # eighth notes (C4/E4/G4) filling the first beat, then a quarter note (C5).
    # Bug: the pitch-swap heuristic copied the tuplet byte (0x32 = 50) into the
    # pitch, importing all three triplet notes as MIDI 50 and dropping the ratio.
    e  = note_v0c2_tuplet_pitch(  0, 0, 0, 4, 60, 0x32)   # triplet eighth C4
    e += note_v0c2_tuplet_pitch( 80, 0, 0, 4, 64, 0x32)   # triplet eighth E4
    e += note_v0c2_tuplet_pitch(160, 0, 0, 4, 67, 0x32)   # triplet eighth G4
    e += note_v0c2_tuplet_pitch(240, 0, 0, 3, 72, 0x00)   # quarter C5 (no tuplet)
    e += end_marker()
    return assemble(0xC2, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))

def gen_v0c2_spurious_semitone_flag():
    # v0xC2 sub-variant A (pitch at +13) where +15 holds a small spurious flag (1).
    # A chord C4/E4/G4 (quarter) at tick 0, all three notes carrying +15 == 1, then a
    # plain C5 quarter. Bug: the swap heuristic treated +15 != 0 as "pitch is at +15"
    # and imported every flagged note as MIDI 1 (C#-1); the three chord notes, now
    # sharing pitch 1, collapsed into a single note. The pitch must come from +13.
    e  = note_v0c2_spurious_semitone(  0, 0, 0, 3, 60, flag=1)   # C4
    e += note_v0c2_spurious_semitone(  0, 0, 0, 3, 64, flag=1)   # E4 (same tick: chord)
    e += note_v0c2_spurious_semitone(  0, 0, 0, 3, 67, flag=1)   # G4 (same tick: chord)
    e += note_v0c2_spurious_semitone(480, 0, 0, 3, 72, flag=3)   # C5, +15 == 3
    e += end_marker()
    return assemble(0xC2, [(meas_hdr(4, 4), e)])

def gen_v0c2_snap():
    e = b''.join(note_v0c2(i*80,0,0,4,p) for i,p in enumerate([67,69,71,67,64,62]))
    e += end_marker()
    return assemble(0xC2,[(meas_hdr(2,4),e)],fill_ts=(2,4))

def gen_v0c4_triplets():
    e1 = b''.join(note_v0c4(i*80,0,0,4,p,tuplet=0x32)
                   for i,p in enumerate([60,62,64,65,67,69,71,72,74]))
    e1 += end_marker()
    e2 = b''.join(note_v0c4(i*240,0,0,3,p) for i,p in enumerate([60,64,67]))
    e2 += end_marker()
    return assemble(0xC4,[(meas_hdr(3,4),e1),(meas_hdr(3,4),e2)],fill_ts=(3,4))

def gen_v0c4_irregular_measure_len_reduced():
    """A 2/4 bar overfilled with nine eighth-note triplets: content = 9 * 1/12 = 9/12 = 3/4, one
    beat past the 2/4 nominal. Under the IrregularMeasure strategy the bar is extended to hold the
    content, and its actual duration must be stored in lowest terms (3/4), not the raw unreduced
    9/12 that summing triplet ticks (denominator 12/24/96) produces. Reproduces the 99/96 and 21/24
    measure lengths seen in real files (Ritmo de Tango, Ritmo de fandango castellano)."""
    e = b''.join(note_v0c4(i*80, 0, 0, fv=4, pitch=60, tuplet=0x32) for i in range(9))
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))

def gen_v0c4_tuplet_sort():
    e  = note_v0c4(0,  0,0,fv=3,pitch=67,tuplet=0x00)
    e += note_v0c4(0,  0,0,fv=4,pitch=60,tuplet=0x32)
    e += note_v0c4(80, 0,0,fv=4,pitch=64,tuplet=0x32)
    e += note_v0c4(160,0,0,fv=4,pitch=67,tuplet=0x32)
    e += note_v0c4(240,0,0,fv=3,pitch=60,tuplet=0x00)
    e += end_marker()
    return assemble(0xC4,[(meas_hdr(2,4),e)],fill_ts=(2,4))

def gen_v0c4_counter_bytes():
    e  = note_v0c4(0,  0,0,fv=3,pitch=60,tuplet=0x41)
    e += note_v0c4(240,0,0,fv=3,pitch=64,tuplet=0x43)
    e += end_marker()
    return assemble(0xC4,[(meas_hdr(2,4),e)],fill_ts=(2,4))

def gen_v0xa6_basic():
    def mhdr_a6(tsNum,tsDen,bpm=100):
        h=bytearray(0x1A)
        struct.pack_into('<H',h,0,bpm)
        struct.pack_into('<HH',h,4,240,240*tsNum*4//tsDen)
        h[8],h[9]=tsNum,tsDen
        return bytes(h)
    def one_meas(notes,hdr):
        e=b''.join(note_v0xa6(t,0,0,4,p) for t,p in notes)+end_marker()
        return b'MEAS'+struct.pack('<I',len(e))+hdr+e
    pre=bytearray(SKELETON_PRE); pre[4]=0xA6
    struct.pack_into('<h',pre,0x34,2)
    m1=one_meas([(0,0),(120,2),(240,4),(360,7)],mhdr_a6(2,4))
    m2=one_meas([(0,9),(120,11),(240,12),(360,14)],mhdr_a6(2,4))
    return bytes(pre)+m1+m2+SKELETON_POST

# ---------------------------------------------------------------------------
# Genuine v0xA6 builder (Encore 2.x/4.0): real 0xA6 header with the global
# staff-size selector at byte 0x8D, plus 64-byte TK blocks with MIDI at
# content+52. Reproduces the on-disk v0xA6 instrument/header layout so the
# v0xA6-specific MIDI and staff-size reads can be exercised.
# ---------------------------------------------------------------------------
def build_v0xa6(instruments, meas_list, staff_size=1, keyIndex=0, clefs=None, sizes=None,
                channels=None, magic=b'SCOW', chu_versio=592, end='<'):
    """instruments: list of (name:str, midi_1indexed:int, key_signed:int).
    staff_size: global 1-4 selector at header 0x8D (1=60%, 2=75%, 3=100%, 4=130%).
    keyIndex: Encore key-signature index (0=C, 2=Bb, 9=D, 10=A, 11=E...) written at
    offset 14 of every 22-byte LINE staff entry, the real v0xA6 key-signature location.
    clefs/sizes: per-staff clef and 0-indexed display size, written at offsets 13 and 12 of the
    same entry. channels: per-instrument 1-indexed MIDI channel, stored from zero at TK content+48,
    the four bytes that precede the program in this generation."""
    n = len(instruments)
    hdr = bytearray(0xA6)
    hdr[0:4] = magic
    # The field at +4 is the offset of the first block. A Windows container has it as a
    # little-endian word, a macOS one as a big-endian word, so packing it follows the byte order.
    struct.pack_into(end+'I', hdr, 4, 0xA6)
    struct.pack_into(end+'H', hdr, 0x28, chu_versio)   # format version
    struct.pack_into(end+'h', hdr, 0x2E, 1)        # lineCount
    struct.pack_into(end+'h', hdr, 0x30, 1)        # pageCount
    hdr[0x32] = n                               # instrumentCount
    hdr[0x33] = 0                               # staffPerSystem (v0xA6 stores 0)
    struct.pack_into(end+'h', hdr, 0x34, len(meas_list))   # measureCount
    hdr[0x52] = 8                               # 0x52 is unrelated in v0xA6 (invalid 8)
    hdr[0x8D] = staff_size & 0xFF               # global staff-size selector (v0xA6)
    tks = b''
    for i, (name, midi, key) in enumerate(instruments):
        content = bytearray(56)
        nm = name.encode('latin1')[:40]
        content[0:len(nm)] = nm                 # name, NUL-terminated by zero fill
        content[42] = key & 0xFF                # Key/octave transpose (content+42)
        if channels and channels[i]:
            content[48] = (channels[i] - 1) & 0xFF   # MIDI channel, stored from zero
        content[52] = midi & 0xFF               # MIDI program (content+52 == block+60)
        tk_magic = ('TK%02d' % i).encode('ascii')
        tks += tk_magic + struct.pack(end+'I', 64) + bytes(content)
    # v0xA6 LINE: 14-byte pre-header (skip10 + start u16 + measureCount u8 + pad), then one
    # 22-byte staff entry per instrument. The written key index sits at entry offset 14 and a
    # 0x0E 0xFC marker at offset 16 bounds the run (matches real MusicTime/Encore-2.x files).
    line = bytearray(14)
    struct.pack_into(end+'H', line, 10, 0)         # start
    line[12] = len(meas_list) & 0xFF            # measureCount
    for i in range(n):
        ent = bytearray(22)
        if sizes:
            ent[12] = sizes[i] & 0xFF
        if clefs:
            ent[13] = clefs[i] & 0xFF
        ent[14] = keyIndex & 0xFF
        if clefs or sizes:
            ent[15] = i & 0xFF          # staff index, written by the fixtures that exercise the entry
        ent[16] = 0x0E
        ent[17] = 0xFC
        line += ent
    line_block = b'LINE' + struct.pack(end+'I', len(line)) + bytes(line)
    body = b''.join(meas_list)
    return bytes(hdr) + tks + line_block + body

def _mhdr_a6(tsNum, tsDen, bpm=100, end='<'):
    h = bytearray(0x1A)
    struct.pack_into(end+'H', h, 0, bpm)
    struct.pack_into(end+'HH', h, 4, 240, 240 * tsNum * 4 // tsDen)
    h[8], h[9] = tsNum, tsDen
    return bytes(h)

def _meas_a6(note_specs, tsNum=2, tsDen=4, end='<'):
    e = b''.join(note_v0xa6(t, v, s, fv, p, end=end) for (t, v, s, fv, p) in note_specs) + end_marker()
    return b'MEAS' + struct.pack(end+'I', len(e)) + _mhdr_a6(tsNum, tsDen, end=end) + e

# ===========================================================================
# structure_musictime_windows.mus and structure_musictime_mac.mus
# MusicTime is Passport's smaller sibling of Encore and writes the same file with
# its own magic: MTIW on Windows, MTIM on macOS, the second big-endian the way SCO5
# is. Both carry the version byte of the compact 2.x generation and format 2.62, so
# they read with the same geometry as an Encore 2.x file. Same two notes in each, so
# the pair also proves the byte order is honoured.
# ===========================================================================
def gen_musictime_windows():
    m = _meas_a6([(0, 0, 0, 3, 0), (240, 0, 0, 3, 4)])
    return build_v0xa6([('Melody', 74, 0)], [m], magic=b'MTIW', chu_versio=0x0262)


def gen_musictime_mac():
    m = _meas_a6([(0, 0, 0, 3, 0), (240, 0, 0, 3, 4)], end='>')
    return build_v0xa6([('Melody', 74, 0)], [m], magic=b'MTIM', chu_versio=0x0262, end='>')


# instruments_v0xa6_midi_program.enc
# v0xA6 stores the MIDI program at TK content+52. The reader used to skip it
# (midiProgram=0 -> Grand Piano). MIDI=119 (>=113) must route to a drumset. Name
# "Voz" is <4 chars so only the MIDI path can trigger the match.
def gen_v0xa6_midi_program():
    m = _meas_a6([(0, 0, 0, 4, 0)])
    return build_v0xa6([('Voz', 119, 0)], [m], staff_size=1)

# structure_v0xa6_score_size.enc
# v0xA6 stores the global staff size at header byte 0x8D, NOT 0x52. staff_size=1
# must yield 60% for every staff.
def gen_v0xa6_score_size():
    m = _meas_a6([(0, 0, 0, 4, 0), (0, 0, 1, 4, 7)])
    return build_v0xa6([('Vz1', 0, 0), ('Vz2', 0, 0)], [m], staff_size=1)

# structure_v0xa6_key_signature.enc
# v0xA6 stores the written key signature at offset 14 of each 22-byte LINE staff entry,
# NOT where v0xC2/C4 keep it (the generic 30-byte parse reads garbage there, and v0xA6's
# header staffPerSystem reads 0 so staffData is empty). keyIndex=10 = A major (3 sharps).
# Both staves must import with concert key = 3 sharps.
def gen_v0xa6_key_signature():
    m = _meas_a6([(0, 0, 0, 4, 0), (0, 0, 1, 4, 7)])
    return build_v0xa6([('Vz1', 0, 0), ('Vz2', 0, 0)], [m], staff_size=1, keyIndex=10)

def gen_v0xa6_lyrics_and_stafftext():
    """Encore 2.x note carrying a compact lyric syllable plus a STAFFTEXT ornament that
    references a TEXT-block entry. Both the compact lyric and the compact-ornament TEXT
    index were dropped before the v0xA6 compact-layout fix; this exercises both end to end."""
    e  = note_v0xa6(0, 0, 0, 3, 0)          # quarter C4
    e += lyric_v0xa6(0, 0, 0, "loco")       # lyric syllable under it
    e += stafftext_orn_v0xa6(0, 0, 0, 0)    # STAFFTEXT -> TEXT entry 0
    e += end_marker()
    meas = b'MEAS' + struct.pack('<I', len(e)) + _mhdr_a6(2, 4) + e
    f = build_v0xa6([('Voz', 1, 0)], [meas], staff_size=1)
    return f + text_block_v0xa6(["dolce"])   # entry 0 = "dolce" (non-tempo -> stays StaffText)

def gen_v0xa6_stafftext_placement():
    """Two STAFFTEXT ornaments on one note: one with a positive placement value (above) and one
    with a negative value (below). The v0xA6 compact ornament stores this y at +6 from the
    type/voice byte; reading it at the v0xC4 offset (+10) yields 0 and forces both above."""
    e  = note_v0xa6(0, 0, 0, 3, 0)
    e += stafftext_orn_v0xa6(0, 0, 0, 0, yoffset=40)    # entry 0 -> above
    e += stafftext_orn_v0xa6(0, 0, 0, 1, yoffset=-40)   # entry 1 -> below
    e += end_marker()
    meas = b'MEAS' + struct.pack('<I', len(e)) + _mhdr_a6(2, 4) + e
    f = build_v0xa6([('Voz', 1, 0)], [meas], staff_size=1)
    return f + text_block_v0xa6(["cresc.", "espressivo"])

def gen_v0xa6_note_position_and_rest_fields():
    """Two notes carrying an explicit staff position at +9 (A4 = 5, B3 = -1), then a rest whose
    duration word at +12 is 960 so its high byte is 3, followed by a note at tick 480 so the byte
    just past the rest is 0xE0. Reading the later layout's slots gave every note position -128 and
    turned the rest's duration byte into a tuplet descriptor and the neighbour's byte into a dot
    control."""
    e  = note_v0xa6(0,   0, 0, 3, 9,  position=5)     # A4, five diatonic steps above middle C
    e += note_v0xa6(240, 0, 0, 3, -1, position=-1)    # B3, one step below
    e += rest_v0xa6(360, 0, 0, 1, dur_ticks=960)
    e += note_v0xa6(480, 0, 0, 3, 0,  position=0)     # middle C, tick 480 -> 0xE0 past the rest
    e += note_v0xa6(481, 0, 0, 3, 2,  position=1)     # tick 481 = 0xE1 0x01, the bytes at +20/+21
    e += end_marker()
    meas = b'MEAS' + struct.pack('<I', len(e)) + _mhdr_a6(2, 4) + e
    return build_v0xa6([('Voz', 1, 0)], [meas], staff_size=1)

def gen_v0xa6_two_verse_alignment():
    """Two lyric verses over three notes. Encore stores verse 2 (voice 1) with tick=0 on EVERY
    syllable; the real horizontal position is the xoffset (kie), identical to verse 1's syllable at
    the same spot. The importer must align verse 2 to the same notes as verse 1 (via xoffset), not
    collapse every verse-2 syllable onto the first notes."""
    notes = (note_v0xa6(0, 0, 0, 3, 0)
             + note_v0xa6(240, 0, 0, 3, 4)
             + note_v0xa6(480, 0, 0, 3, 7))
    # verse 1 (voice 0): correct ticks, x-offsets 10/50/90
    v1 = (lyric_v0xa6(0,   0, 0, "A", kie=10)
          + lyric_v0xa6(240, 0, 0, "B", kie=50)
          + lyric_v0xa6(480, 0, 0, "C", kie=90))
    # verse 2 (voice 1): all tick 0, same x-offsets as verse 1, but emitted in REVERSE x-offset
    # order (as Encore does, e.g. storing the "2." first syllable last) so a tick-only greedy match
    # would scramble them; only xoffset alignment recovers X/Y/Z on notes 1/2/3.
    v2 = (lyric_v0xa6(0, 1, 0, "Z", kie=90)
          + lyric_v0xa6(0, 1, 0, "Y", kie=50)
          + lyric_v0xa6(0, 1, 0, "X", kie=10))
    e = notes + v1 + v2 + end_marker()
    meas = b'MEAS' + struct.pack('<I', len(e)) + _mhdr_a6(3, 4) + e
    return build_v0xa6([('Voz', 1, 0)], [meas], staff_size=1)

def gen_v0xa6_melisma_verse_alignment():
    """A final melisma word: two notes and a single held syllable per verse. Encore stores verse 1's
    syllable at the melisma's END note (tick 360) and collapses verse 2 to tick 0, but both are sung
    on the first note (near-equal x-offsets). No verse spans the bar, so tick-based matching sends
    verse 1 to note 2; only x-offset alignment keeps both on note 1."""
    notes = note_v0xa6(0, 0, 0, 3, 0) + note_v0xa6(360, 0, 0, 3, 7)
    # verse 1 (voice 0): single syllable stored at the end note (tick 360), x-offset 59
    v1 = lyric_v0xa6(360, 0, 0, "peace", kie=59)
    # verse 2 (voice 1): single syllable collapsed to tick 0, near-equal x-offset 64
    v2 = lyric_v0xa6(0, 1, 0, "born", kie=64)
    e = notes + v1 + v2 + end_marker()
    meas = b'MEAS' + struct.pack('<I', len(e)) + _mhdr_a6(2, 4) + e
    return build_v0xa6([('Voz', 1, 0)], [meas], staff_size=1)

# ---------------------------------------------------------------------------
# File generators, new set (replacing Encore 5 example files)
# ---------------------------------------------------------------------------

def gen_v0c4_corrupted():
    """Exercises: tuplet=0xFF, invalid faceValue=0, voice>=4, open SLURSTART.
    Replaces: Beethoven.enc, Opus 27 First Movement.enc.
    Notes use fv=3 (quarter, realDur=240) so realDur*3 != base*2 and
    no implied triplet is triggered by the 0xFF note."""
    e  = note_v0c4(0,  0,0,fv=3,pitch=60,tuplet=0xFF)  # degenerate 15:15 ratio
    e += ornament_v0c4(0,0,0,tipo=0x21)                  # SLURSTART, no SLURSTOP
    e += note_v0c4(240,0,0,fv=0,pitch=60,tuplet=0x00)  # invalid faceValue=0 → SKIP
    e += note_v0c4(480,0,0,fv=3,pitch=64,tuplet=0x00)  # plain quarter
    e += note_v0c4_voice4(720,0,fv=4,pitch=67)          # voice=4 (>= VOICES) → SKIP
    e += end_marker()
    return assemble(0xC4,[(meas_hdr(4,4),e)])

def gen_v0c4_swing():
    """Exercises: dotted rest (realDur=180), tiny duration skip (realDur=2).
    Replaces: Well, Licky Hear.enc."""
    # REST at tick=0 (next element at 180) → realDur=180=(120*3/2) → dotted eighth
    e  = rest_v0c4(0,  0,0,fv=4)
    # NOTE at tick=180 (next at 182) → realDur=2 < 15 → SKIPPED (timing artifact)
    e += note_v0c4(180,0,0,fv=5,pitch=67,tuplet=0)
    # NOTE at tick=182 → processed normally
    e += note_v0c4(182,0,0,fv=4,pitch=69,tuplet=0)
    e += end_marker()
    return assemble(0xC4,[(meas_hdr(2,4),e)],fill_ts=(2,4))

def gen_v0c4_grace():
    """Exercises grace note filtering: only fv>=4 (eighth+) accepted as grace.
    Replaces: Grace.enc, Beethoven.enc (grace note behavior)."""
    # fv=4 (eighth) + grace bytes -> ACCIACCATURA grace note
    e  = note_v0c4_grace(0,  0,0,fv=4,pitch=60,grace1=0x20,grace2=0x04)
    # fv=3 (quarter) + same grace bytes -> NOT grace (fv<4 filter)
    e += note_v0c4_grace(120,0,0,fv=3,pitch=64,grace1=0x20,grace2=0x04)
    e += note_v0c4(360,0,0,fv=3,pitch=67,tuplet=0)  # plain quarter to fill
    e += end_marker()
    return assemble(0xC4,[(meas_hdr(2,4),e)],fill_ts=(2,4))

# ===========================================================================
# importer_grace1_0x30_normal_notes.enc
#
# 4/4 measure with four eighth notes at distinct ticks (0, 120, 240, 360),
# each with grace1=0x30 (bits 0x20 and 0x10 both set) and grace2=0x04. The
# eighth face value (>= 4) clears the grace-size filter so graceType() runs.
# Encore uses grace1 & 0x30 == 0x20 for an appoggiatura and == 0x10 for an
# inner grace; == 0x30 is NOT a grace. Before the fix, graceType() treated
# any grace1 & 0x30 > 0x10 (so also 0x30) as an appoggiatura, so these normal
# notes were queued as grace chords and, with no principal chord to attach to,
# discarded -- the measure ended up with only a rest.
# ===========================================================================
def gen_v0c4_grace1_0x30_normal_notes():
    e  = note_v0c4_grace(  0, 0, 0, fv=4, pitch=60, grace1=0x30, grace2=0x04)
    e += note_v0c4_grace(120, 0, 0, fv=4, pitch=62, grace1=0x30, grace2=0x04)
    e += note_v0c4_grace(240, 0, 0, fv=4, pitch=64, grace1=0x30, grace2=0x04)
    e += note_v0c4_grace(360, 0, 0, fv=4, pitch=65, grace1=0x30, grace2=0x04)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# A cue note: grace1 0x20 (small bit) + grace2 0x01 (cue). It keeps its full quarter value but must
# import small (Note::setSmall) and muted (Note::setPlay(false)), routed to the spare cue voice.
# A beamed grace group (grace1 & 0x10) is a melodic run of separate grace notes (own stems, joined by
# a beam), NOT a stacked chord. m1: two beamed graces (0x30) at one tick before a principal -> must be
# TWO grace chords. m2: two non-beamed graces (0x20) at one tick -> ONE grace chord of two notes
# (the stacked grace-chord case the merge protects). Keeps both sides of the discriminator honest.
def gen_v0c4_grace_beamed_group():
    m1 = (note_v0c4_grace(0, 0, 0, fv=5, pitch=72, grace1=0x30, grace2=0x04)
          + note_v0c4_grace(0, 0, 0, fv=5, pitch=74, grace1=0x30, grace2=0x04)
          + note_v0c4(0, 0, 0, fv=3, pitch=60)
          + note_v0c4(240, 0, 0, fv=3, pitch=64)
          + note_v0c4(480, 0, 0, fv=3, pitch=65)
          + note_v0c4(720, 0, 0, fv=3, pitch=67)
          + end_marker())
    m2 = (note_v0c4_grace(0, 0, 0, fv=5, pitch=72, grace1=0x20, grace2=0x04)
          + note_v0c4_grace(0, 0, 0, fv=5, pitch=74, grace1=0x20, grace2=0x04)
          + note_v0c4(0, 0, 0, fv=3, pitch=60)
          + note_v0c4(240, 0, 0, fv=3, pitch=64)
          + note_v0c4(480, 0, 0, fv=3, pitch=65)
          + note_v0c4(720, 0, 0, fv=3, pitch=67)
          + end_marker())
    return assemble(0xC4, [(meas_hdr(4, 4), m1), (meas_hdr(4, 4), m2)], fill_ts=(4, 4))


def gen_v0c4_cue_note():
    e  = note_v0c4_grace(0, 0, 0, fv=3, pitch=67, grace1=0x20, grace2=0x01)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# A principal quarter (span 0..240) with an acciaccatura at tick 120, inside that span (contiguous,
# no silence between). The grace belongs to the preceding note and must import as a grace-AFTER
# (a GRACE*_AFTER), staying in the same bar rather than jumping to a later note.
def gen_v0c4_grace_after_contiguous():
    e  = note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += note_v0c4_grace(120, 0, 0, fv=4, pitch=67, grace1=0x20, grace2=0x04)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# A quarter at tick 0 followed only by a grace at tick 360 (a dotted-quarter distance). The grace has
# no rhythmic footprint and must NOT inflate the quarter into a dotted quarter: it stays a plain
# quarter + rest. The trailing grace, having no principal to attach to, becomes a cue note.
def gen_v0c4_grace_trailing_no_dot():
    e  = note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += note_v0c4_grace(360, 0, 0, fv=5, pitch=67, grace1=0x20, grace2=0x04)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# Isolates grace1 bit 0x20 (small) and grace2 bit 0x01 (mute) with one standalone note per bar:
#   m1 cue muted (0x20/0x01), m2 cue sounding (0x20/0x00), m3 normal muted (0x00/0x01), m4 normal.
# Each small note is alone in its bar (no principal to ornament) so it imports as a cue, not a grace.
def gen_v0c4_cue_mute_flags():
    m1 = note_v0c4_grace(0, 0, 0, fv=3, pitch=69, grace1=0x20, grace2=0x01) + end_marker()
    m2 = note_v0c4_grace(0, 0, 0, fv=3, pitch=69, grace1=0x20, grace2=0x00) + end_marker()
    m3 = note_v0c4_grace(0, 0, 0, fv=3, pitch=69, grace1=0x00, grace2=0x01) + end_marker()
    m4 = note_v0c4_grace(0, 0, 0, fv=3, pitch=69, grace1=0x00, grace2=0x00) + end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), m1), (meas_hdr(4, 4), m2),
                           (meas_hdr(4, 4), m3), (meas_hdr(4, 4), m4)], fill_ts=(4, 4))

def note_v0c2_grace(tick, voice, staffIdx, fv, pitch, grace1, grace2, xoff=0):
    """22-byte v0xC2 note carrying grace1 (+6), grace2 (+7) and xoffset (+10). Pitch at +13."""
    d = bytearray(19)
    d[0]=22; d[1]=staffIdx&0x3F; d[2]=fv
    d[3]=grace1; d[4]=grace2; d[7]=xoff; d[10]=pitch
    return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)


# A small note (grace1 0x20) with no slash and no principal note to ornament is a cue: full value,
# drawn small, silent when the mute bit (grace2 0x01) is set. Two measures:
#   m1: two same-tick, same-voice quarters at nearly-equal xoffsets (a chord) with grace1 0x30 /
#       grace2 0x01. Must import as ONE small, silent chord of two notes, not split into two.
#   m2: a lone slashed quarter (grace1 0x20 / grace2 0x04), a real grace, which must import SMALL.
def gen_v0c2_small_flag_chord():
    m1  = note_v0c2_grace(0, 0, 0, fv=3, pitch=71, grace1=0x30, grace2=0x01, xoff=8)
    m1 += note_v0c2_grace(0, 0, 0, fv=3, pitch=74, grace1=0x70, grace2=0x01, xoff=6)
    m1 += end_marker()
    m2  = note_v0c2_grace(0, 0, 0, fv=4, pitch=67, grace1=0x20, grace2=0x04, xoff=8)
    m2 += end_marker()
    return assemble(0xC2, [(meas_hdr(4, 4), m1), (meas_hdr(4, 4), m2)], fill_ts=(4, 4))


def gen_v0c4_rest_not_chord_anchor():
    """A rest must not act as a chord-extension anchor. With the bug, prevMidiTick
    was updated for both notes and rests, so a NOTE arriving at the same MIDI tick
    as a recent REST was mis-detected as a chord extension. The reuse path then
    silently replaced the rest's segment with the note's chord while cumTick still
    carried the rest's contribution, producing voice-content / cumTick mismatch.
    Layout: 2/4 measure. A triplet quarter rest (fv=3, tuplet 3:2, actualTicks=1/6)
    at tick 0, followed by a triplet quarter NOTE at tick 0 (same MIDI tick).
    With the fix the note is placed at elemTick = measTick + 1/6 (after the rest),
    creating two separate elements that together fill the tuplet (2 of 3 members).
    Two more triplet quarter notes complete the group and the measure."""
    e  = rest_v0c4_tup(0, 0, 0, fv=3, tuplet=0x32)          # triplet Q rest
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60, tuplet=0x32) # same MIDI tick, NOT chord-ext
    e += note_v0c4( 320, 0, 0, fv=3, pitch=62, tuplet=0x32)
    e += note_v0c4( 640, 0, 0, fv=3, pitch=64, tuplet=0x32) # closes 1st 3:2 group (3 members)
    # second triplet group to fill the remaining 1/4
    e += note_v0c4( 960, 0, 0, fv=3, pitch=65, tuplet=0x32)
    e += note_v0c4(1280, 0, 0, fv=3, pitch=67, tuplet=0x32)
    e += note_v0c4(1600, 0, 0, fv=3, pitch=69, tuplet=0x32)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))

def gen_v0c4_rest_coincident_with_note():
    """Encore stores a placeholder rest at the SAME tick as the beat's note in one voice (a voice
    that has a real note still gets a rest slot). The redundant, non-tuplet rest must be dropped so
    the note keeps beat 1: with the bug the rest is placed first (0..120) and pushes the note to
    tick 120, also overflowing the bar. Distinct from rest_not_chord_anchor, where same-tick TUPLET
    members are kept sequential. Layout: 2/4, voice 0 = eighth rest @0 + quarter @0 + quarter @beat2."""
    e  = rest_v0c4(0, 0, 0, fv=4)                 # placeholder eighth rest at tick 0
    e += note_v0c4(0, 0, 0, fv=3, pitch=60)       # quarter at tick 0 (coincident, the real note)
    e += note_v0c4(240, 0, 0, fv=3, pitch=62)     # quarter at beat 2 fills the 2/4 bar
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))

def gen_v0c4_rest_caps_in_open_tuplet():
    """When a rest's first-cap is skipped because a tuplet is still open
    (willBeTuplet=true), and the rest then closes the tuplet (it has no
    tuplet bytes), the second cap path applies. Until the fix the rest's
    `ticks()` stayed at the uncapped face value while cumTick advanced by
    the capped amount, producing a per-rest voice overshoot.
    Layout: 4/4 measure. 5 explicit 3:2 triplet quarters fill placedTicks
    to 5/6 in the open tuplet (the group has 3 face quarters but the file
    encodes 5 in a row, which the importer treats as one open group that
    never reaches groupFull-via-faceTicks). Then a half rest arrives:
    willBeTuplet=true skips the first cap; tt closes; advance=1/2 exceeds
    remaining=1/6; second cap caps the advance. The fix also caps the
    rest's `ticks()` so cr->actualTicks() matches the advance."""
    e  = note_v0c4(   0, 0, 0, fv=3, pitch=60, tuplet=0x32)
    e += note_v0c4( 320, 0, 0, fv=3, pitch=62, tuplet=0x32)
    e += note_v0c4( 640, 0, 0, fv=3, pitch=64, tuplet=0x32)
    e += note_v0c4( 960, 0, 0, fv=3, pitch=65, tuplet=0x32)
    e += note_v0c4(1280, 0, 0, fv=3, pitch=67, tuplet=0x32)
    e += rest_v0c4(1600, 0, 0, fv=2)                         # half rest, must be capped
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)])

def gen_v0c4_full_voice_skipped_via_loop():
    """Multi-stream voice switch must iterate past voices already filled.
    voice 0 fills the 2/4 measure with 4 eighth notes; voice 1 fills with a
    half rest. Then a 5th eighth in voice 0 (a second simultaneous stream)
    arrives at cumTick=1/2, triggering the multi-stream switch. A single
    switch lands on voice 1, which is also full from the rest. Without the
    loop the note was placed in already-full voice 1 and overran the measure;
    with the loop the switch continues to voice 2 (free) and the import is
    sane."""
    e  = note_v0c4(  0, 0, 0, fv=4, pitch=60, tuplet=0)
    e += note_v0c4(120, 0, 0, fv=4, pitch=62, tuplet=0)
    e += note_v0c4(240, 0, 0, fv=4, pitch=64, tuplet=0)
    e += note_v0c4(360, 0, 0, fv=4, pitch=65, tuplet=0)
    e += rest_v0c4(  0, 1, 0, fv=2)  # half rest in voice 1 fills it
    e += note_v0c4(480, 0, 0, fv=4, pitch=67, tuplet=0)  # 5th eighth -> multi-stream
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))

def gen_v0c4_merge_voices_non_overlapping():
    """Single staff whose two voices never sound at the same time.
    4/4 measure: voice 0 has a quarter C4 on beat 1 (tick 0), voice 1 has a
    quarter E4 on beat 2 (tick 240). The intervals [0,240) and [240,480) do
    not overlap, so with mergeVoices the staff collapses to voice 1 alone
    (C4 then E4); with mergeVoices off both voices survive."""
    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60)  # voice 0: quarter C4 on beat 1
    e += note_v0c4(240, 1, 0, fv=3, pitch=64)  # voice 1: quarter E4 on beat 2
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)])

def gen_v0c4_merge_voices_overlapping():
    """Single staff whose two voices genuinely overlap in time.
    4/4 measure: voice 0 has a half C4 spanning beats 1-2 (tick 0, [0,480)),
    voice 1 has a quarter E4 on beat 2 (tick 240, [240,480)). The intervals
    overlap and are not identical, so the staff cannot collapse: even with
    mergeVoices both voices must be preserved."""
    e  = note_v0c4(  0, 0, 0, fv=2, pitch=60)  # voice 0: half C4 over beats 1-2
    e += note_v0c4(240, 1, 0, fv=3, pitch=64)  # voice 1: quarter E4 on beat 2
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)])

def gen_v0c4_merge_voices_tremolo():
    """Single staff whose two voices never overlap, where the UPPER voice carries a
    single-note tremolo. 4/4 measure: voice 0 has a quarter C4 on beat 1 (tick 0),
    voice 1 has a quarter E4 on beat 2 (tick 240) with a per-note tremolo (artic
    0x42 -> R16, two strokes). The intervals [0,240) and [240,480) do not overlap,
    so with mergeVoices the voice-1 E4 is moved into voice 0; its tremolo must move
    with it. Without the fix the voice change rebuilt the destination chord and the
    tremolo was dropped."""
    e  = note_v0c4(        0, 0, 0, fv=3, pitch=60)                # voice 0: quarter C4 on beat 1
    e += note_v0c4_artic(240, 1, 0, fv=3, pitch=64, articUp=0x42)  # voice 1: quarter E4 + R16 tremolo
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)])

def gen_v0c4_isolated_explicit_tuplet_capped():
    """Non-tuplet second cap must also update chord->ticks. An explicit-but-
    not-validated tuplet note ends up treated as a plain note (tupAdv !=
    remaining at the isolated-explicit branch), but its full face value
    exceeds remaining, so the second cap shortens the advance. Previously
    the chord's ticks stayed at the face value while cumTick advanced by
    the capped amount, so chord->actualTicks() > advance and the voice
    overran by face - capped. Layout: 4/4 measure with 7 plain eighths
    (cumTick=7/8) plus one tuplet-flagged quarter (face=1/4, advance capped
    to remaining=1/8). After the fix the chord's duration is 1/8."""
    e  = b''.join(note_v0c4(i*120, 0, 0, fv=4, pitch=60+i, tuplet=0)
                  for i in range(7))
    e += note_v0c4(840, 0, 0, fv=3, pitch=70, tuplet=0x32)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)])

def gen_v0c4_rest_in_tuplet():
    """3:2 quarter triplet whose first member is a rest, followed by two
    eighth-note chords. Reproduces a bug where rest_v0c4 added 'advance' to
    tt.placedTicks twice, so the tracker saw the tuplet as full (1/2) when
    its content actually summed to 1/3. closeTuplet's `placedTicks < expected`
    shrink was skipped, the tuplet kept its default span 1/2, and checkMeasure
    reported the resulting non-standard gap as an Incomplete measure.
    Layout: 2/4 measure, quarter rest (tuplet 3:2) + eighth + eighth (same
    tuplet). After the fix the tuplet shrinks to 1/3, leaving 1/6 to be
    filled by ordinary rests."""
    e  = rest_v0c4_tup(  0, 0, 0, fv=3, tuplet=0x32)   # quarter rest in 3:2
    e += note_v0c4(    320, 0, 0, fv=4, pitch=60, tuplet=0x32)  # eighth in 3:2
    e += note_v0c4(    400, 0, 0, fv=4, pitch=64, tuplet=0x32)  # eighth in 3:2
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))

def gen_v0c4_grace_beam():
    """Regression for the beam-layout crash on grace + adjacent beamed eighths.
    With the old importer the grace chord was attached to a Segment, then included
    in a beam group with the following eighths. doLayout -> BeamTremoloLayout
    -> Chord::pagePos -> toChord(explicitParent()) asserted because the parent
    was a Segment, not the main Chord.
    Now the grace lives under the next main chord's graceNotes() and is not in
    any segment-level beam group; doLayout must complete without crash.
    Layout: 4/4 measure with grace eighth + 4 plain eighths + half rest."""
    e  = note_v0c4_grace(0,   0,0,fv=4,pitch=72,grace1=0x20,grace2=0x04)  # grace
    e += note_v0c4(120, 0,0,fv=4,pitch=60,tuplet=0)  # eighth
    e += note_v0c4(240, 0,0,fv=4,pitch=62,tuplet=0)  # eighth (beam with prev)
    e += note_v0c4(360, 0,0,fv=4,pitch=64,tuplet=0)  # eighth (beam group 2)
    e += note_v0c4(480, 0,0,fv=4,pitch=65,tuplet=0)  # eighth (beam with prev)
    e += rest_v0c4(600, 0, 0, fv=2)                  # half rest fills the rest
    e += end_marker()
    return assemble(0xC4,[(meas_hdr(4,4),e)])

# ===========================================================================
# notes_swing_offgrid.enc
# 3/4, 3 quarter notes at off-beat positions (bat-b1 m138 pattern).
# Note 3 at tick=560: realDur=160 triggers implied 3:2 triplet,
# but 560*2=1120 MS, 1120 % (480*2/3=320) = 160 ≠ 0 → off canonical grid
# → spurious triplet → removed by adjustMeasureTuplets.
# Result: 3 plain quarters → sanityCheck passes.
# ===========================================================================
def gen_v0c4_swing_offgrid():
    # Simulate swing-timed notes: positions 0, 320, 560 Encore ticks
    # (instead of canonical 0, 240, 480 for 3/4).
    # realDurs will be: 320, 240, 160 (computed from tick differences).
    e  = note_v0c4(0,   0, 0, fv=3, pitch=60, tuplet=0)  # tick=0,   realDur=320
    e += note_v0c4(320, 0, 0, fv=3, pitch=64, tuplet=0)  # tick=320, realDur=240
    e += note_v0c4(560, 0, 0, fv=3, pitch=67, tuplet=0)  # tick=560, realDur=160 → triggers implied triplet
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(3, 4), e)], fill_ts=(3, 4))

# ===========================================================================
# notes_canonical_triplet.enc
# 3/4, 3 quarter notes at CANONICAL triplet positions + 1 regular quarter.
# Notes at Encore ticks 0, 160, 320 → MS 0, 320, 640 (all divisible by 320).
# Each has realDur=160 → implied 3:2 triplet detected.
# Canonical check: 0%320=0, 320%320=0, 640%320=0 → all on-grid → preserved.
# + 1 plain quarter at tick=480 to fill remainder.
# Result: 3 triplet quarters (1/6 each) + 1 quarter = 3/4. Passes sanityCheck.
# ===========================================================================
def gen_v0c4_canonical_triplet():
    # 3 EXPLICIT triplet quarter notes (tuplet=0x32) + 1 plain quarter.
    # Explicit bytes ensure the triplets are preserved regardless of implied-detection logic.
    # Sum: 3*(1/4*2/3) + 1/4 = 1/2 + 1/4 = 3/4 = mLen. sanityCheck passes.
    e  = note_v0c4(0,   0, 0, fv=3, pitch=60, tuplet=0x32)  # explicit 3:2
    e += note_v0c4(160, 0, 0, fv=3, pitch=62, tuplet=0x32)
    e += note_v0c4(320, 0, 0, fv=3, pitch=64, tuplet=0x32)
    e += note_v0c4(480, 0, 0, fv=3, pitch=60, tuplet=0x00)  # plain quarter
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(3, 4), e)], fill_ts=(3, 4))

# ===========================================================================
# notes_overflow_extend.enc
# 2/4, 1 whole note (fv=1) at tick=0.
# A whole note actualTicks=1 > mLen=1/2 → overflow.
# adjustMeasureTuplets extends measure to 1 (whole note).
# sanityCheck: voices[0]=1 == mLen(new)=1 → passes.
# ===========================================================================
def gen_v0c4_overflow_extend():
    # Overflow via face-value mismatch: note 1 has fv=2 (half) but realDur=120
    # (only 1/8 of the measure).  realDuration2DurationType(120, 2) falls through
    # to V_HALF (1/2).  Note 2 at tick=120 with fv=2 and realDur=360 → V_QUARTER.
    # notes_sum = 1/2 + 1/4 = 3/4 > mLen=1/2 → overflow branch extends measure.
    e  = note_v0c4(0,   0, 0, fv=2, pitch=60, tuplet=0)  # realDur=120 → V_HALF (1/2)
    e += note_v0c4(120, 0, 0, fv=2, pitch=64, tuplet=0)  # realDur=360 → V_QUARTER(1/4)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))

# ===========================================================================
# notes_whole_rest_2_4.enc
# 2/4 measure with one whole-measure rest (fv=1, rdur=480 Encore ticks).
# With faceValue-cumulative placement: realDuration2DurationType(480, 1) = V_HALF.
# The rest gets V_HALF (= 1/2 = mLen), NOT V_WHOLE (= 1).
# This is a regression check: faceValue2DurationType(1) = V_WHOLE would break it.
# ===========================================================================
def gen_v0c4_mrest_followed_by_rest():
    """mrestCount=3 block followed by a single rest (not notes).
    Regression: the mrest must expand even when the successor is not a note measure.
    Layout: [note][note][mrest=3][whole rest][note]
    Expected: 2 + 3 + 1 + 1 = 7 MuseScore measures."""
    n = note_v0c4(0, 0, 0, 3, 60) + end_marker()        # C4 quarter
    mrest = rest_v0c4_mrest(0, 0, 0, 1, 3) + end_marker()  # whole rest, mrestCount=3
    single_rest = rest_v0c4(0, 0, 0, 1) + end_marker()  # whole rest, mrestCount=0
    n2 = note_v0c4(0, 0, 0, 3, 67) + end_marker()       # G4 quarter
    return assemble(0xC4, [
        (meas_hdr(4, 4), n),
        (meas_hdr(4, 4), n),
        (meas_hdr(4, 4), mrest),
        (meas_hdr(4, 4), single_rest),
        (meas_hdr(4, 4), n2),
    ])

def gen_v0c4_mrest_preceded_by_rest():
    """mrestCount=7 block preceded by two single-measure rests.
    Regression: a multi-measure rest must expand even when the predecessor MEAS block
    contains a plain single-measure rest (mrestCount=1).  Before the fix the guard
    encMeasHasSingleRest(*prev) incorrectly suppressed expansion.
    Layout: [rest][rest][mrest=7][note]  (assemble pads 4 blocks to min 6, adding 2 empty)
    Expected: 1 + 1 + 7 + 1 + 1(pad) + 1(pad) = 12 MuseScore measures.
    Without fix: 1 + 1 + 1 + 1 + 1 + 1 = 6 (the mrest=7 collapses to 1)."""
    single_rest = rest_v0c4(0, 0, 0, 1) + end_marker()       # whole rest, mrestCount=0
    mrest = rest_v0c4_mrest(0, 0, 0, 1, 7) + end_marker()    # whole rest, mrestCount=7
    n = note_v0c4(0, 0, 0, 3, 60) + end_marker()             # C4 quarter
    return assemble(0xC4, [
        (meas_hdr(4, 4), single_rest),
        (meas_hdr(4, 4), single_rest),
        (meas_hdr(4, 4), mrest),
        (meas_hdr(4, 4), n),
    ])

def gen_v0c4_mrest_consecutive_groups():
    """Two CONSECUTIVE multi-rest groups with DIFFERENT counts, the first also carrying a key change.
    Regression: (1) the old anti-cascade collapsed any mrest block whose predecessor was also a mrest
    block, so genuine consecutive groups lost measures; (2) the all-REST check rejected a mrest block
    that also held a companion KEYCHANGE element. Layout: [note][mrest=3 + keychange][mrest=2][note].
    Expected: 1 + 3 + 2 + 1 + 1(pad) + 1(pad) = 9. Without fix: 1 + 1 + 1 + 1 + 1 + 1 = 6."""
    n = note_v0c4(0, 0, 0, 3, 60) + end_marker()                                  # C4 quarter
    mrest3_kc = keychange_v0c4(0, 0, 0, tipo=8) + rest_v0c4_mrest(0, 0, 0, 1, 3) + end_marker()
    mrest2 = rest_v0c4_mrest(0, 0, 0, 1, 2) + end_marker()
    n2 = note_v0c4(0, 0, 0, 3, 67) + end_marker()                                 # G4 quarter
    return assemble(0xC4, [
        (meas_hdr(4, 4), n),
        (meas_hdr(4, 4), mrest3_kc),
        (meas_hdr(4, 4), mrest2),
        (meas_hdr(4, 4), n2),
    ])

def gen_v0c4_mrest_multistaff():
    """Multi-staff file (2 staves) with a mrest=7 block that has one REST element
    per staff inside the same MEAS block (elements.size()==2, both mrestCount=7).
    Regression: the old check 'elements.size() != 1' incorrectly suppressed expansion
    for any multi-staff file, collapsing the 7-measure rest to 1 measure.
    Layout (Encore): [mrest=7 (2 staves)][note (2 staves)]
    Expected: 7 + 1 = 8 MuseScore measures.
    Without fix: 1 + 1 = 2 MuseScore measures."""
    # Custom 194-byte SCOW header for a 2-staff file
    hdr = bytearray(194)
    hdr[0:4] = b'SCOW'
    hdr[4] = 0xC4
    struct.pack_into('<H', hdr, 0x28, 0x0420)   # chuVersio
    struct.pack_into('<h', hdr, 0x2E, 1)         # lineCount
    struct.pack_into('<h', hdr, 0x30, 1)         # pageCount
    hdr[0x32] = 2                                # instrumentCount
    hdr[0x33] = 2                                # staffPerSystem
    struct.pack_into('<h', hdr, 0x34, 2)         # measureCount

    # LINE block: 1 system covering both MEAS blocks, 2 staff entries (30 bytes each)
    def staff_entry(clef_byte, instr_staff_idx):
        e = bytearray(30)
        e[14] = clef_byte
        e[19] = 1   # showByte = visible
        e[21] = instr_staff_idx
        return bytes(e)

    line_data = (b'\x00' * 10
                 + struct.pack('<H', 0)         # start measure index
                 + bytes([2])                   # measureCount in this system = 2
                 + staff_entry(0, 0x00)         # staff 0
                 + staff_entry(0, 0x01))        # staff 1
    line_block = b'LINE' + struct.pack('<I', len(line_data)) + line_data

    # MEAS 1: both staves have mrestCount=7 (the multi-measure rest block)
    mrest_elems = (rest_v0c4_mrest(0, 0, 0, 1, 7)   # staff 0
                 + rest_v0c4_mrest(0, 0, 1, 1, 7)   # staff 1
                 + end_marker())
    meas1 = meas_block(meas_hdr(4, 4), mrest_elems)

    # MEAS 2: a note on each staff
    note_elems = (note_v0c4(0, 0, 0, 3, 60)   # C4, staff 0
                + note_v0c4(0, 0, 1, 3, 64)   # E4, staff 1
                + end_marker())
    meas2 = meas_block(meas_hdr(4, 4), note_elems)

    return bytes(hdr) + line_block + meas1 + meas2 + SKELETON_POST

def gen_v0c4_whole_rest_2_4():
    # Single whole-measure rest in 2/4 (rdur=480 in Encore ticks)
    e  = rest_v0c4(0, 0, 0, fv=1)   # whole-measure rest
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))

# ===========================================================================
# notes_offbeat_canonical.enc
# 2/4 measure with 2 quarter notes at off-beat MIDI ticks (0 and 241, not 0 and 240).
# With faceValue-cumulative placement: both notes land at canonical positions
# (cumTick 0 and 1/4) regardless of MIDI timing drift.
# Voice sums to exactly 2/4 = mLen, no gaps, no fills, sanityCheck passes cleanly.
# Previously (MIDI-tick placement) tick=241 → MS 482 ≠ 480, creating a 2-tick gap
# that checkMeasure couldn't fill exactly.
# ===========================================================================
def gen_v0c4_offbeat_canonical():
    e  = note_v0c4(0,   0, 0, fv=3, pitch=60, tuplet=0)  # tick=0, canonical beat 1
    e += note_v0c4(241, 0, 0, fv=3, pitch=64, tuplet=0)  # tick=241 (1 tick late), beat 2
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))


# ===========================================================================
# notes_explicit_tup_rdur_truncated.enc
# 6/8 measure that reproduces the chab1_c pattern:
# 3 explicit 3:2 triplet 8th notes (tup=0x32) where the LAST note's rdur is
# truncated to 30 (V_32ND) because the following rest starts at tick=660
# (only 30 Encore ticks after the note at tick=630).
#
# Without faceValue fix: note3 gets dt=V_32ND → triplet group sums to
#   (1/8 + 1/8 + 1/32)*(2/3) → wrong → sanityCheck fails.
# With faceValue fix: all 3 notes use fv=4 (8th) → dt=V_EIGHTH regardless
#   of rdur → triplet group = 3*(1/12) = 1/4 → sanityCheck passes.
# ===========================================================================
def gen_v0c4_explicit_tup_rdur_truncated():
    # Pattern (Encore ticks): rest@0, note@120, note@240, note@360(16th),
    #   triplet-8th@450, triplet-8th@540, triplet-8th@630, rest@660
    # rdur for note@630 = 660-630 = 30 (truncated by rest@660)
    e  = rest_v0c4( 0,   0, 0, fv=4)                          # 8th rest
    e += note_v0c4(120, 0, 0, fv=4, pitch=60, tuplet=0x00)    # plain 8th
    e += note_v0c4(240, 0, 0, fv=4, pitch=62, tuplet=0x00)    # plain 8th
    e += note_v0c4(360, 0, 0, fv=5, pitch=64, tuplet=0x00)    # 16th (rdur=90 → dotted)
    e += note_v0c4(450, 0, 0, fv=4, pitch=65, tuplet=0x32)    # triplet 8th 1
    e += note_v0c4(540, 0, 0, fv=4, pitch=67, tuplet=0x32)    # triplet 8th 2
    e += note_v0c4(630, 0, 0, fv=4, pitch=69, tuplet=0x32)    # triplet 8th 3 (rdur=30 truncated)
    e += rest_v0c4(660, 0, 0, fv=5)                           # 16th rest (causes truncation)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(6, 8), e)], fill_ts=(6, 8))


# ===========================================================================
# notes_partial_explicit_group.enc
# 4/4 measure with 3 explicit 3:2 triplet Q (complete group) followed by a
# 4th isolated Q with tup=0x32, then a plain Q.
#
# Without fix: note4 starts a new partial tuplet group → checkMeasure sees
#   partial tuplet ticks=1/2, breaks → sum=11/12 ≠ 4/4.
# With fix: note4 is NOT in validTupletGroupMember → treated as plain Q
#   → sum = 3*(1/6) + 1/4 + 1/4 = 1/2 + 1/2 = 1 = 4/4. PASS.
# ===========================================================================
def gen_v0c4_partial_explicit_group():
    # Ticks: 0, 160, 320 (3:2 triplet Q), 480 (isolated Q tup=0x32), 720 (plain Q)
    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60, tuplet=0x32)   # triplet Q 1
    e += note_v0c4(160, 0, 0, fv=3, pitch=64, tuplet=0x32)   # triplet Q 2
    e += note_v0c4(320, 0, 0, fv=3, pitch=67, tuplet=0x32)   # triplet Q 3 [group complete]
    e += note_v0c4(480, 0, 0, fv=3, pitch=65, tuplet=0x32)   # isolated Q (NOT valid group)
    e += note_v0c4(720, 0, 0, fv=3, pitch=60, tuplet=0x00)   # plain Q
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ---------------------------------------------------------------------------
# ===========================================================================
# notes_dotted_note_capping.enc
# 2/4 measure: 3 sixteenth notes followed by a dotted quarter.
# cumTick after the 3 sixteenths = 3/16. remaining = 2/4 - 3/16 = 5/16.
# Face value of the quarter = 1/4 = 4/16 ≤ 5/16 → old code did NOT cap.
# Dotted quarter = 3/8 = 6/16 > 5/16 → new code caps to V_QUARTER (1/4).
# Without fix: chord gets ticks=3/8 → sum=3/16+3/8=9/16 > 2/4 → FAIL.
# With fix:    chord capped to 1/4 → sum=3/16+1/4+1/16(fill)=8/16 → PASS.
#
# rdur for the last note (tick=120) = durTicks-120 = 480-120 = 360.
# realDuration2DurationType(360, 3=Q) → V_QUARTER. calcDots(360, 3) → 1 dot.
# Full dotted value: (1/4)*(3/2) = 3/8 = 6/16 > remaining=5/16 → cap.
# ===========================================================================
def gen_v0c4_dotted_note_capping():
    e  = note_v0c4(  0, 0, 0, fv=5, pitch=60, tuplet=0)  # 16th, rdur=40
    e += note_v0c4( 40, 0, 0, fv=5, pitch=62, tuplet=0)  # 16th, rdur=40
    e += note_v0c4( 80, 0, 0, fv=5, pitch=64, tuplet=0)  # 16th, rdur=40
    e += note_v0c4(120, 0, 0, fv=3, pitch=65, tuplet=0)  # Q, rdur=360 → dotted Q
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))


# ===========================================================================
# notes_mixed_value_tuplet.enc
# 2/4 measure reproducing the besamemucho-style mixed-value tuplet pattern:
# - Complete 3:2 group: Q(0x32), Q(0x32), 8th(0x32), MIXED face values.
#   group advance = 1/6+1/6+1/12 = 5/12 < default startTuplet ticks 1/2.
#   Without exact-ticks fix: tuplet->ticks()=1/2, next note at 5/12 < 1/2 →
#     checkMeasure breaks → stray large fill → corrupted.
#   With fix: tuplet->ticks()=5/12, expectedPos=5/12 → no break, no fill.
# - Isolated 8th (tup=0x32): face-value advance=(1/8)*(2/3)=1/12 = remaining=1/12.
#   Creates a partial tuplet so checkMeasure spans exactly to mLen=2/4=1/2.
#
# mLen=2/4=1/2=6/12. After complete group: cumTick=5/12. remaining=1/12.
# Isolated 8th advance=1/12 → cumTick=1/2=mLen → no fills.
# Expected sum: 5/12 + 1/12 = 6/12 = 1/2 = 2/4. PASS.
#
# Ticks (Encore, 240/quarter): 0=Q, 160=Q, 320=8th, 400=8th(isolated)
# ===========================================================================
def gen_v0c4_mixed_value_tuplet():
    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60, tuplet=0x32)  # Q in 3:2 group
    e += note_v0c4(160, 0, 0, fv=3, pitch=64, tuplet=0x32)  # Q in 3:2 group
    e += note_v0c4(320, 0, 0, fv=4, pitch=67, tuplet=0x32)  # 8th in 3:2 group [COMPLETE]
    e += note_v0c4(400, 0, 0, fv=4, pitch=65, tuplet=0x32)  # isolated 8th: fills remaining 1/12
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))


# ===========================================================================
# notes_v0c2_implied_group_boundary.enc
# v0xC2 2/4 measure: 3 implied 3:2 triplet 16ths (rdur=40 each) forming a
# complete group, followed by an ISOLATED 16th (rdur=40) that is NOT in the
# validated group.
#
# Bug: after the complete group closes (groupFull=true), the isolated note
# detects an implied triplet via `tt.inTuplet()=true` (group still open at
# detection time), starts a NEW unvalidated group, and places the note with
# 1/24 advance instead of 1/16. cumTick overshoots → cascading fills → FAIL.
#
# Fix: add !tt.groupFull() to the implied detection guard. The isolated note
# is then treated as a plain 16th. Sum = 1/8+1/16+1/16+3*(1/24)+1/16+1/16
#                                       = 6+3+3+6+3+3 = 24/48 = 1/2 = PASS.
#
# Ticks (Encore): 0=8th, 120=16th, 180=16th, 240-280-320=triplet16th (group),
#                 360=isolated16th (plain), 400=16th(capped from 8th).
# This is the arroyb.enc measure-4 staff-5 pattern.
# ===========================================================================
def gen_v0c2_implied_group_boundary():
    # Notes at rdur spacings: 120, 60, 60, 40, 40, 40, 40, 80
    # Ticks: 0,120,180,240,280,320,360,400  (durTicks=480 for 2/4)
    e  = note_v0c2(  0, 0, 0, fv=4, pitch=62)   # 8th  rdur=120
    e += note_v0c2(120, 0, 0, fv=5, pitch=64)   # 16th rdur=60
    e += note_v0c2(180, 0, 0, fv=5, pitch=62)   # 16th rdur=60
    e += note_v0c2(240, 0, 0, fv=5, pitch=64)   # 16th rdur=40 → implied 3:2 [group]
    e += note_v0c2(280, 0, 0, fv=5, pitch=65)   # 16th rdur=40 [group]
    e += note_v0c2(320, 0, 0, fv=5, pitch=64)   # 16th rdur=40 [group COMPLETE]
    e += note_v0c2(360, 0, 0, fv=5, pitch=62)   # 16th rdur=40 NOT in group (isolated)
    e += note_v0c2(400, 0, 0, fv=4, pitch=59)   # 8th  rdur=80 → cap to 16th (remaining=1/16)
    e += end_marker()
    return assemble(0xC2, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))


# ===========================================================================
# notes_v0c2_capped_tuplet_note.enc  (generated inline; no separate write call)
# v0xC2 2/4 measure: 2 quarter notes + 3:2 triplet group of 3 eighth notes
# where the LAST triplet eighth has remaining < tuplet_advance (1/12).
#
# Structure: Q(240) Q(480) + 3 implied triplet 8ths (rdur=80 each).
# After 2 Qs: cumTick=1/2=mLen. No room for the triplet.
# So we use a smaller first section that leaves 1/6 for the triplet:
# 3 eighth notes (1/8 each = 3/8) + implied triplet 8ths (1/12 each).
# After 3 eighths: cumTick=3/8. 3 triplet eighths = 3*(1/12)=1/4. Total=5/8>1/2.
# Last triplet note: remaining=1/2-3/8-2*(1/12)=1/2-3/8-1/6=12/24-9/24-4/24=-1/24<0!
# Hmm, that's negative. Let me use 2/4 with: 2 eights + 3 triplet sixteenths.
# 2*(1/8) = 1/4. Triplet 16ths: 3*(1/24) = 1/8. Total=3/8 > 1/2? No: 3/8 < 1/2.
# Gap=1/8. Not what I want.
#
# Simplest case: 4/4 with a triplet group where last note's advance exceeds remaining.
# 6 quarter notes (each 1/4): cumTick=6/4=3/2 > 1. But voice-full guard stops at 1.
# Let me use: 3Q (=3/4) + implied triplet 8ths where last one crosses mLen.
# After 3Q: cumTick=3/4. Remaining=1/4. Triplet 8ths: advance=1/12 each.
# After 2 triplet 8ths: cumTick=3/4+2*(1/12)=3/4+1/6=11/12. Remaining=1/12.
# 3rd triplet 8th: advance=1/12 = remaining. NOT > remaining. Not capped!
#
# Need advance > remaining. Use 3Q + 2 triplet 8ths where the last triplet advance
# = 1/12 > remaining = 1/4 - 2*(1/12) = 1/4 - 1/6 = 1/12... equal again.
#
# OK: 3Q + implied triplet of 8ths (rdur=80). After 3Q: cumTick=3/4=18/24.
# Remaining=6/24. Triplet advance=4/24. After 1st: cumTick=22/24. After 2nd: 26/24>1.
# But voice-full guard: 22/24 < 1 → place 2nd. After 2nd: advance=4/24. Remaining=2/24.
# 4/24 > 2/24 → CAP to TDuration(2/24=1/12, true)=V_16TH(3/48=1/16 < 1/12=4/48? YES).
# dt=V_16TH (plain). advance=3/48=1/16. cumTick=22/24+1/16=44/48+3/48=47/48.
# Wait but we said remaining=2/24 = 4/48. TDuration(4/48,true)=V_16TH(3/48 < 4/48? Yes).
# advance=3/48. cumTick=1-1/48. gap=1/48. micro-fill adds 1/48. PASS!
#
# The 2nd triplet note is CAPPED and removed from tuplet. Its ticks = 1/16.
# actualTicks = 1/16 (no tuplet). sanityCheck: 3*(1/4)+1/12+1/16+cascade+micro = ?
# 3*(12/48)+4/48+3/48 = 36+4+3=43/48. micro adds 1/48. Total=44/48 ≠ 48/48.
# Hmm, doesn't add up to 4/4. Let me recalculate properly.
#
# Actually: let me use a 3/4 measure (mLen=3/4=36/48) with:
# 2Q (=24/48) + 3 implied triplet 8ths (rdur=80 each).
# After 2Q: cumTick=24/48. Remaining=12/48. 3 triplet 8ths each advance 8/48=1/12.
# After 1st: cumTick=32/48. After 2nd: 40/48. Remaining=36/48-32/48=4/48 before 3rd.
# Wait: after 1st triplet: cumTick=32/48. Remaining=36-32=4/48. 8/48 > 4/48 → CAP!
# CAP to TDuration(4/48, true): 4/48=1/12. Largest standard ≤ 1/12:
# V_16TH=3/48 ≤ 4/48. Yes. So TDuration=V_16TH. advance=3/48.
# cumTick=32+3=35/48. Remaining=1/48. micro-fill. PASS.
#
# But then there's only 1 triplet note + 1 capped plain note. The group has 1 note.
# Hmm, we need 2 triplet notes in the group so the "capping" test is meaningful.
#
# Let me use: 2 Q + 2 implied triplet 8ths (group size 3, but only 2 notes present):
# After 2Q (cumTick=24/48): remaining=12/48=1/4.
# detectImplied(80,4): 3:2. Need 3 consecutive with rdur=80. Only 2 available.
# Pre-pass: 2 < 3. Not a valid group! → treated as plain 8ths.
# Not what I want.
#
# OK simplest approach: use a 4/4 explicit tuplet (tup=0x32) where the group has
# exactly 3 notes but the last one overflows mLen:
# Notes: Q Q Q + 3 explicit triplet 8ths (tup=0x32).
# After 3Q: cumTick=3/4=36/48. Remaining=12/48.
# Triplet 8ths: advance=1/12=4/48 each.
# 1st: cumTick=40/48. 2nd: cumTick=44/48. 3rd: remaining=4/48, advance=4/48 = equal. NOT capped.
# Hmm still not capped.
#
# Try: 3Q + 3 explicit triplet QUARTERS (tup=0x32, fv=3):
# After 3Q: cumTick=3/4. Each triplet Q advance=1/6.
# 1st: 3/4+1/6=11/12. 2nd: remaining=1/12. advance=1/6 > 1/12 → CAP!
# CAP to TDuration(1/12,true)=V_16TH=1/16. 3rd note removed from tuplet.
# This is the pattern we want to exercise.
# ===========================================================================
def gen_v0c4_capped_tuplet_note():
    # 4/4 measure: 3 quarter notes, then 3 explicit 3:2 triplet quarters (tup=0x32).
    # After 3 plain Q: cumTick=3/4. Each triplet Q advance=(1/4)*(2/3)=1/6.
    # 1st triplet Q: cumTick=3/4+1/6=11/12.
    # 2nd triplet Q: remaining=1/12. advance=1/6 > 1/12 → CAPPED to V_16TH.
    # 2nd note removed from tuplet, plain V_16TH. cumTick=11/12+1/16=47/48.
    # 3rd triplet Q: voice_full (47/48 < 1, place). remaining=1/48.
    #   advance=1/6 > 1/48 → CAP to V_64TH. Remove from tuplet.
    #   cumTick=47/48+1/64=188/192+3/192=191/192.
    # micro-fill: adds 1/192 to reach mLen=1.
    # sum = 3*(1/4) + 1/6 + 1/16 + 1/64 + micro = 1. PASS.
    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60, tuplet=0x00)  # plain Q
    e += note_v0c4(240, 0, 0, fv=3, pitch=62, tuplet=0x00)  # plain Q
    e += note_v0c4(480, 0, 0, fv=3, pitch=64, tuplet=0x00)  # plain Q
    e += note_v0c4(720, 0, 0, fv=3, pitch=65, tuplet=0x32)  # triplet Q 1 (fits)
    e += note_v0c4(800, 0, 0, fv=3, pitch=67, tuplet=0x32)  # triplet Q 2 (CAPPED)
    e += note_v0c4(880, 0, 0, fv=3, pitch=65, tuplet=0x32)  # triplet Q 3 (CAPPED more)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_overfull_messy_precontent_tuplet():
    # Mirrors a real overfull measure (Lagrimas, Trumpet 1, m.3): 4/4 with messy
    # pre-content 17/32 (16th + 32nd + 16th + dotted-quarter) then a 3:2 quarter triplet
    # (natural span 1/2). Total = 33/32, overflowing 4/4 by exactly 1/32.
    # Truncate must produce an exact 4/4; Stretch must compress the triplet to fit.
    e  = note_v0c4(  0, 0, 0, fv=5, pitch=77)                 # 16th
    e += note_v0c4( 60, 0, 0, fv=6, pitch=76)                 # 32nd
    e += note_v0c4( 90, 0, 0, fv=5, pitch=77)                 # 16th
    e += note_v0c4_dotctrl(150, 0, 0, fv=3, pitch=76, dotControl=1)  # dotted quarter
    e += note_v0c4(510, 0, 0, fv=3, pitch=76, tuplet=0x32)    # triplet Q 1
    e += note_v0c4(670, 0, 0, fv=3, pitch=76, tuplet=0x32)    # triplet Q 2
    e += note_v0c4(830, 0, 0, fv=3, pitch=72, tuplet=0x32)    # triplet Q 3
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_overfull_tuplet_with_slur():
    # Overfull measure (like overfull_messy_precontent_tuplet) PLUS a SLURSTART (0x21)
    # spanning into the 3:2 quarter triplet (alMezuro=0 -> ends in same measure). Exercises
    # the overfull surgery in the presence of a slur whose endpoint resolves into the
    # truncated/compressed region.
    e  = ornament_v0c4(0, 0, 0, tipo=0x21, xoffset=3, alMezuro=0, xoffset2=5)  # SLURSTART
    e += note_v0c4(  0, 0, 0, fv=5, pitch=77)
    e += note_v0c4( 60, 0, 0, fv=6, pitch=76)
    e += note_v0c4( 90, 0, 0, fv=5, pitch=77)
    e += note_v0c4_dotctrl(150, 0, 0, fv=3, pitch=76, dotControl=1)
    e += note_v0c4(510, 0, 0, fv=3, pitch=76, tuplet=0x32)
    e += note_v0c4(670, 0, 0, fv=3, pitch=76, tuplet=0x32)
    e += note_v0c4(830, 0, 0, fv=3, pitch=72, tuplet=0x32)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_stretch_irregular_fallback():
    # 4/4: 3 plain quarters (cumTick=3/4) then a 3:2 triplet of HALF notes (tup=0x32).
    # Natural tuplet bracket = 2*half = a whole note (4/4). Only a quarter of space is
    # left, so the largest bracket that fits (a quarter) is < half the natural span ->
    # the Stretch strategy declines to compress and falls back to IrregularMeasure,
    # extending the bar to hold all three half-note-triplet members intact.
    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60, tuplet=0x00)  # plain Q
    e += note_v0c4(240, 0, 0, fv=3, pitch=62, tuplet=0x00)  # plain Q
    e += note_v0c4(480, 0, 0, fv=3, pitch=64, tuplet=0x00)  # plain Q
    e += note_v0c4(720, 0, 0, fv=2, pitch=65, tuplet=0x32)  # half-note triplet 1
    e += note_v0c4(900, 0, 0, fv=2, pitch=67, tuplet=0x32)  # half-note triplet 2
    e += note_v0c4(1080, 0, 0, fv=2, pitch=65, tuplet=0x32)  # half-note triplet 3
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_stretch_rob_rest():
    # 4/4 percussion "flourish preceded by rests" (Himno instrument 9, m3/m5): a quarter, a quarter
    # rest, a quarter and a quarter rest fill the bar (cumTick reaches 4/4), then a 3-sixteenth
    # flourish arrives. With the voice already full, the non-IrregularMeasure path used to DROP the
    # flourish; keeping it (for Stretch) lets the overfull post-pass reclaim the preceding rests
    # (robRestsToFit) so all three sixteenths survive in a standard 4/4 bar. Truncate still drops.
    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60)   # quarter (beat 1)
    e += rest_v0c4(240, 0, 0, fv=3)             # quarter rest (beat 2)
    e += note_v0c4(480, 0, 0, fv=3, pitch=62)   # quarter (beat 3)
    e += rest_v0c4(720, 0, 0, fv=3)             # quarter rest (beat 4) -> bar full at cumTick 4/4
    e += note_v0c4(840, 0, 0, fv=5, pitch=64)   # 16th flourish, arrives with the voice full
    e += note_v0c4(870, 0, 0, fv=5, pitch=64)   # 16th flourish
    e += note_v0c4(900, 0, 0, fv=5, pitch=64)   # 16th flourish
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# importer_inner_tuplet_note_level_cap.enc
#
# Regression for SIGSEGV caused by note-level cap calling Tuplet::remove() on
# the OUTER tuplet instead of the inner one that actually owns the chord.
#
# 4/4 measure (beatTicks=240, mLen=1920 MuseScore ticks):
#   half_rest  (0)    + quarter (480) + 16th (720) + 64th (780)
#   → cumTick = 960+480+120+30 = 1590/1920 = 53/64
#
# Outer 3:2 group (tuplet=0x32):
#   outer_quarter (795, fv=3): advance = 1/4*2/3 = 1/6 = 320 ticks
#   → cumTick = 53/64 + 1/6 = 191/192.  remaining = 1/192 ≈ 10 ticks.
#
# Inner 3:2 group (nested, via no-downdate: baseLen q→8th triggers innerGroupStartIdx):
#   inner_e0 (1035, fv=4, innerFirst): advance = 1/8*2/3*2/3 = 1/18 ≈ 107 ticks.
#     remaining=10 < advance=107.  TDuration(10 ticks, truncate) < 1/128=15 ticks
#     → advance.numerator()==0 → PATH A (chord deleted).
#     OLD: tt.currentTuplet->remove(chord) removes from OUTER tuplet → "cannot find"
#          → inner tuplet retains dangling ptr → SIGSEGV on innerTuplet->add(inner_e1).
#     NEW: chord->tuplet()->remove(chord) removes from INNER tuplet → clean.
#   inner_e1 (1155, fv=4): peekAhead note 1
#   inner_e2 (1275, fv=4, innerLast): peekAhead note 2
# ===========================================================================
def gen_v0c4_inner_tuplet_note_level_cap():
    e  = rest_v0c4( 0,   0, 0, fv=2)                         # half rest
    e += note_v0c4(480,  0, 0, fv=3, pitch=60, tuplet=0)     # quarter
    e += note_v0c4(720,  0, 0, fv=5, pitch=62, tuplet=0)     # 16th
    e += note_v0c4(780,  0, 0, fv=7, pitch=64, tuplet=0)     # 64th
    e += note_v0c4(795,  0, 0, fv=3, pitch=65, tuplet=0x32)  # outer quarter [group start]
    e += note_v0c4(1035, 0, 0, fv=4, pitch=67, tuplet=0x32)  # inner 8th 0 [innerFirst; capped]
    e += note_v0c4(1155, 0, 0, fv=4, pitch=65, tuplet=0x32)  # inner 8th 1 [peekAhead 1]
    e += note_v0c4(1275, 0, 0, fv=4, pitch=64, tuplet=0x32)  # inner 8th 2 [peekAhead 2; innerLast]
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_perc_clef_positions.enc
# 4/4 PERC clef staff (LINE block patched to clef=7) with three pitches at
# distinct Encore position bytes. Verifies that the importer:
#   (a) places each note at a different staff line derived from position_byte
#   (b) assigns HEAD_CROSS for faceValue high nibble=5, HEAD_NORMAL otherwise
#
# Notes:
#   pitch=62 fv=0x03 (hi=0, normal) position=1  → MuseScore line=8  HEAD_NORMAL
#   pitch=65 fv=0x03 (hi=0, normal) position=3  → MuseScore line=6  HEAD_NORMAL
#   pitch=81 fv=0x53 (hi=5, cross)  position=12 → MuseScore line=-3 HEAD_CROSS
# ===========================================================================
def gen_v0c4_perc_clef_positions():
    e  = note_v0c4_perc(  0, 0, 0, fv=0x03, pitch=62, position=1)
    e += note_v0c4_perc(240, 0, 0, fv=0x03, pitch=65, position=3)
    e += note_v0c4_perc(480, 0, 0, fv=0x53, pitch=81, position=12)
    e += note_v0c4_perc(720, 0, 0, fv=0x03, pitch=62, position=1)
    e += end_marker()
    result = assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))
    return set_staff_clef(result, staff_idx=0, clef=7)   # PERC clef = 7


# ===========================================================================
# notes_perc_clef_standard_drumset_notehead.enc
# 4/4 PERC clef staff with one note at pitch 40 (Electric Snare) and faceValue
# high nibble=0 (normal head in Encore).
#
# The standard MIDI drumset pre-registers pitch 40 as HEAD_SLASH.  The importer
# must override that with HEAD_NORMAL based on faceValue, regardless of the
# drumset's existing entry.
#
#   pitch=40 fv=0x03 (hi=0, normal) position=7 → HEAD_NORMAL (not HEAD_SLASH)
# ===========================================================================
def gen_v0c4_perc_notehead_all_nibbles():
    # Regression for all 10 faceValue high-nibble notehead types (0-9).
    # Each note uses a DISTINCT pitch so drumset entries do not collide.
    # position=5 (middle line) for all; fv=(nibble<<4)|3 (quarter note).
    # Notes spaced at 240-tick (quarter) intervals across three 4/4 measures
    # so encTick matches cumTick and notes are not dropped as overlapping.
    #
    # Expected headGroup after layout:
    #   0=NORMAL  1=DIAMOND     2=TRIANGLE_UP  3=CUSTOM(square)
    #   4=CROSS   5=XCIRCLE     6=PLUS         7=SLASH
    #   8=LARGE_DIAMOND         9=NORMAL(invisible)
    measures = []
    for m_start in range(3):
        e = b''
        for beat in range(4):
            nibble = m_start * 4 + beat
            if nibble >= 10:
                e += rest_v0c4(beat * 240, 0, 0, fv=3)
                continue
            pitch = 50 + nibble
            fv    = (nibble << 4) | 3
            e += note_v0c4_perc(beat * 240, 0, 0, fv=fv, pitch=pitch, position=5)
        e += end_marker()
        measures.append((meas_hdr(4, 4), e))
    result = assemble(0xC4, measures, fill_ts=(4, 4))
    return set_staff_clef(result, staff_idx=0, clef=7)   # PERC clef = 7


def gen_v0c4_perc_shared_pitch_nibbles():
    # Regression: when two PERC notes share the same pitch but have different
    # faceValue high nibbles, layoutDrumset() must not override the earlier note's
    # headGroup (because the shared drumset entry is updated by the later note).
    # Fix: all non-normal nibbles call setFixed(true) so the headGroup is immune.
    #
    # Two notes with pitch=60, same voice, consecutive beats:
    #   beat 0: nibble=7 (slash)      → must remain HEAD_SLASH
    #   beat 1: nibble=8 (large-diam) → must remain HEAD_LARGE_DIAMOND
    PITCH = 60
    e  = note_v0c4_perc(0,   0, 0, fv=0x73, pitch=PITCH, position=5)  # nibble=7 slash
    e += note_v0c4_perc(240, 0, 0, fv=0x83, pitch=PITCH, position=5)  # nibble=8 large-diamond
    e += end_marker()
    result = assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))
    return set_staff_clef(result, staff_idx=0, clef=7)

def gen_v0c4_perc_standard_drumset_notehead():
    e  = note_v0c4_perc(0, 0, 0, fv=0x03, pitch=40, position=7)
    e += end_marker()
    result = assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))
    return set_staff_clef(result, staff_idx=0, clef=7)   # PERC clef = 7


# ===========================================================================
# notes_mixed_duration_tuplet_boundary_fill.enc
# 4/4 measure: half note + {qtr/3:2, qtr/3:2, 8th/3:2} where the closing 4th
# note (8th/3:2) falls exactly at tick=960=durTicks and Encore omits it.
#
# The tuplet group {qtr,qtr,8th,8th}/3:2 needs face sum=3/4 to close, but
# only 3 notes are present (face sum=5/8 < 3/4). The importer must detect
# faceTicks < fullFaceSum and add an invisible 8th fill rest to complete the
# group, bringing cumTick from 11/12 to 12/12 so sanityCheck passes.
#
# Ticks (Encore, beatTicks=240):
#   0=half(plain), 480=qtr(3:2), 640=qtr(3:2), 800=8th(3:2), [960=8th(3:2) MISSING]
# ===========================================================================
def gen_v0c4_mixed_duration_tuplet_boundary_fill():
    # The 8th is at tick=880 (not 800) so its rdur=80 does NOT match
    # expectedBeatAdv=160 (beatTicks×normalN/actualN = 240×2/3 = 160), avoiding
    # the beat-relative face-value override that would otherwise promote the 8th to
    # a quarter and fill the group without needing the faceShort fill path.
    e  = note_v0c4(  0, 0, 0, fv=2, pitch=60, tuplet=0x00)  # half note (plain)
    e += note_v0c4(480, 0, 0, fv=3, pitch=62, tuplet=0x32)  # qtr in 3:2 group
    e += note_v0c4(640, 0, 0, fv=3, pitch=64, tuplet=0x32)  # qtr in 3:2 group
    e += note_v0c4(880, 0, 0, fv=4, pitch=65, tuplet=0x32)  # 8th in 3:2 group (MIDI gap 640→880)
    # 4th note (8th/3:2, tick=960=durTicks) omitted: falls at measure boundary
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_mixed_duration_triplet.enc
# 2/4 measure with two 3:2 triplet brackets whose notes have MIXED face values.
# The first bracket contains 8th + 8th_rest + 16th_rest + 16th (face sum = 3/8).
# The second bracket contains three 8ths (face sum = 3/8).
# Together they fill exactly 2/4: 2 × (3/8 × 2/3) = 2 × 1/4 = 1/2.
#
# Bug: with count-based grouping (actualN=3), the first bracket would close
#   after the 3rd element (8th rest), making the 4th element (16th) start a
#   new incomplete group → sanityCheck fails.
# Fix: with face-value-sum grouping (close when sum = 3/8), the first bracket
#   correctly spans all 4 elements and both groups produce valid content. PASS.
#
# Ticks (Encore, 240/quarter):
#   0=8th(tup), 80=8th_rest(tup), 160=16th_rest(tup), 200=16th(tup) [group1]
#   240=8th(tup), 320=8th(tup), 400=8th(tup)                         [group2]
# ===========================================================================
def gen_v0c4_mixed_duration_triplet():
    e  = note_v0c4(  0, 0, 0, fv=4, pitch=77, tuplet=0x32)  # 8th  in tup (group1)
    e += rest_v0c4_tup(80, 0, 0, fv=4, tuplet=0x32)          # 8th rest in tup
    e += rest_v0c4_tup(160, 0, 0, fv=5, tuplet=0x32)         # 16th rest in tup
    e += note_v0c4(200, 0, 0, fv=5, pitch=81, tuplet=0x32)   # 16th in tup (end group1)
    e += note_v0c4(240, 0, 0, fv=4, pitch=83, tuplet=0x32)   # 8th  in tup (group2)
    e += note_v0c4(320, 0, 0, fv=4, pitch=81, tuplet=0x32)   # 8th  in tup
    e += note_v0c4(400, 0, 0, fv=4, pitch=79, tuplet=0x32)   # 8th  in tup (end group2)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))


# ===========================================================================
# notes_partial_triplet_measure_end.enc
# 2/4 measure: 3 plain eighth notes (fill ticks 0-359), then a 2-note partial
# 3:2 triplet group at the measure end (ticks 360 and 440).
#
# Partial group: both notes have tup=0x32 (3:2), fv=4 (eighth).
#   rdur(note1=tick 360) = 440-360 = 80  (2 triplet slots)
#   rdur(note2=tick 440) = 480-440 = 40  (1 triplet slot)
# startTick + rdurSum = 360 + 120 = 480 = durTicks  -> fills measure by rdur
# startTick + faceTickSum = 360 + 240 = 600 > 480   -> face-values would overflow
#
# Fix: both conditions hold -> mark as partial measure-end group (Fix 1).
# Fix 3: baseLen = remaining/normalN = (1/8)/2 = 1/16 (V_16TH).
# Fix 2: note2 dt reduced V_EIGHTH -> V_16TH to fit remaining 1/24.
#
# Without fix: note1 (V_EIGHTH plain) fills remaining 1/8, note2 overflows to
# voice 1. In POLCA this caused a phantom note and unresolved tie.
# ===========================================================================
def gen_v0c4_partial_triplet_measure_end():
    e  = note_v0c4(  0, 0, 0, fv=4, pitch=60, tuplet=0x00)  # plain 8th (rdur=120)
    e += note_v0c4(120, 0, 0, fv=4, pitch=62, tuplet=0x00)  # plain 8th (rdur=120)
    e += note_v0c4(240, 0, 0, fv=4, pitch=64, tuplet=0x00)  # plain 8th (rdur=120)
    e += note_v0c4(360, 0, 0, fv=4, pitch=67, tuplet=0x32)  # triplet slot 1 (rdur=80, 2 slots)
    e += note_v0c4(440, 0, 0, fv=4, pitch=69, tuplet=0x32)  # triplet slot 2 (rdur=40, 1 slot)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))


# ===========================================================================
# notes_triple_dotted_advance.enc
# 4/4 measure: a triple-dotted 8th note (rdur = 120*15/8 = 225 Encore ticks).
# calcDots(225, 4=8th) returns 3. The chord gets ticks = (1/8)*(15/8) = 15/64.
#
# Bug: advance used Fraction(7,4) for dots>=2, giving (1/8)*(7/4)=14/64 instead
#   of the correct (1/8)*(15/8)=15/64. cumTick advanced by 14/64 while the chord
#   occupied 15/64, placing the next element 1/64 behind the chord's end.
#   checkMeasure detected an overrun break and inserted a large fill → sum > 4/4.
# Fix: add dots==3 branch using Fraction(15,8). advance=15/64=ticks. ✓
#
# Structure: triple-dotted 8th (225 ticks) + 8th = 15/64+1/8 = 23/64 in first beat.
# Then a quarter + dotted-quarter fills the rest: 23/64+16/64+24/64=63/64, leaving
# 1/64 gap filled by checkMeasure with a 64th rest. Sum=1=4/4. PASS.
# ===========================================================================
def gen_v0c4_triple_dotted_advance():
    # tick=0:  note fv=8(8th), rdur=225 → triple-dotted 8th. advance=15/64.
    # tick=225: note fv=8(8th), rdur=120 → plain 8th. advance=8/64. cumTick=23/64.
    # tick=345: note fv=3(Q), rdur=240 → plain Q. advance=16/64. cumTick=39/64.
    # tick=585: note fv=2(H) but rdur=360→dot Q (3 dots?no: 240*3/2=360 → 1 dot Q).
    #   advance=24/64=3/8. cumTick=63/64. Gap=1/64. Fill V_64TH. Sum=1. PASS.
    e  = note_v0c4(  0, 0, 0, fv=4, pitch=60, tuplet=0)   # 8th, rdur=225 → 3-dot
    e += note_v0c4(225, 0, 0, fv=4, pitch=62, tuplet=0)   # 8th, rdur=120
    e += note_v0c4(345, 0, 0, fv=3, pitch=64, tuplet=0)   # Q,   rdur=240
    e += note_v0c4(585, 0, 0, fv=3, pitch=65, tuplet=0)   # Q,   rdur=360 → dot Q
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_v0c2_near_simultaneous_chord.enc
# v0xC2 2/4 measure: two notes meant to be simultaneous but stored at ticks
# 0 and 3 (3-tick MIDI offset, < CHORD_CLUSTER_THRESHOLD=4).
# Both are quarter notes with different pitches, a chord in Encore display.
#
# Bug: calculateRealDurations gave the first note rdur=3 (<15) → SKIP.
#   Only the second note survived → 1 note instead of 2-note chord.
# Fix: cluster skip in calculateRealDurations gives rdur=durTicks-0=480 (not 3)
#   → not skipped. isChordExt threshold groups the two as one chord. ✓
# ===========================================================================
def gen_v0c2_near_simultaneous_chord():
    # Two quarter notes at ticks 0 and 3 (3-tick offset = chord cluster)
    e  = note_v0c2(  0, 0, 0, fv=3, pitch=60)  # C4 quarter at tick=0
    e += note_v0c2(  3, 0, 0, fv=3, pitch=64)  # E4 quarter at tick=3 (near-simultaneous)
    e += rest_v0c4(240, 0, 0, fv=2)            # half rest to fill 2/4
    e += end_marker()
    return assemble(0xC2, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))


# ===========================================================================
# notes_tie.enc
# v0xC4 2/4 measure: a quarter note at tick=0 with a TIE element at tick=0,
# followed by a quarter note at tick=240 (same pitch=60, C4).
# The two notes must be linked by a Tie object in the resulting score.
#
# Bug: TIE elements (type=3) were parsed as EncGenericElem and discarded.
#   No Tie objects were created; notes appeared as two separate unlinked notes.
# Fix: EncTie struct + pre-scan tieStartSet + pendingTieNote map creates
#   Factory::createTie() linking startNote → endNote. ✓
# ===========================================================================
def gen_v0c4_tie():
    # Quarter note C4 at tick=0, TIE element at tick=0, quarter note C4 at tick=240
    e  = note_v0c4(0,   0, 0, fv=3, pitch=60)  # C4 quarter
    e += tie_v0c4( 0,   0, 0)                   # TIE at same tick
    e += note_v0c4(240, 0, 0, fv=3, pitch=60)  # C4 quarter (tie-end)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))


# ===========================================================================
# notes_dotted_rest.enc
# v0xC4 3/4 measure: a quarter note, then a dotted 8th rest (dotControl=180),
# then a 16th rest to fill to 3/4.
# The dotted 8th rest must have dots()==1 in the resulting score.
#
# Bug: calcDots used er->realDuration (MIDI tick spacing, may be wrong due to
#   timing drift) instead of er->dotControl (actual sounding duration from Encore).
#   realDuration=154 gave calcDots(154, 4=8th)=0; dotControl=180 gives 1. ✓
# ===========================================================================
def gen_v0c4_dotted_rest():
    # Quarter note at tick=0, dotted 8th rest at tick=240 (dotControl=180=8th*3/2),
    # 16th rest at tick=420 to fill remaining 3/4 - Q - dot8th = 1/16.
    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60)          # C4 quarter  (Q=240 ticks)
    e += rest_v0c4_dotted(240, 0, 0, fv=4, dotControl=180)  # dotted 8th rest
    e += rest_v0c4(420, 0, 0, fv=5)                     # 16th rest fills to 3/4
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(3, 4), e)], fill_ts=(3, 4))


# ===========================================================================
# notes_dotted_note.enc
# v0xC4 2/4 measure: a dotted 8th note (dotControl=180) at tick=0, followed by
# a 16th note at tick=90 (placed by MIDI timing). The rdur of the first note
# from MIDI spacing is 90-0=90, which also happens to match dotControl=180
# for a dotted 8th. But to test independently: use rdur=86 (MIDI drift) with
# dotControl=180, placed at tick=0 and next note at tick=86.
#
# Bug: calcDots used en->realDuration (MIDI spacing, e.g. 86 ticks) instead of
#   en->dotControl (actual sounding duration = 180 for dotted 8th).
#   calcDots(86, 4=8th)=0 → plain 8th; calcDots(180, 4=8th)=1 → dotted ✓
# Fix: use en->dotControl when non-zero. ✓
#
# Structure: dotted 8th (dotControl=180) at tick=0, 16th at tick=86 (simulated
# MIDI drift: real dotted 8th ends at tick=90, but next note is at tick=86).
# This makes rdur=86 (not 90) for the first note, so realDuration-based calcDots
# gives 0 while dotControl-based calcDots gives 1. ✓
# ===========================================================================
def gen_v0c4_dotted_note():
    # Dotted 8th note at tick=0: dotControl=180 (8th*3/2), rdur from MIDI = 86 (drift)
    # 16th note at tick=86: rdur=240+90-86=244? Use tick=90 to get proper 2/4.
    # Actually: dotted 8th (90 ticks) + 16th (60 ticks) = 150 ticks = 5/16. Then fill.
    # Place next note at tick=86 to create the rdur=86 drift scenario.
    # Rest of measure: Q+16th fill (240-90-60=90 remainder of 2/4=480 total).
    # For simplicity: dotted 8th at tick=0 (rdur=86), 16th at tick=86, half rest fill.
    # 2/4 = 480 ticks. dot8th(90) + 16th(60) = 150 ticks = 5/16. Remaining: 3/16+1/4.
    e  = note_v0c4_dotctrl(0,  0, 0, fv=4, pitch=60, dotControl=180)  # dot 8th, but rdur=86
    e += note_v0c4(86, 0, 0, fv=5, pitch=62)                          # 16th at tick=86
    e += rest_v0c4(146, 0, 0, fv=3)                                    # Q fill
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))


# ===========================================================================
# notes_rdur_snap.enc
# v0xC4 4/4 measure: an 8th note at tick=0 with dotControl=0 (no hint) whose
# MIDI realDuration is 211 ticks, exactly 1 tick away from dd8th=210.
# calcDots(211, 8th) = 0 (no exact match), but calcDotsSnap(211, 8th, tol=1)
# returns 2 because |211-210| = 1 <= 1.
#
# Bug: before calcDotsSnap, the note would appear as a plain 8th (0 dots).
# Fix: calcDotsSnap with tolerance=1 snaps rdur=211 to dd8th (2 dots). ✓
#
# Structure: 8th note at tick=0, next event at tick=211 (rdur=211 for 8th),
# Q rest at tick=211 to pad, checkMeasure fills remainder.
# ===========================================================================
def gen_v0c4_rdur_snap():
    # 8th note at tick=0 (dotControl=0, so dot count from dotControl is 0)
    # Next event at tick=211 → rdur=211 → calcDotsSnap(211, 8th, 1) = 2 dots (dd8th=210)
    e  = note_v0c4(  0, 0, 0, fv=4, pitch=60)   # 8th, dotControl=0, rdur=211
    e += rest_v0c4(211, 0, 0, fv=3)              # Q rest at MIDI tick=211
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_sf_tiestart.enc
# v0xC4 4/4 measure: a 64th note at tick=0 with a TIE element at tick=0,
# followed by a Q note at tick=11 (same pitch). The 64th has rdur=11 < 15.
# Without the bypass, rdur<15 skips the 64th. With the bypass, the TIE
# element at tick=0 marks it as a real short note and it is placed.
#
# Bug: rdur=11 < 15 → 64th note skipped; pending tie from pre-measure
#   gets misapplied to the next note of same pitch.
# Fix: 64th notes with a TIE element at their tick bypass the rdur<15
#   filter, are placed as 64th notes, and tie correctly to the Q note. ✓
#
# Structure: TIE@0, 64th@0 (rdur=11), Q@11 (tie-end), checkMeasure fills.
# ===========================================================================
def gen_v0c4_sf_tiestart():
    # TIE element at tick=0 marks the 64th as a tie-start
    # 64th note at tick=0: next event at tick=11 → rdur=11 < 15
    # Q note at tick=11: tie-end of same pitch
    e  = tie_v0c4(  0, 0, 0)                     # TIE at tick=0
    e += note_v0c4( 0, 0, 0, fv=7, pitch=60)     # 64th C4 at tick=0
    e += note_v0c4(11, 0, 0, fv=3, pitch=60)     # Q C4 at tick=11 (tie-end)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_rest_before_note_midi_slop.enc
# v0xC4 5/8 measure: eighth at tick=0, 32nd REST at tick=120, dotted-16th at
# tick=125, three eighths completing the measure.
#
# calculateRealDurations sets rdur of the 32nd rest to 125-120=5 (< 15).
# Without the fix, rdur<15 filter drops the rest; checkMeasure fills the gap
# at the end instead, producing wrong order: E dot16 E E E R32.
# With the fix: face value (32nd, faceTicks=30 >= 30) is trusted; rdur=5 is
# MIDI timing slop; rest is placed correctly in order: E R32 dot16 E E E.
# ===========================================================================
def gen_v0c4_rest_before_note_midi_slop():
    # 5/8 measure: beatTicks=120 (eighth), durTicks=600
    e  = note_v0c4(  0, 0, 0, fv=4, pitch=58)  # E3 eighth
    e += rest_v0c4(120, 0, 0, fv=6)            # 32nd rest (rdur set to 5 by calc)
    e += note_v0c4(125, 0, 0, fv=5, pitch=62)  # D4 16th (dotted via rdur=91)
    e += note_v0c4(240, 0, 0, fv=4, pitch=65)  # F4 eighth
    e += note_v0c4(360, 0, 0, fv=4, pitch=70)  # Bb4 eighth
    e += note_v0c4(483, 0, 0, fv=4, pitch=74)  # D5 eighth
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(5, 8, beatTicks=120), e)], fill_ts=(5, 8))


# ===========================================================================
# notes_rdur_non_chord_ext_filtered.enc
# v0xC4 4/4 measure: Q rest at tick=0, then a 64th C4 at tick=240 (rdur=11
# < 15, not a tie-start, not a chord extension), then a Q E4 at tick=251.
# The 64th must be filtered; only the Q E4 must appear.
#
# Bug: prevMidiTick was updated to e->tick before the rdur<15 filter check.
#   The subsequent isChordExtCandidate saw delta=240-240=0<4 → TRUE → bypass.
#   The 64th artifact was incorrectly placed.
# Fix: use isChordExt (computed from OLD prevMidiTick = 0 from the rest).
#   240 - 0 = 240 >= 4 → not a chord extension → 64th filtered. ✓
# ===========================================================================
def gen_v0c4_rdur_non_chord_ext_filtered():
    e  = rest_v0c4(  0, 0, 0, fv=3)             # Q rest (establishes prevMidiTick=0)
    e += note_v0c4(240, 0, 0, fv=7, pitch=60)   # 64th C4 (rdur=251-240=11, filtered)
    e += note_v0c4(251, 0, 0, fv=3, pitch=64)   # Q E4 (placed normally)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_grace1_cascade_filter.enc
# v0xC4 4/4 measure: a 64th C4 at tick=0 with grace1 low=1 (tie-sender) and
# rdur=11 (no TIE element → filtered), a Q C4 at tick=11 with grace1 low=2
# (tie-receiver of the filtered note → cascade-filtered), and a Q E4 at
# tick=240 (standalone → placed).
#
# Bug: before cascade fix, Q C4 (g1low=2) would appear in output as a
#   spurious note because it was not subject to any filter check.
# Fix: when a note with g1low=1 is filtered, record its pitch; the next note
#   with g1low=2 and the same pitch is also filtered. ✓
# ===========================================================================
def gen_v0c4_grace1_cascade_filter():
    e  = note_v0c4_grace(  0, 0, 0, fv=7, pitch=60, grace1=0x01, grace2=0)  # 64th C4, g1low=1
    e += note_v0c4_grace( 11, 0, 0, fv=3, pitch=60, grace1=0x02, grace2=0)  # Q C4, g1low=2
    e += note_v0c4(       240, 0, 0, fv=3, pitch=64)                         # Q E4, standalone
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_chord_duplicate.enc
# Two NOTE elements at tick=0, same pitch=60 (C4): first has grace1=0x00
# (real note), second has grace1=0x40 (chord-extension copy). The importer
# must suppress the second and produce exactly one notehead.
# ===========================================================================
def gen_v0c4_chord_duplicate():
    e  = note_v0c4_grace(0, 0, 0, fv=3, pitch=60, grace1=0x00, grace2=0)   # real note
    e += note_v0c4_grace(0, 0, 0, fv=3, pitch=60, grace1=0x40, grace2=0)   # chord-ext copy
    e += note_v0c4(240, 0, 0, fv=3, pitch=64)                               # E4 standalone
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_chord_duplicate_no_ext_bit.enc
# BUG FIX: Two NOTE elements at tick=0, same pitch=60, NEITHER has grace1 0x40.
# This reproduces the encoding seen in some Encore 5 files (e.g. v0xC2 files)
# where the same note appears twice without the chord-extension marker.
# The old check "(grace1 & 0x40) && findNote" missed this; the fix broadens
# the duplicate suppression to any pitch already present in the chord.
# ===========================================================================
def gen_v0c4_chord_duplicate_no_ext_bit():
    e  = note_v0c4_grace(0, 0, 0, fv=3, pitch=60, grace1=0x00, grace2=0)   # real note
    e += note_v0c4_grace(0, 0, 0, fv=3, pitch=60, grace1=0x00, grace2=0)   # dup, no 0x40
    e += note_v0c4(240, 0, 0, fv=3, pitch=64)                               # E4 standalone
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_v0c2_chord_cluster_5tick.enc
# v0xC2 4/4 measure: a live-recorded 4-note chord spanning 5 MIDI ticks (100,
# 103, 104, 105) followed by 4 receiver notes at tick=240 (same pitch, tied).
# A TIE element at tick=100 marks the chord as a tie-start.
# Notes at ticks 103, 104, 105 have grace1 low=1 (tie-sender indicator).
#
# calculateRealDurations assigns rdur=4 to the root note at tick=100 (because
# tick=104 is exactly CHORD_CLUSTER_THRESHOLD away and not skipped).
#
# Bug A: the root note (rdur=4 == CHORD_CLUSTER_THRESHOLD, fvBase=60 > 15) was
#   filtered by the unconditional "else: continue", leaving a singleton chord.
# Bug B: note@104 (4 ticks from root) was not a chord extension because
#   CHORD_MIDI_THRESHOLD was equal to CHORD_CLUSTER_THRESHOLD (4, strict <).
# Bug C: notes@104 and @105 missed tie registration because TIE@100 is outside
#   the ±3-tick isTieStart window for those ticks.
# Fix A: filter fvBase>15 only if realDuration > CHORD_CLUSTER_THRESHOLD.
# Fix B: CHORD_MIDI_THRESHOLD = 2*CHORD_CLUSTER_THRESHOLD (= 8).
# Fix C: grace1 low=1 used as secondary tie-start indicator (v0xC2 only). ✓
# ===========================================================================
def gen_v0c2_chord_cluster_5tick():
    fv16 = 5    # 16th note face-value code
    dc90 = 90   # dotControl=90 = dotted 16th in Encore ticks
    # Half rest to push the chord to the middle of the measure
    e  = rest_v0c4(  0, 0, 0, fv=2)                                    # half rest
    # 4-note chord: root@100 + 3 near-simultaneous notes at +3, +4, +5 ticks
    e += tie_v0c4(100, 0, 0)                                            # TIE at root
    e += note_v0c2_ext(100, 0, 0, fv=fv16, pitch=60, grace1=0x01, dotControl=dc90)  # C, g1low=1
    e += note_v0c2_ext(103, 0, 0, fv=fv16, pitch=64, grace1=0x01, dotControl=dc90)  # E, g1low=1
    e += note_v0c2_ext(104, 0, 0, fv=fv16, pitch=67, grace1=0x01, dotControl=dc90)  # G, g1low=1
    e += note_v0c2_ext(105, 0, 0, fv=fv16, pitch=71, grace1=0x01, dotControl=dc90)  # B, g1low=1
    # 4 receiver notes at tick=240 (all at same tick, same pitches, g1low=2)
    e += note_v0c2_ext(240, 0, 0, fv=3, pitch=60, grace1=0x02)         # C quarter, g1low=2
    e += note_v0c2_ext(240, 0, 0, fv=3, pitch=64, grace1=0x02)         # E quarter
    e += note_v0c2_ext(240, 0, 0, fv=3, pitch=67, grace1=0x02)         # G quarter
    e += note_v0c2_ext(240, 0, 0, fv=3, pitch=71, grace1=0x02)         # B quarter
    e += end_marker()
    return assemble(0xC2, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# text_title_instruction_copyright.enc
# v0xC4 file with a TITL block containing title, subtitle, instruction
# (arranger/source), author (composer), and copyright.
# instruction[0] must appear as LYRICIST in the VBox title frame.
# copyright[0] must be stored in score metadata as "copyright".
# ===========================================================================
# Alignment byte values stored at offset +14 of the 30-byte line prefix.
# Header/footer lines use these to pick the page corner; other lines keep
# this byte at 0x00 so the importer's default LEFT mapping applies.
ALIGN_LEFT   = 0x04
ALIGN_CENTER = 0x06
ALIGN_RIGHT  = 0x02


def _titl_content(title='', subtitle0='', instruction0='', author0='', copyright0='',
                  header=(('', 0), ('', 0)), footer=(('', 0), ('', 0))):
    """Build 21242-byte TITL block content for TWO_BYTE (UTF-16 LE) encoding.
    The bazo.enc skeleton has TK00 offset=0x086E > 250, so EncInstrument::charSize()
    returns TWO_BYTES.  readTextItem() then reads 30 skip + 1026 text bytes per item.

    `header` and `footer` are 2-tuples of (text, align_byte). The align byte
    is placed at prefix offset +14 (0x02=right, 0x04=left, 0x06=center).
    """
    def item(text, align=0):
        skip = bytearray(30)           # 30 prefix bytes (defaults to 0x00)
        skip[14] = align & 0xFF        # horizontal alignment byte
        utf16 = text.encode('utf-16-le')
        utf16 = utf16[:1024]           # cap at 512 chars
        text_bytes = utf16 + b'\x00\x00'  # null terminator
        text_bytes += b'\x00' * (1026 - len(text_bytes))  # pad to 1026
        return bytes(skip) + text_bytes  # 1056 bytes total

    c  = b'\x00\x00'           # initial skip (2 bytes)
    c += item(title)            # title
    c += item(subtitle0)        # subtitle[0]
    c += item('')               # subtitle[1]
    c += item(instruction0)     # instruction[0]
    c += item('') * 2           # instruction[1,2]
    c += item(author0)          # author[0]
    c += item('') * 3           # author[1,2,3]
    c += item(header[0][0], header[0][1])  # header[0]
    c += item(header[1][0], header[1][1])  # header[1]
    c += item(footer[0][0], footer[0][1])  # footer[0]
    c += item(footer[1][0], footer[1][1])  # footer[1]
    c += item(copyright0)       # copyright[0]
    c += item('') * 5           # copyright[1-5]
    c += b'\x00' * 120          # trailing skip (TWO_BYTE encoding)
    # 2 + 20*1056 + 120 = 21242
    assert len(c) == 21242, f"TITL content must be 21242 bytes, got {len(c)}"
    return c


def gen_v0c4_title_instruction_copyright():
    # Patch the TITL block that already lives in SKELETON_POST with test data.
    # The EncTitle::read() reads the first 2426 bytes of the TITL content;
    # overwriting those bytes is sufficient to inject the test strings.
    post = bytearray(SKELETON_POST)
    idx = post.find(b'TITL')
    assert idx >= 0, "TITL block not found in SKELETON_POST"
    content = _titl_content(
        title='Test Title',
        subtitle0='Test Subtitle',
        instruction0='Test Instruction',
        author0='Test Composer',
        copyright0='(c) 2026 Test',
    )
    post[idx + 8: idx + 8 + len(content)] = content

    e  = end_marker()  # empty measure
    pre  = set_chumagio(0xC4)
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + bytes(post)


# ===========================================================================
# text_titl_headers_footers.enc
# TITL block with header[0..1] and footer[0..1] using all three alignment
# variants (right, center, left -- 0x02/0x06/0x04 at prefix offset +14).
# Encore distributes the four slots across the 6 MuseScore Sid corners
# (oddHeaderL/C/R + evenHeaderL/C/R, same for footers).
# ===========================================================================
def gen_v0c4_titl_headers_footers():
    post = bytearray(SKELETON_POST)
    idx = post.find(b'TITL')
    assert idx >= 0, "TITL block not found in SKELETON_POST"
    content = _titl_content(
        title='HF Title',
        # header[0] right-aligned, header[1] centered
        header=(('Header Right', ALIGN_RIGHT),
                ('Header Center', ALIGN_CENTER)),
        # footer[0] centered, footer[1] right-aligned
        footer=(('Footer Center', ALIGN_CENTER),
                ('Footer Right', ALIGN_RIGHT)),
    )
    post[idx + 8: idx + 8 + len(content)] = content

    e  = end_marker()
    pre  = set_chumagio(0xC4)
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + bytes(post)


# ===========================================================================
# text_header_footer_tokens.enc
# TITL block whose header/footer lines carry Encore's `#P`/`#D`/`#T`
# placeholder tokens (page, date, time). The importer must rewrite them to
# the matching MuseScore macros (`$P`/`$D`/`$m`) before assigning the text
# to the page header / footer style slots, otherwise the literal text would
# print on every page instead of expanding.
# ===========================================================================
def gen_v0c4_header_footer_tokens():
    post = bytearray(SKELETON_POST)
    idx = post.find(b'TITL')
    assert idx >= 0, "TITL block not found in SKELETON_POST"
    content = _titl_content(
        title='Token Test',
        header=(('Page #P', ALIGN_RIGHT),
                ('Created #D', ALIGN_CENTER)),
        footer=(('Time #T', ALIGN_LEFT),
                ('', 0)),
    )
    post[idx + 8: idx + 8 + len(content)] = content

    e  = end_marker()
    pre  = set_chumagio(0xC4)
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + bytes(post)


# ===========================================================================
# text_multi_slot_stacked_text.enc
# Each TITL category (subtitle / instruction / author / copyright /
# header / footer) can carry multiple non-empty slots. Encore renders them
# as stacked lines, so the importer must join all non-empty slots of the
# same category with a newline before assigning to the VBox text / score
# metaTag / Sid. This fixture has three composers, two headers with the
# SAME center alignment (stacked) and a left+right footer pair.
# Mirrors `Mamae_eu_quero-Bateria.enc`'s author block (Vicente Paiva e
# Jararáca / Adapt.: Sgt Solano / Banda de Música do CRPO/VRS).
# ===========================================================================
def gen_v0c4_multi_slot_stacked_text():
    post = bytearray(SKELETON_POST)
    idx = post.find(b'TITL')
    assert idx >= 0, "TITL block not found in SKELETON_POST"
    content = _titl_content(
        title='Multi-slot Title',
        # 4 authors: 3 non-empty and 1 empty - the empty slot is dropped.
        # _titl_content only accepts author0 but we patch the line directly
        # by reusing the header/footer pattern through manual layout.
    )
    # Patch authors 0..2 directly into the content buffer. Layout:
    # initial skip 2 + (title 1056) + (subtitle 0..1: 2*1056) +
    # (instruction 0..2: 3*1056) + (author 0..3: 4*1056) + ...
    LINE = 1056
    author_base = 2 + 1 * LINE + 2 * LINE + 3 * LINE  # = 6338
    def utf16_line(text, align=0):
        prefix = bytearray(30)
        prefix[14] = align & 0xFF
        utf16 = text.encode('utf-16-le')[:1024] + b'\x00\x00'
        utf16 += b'\x00' * (1026 - len(utf16))
        return bytes(prefix) + utf16
    composers = ['Vicente Paiva e Jararáca',
                 'Adapt.: Sgt Solano',
                 'Banda de Música do CRPO/VRS']
    for i, c in enumerate(composers):
        line = utf16_line(c)
        s = author_base + i * LINE
        content = content[:s] + line + content[s + LINE:]
    # Patch header/footer slots: two center-aligned headers + a left
    # footer + a right footer.
    header_base = 2 + 1 * LINE + 2 * LINE + 3 * LINE + 4 * LINE  # = 10562
    headers = [('Top line one', ALIGN_CENTER),
               ('Top line two', ALIGN_CENTER)]
    footers = [('Left footer',  ALIGN_LEFT),
               ('Right footer', ALIGN_RIGHT)]
    for i, (t, a) in enumerate(headers):
        line = utf16_line(t, a)
        s = header_base + i * LINE
        content = content[:s] + line + content[s + LINE:]
    for i, (t, a) in enumerate(footers):
        line = utf16_line(t, a)
        s = header_base + (2 + i) * LINE
        content = content[:s] + line + content[s + LINE:]

    post[idx + 8: idx + 8 + len(content)] = content

    e  = end_marker()
    pre  = set_chumagio(0xC4)
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + bytes(post)


# ===========================================================================
# text_duplicate_titl_block.enc
# Some Encore files write the TITL block twice (e.g. Mamae_eu_quero-
# Bateria.enc). Both blocks carry the same content; the importer must
# clear the EncTitle slot vectors at the start of every read() pass so
# the second block replaces (rather than appends to) the first one's
# data. Without the reset every author / header / footer line shows up
# duplicated in the resulting MuseScore score.
# ===========================================================================
def gen_v0c4_duplicate_titl_block():
    post = bytearray(SKELETON_POST)
    idx = post.find(b'TITL')
    assert idx >= 0, "TITL block not found in SKELETON_POST"
    content = _titl_content(title='Duped TITL', author0='Sole Composer')
    # Patch the SKELETON_POST TITL block with our content.
    post[idx + 8: idx + 8 + len(content)] = content

    e  = end_marker()
    pre  = set_chumagio(0xC4)
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    # Append an identical TITL block at the end so EncFile::read encounters
    # two TITL magics.  The importer should end up with a single set of
    # texts, not two copies.
    dup_block = b'TITL' + struct.pack('<I', len(content)) + content
    return pre + body + bytes(post) + dup_block


# ===========================================================================
# text_tempo_changes.enc
# Six measures whose MEAS header BPM field carries an initial tempo of 100
# followed by jumps to 60 (m2), back to 100 (m3, no new mark expected
# beyond the resume), 60 (m4), 200 (m5) and steady 200 (m6 - no mark, same
# as previous). The importer emits one TempoText per change plus the
# initial mark, and updates the score's tempo map for each.
# ===========================================================================
def gen_v0c4_tempo_changes():
    pre  = set_chumagio(0xC4)
    custom = [
        (meas_hdr(4, 4, bpm=100), end_marker()),
        (meas_hdr(4, 4, bpm=60),  end_marker()),
        (meas_hdr(4, 4, bpm=100), end_marker()),
        (meas_hdr(4, 4, bpm=60),  end_marker()),
        (meas_hdr(4, 4, bpm=200), end_marker()),
        (meas_hdr(4, 4, bpm=200), end_marker()),
    ]
    body = b''.join(meas_block(h, e) for h, e in custom)
    return pre + body + SKELETON_POST


def tempo_orn_v0c2_old(tick, voice, staffIdx, beatUnit, bpm):
    """36-byte v0xC2 TEMPO ornament in the OLDER layout (as written by Encore 3.x).

    Unlike the newer v0xC2/v0xC4 layout (beat-unit code at element +28, real BPM at
    +30), the older layout stores the BPM directly at element +28 and leaves a
    constant 0x34 at +30. The per-mark beat unit lives two bytes earlier, at element
    +26 (0-indexed note value: 0=whole, 2=quarter, 3=eighth, ...). The importer must
    read +26 to show the composer's beat unit (e.g. eighth) instead of defaulting to
    the compound-meter dotted quarter.
    """
    d = bytearray(36)
    struct.pack_into('<H', d, 0, tick)
    d[2] = (5 << 4) | (voice & 0xF)     # typeVoice: ORN | voice
    d[3] = 36                            # size (older layout)
    d[4] = staffIdx & 0x3F
    d[5] = 0x32                          # tipo = TEMPO
    d[26] = beatUnit & 0xFF             # older-layout beat unit
    d[28] = bpm & 0xFF                  # older-layout BPM (read into `noto`, then moved to `tempo`)
    d[30] = 0x34                         # constant marker (0x34=52) that the newer layout uses for BPM
    return bytes(d)


# ===========================================================================
# tempo_v0c2_eighth_beat_unit.enc
# A 6/8 measure (compound meter) with an older-layout v0xC2 TEMPO mark whose beat
# unit is the eighth note (beatUnit=3) at BPM 240 -> "eighth = 240". Because 6/8 is
# compound, the meter heuristic would render this as "dotted-quarter = 240" (and play
# it 3x too fast) if the per-mark beat unit at element +26 were ignored. The importer
# must recover the eighth beat unit and emit eighth=240 (playback = 2 beats/sec).
# ===========================================================================
def gen_v0c2_tempo_eighth_beat_unit():
    # 6/8 measure = 720 Encore ticks; one dotted-half filling the bar, plus the tempo mark.
    elems = (
        tempo_orn_v0c2_old(tick=0, voice=0, staffIdx=0, beatUnit=3, bpm=240)
      + note_v0c2(tick=0, voice=0, staffIdx=0, fv=2, pitch=60)   # half
      + note_v0c2(tick=480, voice=0, staffIdx=0, fv=4, pitch=60) # eighth -> fills 6/8
      + end_marker()
    )
    custom = [(meas_hdr(6, 8, bpm=0, beatTicks=120), elems)]
    return assemble(0xC2, custom, fill_ts=(6, 8))


# ===========================================================================
# structure_stale_tick_by_column.enc
# A 4/4 bar over two staves. The top staff plays quarter/quarter/half, establishing the
# xoffset columns (xoff 8 -> beat 1, 25 -> beat 2, 49 -> beat 3). The bottom staff holds a
# single half note drawn in the BEAT-1 column (xoff 8) but stored with a stale MIDI tick of
# 480 (beat 3), as happens when a note is edited in Encore. The importer used to trust the
# tick and place rest+note; it must snap the note to its column's beat so it imports as
# note (beat 1) + rest (beat 3), matching what Encore draws.
# ===========================================================================
def gen_v0c4_stale_tick_by_column():
    # Voice 0 plays quarter/quarter/half, establishing the xoffset columns (8 -> beat 1,
    # 25 -> beat 2, 49 -> beat 3). Voice 1 holds one half note drawn in the beat-1 column
    # (xoff 8) but stored with a stale MIDI tick of 480 (beat 3).
    v0 = (note_v0c4_xoff(  0, 0, 0, fv=3, pitch=72, xoff=8)
        + note_v0c4_xoff(240, 0, 0, fv=3, pitch=74, xoff=25)
        + note_v0c4_xoff(480, 0, 0, fv=2, pitch=76, xoff=49))
    v1 = note_v0c4_xoff(480, 1, 0, fv=2, pitch=60, xoff=8)   # stale tick: xoff 8 = beat 1
    elems = v0 + v1 + end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), elems)], fill_ts=(4, 4))


# ===========================================================================
# structure_voice4_rest_with_notes.enc
# A 4/4 bar whose voice 0 is full (four quarter notes) plus a whole-measure rest on
# Encore's "voice 4" (the silent-voice placeholder, voice nibble = 4). Routing folds
# voice 4 into voice 0; merging the redundant rest there collides with the notes and
# prepends a spurious rest, pushing the content past the barline so the 4/4 bar imports
# as 9/8. The importer must drop the voice-4 rest when the staff already has real notes.
# ===========================================================================
def gen_v0c4_voice4_rest_with_notes():
    # voice-4 whole rest emitted BEFORE the notes (as in the real file), then a mixed
    # rhythm that fills the 4/4 bar exactly (quarter, 8th, 8th, quarter, quarter).
    elems = rest_v0c4(0, 4, 0, fv=4)                   # voice 4: eighth-face rest spanning the bar (rdur=960)
    elems += note_v0c4(0,   0, 0, fv=3, pitch=60)
    elems += note_v0c4(240, 0, 0, fv=4, pitch=62)
    elems += note_v0c4(360, 0, 0, fv=4, pitch=64)
    elems += note_v0c4(480, 0, 0, fv=3, pitch=65)
    elems += note_v0c4(720, 0, 0, fv=3, pitch=67)
    elems += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), elems)], fill_ts=(4, 4))


# ===========================================================================
# structure_merge_stray_voice_rests.enc
# A single staff whose voice 1 (the second voice) holds only rests over a bar that voice 0
# already fills. Encore leaves such stray rests behind; with "combine non-overlapping
# voices" on they must be removed so no spurious empty second voice remains. There are no
# upper-voice NOTES anywhere, so the staff is not a merge candidate -- the cleanup must run
# regardless. Imported with mergeVoices=true.
# ===========================================================================
def gen_v0c4_merge_stray_voice_rests():
    elems = b''
    for t in (0, 240, 480, 720):
        elems += note_v0c4(t, 0, 0, fv=3, pitch=60)   # voice 0 fills the 4/4 bar
    for t in (0, 240, 480, 720):
        elems += rest_v0c4(t, 1, 0, fv=3)             # voice 1: stray quarter rests
    elems += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), elems)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_trill_between_notes.enc
# A simple "TR" trill whose stored tick (120) falls between two notes, with no note on
# that exact tick. The importer's cumulative-tick anchor overshoots to the FOLLOWING
# note; Encore actually draws the TR on the preceding note (its xoffset aligns with it).
# The importer must anchor from the raw tick and snap to the note it sits on.
#   note@0   xoff=10 (the trill's note)   note@240 xoff=40   note@480/720 fill the bar
#   TR ORN @120, xoff=15 (aligned to note@0, left of note@240)
# Expected: the trill lands on note@0, not note@240.
# ===========================================================================
def gen_v0c4_trill_between_notes():
    elems = (
        note_v0c4_xoff(  0, 0, 0, fv=3, pitch=60, xoff=10)
      + ornament_v0c4( 120, 0, 0, tipo=0xB0, xoffset=15)   # TRILL_TR between note@0 and note@240
      + note_v0c4_xoff(240, 0, 0, fv=3, pitch=62, xoff=40)
      + note_v0c4_xoff(480, 0, 0, fv=3, pitch=64, xoff=70)
      + note_v0c4_xoff(720, 0, 0, fv=3, pitch=65, xoff=100)
      + end_marker()
    )
    return assemble(0xC4, [(meas_hdr(4, 4), elems)], fill_ts=(4, 4))


# A "TR" (TRILL_TR) whose stored tick sits ON the second note (enc 120), but whose xoffset is drawn
# well to the LEFT of that note's xoffset (the "tr" text is left-anchored by convention). The
# xoffset-snap must NOT drag it back to the first note: when a note exists at the ornament's own
# tick, the TR belongs to that note. Before the fix the >20px xoffset gap snapped it onto note@0.
def gen_v0c4_trill_tr_on_own_note():
    elems = (
        note_v0c4_xoff(  0, 0, 0, fv=4, pitch=60, xoff=10)
      + note_v0c4_xoff(120, 0, 0, fv=4, pitch=64, xoff=60)  # the trilled 2nd note
      + ornament_v0c4( 120, 0, 0, tipo=0xB0, xoffset=25)    # "tr" text, left of note@120
      + note_v0c4_xoff(240, 0, 0, fv=3, pitch=67, xoff=100)
      + note_v0c4_xoff(480, 0, 0, fv=3, pitch=65, xoff=150)
      + end_marker()
    )
    return assemble(0xC4, [(meas_hdr(4, 4), elems)], fill_ts=(4, 4))


# ===========================================================================
# structure_start_double_barline.enc
# A double barline drawn between two measures is stored by Encore as the SECOND
# measure's start barline (byte 0x0C = 3 = DOUBLEL), not the first measure's end
# barline. The importer used to handle only end barlines, so the divider was dropped.
# Fixture: measure 1 has barTypeStart = DOUBLEL. Expected: measure 0 ends with a
# double barline.
# ===========================================================================
def gen_v0c4_start_double_barline():
    def bar(pitch):
        return (note_v0c4(0, 0, 0, fv=1, pitch=pitch) + end_marker())
    h0 = meas_hdr(4, 4)
    h1 = bytearray(meas_hdr(4, 4))
    h1[0x0C] = 3                      # barTypeStart = DOUBLEL (double bar before this measure)
    custom = [(bytes(h0), bar(60)), (bytes(h1), bar(62))]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# ornaments_accents_distributed.enc
# A 4/4 bar of four quarter notes, each carrying an "accent above" articulation.
# Encore stores every accent ORN at the downbeat tick (0) and separates them only by
# xoffset (matching each note's own xoffset). The importer used to trust the raw tick
# whenever a note sat on the downbeat, stacking all four accents on the first chord.
# It must instead spread a run of same-tick marks across the notes by xoffset, so each
# of the four chords gets exactly one accent.
# ===========================================================================
def gen_v0c4_accents_distributed():
    NOTE_XOFFS = [10, 30, 50, 70]
    elems = b''
    for i, xo in enumerate(NOTE_XOFFS):
        elems += note_v0c4_xoff(tick=i * 240, voice=0, staffIdx=0, fv=3, pitch=60, xoff=xo)
    # Four ACCENT (0xBE) ORNs, all at tick 0, xoffset aligned to each note.
    for xo in NOTE_XOFFS:
        elems += ornament_v0c4(0, 0, 0, tipo=0xBE, xoffset=xo)
    elems += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), elems)], fill_ts=(4, 4))


# ===========================================================================
# notes_implicit_leading_rest.enc
# A measure that encodes a leading silence implicitly: the first NOTE is at
# Encore tick 240 (= MuseScore beat 2 of a 3/4 measure) instead of 0, and
# no REST element precedes it. The importer must honor the Encore tick and
# insert a quarter rest at beat 1, otherwise the notes squash to beats 1-2
# with the trailing rest pushed past the actual sounding content (changing
# the song's timing).
# ===========================================================================
def gen_v0c4_implicit_leading_rest():
    # 3/4 measure (720 Encore ticks): leading quarter silence, then two
    # quarter notes at ticks 240 and 480.  No REST element written; the
    # silence is encoded only via the tick offset.
    elems = (
        note_v0c4(tick=240, voice=0, staffIdx=0, fv=3, pitch=72)
      + note_v0c4(tick=480, voice=0, staffIdx=0, fv=3, pitch=74)
      + end_marker()
    )
    custom = [(meas_hdr(3, 4), elems)]
    return assemble(0xC4, custom, fill_ts=(3, 4))


# ===========================================================================
# notes_inflated_rdur_quarter_chord.enc
# A measure that contains a single chord in a non-zero Encore voice nibble
# (voice=1) with NO further events. `EncMeasure::calculateRealDurations`
# will inflate rdur to the gap-to-measure-end (720 in 3/4) which lands on
# the "dotted half" dotted-mapping bucket. Without the inflated-promotion
# guard the importer would emit a dotted half chord instead of the quarter
# the binary actually encodes.
# ===========================================================================
def gen_v0c4_inflated_rdur_quarter_chord():
    # 3/4 measure: a single quarter chord on encVoice=1 at tick=0 (2 pitches
    # via chord extension at the same tick). The rest of the measure (480
    # ticks) is implicit -- no further elements in this voice.
    elems = (
        note_v0c4(tick=0, voice=1, staffIdx=0, fv=3, pitch=73)
      + note_v0c4(tick=0, voice=1, staffIdx=0, fv=3, pitch=64)
      + end_marker()
    )
    custom = [(meas_hdr(3, 4), elems)]
    return assemble(0xC4, custom, fill_ts=(3, 4))


# ---------------------------------------------------------------------------
# Helpers: patch TK00 in SKELETON_PRE.
# SKELETON_PRE TK00 has varsize=2158 (offset>250 → charSize=TWO_BYTES).
# To test the ONE_BYTE probe path we also need to patch the varsize to 112.
# ---------------------------------------------------------------------------
def _patch_tk00(name_bytes, new_varsize=None):
    """Return SKELETON_PRE with TK00 name (and optionally varsize) replaced.

    Only overwrites the name prefix.  The rest of the TK00 content area is
    left intact because the skeleton's TK00 region overlaps the start of the
    PAGE block (a quirk of the bazo.enc varsize), and Encore needs those
    structural bytes intact to open the file.
    """
    pre = bytearray(SKELETON_PRE)
    if new_varsize is not None:
        struct.pack_into('<I', pre, 194+4, new_varsize)
    content_start = 194 + 8
    pre[content_start: content_start + len(name_bytes)] = name_bytes
    return bytes(pre)


def _patch_midi_program(pre_bytes, instrument_index, program):
    """Return pre with the instrument MIDI program byte set. v0xC4 stores
    the 1-indexed GM program at PRG_BASE + n * PRG_STEP (see EncFile::read).
    """
    PRG_BASE = 2278
    PRG_STEP = 2158
    pre = bytearray(pre_bytes)
    off = PRG_BASE + instrument_index * PRG_STEP
    while len(pre) <= off:
        pre.extend(b'\x00' * 64)
    pre[off] = program & 0xFF
    return bytes(pre)


def _patch_small_tk_key(pre_bytes, instrument_index, key_semitones):
    """For smallTK layout (TK varsize=112), write key transposition at contentFilePos+varSize+53.
    KEY_IN_EXTRA=53 = MIDI_IN_EXTRA(76) - KEY_OFF(23); matches large-TK KEY_OFF=-23 convention."""
    FIRST_TK_START  = 194
    TK_SLOT         = 242
    CONTENT_OFFSET  = 8
    TK_CONTENT_SIZE = 112
    KEY_IN_EXTRA    = 53
    pre = bytearray(pre_bytes)
    tk_start = FIRST_TK_START + instrument_index * TK_SLOT
    content_pos = tk_start + CONTENT_OFFSET
    off = content_pos + TK_CONTENT_SIZE + KEY_IN_EXTRA
    while len(pre) <= off:
        pre.extend(b'\x00' * 64)
    pre[off] = key_semitones & 0xFF
    return bytes(pre)

def _patch_small_tk_midi(pre_bytes, instrument_index, program):
    """For smallTK layout (TK varsize=112), write MIDI at contentFilePos+offset+76.
    contentFilePos = 202 (TK00 at 194, header=8). offset=112, MIDI_AFTER_CONTENT=76.
    Each TK slot is 242 bytes (8+112+122), so instrument n starts at 194+n*242."""
    FIRST_TK_START  = 194
    TK_SLOT         = 242   # 8-byte header + 112-byte content + 122-byte extra
    CONTENT_OFFSET  = 8     # bytes from TK start to content start
    TK_CONTENT_SIZE = 112   # varsize used for smallTK test files
    MIDI_IN_EXTRA   = 76    # offset within the extra-data region
    pre = bytearray(pre_bytes)
    tk_start = FIRST_TK_START + instrument_index * TK_SLOT
    content_pos = tk_start + CONTENT_OFFSET
    off = content_pos + TK_CONTENT_SIZE + MIDI_IN_EXTRA
    while len(pre) <= off:
        pre.extend(b'\x00' * 64)
    pre[off] = program & 0xFF
    return bytes(pre)


# ===========================================================================
# instruments_tk_utf16_name.enc
# v0xC4 file where TK00 has varsize=2158 (→charSize=TWO_BYTES) and contains
# a UTF-16 LE name "Bandurria".  charSize() already picks TWO_BYTES; the
# name is read fully without truncation.
# Simulates a v0xC4 file from an Encore version that sets offset>250.
# ===========================================================================
def gen_v0c4_tk_utf16_name():
    name_utf16 = 'Bandurria'.encode('utf-16-le') + b'\x00\x00'
    pre = _patch_tk00(name_utf16)   # keeps SKELETON_PRE varsize=2158
    e   = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_tk_probe_utf16.enc
# v0xC4 file where TK00 has varsize=112 (→charSize=ONE_BYTE, offset<=250)
# but the content is UTF-16 LE "Bandurria" (b0=0x42='B', b1=0x00).
# The probe (b0 printable, b1=0x00) must upgrade cs to TWO_BYTES and read
# the full name.  Simulates Encore 5.0.2 files (e.g. pachbel.enc resaved).
# ===========================================================================
def gen_v0c4_tk_probe_utf16():
    name_utf16 = 'Bandurria'.encode('utf-16-le') + b'\x00\x00'
    pre = _patch_tk00(name_utf16, new_varsize=112)  # offset=112<=250 → probe fires
    e   = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_tk_onebyte_name.enc
# v0xC4 file where TK00 has varsize=112 (→charSize=ONE_BYTE, offset<=250)
# and content is ONE_BYTE "Bandurria 1" (b0=0x42='B', b1=0x61='a'!=0x00).
# The probe must see b1!=0 and keep ONE_BYTE, reading the full Latin-1 name.
# ===========================================================================
def gen_v0c4_tk_onebyte_name():
    name_latin1 = b'Bandurria 1\x00'
    pre = _patch_tk00(name_latin1, new_varsize=112)  # offset=112<=250, b1='a'≠0
    e   = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_instrument_count_padding.enc
# v0xC4 file with instrumentCount=2 in the header but only 1 TK block (TK00).
# Encore 5.0.2 can omit TK blocks for some instruments (e.g. pachbel.enc has
# 5 instruments but only 4 TK blocks, Guitarra has no TK block).
# The importer must pad the instruments vector to instrumentCount so all
# parts referenced by LINE staffData are created.
# ===========================================================================
def gen_v0c4_instrument_count_padding():
    pre = bytearray(SKELETON_PRE)
    pre[0x32] = 2   # instrumentCount = 2 (skeleton has only 1 TK block)
    # staffPerSystem stays 1; the 2nd instrument gets nstaves=0 → fallback ns=1
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return bytes(pre) + body + SKELETON_POST


# ===========================================================================
# instruments_staff_hidden.enc
# v0xC4 file where the single staff has showByte=0x00 (hidden from score).
# The importer must call part->setShow(false) for this staff.
#
# showByte location: LINE block → staff entry +19 (3rd byte of skip3).
# Verified by binary-diffing pachbel-shown.enc vs pachbel-hiden.enc.
# ===========================================================================
# ===========================================================================
# instruments_name_recovery.enc
# v0xC4 file with instrumentCount=2 but only 1 TK block (TK00 with name
# "Bandurria").  The 2nd instrument (N=1) has no TK block header, but its
# name "Guitarra" is stored as UTF-16 LE at formula offset NAME_BASE+1*NAME_STEP
# = 202 + 2158 = 2360 within the file (matching Encore 5.0.2 behaviour for
# pachbel.enc where TK04 is absent but "Guitarra" lives at 0x2282).
# The importer must recover "Guitarra" via the formula-based name scan.
# ===========================================================================
def gen_v0c4_name_recovery():
    # Build pre: header with instrumentCount=2, TK00 with "Bandurria"
    pre = bytearray(SKELETON_PRE)
    pre[0x32] = 2   # instrumentCount = 2
    # TK00 content: UTF-16 "Bandurria" (already in skeleton via _patch_tk00)
    # Write "Guitarra" as UTF-16 LE at offset NAME_BASE + 1*NAME_STEP = 202+2158=2360
    name_utf16 = 'Guitarra'.encode('utf-16-le') + b'\x00\x00'
    target_offset = 202 + 1 * 2158   # = 2360
    # Pad pre to at least target_offset + len(name)
    while len(pre) < target_offset + len(name_utf16) + 4:
        pre.extend(b'\x00' * 64)
    pre[target_offset: target_offset + len(name_utf16)] = name_utf16
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return bytes(pre) + body + SKELETON_POST


# ===========================================================================
# instruments_tk_empty_name_authoritative.enc
# v0xC4 file with two REAL TK blocks (total-block-size layout): TK00 named
# "InstrA", TK01 with an EMPTY name. There is no ~~~~ block. A printable
# string "ZZTOP" sits at the formula recovery offset for instrument 1
# (NAME_BASE + 1*NAME_STEP = 202 + 2158 = 2360), mimicking a real file where
# that offset falls on unrelated music/structure bytes.
#
# An empty name on a real TK block is authoritative (the instrument is
# genuinely unnamed), so the importer must leave it empty and apply the
# "Part N" fallback -- it must NOT positionally recover a name.
# Before the fix, recoverMissingNames probed offset 2360 and assigned the
# garbage "ZZTOP" to instrument 1.
# ===========================================================================
def gen_v0c4_tk_empty_name_authoritative():
    VARSIZE      = 80
    CONTENT      = VARSIZE - 8
    MIDI_IN_CONT = 60
    TK_START     = 194

    header = bytearray(SKELETON_PRE[:TK_START])
    header[0x32] = 2   # instrumentCount = 2

    def make_tk(idx, name_str, midi_1idx):
        magic   = 'TK{:02d}'.format(idx).encode('ascii')
        content = bytearray(CONTENT)
        nb      = (name_str.encode('ascii') + b'\x00') if name_str else b'\x00'
        content[:len(nb)] = nb
        content[MIDI_IN_CONT] = midi_1idx & 0xFF
        return bytes(magic) + struct.pack('<I', VARSIZE) + bytes(content)

    tk00 = make_tk(0, 'InstrA', 49)
    tk01 = make_tk(1, '',       34)   # real TK block, deliberately unnamed

    LEGACY_TK_END = 194 + 8 + 2158
    page_line     = SKELETON_PRE[LEGACY_TK_END:]

    pre  = bytes(header) + tk00 + tk01 + page_line
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    data = bytearray(pre + body + SKELETON_POST)

    # Plant printable garbage at instrument 1's formula recovery offset (2360),
    # past all real blocks so findNextKnownMagic ignores it.
    target  = 202 + 1 * 2158
    planted = b'ZZTOP\x00'
    if len(data) < target + len(planted):
        data.extend(b'\x00' * (target + len(planted) - len(data)))
    data[target:target + len(planted)] = planted
    return bytes(data)


def gen_v0c4_staff_hidden():
    import struct as _s
    pre = bytearray(SKELETON_PRE)
    # SKELETON_PRE has LINE at offset 2386; staff 0 showByte at 2386+8+13+19=2426
    _LINE_POS   = SKELETON_PRE.find(b'LINE')
    _SHOW_OFFSET = _LINE_POS + 8 + 13 + 19   # +19 within staff 0 entry
    pre[_SHOW_OFFSET] = 0x00                  # 0x00 = hidden
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return bytes(pre) + body + SKELETON_POST


# ===========================================================================
# ornaments_beamed_triplet_capped.enc
#
# 4/4 measure: 3 quarters + 1 plain 8th + 2 explicit 3:2 8th triplets where
# the 2nd triplet note is capped at the measure boundary.
#
# After 3 Q + 1 8th: cumTick = 3/4 + 1/8 = 7/8.
# 1st triplet 8th: advance = 1/8 * 2/3 = 1/12.  cumTick = 7/8 + 1/12 = 23/24.
# 2nd triplet 8th: remaining = 1/24 < advance (1/12) → CAPPED to V_32ND (1/32).
#   Chord removed from tuplet.  placedTicks = 1/12 + 1/32 = 11/96 (non-standard).
#
# "Before checkMeasure": placedTicks (11/96) < expected (1/4) →
#   closeTuplet / correction block calls setTicks(11/96).
#
# doLayout → beam.cpp reads TDuration(tuplet->ticks()) without truncate=true.
# Without importer fix: TDuration(11/96) is non-standard → assert fires.
# With importer fix:    setTicks snaps to TDuration(11/96,true) = 7/64 (standard) →
#   no assert; checkMeasure adds a tiny invisible rest inside the bracket.
# ===========================================================================
def gen_v0c4_beamed_triplet_capped():
    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60, tuplet=0)   # Q
    e += note_v0c4(240, 0, 0, fv=3, pitch=62, tuplet=0)   # Q
    e += note_v0c4(480, 0, 0, fv=3, pitch=64, tuplet=0)   # Q
    e += note_v0c4(720, 0, 0, fv=4, pitch=65, tuplet=0)   # 8th (plain, beamed with triplets)
    e += note_v0c4(840, 0, 0, fv=4, pitch=67, tuplet=0x32)  # 8th triplet 1 (fits)
    e += note_v0c4(880, 0, 0, fv=4, pitch=69, tuplet=0x32)  # 8th triplet 2 (CAPPED)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_zero_hairpin.enc
#
# 4/4 measure with a WEDGESTART ornament whose alMezuro=0 (same measure end).
# Regression coverage: importer must produce a hairpin with a positive span
# (tick < tick2) and pass sanityCheck. Earlier code synthesized a WEDGESTOP at
# the same tick as the WEDGESTART, producing setTicks(0) and an assert.
# Current importer resolves the end tick from alMezuro directly, so the
# hairpin spans from tick=0 to the end of the same measure.
# ===========================================================================
def gen_v0c4_zero_hairpin():
    # WEDGESTART at tick=0: alMezuro=0 (same measure), xoffset=xoffset2=5.
    e  = ornament_v0c4(0, 0, 0, tipo=0x1D, xoffset=5, alMezuro=0, xoffset2=5)
    e += note_v0c4(  0, 0, 0, fv=3, pitch=60, tuplet=0)
    e += note_v0c4(240, 0, 0, fv=3, pitch=62, tuplet=0)
    e += note_v0c4(480, 0, 0, fv=3, pitch=64, tuplet=0)
    e += note_v0c4(720, 0, 0, fv=3, pitch=65, tuplet=0)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_multi_measure_hairpin.enc
#
# Three 4/4 measures. A WEDGESTART at tick=0 of measure 0 with alMezuro=2
# (crescendo spanning measures 0..2) and a WEDGESTART at tick=480 of measure
# 1 with alMezuro=1 and speguleco=1 (diminuendo spanning measures 1..2).
# Encore .enc binaries do NOT emit a separate WEDGESTOP; the importer must
# read the end measure offset from alMezuro and produce a hairpin with the
# correct tick2 itself.
# ===========================================================================
# ===========================================================================
# ornaments_partial_quarter_triplet.enc
#
# Two notes of a 3:2 quarter triplet (a partial group). For a 3:2 quarter
# triplet, each note advances cumTick by baseLen * normalN / actualN
# = (1/4) * 2/3 = 2/12. Two notes give placedTicks = 4/12 = 1/3, which is
# not representable as a TDuration. closeTuplet must NOT call setTicks(1/3)
# on the tuplet -- beam layout (Beam::calcBeamBreaks) constructs
# TDuration(tuplet->ticks(), false) and asserts on non-TDuration fractions.
# Regression coverage for that abort.
# ===========================================================================
def note_v0c4_artic(tick, voice, staffIdx, fv, pitch, articUp=0, articDown=0):
    """v0xC4 note with articulation bytes set.
    articulationUp at elemStart+24 (body+21), articulationDown at elemStart+26
    (body+23). Body offsets = elemStart offsets - 3.
    """
    d = bytearray(25)
    d[0] = 28
    d[1] = staffIdx & 0x3F
    d[2] = fv
    d[12] = pitch
    d[21] = articUp
    d[23] = articDown
    return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)


def note_v0c4_grace_artic(tick, voice, staffIdx, fv, pitch, grace1, grace2, articUp=0):
    """v0xC4 grace note carrying an articulation byte (elemStart+24 = body+21)."""
    d = bytearray(25)
    d[0] = 28
    d[1] = staffIdx & 0x3F
    d[2] = fv
    d[3] = grace1
    d[4] = grace2
    d[12] = pitch
    d[21] = articUp
    return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)


def gen_v0c4_grace_ornament():
    # A normal note followed by an acciaccatura grace (grace1=0x20, grace2=0x04) that carries
    # a trill articulation byte (0x04 -> ornamentTrill). The grace attaches to the main chord;
    # the importer must apply the trill as an Ornament on the grace chord. The old grace path
    # made a plain Articulation, so the grace chord had no Ornament (isOrnament()==false).
    e = note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += note_v0c4_grace_artic(0, 0, 0, fv=4, pitch=62, grace1=0x20, grace2=0x04, articUp=0x04)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def note_v0c4_opts(tick, voice, staffIdx, fv, pitch, options=0, position=0):
    """v0xC4 note with options byte (elemStart+20 = d[17]) and position (elemStart+12 = d[9]).
    Used to trigger the hasScaleStringAnchors options-bit-0 string-number path."""
    d = bytearray(25)
    d[0] = 28; d[1] = staffIdx & 0x3F; d[2] = fv; d[9] = position & 0xFF
    d[12] = pitch; d[17] = options & 0xFF
    return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)


def lyric_v0c4(tick, voice, staffIdx, text, kie=0):
    """Lyric (type 6) element, UTF-16 LE (v0xC4 default). The element size
    grows with the text length:
      preamble (20 bytes) + text * 2 + null terminator (2) + 4-byte trailer
    Layout (offsets from elemStart):
      +0..+1: tick
      +2:     typeVoice
      +3:     size
      +4:     staffIdx
      +5..+9: 5 unknown bytes
      +0xA:   kie (text anchor)
      +0xB..+0x13: 9 unknown bytes
      +0x14..: UTF-16 LE text, null-terminated, zero-padded trailer
    """
    enc = text.encode('utf-16-le') + b'\x00\x00\x00\x00\x00\x00'   # null + 4 bytes
    body = bytearray(0x14 - 3 + len(enc))
    body[0] = 3 + len(body)        # size  (body+0  = elemStart+3)
    body[1] = staffIdx & 0x3F      # staffIdx (body+1)
    body[7] = kie                  # kie   (body+7  = elemStart+0xA)
    body[17:17 + len(enc)] = enc
    return struct.pack('<H', tick) + bytes([(6 << 4) | (voice & 0xF)]) + bytes(body)


def lyric_v0c4_latin1(tick, voice, staffIdx, text_latin1, kie=0):
    """Lyric element with text written as 1 byte/char (Latin-1), as some real
    v0xC4 files in the wild (e.g. Fe_cega_faca_amolada_tk.enc) store lyrics.
    text_latin1 must be a bytes object containing the per-character codepoints
    in Latin-1 (e.g. b'tx\\xe3' for 'txã').
    """
    enc = bytes(text_latin1) + b'\x00\x00\x00\x00'   # null + 3 bytes padding
    body = bytearray(0x14 - 3 + len(enc))
    body[0] = 3 + len(body)
    body[1] = staffIdx & 0x3F
    body[7] = kie
    body[17:17 + len(enc)] = enc
    return struct.pack('<H', tick) + bytes([(6 << 4) | (voice & 0xF)]) + bytes(body)


def lyric_v0c2(tick, voice, staffIdx, text, kie=0):
    """v0xC2 lyric element: text at elemStart+18 (textGapAfterKie=7, vs 9 for v0xC4).
    Layout (from elemStart):
      +3: size, +4: staffIdx, +5..+9: skip, +10: kie, +11..+17: skip, +18..: UTF-16 LE text.
    """
    enc = text.encode('utf-16-le') + b'\x00\x00\x00\x00\x00\x00'  # null + 4-byte padding
    body = bytearray(0x12 - 3 + len(enc))  # 0x12=18 = text offset; body starts at elemStart+3
    body[0] = 3 + len(body)    # size
    body[1] = staffIdx & 0x3F  # staffIdx
    body[7] = kie              # kie at elemStart+10 (= body offset 7)
    body[15:15 + len(enc)] = enc  # text at elemStart+18 (= body offset 15)
    return struct.pack('<H', tick) + bytes([(6 << 4) | (voice & 0xF)]) + bytes(body)


def gen_v0c4_partial_quarter_triplet():
    # Encore tick scale: 240 = quarter; triplet quarter = 240 * 2/3 = 160.
    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60, tuplet=0x32)  # 1st quarter triplet
    e += note_v0c4(160, 0, 0, fv=3, pitch=64, tuplet=0x32)  # 2nd quarter triplet (no 3rd)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_partial_triplet_unreduced_cumtick.enc
#
# 4/4 measure: quarter note at tick=0, then a large gap to tick=800, then a
# single explicit 3:2 quarter-triplet note at tick=800 (fv=3, tup=0x32).
#
# rdur(tick=800) = durTicks - 800 = 960 - 800 = 160.
# rdurFillsMeasure: 800 + 160 == 960 -> TRUE.
# faceWouldOverflow: 800 + 240(quarter) > 960 -> TRUE.
# -> marked in partialEndGroup (Fix 1).
#
# Gap-snap path: encTickFrac = Fraction(800, 960) (unreduced) > cumTick=1/4.
# -> cumTick = Fraction(800, 960) stored unreduced.
# rem3 = Fraction(1,1) - Fraction(800,960) = Fraction(160, 960) (unreduced).
# Fix 3 (old code): baseFrac = Fraction(160, 960*2) = Fraction(160, 1920).
# TDuration(Fraction(160,1920), truncate=false) -> assertion / crash.
# Fix 3 (new code): .reduced() = Fraction(1,12); TDuration(1/12, truncate=true)
#   = V_16TH; 1/16 != 1/12 -> validity check fails -> Fix 3 skipped. No crash.
# ===========================================================================
def gen_v0c4_partial_triplet_unreduced_cumtick():
    # Q at tick=0 (rdur=800, placed as V_QUARTER via face value).
    # 3:2 Q-triplet at tick=800 (rdur=160, fills measure end).
    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60, tuplet=0x00)  # Q at tick=0
    e += note_v0c4(800, 0, 0, fv=3, pitch=64, tuplet=0x32)  # 3:2 Q at tick=800 (rdur=160)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_multi_measure_slur.enc
#
# Three 4/4 measures. SLURSTART (0x21) ornaments at:
#   - measure 0 tick=0 with alMezuro=2 -> slur spans through measure 2
#   - measure 1 tick=480 with alMezuro=0 -> slur ends in same measure
# Encore .enc binaries do NOT emit SLURSTOP; the importer must resolve
# the endpoint from alMezuro and find a ChordRest in the target measure.
# ===========================================================================
def gen_v0c4_grace_slur_to_main():
    # 4/4 measure: appoggiatura grace at tick=0 + SLURSTART at tick=0 (alMezuro=0).
    # Regular note (the grace's parent) is at Encore tick=15, 15 ticks stolen by the
    # grace, so in MuseScore both grace and parent land at cumTick=0 (same beat).
    #
    # The slur resolver previously converted tick=15 to measTick+Fraction(15,960),
    # found no chord there, and dropped the slur.  With the fix, it snaps to the
    # nearest chord (measTick), detects the zero-span = grace-to-main case, and
    # creates a slur whose startElement is the grace chord.
    #
    # xoffset / xoffset2: grace=3, slurstart-xoffset=3, regular-note=5, xoffset2=5.
    # pixelSpan=2 → targetEndXoff=5 → regular note is the best end candidate.
    e  = ornament_v0c4(0, 0, 0, tipo=0x21, xoffset=3, alMezuro=0, xoffset2=5)  # SLURSTART
    e += note_v0c4_grace_xoff(0,  0, 0, fv=0x05, pitch=60, grace1=0x30, grace2=0x00, xoff=3)  # appoggiatura
    e += note_v0c4_xoff(15,  0, 0, fv=0x03, pitch=60, xoff=5)   # main note (parent of grace)
    e += note_v0c4(240, 0, 0, fv=0x03, pitch=62)                 # filler
    e += note_v0c4(480, 0, 0, fv=0x03, pitch=64)                 # filler
    e += note_v0c4(720, 0, 0, fv=0x03, pitch=65)                 # filler
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_grace_slur_to_later():
    # 3/4 measure: half note at tick=0, then SLURSTART+graces+quarter at tick=450/480.
    # Models the "measure 3" scenario in LA_LLEGA5.enc:
    #   tick=  0: half note (blanca)
    #   tick=450: SLURSTART + two appoggiatura graces (xoffset=20)
    #   tick=480: quarter note (the grace's parent) at xoffset=22
    #
    # In MuseScore: half=cumTick=0, graces+quarter=cumTick=1/2 (after half).
    # ps.startTick = measTick + Fraction(450,960) = measTick+15/32.
    # tick2rightSegment(15/32) → quarter note at 1/2 → has graces → slur starts from grace.
    # endTick = quarter note tick = measTick+1/2 > startTick → grace-to-LATER slur.
    #
    # Without fix: computeStartElement() creates a TimeTick anchor at 15/32 and
    #   falls back to firstElement(staff=0) → returns the half note → wrong start.
    # With fix: startElement explicitly set to first grace → correct.
    e  = note_v0c4(  0, 0, 0, fv=0x02, pitch=60)                                          # half note
    e += ornament_v0c4(450, 0, 0, tipo=0x21, xoffset=20, alMezuro=0, xoffset2=22)          # SLURSTART
    e += note_v0c4_grace_xoff(450, 0, 0, fv=0x05, pitch=64, grace1=0x30, grace2=0x00, xoff=20)  # grace 1
    e += note_v0c4_grace_xoff(450, 0, 0, fv=0x05, pitch=65, grace1=0x30, grace2=0x00, xoff=21)  # grace 2
    e += note_v0c4_xoff(480, 0, 0, fv=0x03, pitch=64, xoff=22)                            # quarter (parent)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(3, 4), e)], fill_ts=(3, 4))


def gen_v0c4_multi_measure_slur():
    # Measure 0: cross-measure slur from tick 0 across 2 measures.
    m0  = ornament_v0c4(0, 0, 0, tipo=0x21, xoffset=5, alMezuro=2, xoffset2=20)
    m0 += note_v0c4(  0, 0, 0, fv=3, pitch=60, tuplet=0)
    m0 += note_v0c4(240, 0, 0, fv=3, pitch=62, tuplet=0)
    m0 += note_v0c4(480, 0, 0, fv=3, pitch=64, tuplet=0)
    m0 += note_v0c4(720, 0, 0, fv=3, pitch=65, tuplet=0)
    m0 += end_marker()
    # Measure 1: same-measure slur from tick 480 (beat 3).
    m1  = ornament_v0c4(480, 0, 0, tipo=0x21, xoffset=20, alMezuro=0, xoffset2=30)
    m1 += note_v0c4(  0, 0, 0, fv=3, pitch=67, tuplet=0)
    m1 += note_v0c4(240, 0, 0, fv=3, pitch=69, tuplet=0)
    m1 += note_v0c4(480, 0, 0, fv=3, pitch=71, tuplet=0)
    m1 += note_v0c4(720, 0, 0, fv=3, pitch=72, tuplet=0)
    m1 += end_marker()
    # Measure 2: filler notes.
    m2  = note_v0c4(  0, 0, 0, fv=3, pitch=72, tuplet=0)
    m2 += note_v0c4(240, 0, 0, fv=3, pitch=71, tuplet=0)
    m2 += note_v0c4(480, 0, 0, fv=3, pitch=69, tuplet=0)
    m2 += note_v0c4(720, 0, 0, fv=3, pitch=67, tuplet=0)
    m2 += end_marker()
    return assemble(0xC4,
                    [(meas_hdr(4, 4), m0),
                     (meas_hdr(4, 4), m1),
                     (meas_hdr(4, 4), m2)],
                    fill_ts=(4, 4))


# ===========================================================================
# text_lyrics.enc
#
# 4/4 measure with 4 quarter notes (C D E F) and 4 LYRIC syllables (do re mi
# fa) preceding them at the matching ticks. The importer queues each LYRIC
# during the measure pass and attaches one syllable per ChordRest after the
# main loop completes.
# ===========================================================================
# ===========================================================================
# notes_tie_flag_on_note.enc
# v0xC4 (format 4.20) file whose tie is recorded only on the note: the first
# quarter carries grace1 low nibble 1 (outgoing tie) and there is no TIE
# element anywhere in the measure. A second quarter of the same pitch follows,
# so the tie has a receiver. Every generation writes this flag; reading it only
# for v0xC2 dropped the tie here.
# ===========================================================================
def gen_v0c4_tie_flag_on_note():
    e  = note_v0c4_grace(0,   0, 0, fv=3, pitch=60, grace1=0x01, grace2=0)
    e += note_v0c4_grace(240, 0, 0, fv=3, pitch=60, grace1=0x02, grace2=0)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))


# ===========================================================================
# notes_tie_dir_fc.enc
#
# Two C4 quarter notes tied together with a TIE element whose direction
# byte is 0xfc (instead of the common 0xfe). Both 0xfc and 0xfe denote a
# tie-start in Encore; only the arc direction differs. The importer
# treats any direction byte with the high bit set as a tie-start so the
# pairing succeeds and a Tie element is created.
# ===========================================================================
def stafftext_v0c4(tick, voice, staffIdx, text_index, yoffset=0):
    """STAFFTEXT ornament (type=5, tipo=0x1E). The element is position-only;
    the text payload lives in the TEXT block and is referenced by the
    `tind` byte at element offset +32. Element layout:
      d[0:2]   = tick
      d[2]     = typeVoice
      d[3]     = size (33)
      d[4]     = staffIdx
      d[5]     = tipo (0x1E)
      d[12:14] = yoffset s16 (Cartesian: positive = up, negative = below)
      d[32]    = tind (TEXT block index)
    """
    d = bytearray(33)
    struct.pack_into('<H', d, 0, tick)
    d[2] = (5 << 4) | (voice & 0xF)
    d[3] = 33
    d[4] = staffIdx & 0x3F
    d[5] = 0x1e
    struct.pack_into('<h', d, 12, yoffset)
    d[32] = text_index & 0xFF
    return bytes(d)


def text_block_v0c4(entries):
    """Build a TEXT block carrying `entries` (list of strings).
    Block layout:
      +0..+3: magic "TEXT"
      +4..+7: varSize (little-endian, content size including block header)
      +8..+9: 0x0000 sync
      +10..+11: entry count
      +12..+15: content size (sum of all entry payload sizes)
      then for each entry:
        +0..+1: payload size S
        +2..+S+1: payload
          +0..+13: 14 bytes header (zeros for tests)
          +14..+S-5: UTF-16 LE text
          +S-4..+S-1: 0x04 0x00 0x00 0x00 terminator
    """
    block = bytearray()
    content_size = 0
    entries_bytes = bytearray()
    for text in entries:
        text_bytes = text.encode('utf-16-le')
        payload_size = 14 + len(text_bytes) + 4
        payload = bytearray(14)
        payload += text_bytes
        payload += b'\x04\x00\x00\x00'
        entries_bytes += struct.pack('<H', payload_size) + bytes(payload)
        content_size += payload_size
    body = struct.pack('<HHI', 0, len(entries), content_size) + bytes(entries_bytes)
    block += b'TEXT' + struct.pack('<I', len(body)) + body
    return bytes(block)


def text_block_v0c4_multiline(entries):
    """Like text_block_v0c4 but each entry is a LIST of lines.

    Encore stores a multi-line comment with each line terminated by the
    U+0004 separator (bytes 04 00), and the whole string terminated by a
    U+0000 null. A single-line entry is the degenerate case of one line
    followed by 04 00 then 00 00 (matching text_block_v0c4's
    `\\x04\\x00\\x00\\x00` trailer).
    """
    content_size = 0
    entries_bytes = bytearray()
    for lines in entries:
        # Each line is followed by U+0004; the entry ends with a U+0000 null.
        joined = ''.join(line + chr(0x04) for line in lines)
        text_bytes = joined.encode('utf-16-le') + b'\x00\x00'
        payload_size = 14 + len(text_bytes)
        payload = bytearray(14) + text_bytes
        entries_bytes += struct.pack('<H', payload_size) + bytes(payload)
        content_size += payload_size
    body = struct.pack('<HHI', 0, len(entries), content_size) + bytes(entries_bytes)
    return b'TEXT' + struct.pack('<I', len(body)) + bytes(body)


def text_block_v0c4_rich(entries, run_count=2, desc_count=1, latin1=False):
    """Build a TEXT block whose entries carry Encore's rich-text run header, as real
    v0xC4/v0xC2 files do:
      +0..+1: run-offset table count (uint16 LE)
      +2..+3: formatting-descriptor count (uint16 LE)
      +4.. : run-offset table (run_count * uint32; formatting metadata, ignored on import)
      then desc_count 6-byte descriptors, then the text and a null terminator.
    The text therefore starts at 4 + run_count*4 + desc_count*6. A single-run, single-descriptor
    entry lands at the fixed offset 14; more runs or more descriptors push it further, so an
    importer that assumes a fixed offset (or a single descriptor) reads garbage. Real files use
    both one and two descriptors on the same block. `latin1` stores the text as Latin-1 (as older
    Windows files do) instead of UTF-16 LE, which is what turns a wrong offset into CJK gibberish."""
    content_size = 0
    entries_bytes = bytearray()
    for text in entries:
        if latin1:
            text_bytes = text.encode('latin-1') + b'\x00'
        else:
            text_bytes = text.encode('utf-16-le') + b'\x00\x00'
        header = struct.pack('<HH', run_count, desc_count)   # run-offset count + descriptor count
        header += b'\x00\x00\x00\x00' * run_count            # run-offset table (dummy offsets)
        header += (b'\x7f\x01' + struct.pack('<I', 8)) * desc_count  # desc_count 6-byte descriptors
        payload = bytes(header) + text_bytes
        entries_bytes += struct.pack('<H', len(payload)) + payload
        content_size += len(payload)
    body = struct.pack('<HHI', 0, len(entries), content_size) + bytes(entries_bytes)
    return b'TEXT' + struct.pack('<I', len(body)) + bytes(body)


def keychange_v0c4(tick, voice, staffIdx, tipo):
    """6-byte KEYCHANGE element (type=2). tipo encodes the fifths index:
    0=C, 1=F, 2=Bb, ..., 8=G, 9=D, ..., 14=C#. tipo=0 is a modulation back
    to no accidentals (legitimate key change)."""
    d = bytearray(3)
    d[0] = 6
    d[1] = staffIdx & 0x3F
    d[2] = tipo
    return struct.pack('<H', tick) + bytes([(2 << 4) | (voice & 0xF)]) + bytes(d)


# ===========================================================================
# text_staff_text.enc
#
# 4/4 measure with 4 quarter notes and 4 STAFFTEXT ornaments (tipo=0x1E)
# at the corresponding ticks. Each ornament's `tind` byte indexes into the
# TEXT block, which carries the actual text payload as UTF-16 LE entries.
# Importer wires STAFFTEXT to MuseScore StaffText elements.
# ===========================================================================
def gen_v0c4_staff_text():
    e  = stafftext_v0c4(  0, 0, 0, text_index=0)
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += stafftext_v0c4(240, 0, 0, text_index=1)
    e += note_v0c4( 240, 0, 0, fv=3, pitch=62)
    e += stafftext_v0c4(480, 0, 0, text_index=2)
    e += note_v0c4( 480, 0, 0, fv=3, pitch=64)
    e += stafftext_v0c4(720, 0, 0, text_index=3)
    e += note_v0c4( 720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    text = text_block_v0c4(['Allegretto', 'cresc.', 'dimin.', 'ten.'])
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4), text_override=text)


# ===========================================================================
# text_staff_text_multirun.enc
#
# A STAFFTEXT whose TEXT-block entry uses Encore's rich-text run header with more
# than one formatting run (run_count=3). The displayed text ("TAN TRAN") then
# starts at offset 4 + 3*4 + 6 = 22, not the single-run offset 14. An importer
# that hard-codes offset 14 lands inside the run-offset table and imports nothing;
# the text offset must be derived from the run count. 'TAN TRAN' is a non-tempo
# phrase so it stays a StaffText rather than being promoted to TempoText.
# ===========================================================================
def gen_v0c4_staff_text_multirun():
    e  = stafftext_v0c4(0, 0, 0, text_index=0)
    e += note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e += note_v0c4(480, 0, 0, fv=3, pitch=64)
    e += note_v0c4(720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    text = text_block_v0c4_rich(['TAN TRAN'], run_count=3)
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4), text_override=text)


# ===========================================================================
# text_staff_text_two_descriptors.enc
#
# A STAFFTEXT whose TEXT-block entry has TWO formatting descriptors (the header
# descriptor count at +2 is 2, not 1) and stores its text as Latin-1, as older
# Windows Encore files do for multi-run staff labels (e.g. "Cajas y Tambores").
# The text starts at 4 + run_count*4 + desc_count*6 = 4 + 2*4 + 2*6 = 24.
# An importer that assumes a single descriptor reads from offset 18, lands inside
# the second descriptor, and the encoding probe there sees a byte followed by 0x00
# and decodes the Latin-1 text as byte-swapped UTF-16 (CJK gibberish). Deriving the
# descriptor count from +2 recovers the correct text.
# ===========================================================================
def gen_v0c4_staff_text_two_descriptors():
    e  = stafftext_v0c4(0, 0, 0, text_index=0)
    e += note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e += note_v0c4(480, 0, 0, fv=3, pitch=64)
    e += note_v0c4(720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    text = text_block_v0c4_rich(['Cajas y Tambores'], run_count=2, desc_count=2, latin1=True)
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4), text_override=text)


# ===========================================================================
# text_staff_text_first_block_wins.enc
#
# Multi-part Encore files write one TEXT block per part view, with the SAME
# strings in DIFFERENT order. The ORN `tind` indices match only the FIRST
# (score) TEXT block. The importer must keep the first TEXT block and ignore
# later ones; keeping the last one resolves every tind against a reordered
# table and shows the wrong text (e.g. "Presto" -> "Xilo.").
#
# Fixture: one STAFFTEXT with tind=0, followed by two TEXT blocks whose entry
# 0 differs ('Alpha' in the first, 'Beta' in the second). Correct import keeps
# 'Alpha' (first block). 'Alpha'/'Beta' are non-tempo words so they stay as
# StaffText rather than being promoted to TempoText.
# ===========================================================================
def gen_v0c4_text_first_block_wins():
    e  = stafftext_v0c4(0, 0, 0, text_index=0)
    e += note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += end_marker()
    first  = text_block_v0c4(['Alpha', 'cresc.'])
    second = text_block_v0c4(['Beta', 'cresc.'])
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4),
                    text_override=first + second)


# ===========================================================================
# text_staff_text_multiline.enc
#
# A single STAFFTEXT ornament whose TEXT block entry holds a multi-line
# comment: two lines separated by Encore's U+0004 line separator, terminated
# by a U+0000 null. The importer must preserve BOTH lines (joined with '\n'),
# not truncate at the first U+0004. Regression for the bug where a multi-line
# staff-text comment was cut to its first line only.
# ===========================================================================
def gen_v0c4_staff_text_multiline():
    e  = stafftext_v0c4(0, 0, 0, text_index=0)
    e += note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += end_marker()
    text = text_block_v0c4_multiline([['Notes + change duration', '(third quarter to half)']])
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4), text_override=text)


# ===========================================================================
# text_stafftext_tempo_promotion.enc
# Two STAFFTEXT ornaments whose text payload spells a standard Italian
# tempo term ("Allegro") and a relative marking ("a tempo"). The importer
# should promote both to TempoText so the score's tempo map updates and
# the elements behave as tempo for layout/playback.
# A plain non-tempo word ("ten.") is included to confirm the StaffText
# path still works alongside.
# ===========================================================================
def gen_v0c4_stafftext_tempo_promotion():
    e  = stafftext_v0c4(  0, 0, 0, text_index=0)
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += stafftext_v0c4(240, 0, 0, text_index=1)
    e += note_v0c4( 240, 0, 0, fv=3, pitch=62)
    e += stafftext_v0c4(480, 0, 0, text_index=2)
    e += note_v0c4( 480, 0, 0, fv=3, pitch=64)
    e += note_v0c4( 720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    text = text_block_v0c4(['Allegro', 'ten.', 'a tempo'])
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4), text_override=text)


def gen_v0c4_keychange_to_c():
    # Measure 0: G major (tipo=8 -> 1 sharp) for the first beat then back to C.
    # Measure 1: KEYCHANGE to C major (tipo=0). This was previously dropped
    # because the importer's guard skipped tipo==0; the modulation must
    # still emit a key signature so the staff shows the natural sign.
    m0  = keychange_v0c4(0, 0, 0, tipo=8)
    m0 += note_v0c4(  0, 0, 0, fv=3, pitch=67)
    m0 += note_v0c4(240, 0, 0, fv=3, pitch=67)
    m0 += note_v0c4(480, 0, 0, fv=3, pitch=67)
    m0 += note_v0c4(720, 0, 0, fv=3, pitch=67)
    m0 += end_marker()
    m1  = keychange_v0c4(0, 0, 0, tipo=0)
    m1 += note_v0c4(  0, 0, 0, fv=3, pitch=65)
    m1 += note_v0c4(240, 0, 0, fv=3, pitch=65)
    m1 += note_v0c4(480, 0, 0, fv=3, pitch=65)
    m1 += note_v0c4(720, 0, 0, fv=3, pitch=65)
    m1 += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), m0), (meas_hdr(4, 4), m1)],
                    fill_ts=(4, 4))


def gen_v0c4_tie_dir_fc():
    e  = tie_v0c4( 0, 0, 0, direction=0xfc)
    e += note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e += note_v0c4(480, 0, 0, fv=3, pitch=60)
    e += note_v0c4(960, 0, 0, fv=3, pitch=62)
    e += note_v0c4(1440, 0, 0, fv=3, pitch=64)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def tie_v0c4_18byte(tick, voice, staffIdx, direction=0x02, startFlag=0, arcX1=0, arcX2=0):
    """18-byte TIE element matching real Encore 5 layout.

    Bytes:
      d[0:2]=tick, d[2]=typeVoice, d[3]=size=18, d[4]=rawStaff,
      d[5]=direction, d[6]=startFlag,
      d[7:10]=zeros, d[10]=arcX1, d[11]=zero, d[12]=arcX2, d[13:18]=zeros.

    When arcX1 == arcX2 the arc has zero horizontal extent: intra-chord
    decorative mark, isTieStart is overridden to false.
    When arcX1 != arcX2 the arc spans notes at different x positions: real
    forward tie, isTieStart is determined by the direction/startFlag bits.
    """
    d = bytearray(18)
    struct.pack_into('<H', d, 0, tick)
    d[2] = (3 << 4) | (voice & 0xF)
    d[3] = 18
    d[4] = staffIdx & 0x3F
    d[5] = direction
    d[6] = startFlag
    d[10] = arcX1 & 0xFF
    d[12] = arcX2 & 0xFF
    return bytes(d)


# ===========================================================================
# notes_tie_intra_chord_arc_no_spurious.enc
# BUG FIX: 18-byte TIE elements with arcX1==arcX2 are intra-chord decorative
# arcs in Encore (tiny vertical curves connecting two chord notes at the same
# x position). They must NOT create forward ties in MuseScore.
#
# Without the fix: dirByte=0x02 triggers isTieStart=true → pendingTieNote set
# for both chord notes → long spurious ties reaching the next same-pitch note.
# With the fix: arcX1==arcX2 detected → isTieStart overridden to false → no ties.
#
# Pattern from POPURRI JOTAS5.enc M103: 4 identical 18-byte TIE@0 si=0 with
# arcX1=arcX2=12 (intra-chord arc). Both chord notes p60 and p62 had ties
# created to their next occurrence (p60@tick=240, p62@tick=240).
#
# Fixture: 4/4 measure. Four duplicate 18-byte TIE@0 with arcX1=arcX2=12 and
# dirByte=0x02 (same pattern as POPURRI). Chord: p60+p62 at tick=0.
# Same pitches appear again at tick=240. Expected: NO ties created.
# ===========================================================================
def gen_v0c4_tie_intra_chord_arc_no_spurious():
    # 4 identical intra-chord TIE elements (POPURRI pattern)
    intra = tie_v0c4_18byte(0, 0, 0, direction=0x02, startFlag=0, arcX1=12, arcX2=12)
    e  = intra + intra + intra + intra   # 4 duplicates, same as real Encore files
    e += note_v0c4(  0, 0, 0, fv=3, pitch=60)   # C4 quarter (chord note 1)
    e += note_v0c4(  0, 0, 0, fv=3, pitch=62)   # D4 quarter (chord note 2)
    e += note_v0c4(480, 0, 0, fv=3, pitch=60)   # C4 again, would receive spurious tie
    e += note_v0c4(480, 0, 0, fv=3, pitch=62)   # D4 again, would receive spurious tie
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_tie_18byte_real_forward.enc
# Real forward tie using 18-byte format with arcX1 != arcX2.
# The arc has a non-zero horizontal extent: forward tie must still be created.
# Fixture: C4 quarter at tick=0, TIE@0 with arcX1=12 arcX2=50 (different),
# dirByte=0x02. C4 at tick=480 receives the tie.
# ===========================================================================
def gen_v0c4_tie_18byte_real_forward():
    e  = tie_v0c4_18byte(0, 0, 0, direction=0x02, startFlag=0, arcX1=12, arcX2=50)
    e += note_v0c4(  0, 0, 0, fv=3, pitch=60)   # C4 quarter (tie-start)
    e += note_v0c4(480, 0, 0, fv=3, pitch=60)   # C4 quarter (tie-end)
    e += note_v0c4(960, 0, 0, fv=3, pitch=62)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_tie_dir_04_forward.enc
# Real forward tie whose +5 arc-curvature byte is 0x04 and whose +6 flag is 0.
# Byte +5 is a signed arc-curvature value (0x02/0x04 curve down, 0xFC/0xFE
# curve up), NOT a bitfield. The forward horizontal span (arcX1 < arcX2)
# identifies the real tie. 0x04 sets neither bit 7 nor bit 1, so the old
# bit-based rule (+5&0x80 || +5&0x02 || +6&0x80) missed it and dropped the tie.
#
# Pattern from a real Encore 5 multi-staff score: at one tick several staves
# carry TIE@+5=0x04, +6=0x00, arcX1=56, arcX2=112; the staves with +5=0xFC got
# ties while the +5=0x04 staves silently lost theirs.
#
# Fixture: C4 quarter at tick=0, TIE@0 with +5=0x04, +6=0x00, arcX1=56,
# arcX2=112. C4 at tick=480 receives the tie. Expected: exactly one tie.
# ===========================================================================
def gen_v0c4_tie_dir_04_forward():
    e  = tie_v0c4_18byte(0, 0, 0, direction=0x04, startFlag=0, arcX1=56, arcX2=112)
    e += note_v0c4(  0, 0, 0, fv=3, pitch=60)   # C4 quarter (tie-start)
    e += note_v0c4(480, 0, 0, fv=3, pitch=60)   # C4 quarter (tie-end)
    e += note_v0c4(960, 0, 0, fv=3, pitch=62)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_tie_crossmeasure_arcxx_equal.enc
# BUG FIX: 18-byte TIE elements with arcX1==arcX2 AND startFlag=0x80 are
# cross-measure ties where the destination is in the next measure and Encore
# stores arcX2=arcX1 as a placeholder (no right-side x coordinate available).
#
# Without fix: arcX1==arcX2 override sets isTieStart=false → tie dropped.
# With fix: startFlag=0x80 prevents the override → tie created correctly.
#
# Fixture: two 4/4 measures. Measure 1: TIE@0 with arcX1=arcX2=12 and
# startFlag=0x80 (cross-measure pattern), C4 whole note. Measure 2: C4 whole
# note (tie receiver). Expected: tie connects measure-1 note to measure-2 note.
# ===========================================================================
def gen_v0c4_tie_crossmeasure_arcxx_equal():
    tie_elem = tie_v0c4_18byte(0, 0, 0, direction=0x04, startFlag=0x80, arcX1=12, arcX2=12)
    m1 = tie_elem + note_v0c4(0, 0, 0, fv=1, pitch=60) + end_marker()  # fv=1: whole note
    m2 = note_v0c4(0, 0, 0, fv=1, pitch=60) + end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), m1), (meas_hdr(4, 4), m2)], fill_ts=(4, 4))


# ===========================================================================
# notes_tie_spurious_far_receiver.enc
# BUG FIX: a tie-start whose immediately-following note is a DIFFERENT pitch
# must NOT be completed against a much-later note of the same pitch. A real
# Encore tie always connects a note to the consecutive note (the receiver
# begins exactly where the tie-start note ends). When no consecutive same-pitch
# note exists, the tie-start is spurious and must be dropped.
#
# Without fix: pendingTieNote persists keyed only by (staff, voice, pitch), so
# C4@0 completes against C4@720 → a tie arcing across the whole measure.
# With fix: the receiver's onset (720) does not match the tie-start note's end
# tick (240) → no tie created.
#
# Fixture: one 4/4 measure with four quarters. C4@0 carries a real forward TIE
# (arcX1<arcX2). The next note is D4@240 (different pitch), then D4@480, then
# C4@720 (same pitch as the tie-start, but far away). Expected: zero ties.
# ===========================================================================
def gen_v0c4_tie_spurious_far_receiver():
    e  = tie_v0c4_18byte(0, 0, 0, direction=0x02, arcX1=12, arcX2=50)  # real forward tie-start on C4@0
    e += note_v0c4(  0, 0, 0, fv=3, pitch=60)   # C4 quarter (tie-start)
    e += note_v0c4(240, 0, 0, fv=3, pitch=62)   # D4 quarter (consecutive, different pitch)
    e += note_v0c4(480, 0, 0, fv=3, pitch=62)   # D4 quarter
    e += note_v0c4(720, 0, 0, fv=3, pitch=60)   # C4 quarter (far same-pitch decoy receiver)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_articulations.enc
#
# 4/4 measure with quarter notes carrying staccato (0x1D), accent (0x12),
# tenuto (0x1C), marcato (0x13). Encore stores the articulation glyph index
# in articulationUp / articulationDown; the importer maps each known value
# to a MuseScore SymId via encArticulation2SymId.
# ===========================================================================
# ===========================================================================
# ornaments_double_barline_multi_staff.enc
#
# v0xC4 file with two instruments. m1 ends with a DOUBLEL barline
# (barTypeEnd=3); the rest are normal. Encore renders the double bar
# across every staff on the system. The importer must therefore apply
# BarLineType::DOUBLE to every track, not only track 0 -- the bug the
# user observed on Beethoven Plectro m26 where the double bar only
# showed on instrument 1.
# ===========================================================================
def gen_v0c4_double_barline_multi_staff():
    pre = bytearray(SKELETON_PRE)
    pre[0x32] = 2   # instrumentCount = 2 (skeleton ships with 1 TK block)
    # m1: 4/4 with DOUBLEL (=3) end barline + one quarter note on staff 0
    e1 = note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    body  = meas_block(meas_hdr(4, 4, barTypeEnd=3), e1)
    # Five trailing empty measures to keep the 6-measure skeleton count
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return bytes(pre) + body + SKELETON_POST


# ===========================================================================
# ornaments_wedgestart_at_measure_end.enc
#
# Two-measure 4/4 piece (durTicks=960 per measure at beatTicks=240).
# m1 carries a WEDGESTART (tipo=0x1D, diminuendo) at tick=durTicks=960
# with alMezuro=1. Encore stores hairpins whose visible start sits on
# the bar line at this exact boundary tick (Beethoven Plectro m1 -> m3
# f-p diminuendo); the importer must NOT drop the element just because
# its raw tick equals the measure's duration.
# ===========================================================================
def gen_v0c4_wedgestart_at_measure_end():
    # alMezuro=1: hairpin spans into next measure
    e  = ornament_v0c4(960, 0, 0, tipo=0x1D, alMezuro=1, speguleco=2)  # diminuendo
    e += note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e += note_v0c4(480, 0, 0, fv=3, pitch=64)
    e += note_v0c4(720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_dynamics.enc
#
# 4/4 measure with four quarter notes carrying the four currently mapped
# size-16 dynamic ornaments at the corresponding ticks:
#   0x82=p, 0x81=pp, 0x85=f, 0x86=ff.
# Encore writes the dynamic ORN before the chord note in MEAS order; the
# importer creates a MuseScore Dynamic element on the ChordRest segment.
# ===========================================================================
def gen_v0c4_dynamics():
    e  = ornament_v0c4(  0, 0, 0, tipo=0x82)
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += ornament_v0c4(240, 0, 0, tipo=0x81)
    e += note_v0c4( 240, 0, 0, fv=3, pitch=62)
    e += ornament_v0c4(480, 0, 0, tipo=0x85)
    e += note_v0c4( 480, 0, 0, fv=3, pitch=64)
    e += ornament_v0c4(720, 0, 0, tipo=0x86)
    e += note_v0c4( 720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_dynamics_stacked.enc
#
# 4/4 measure whose first chord carries TWO dynamic ORNs at the identical
# tick (0) and xoffset (13): 0x86=ff followed by 0x87=fff. Encore stores
# such collisions when the score view and a part view each hold a dynamic
# on the same beat (the two differ only in y-placement). Encore renders one
# dynamic per beat; the importer must collapse the pair to a single Dynamic
# (the first-read one, ff) rather than stacking two contradictory dynamics
# on one ChordRest. Before the fix the importer kept both ff and fff.
# ===========================================================================
def gen_v0c4_dynamics_stacked():
    e  = ornament_v0c4(0, 0, 0, tipo=0x86, xoffset=13, yoffset=15)
    e += ornament_v0c4(0, 0, 0, tipo=0x87, xoffset=13, yoffset=-4)
    e += note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e += note_v0c4(480, 0, 0, fv=3, pitch=64)
    e += note_v0c4(720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_dynamics_full.enc
#
# Two-measure 4/4 piece with one of every currently mapped dynamic ORN
# tipo, written voice=4 with the staffByte high bit set -- the encoding
# Encore uses for system-level dynamic markings (encore-symbols.enc m1-m3
# reference). Verifies both the voice=4 acceptance and the new
# tipo-to-DynamicType mapping for the extended ladder
# (ppp / pp / p / mp / mf / f / ff / fff / sfz / sffz / fp).
# ===========================================================================
def gen_v0c4_dynamics_full():
    pairs_m1 = [(0, 0x80), (240, 0x81), (480, 0x82), (720, 0x83)]   # ppp pp p mp
    e1 = b''
    for tick, tipo in pairs_m1:
        e1 += ornament_v0c4_voice4_staffhi(tick, 0, tipo)
        e1 += note_v0c4(tick, 0, 0, fv=3, pitch=60)
    e1 += end_marker()
    pairs_m2 = [(0, 0x84), (240, 0x85), (480, 0x86), (720, 0x87)]   # mf f ff fff
    e2 = b''
    for tick, tipo in pairs_m2:
        e2 += ornament_v0c4_voice4_staffhi(tick, 0, tipo)
        e2 += note_v0c4(tick, 0, 0, fv=3, pitch=60)
    e2 += end_marker()
    pairs_m3 = [(0, 0x88), (240, 0x89), (480, 0x8A), (720, 0xAA)]    # sfz sffz fp fz
    e3 = b''
    for tick, tipo in pairs_m3:
        e3 += ornament_v0c4_voice4_staffhi(tick, 0, tipo)
        e3 += note_v0c4(tick, 0, 0, fv=3, pitch=60)
    e3 += end_marker()
    pairs_m4 = [(0, 0xAB)]                                          # sf
    e4 = b''
    for tick, tipo in pairs_m4:
        e4 += ornament_v0c4_voice4_staffhi(tick, 0, tipo)
        e4 += note_v0c4(tick, 0, 0, fv=3, pitch=60)
    e4 += note_v0c4(240, 0, 0, fv=3, pitch=60)
    e4 += note_v0c4(480, 0, 0, fv=3, pitch=60)
    e4 += note_v0c4(720, 0, 0, fv=3, pitch=60)
    e4 += end_marker()
    return assemble(0xC4,
                    [(meas_hdr(4, 4), e1),
                     (meas_hdr(4, 4), e2),
                     (meas_hdr(4, 4), e3),
                     (meas_hdr(4, 4), e4)],
                    fill_ts=(4, 4))


# ===========================================================================
# text_staff_text_placement.enc
#
# Two STAFFTEXT ornaments at consecutive quarter notes:
#   - tick 0  : yoffset = +10 -> Cartesian "above" -> MuseScore default
#   - tick 480: yoffset = -10 -> Cartesian "below" -> PlacementV::BELOW
# Lets the unit test verify the importer reads the y-offset and sets
# placement to BELOW only when the value is negative.
# ===========================================================================
def gen_v0c4_staff_text_placement():
    e  = stafftext_v0c4(  0, 0, 0, text_index=0, yoffset=10)
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += note_v0c4( 240, 0, 0, fv=3, pitch=62)
    e += stafftext_v0c4(480, 0, 0, text_index=1, yoffset=-10)
    e += note_v0c4( 480, 0, 0, fv=3, pitch=64)
    e += note_v0c4( 720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    text = text_block_v0c4(['Above', 'ten'])
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4), text_override=text)


# ===========================================================================
# ornaments_arpeggio.enc
#
# 4/4 measure with a quarter-note chord at tick 0 (C major triad) and a
# rest filling the rest of the measure. An ORN tipo=0x22 (ARPEGGIO) sits
# at the same tick/voice/staff as the chord; the importer must attach an
# Arpeggio element to the chord.
# ===========================================================================
def gen_v0c4_arpeggio():
    e  = ornament_v0c4(  0, 0, 0, tipo=0x22)
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += note_v0c4(   0, 0, 0, fv=3, pitch=64)
    e += note_v0c4(   0, 0, 0, fv=3, pitch=67)
    e += rest_v0c4( 240, 0, 0, fv=2)  # half rest
    e += rest_v0c4( 720, 0, 0, fv=3)  # quarter rest
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_articulations():
    e  = note_v0c4_artic(  0, 0, 0, fv=3, pitch=60, articUp=0x1D)   # staccato
    e += note_v0c4_artic(240, 0, 0, fv=3, pitch=62, articUp=0x12)   # accent
    e += note_v0c4_artic(480, 0, 0, fv=3, pitch=64, articUp=0x1C)   # tenuto
    e += note_v0c4_artic(720, 0, 0, fv=3, pitch=65, articUp=0x13)   # marcato
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_articulations_combo.enc
#
# Encore packs two articulation glyphs into a single articulationUp byte
# (tested combos derived from encore-symbols.enc m8-m13):
#   0x24 = tenuto + staccato
#   0x17 = accent + staccato
#   0x27 = marcato + tenuto
#   0x15 = marcato + staccato
#   0x23 = accent + tenuto
#   0x2D = tenuto + staccatissimo
#   0x2B = accent + staccatissimo
# The importer must emit BOTH Articulation elements on the chord; if only
# one survives the articulation retention drops by ~85 % (tenuto 9 -> 1).
# ===========================================================================
def meas_hdr_coda(timeSigNum, timeSigDen, bpm=100, barTypeEnd=0, codaByte=0):
    """meas_hdr variant that also writes the repeat-mark byte (low byte of
    the coda u32 at offset 0x1A). Encore stores D.C./D.S./Fine markers
    here: 0x80=DCALCODA, 0x81=DSALCODA, 0x82=DCALFINE, 0x86=FINE, 0x87=DC."""
    h = bytearray(meas_hdr(timeSigNum, timeSigDen, bpm=bpm, barTypeEnd=barTypeEnd))
    h[0x1A] = codaByte & 0xFF
    if codaByte:
        # The coda field is a u32: low byte = mark type, upper bytes = style and
        # x-position. Encore draws the mark below the staff without the style
        # byte; 0x0B at +0x1B places "Fine"/"D.C."/etc. above it.
        h[0x1B] = 0x0B
        h[0x1C] = 12
    return bytes(h)


def gen_v0c4_section_markers():
    # m1: regular measure with a SEGNO ORN (tipo=0xA2) attached.
    # m2: regular measure with a CODA ORN (tipo=0xA6).
    # m3: regular measure with DOTTED end barline (barTypeEnd=0x08).
    e1  = ornament_v0c4(0, 0, 0, tipo=0xA2)
    e1 += note_v0c4(0, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(240, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(480, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(720, 0, 0, fv=3, pitch=60)
    e1 += end_marker()
    e2  = ornament_v0c4(0, 0, 0, tipo=0xA6)
    e2 += note_v0c4(0, 0, 0, fv=3, pitch=60)
    e2 += note_v0c4(240, 0, 0, fv=3, pitch=60)
    e2 += note_v0c4(480, 0, 0, fv=3, pitch=60)
    e2 += note_v0c4(720, 0, 0, fv=3, pitch=60)
    e2 += end_marker()
    e3  = note_v0c4(0, 0, 0, fv=3, pitch=60)
    e3 += note_v0c4(240, 0, 0, fv=3, pitch=60)
    e3 += note_v0c4(480, 0, 0, fv=3, pitch=60)
    e3 += note_v0c4(720, 0, 0, fv=3, pitch=60)
    e3 += end_marker()
    return assemble(0xC4,
                    [(meas_hdr(4, 4), e1),
                     (meas_hdr(4, 4), e2),
                     (meas_hdr(4, 4, barTypeEnd=8), e3)],
                    fill_ts=(4, 4))


def gen_v0c4_jump_marks_all():
    # Exercises every EncRepeatType byte that Encore can place in the
    # meas-header coda field (offset 0x1A low byte) plus the ORN-based
    # SEGNO / TO_CODA / CODA markers.  The reference for the 10 Encore
    # UI options (D.C., D.C. al Coda, D.C. al Fine, Coda, To Coda, D.S.,
    # D.S. al Coda, D.S. al Fine, Segno, Fine).
    nplain = note_v0c4(0, 0, 0, fv=3, pitch=60)
    nplain += note_v0c4(240, 0, 0, fv=3, pitch=60)
    nplain += note_v0c4(480, 0, 0, fv=3, pitch=60)
    nplain += note_v0c4(720, 0, 0, fv=3, pitch=60)
    end = end_marker()
    blocks = []
    # m1 segno (ORN), m2 coda (ORN), m3 to-coda (ORN)
    blocks.append((meas_hdr(4, 4), ornament_v0c4(0, 0, 0, tipo=0xA2) + nplain + end))
    blocks.append((meas_hdr(4, 4), ornament_v0c4(0, 0, 0, tipo=0xA6) + nplain + end))
    blocks.append((meas_hdr(4, 4), ornament_v0c4(0, 0, 0, tipo=0xA5) + nplain + end))
    # m4..m12: every coda-byte variant in EncRepeatType
    for cb in (0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89):
        blocks.append((meas_hdr_coda(4, 4, codaByte=cb), nplain + end))
    return assemble(0xC4, blocks, fill_ts=(4, 4))


def gen_v0c4_jump_marks():
    # Three measures exercising jump-mark encoding:
    #   m1: ORN tipo=0xA5 -> "To Coda" Marker at measure start.
    #   m2: meas-header coda byte = 0x81 -> D.S. al Coda jump at end.
    #   m3: meas-header coda byte = 0x87 -> D.C. jump at end.
    e1  = ornament_v0c4(0, 0, 0, tipo=0xA5)
    e1 += note_v0c4(0, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(240, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(480, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(720, 0, 0, fv=3, pitch=60)
    e1 += end_marker()
    e2  = note_v0c4(0, 0, 0, fv=3, pitch=60)
    e2 += note_v0c4(240, 0, 0, fv=3, pitch=60)
    e2 += note_v0c4(480, 0, 0, fv=3, pitch=60)
    e2 += note_v0c4(720, 0, 0, fv=3, pitch=60)
    e2 += end_marker()
    e3  = note_v0c4(0, 0, 0, fv=3, pitch=60)
    e3 += note_v0c4(240, 0, 0, fv=3, pitch=60)
    e3 += note_v0c4(480, 0, 0, fv=3, pitch=60)
    e3 += note_v0c4(720, 0, 0, fv=3, pitch=60)
    e3 += end_marker()
    return assemble(0xC4,
                    [(meas_hdr(4, 4), e1),
                     (meas_hdr_coda(4, 4, codaByte=0x81), e2),  # DSALCODA
                     (meas_hdr_coda(4, 4, codaByte=0x87), e3)], # DC
                    fill_ts=(4, 4))


def orn16_v0c4(tick, voice, staffIdx, tipo):
    """16-byte size-16 ORN element (bowing marks, stand-alone fingering ORNs).
    Layout: tick(2) + typeVoice(1) + size(1) + staffByte(1) + tipo(1) + 10 zeros.
    """
    d = bytearray(16)
    struct.pack_into('<H', d, 0, tick)
    d[2] = (5 << 4) | (voice & 0xF)
    d[3] = 16
    d[4] = staffIdx & 0x3F
    d[5] = tipo
    return bytes(d)


def gen_v0c4_bowing_orn():
    # Stand-alone size-16 ORN tipos 0xC5 (down-bow) and 0xC4 (up-bow)
    # placed before the chord at the same tick. The importer must add the
    # corresponding Articulation (stringsDownBow / stringsUpBow) to the chord.
    e  = orn16_v0c4(  0, 0, 0, tipo=0xC5)   # down-bow
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += orn16_v0c4(240, 0, 0, tipo=0xC4)   # up-bow
    e += note_v0c4( 240, 0, 0, fv=3, pitch=62)
    e += orn16_v0c4(480, 0, 0, tipo=0xC5)   # down-bow
    e += note_v0c4( 480, 0, 0, fv=3, pitch=64)
    e += orn16_v0c4(720, 0, 0, tipo=0xC4)   # up-bow
    e += note_v0c4( 720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_fingering_orn():
    # Stand-alone size-16 ORN tipos 0xB9..0xBD placed before the chord.
    # tipo = 0xB8 + finger_number (1..5). The importer attaches a Fingering
    # element with text "1".."5" to the top note of the chord.
    e  = orn16_v0c4(  0, 0, 0, tipo=0xB9)   # finger 1
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += orn16_v0c4(240, 0, 0, tipo=0xBA)   # finger 2
    e += note_v0c4( 240, 0, 0, fv=3, pitch=62)
    e += orn16_v0c4(480, 0, 0, tipo=0xBB)   # finger 3
    e += note_v0c4( 480, 0, 0, fv=3, pitch=64)
    e += orn16_v0c4(720, 0, 0, tipo=0xBC)   # finger 4
    e += note_v0c4( 720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    m1 = (meas_hdr(4, 4), e)
    e2  = orn16_v0c4(  0, 0, 0, tipo=0xBD)  # finger 5
    e2 += note_v0c4(   0, 0, 0, fv=3, pitch=67)
    e2 += end_marker()
    return assemble(0xC4, [m1, (meas_hdr(4, 4), e2)], fill_ts=(4, 4))


def gen_v0c4_system_break():
    # Six quarter-note measures (3 per system from the bazo.enc skeleton's LINE
    # blocks). Tests that the importer places a LINE break after measure 2 (the
    # last measure of system 0) and no break after measure 5 (last overall).
    def one_measure():
        e  = note_v0c4(  0, 0, 0, fv=3, pitch=60)
        e += note_v0c4(240, 0, 0, fv=3, pitch=62)
        e += note_v0c4(480, 0, 0, fv=3, pitch=64)
        e += note_v0c4(720, 0, 0, fv=3, pitch=65)
        e += end_marker()
        return (meas_hdr(4, 4), e)
    return assemble(0xC4, [one_measure() for _ in range(6)], fill_ts=(4, 4))


def gen_v0c4_system_break_mcount_zero():
    # SCO5 (big-endian Encore 5) does not surface the per-line measureCount, so the
    # system span must be derived from the line start deltas. Simulate that here in a
    # little-endian file by zeroing every LINE measureCount byte (content +12) while
    # keeping the line starts intact; the importer must still lock system 0 to
    # measures 0..2 (derived as start[1]-start[0] = 3).
    data = bytearray(gen_v0c4_system_break())
    pos = 0
    while True:
        p = data.find(b'LINE', pos)
        if p < 0:
            break
        data[p + 8 + 12] = 0   # content +12 = measureCount byte
        pos = p + 4
    return bytes(data)


def gen_v0c4_staccato_orn():
    # Encore stores per-chord staccato as a separate size-16 ORN tipo=0xC9
    # next to the chord's notes. Its MusicXML export drops the byte (Beethoven
    # Plectro shows 1813 of these but only one <staccato/> in the reference).
    # The importer must add a Staccato articulation per ORN, deduping against
    # a per-note articulation byte (0x1D) that may already be present.
    e  = ornament_v0c4(  0, 0, 0, tipo=0xC9)
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += ornament_v0c4(240, 0, 0, tipo=0xC9)
    e += note_v0c4( 240, 0, 0, fv=3, pitch=62)
    e += note_v0c4( 480, 0, 0, fv=3, pitch=64)  # no staccato
    e += ornament_v0c4(720, 0, 0, tipo=0xC9)
    # This note also carries the artic byte 0x1D directly; dedup must keep
    # exactly one Staccato on the chord.
    e += note_v0c4_artic(720, 0, 0, fv=3, pitch=65, articUp=0x1D)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_trill_spanner():
    # FIX: TRILL_START (0x36) + TRILL_END (0x35) now produce a Trill spanner
    # (tr + wavy line) instead of a glyph-only Ornament.
    # Pattern: 0x36 at tick=0 (start), 0x37 at tick=240 (secondary tr glyph),
    # 0x35 at tick=480 (span end). Expected: 1 Trill spanner + 1 Ornament glyph.
    e  = ornament_v0c4(  0, 0, 0, tipo=0x36)
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += ornament_v0c4(240, 0, 0, tipo=0x37)
    e += note_v0c4( 240, 0, 0, fv=3, pitch=62)
    e += ornament_v0c4(480, 0, 0, tipo=0x35)
    e += note_v0c4( 480, 0, 0, fv=3, pitch=64)
    e += note_v0c4( 720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_trill_no_end_marker.enc
# REGRESSION: TRILL_START (0x36) with no TRILL_END and alMezuro=0 must fall
# back to an Ornament glyph (no Trill spanner). This guards against the
# single-beat trill case becoming an unwanted zero-length spanner.
# ===========================================================================
def gen_v0c4_trill_no_end_marker():
    e  = ornament_v0c4(0,   0, 0, tipo=0x36)   # no matching 0x35, alMezuro=0
    e += note_v0c4(  0,   0, 0, fv=3, pitch=60)
    e += note_v0c4(240,   0, 0, fv=3, pitch=62)
    e += note_v0c4(480,   0, 0, fv=3, pitch=64)
    e += note_v0c4(720,   0, 0, fv=3, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# A TRILL_END (0x35) that lives several measures after an unrelated TRILL_START (0x36) on the same
# track is a standalone terminal trill, NOT the end of that far-away start. m0 has a 0x36 start (no
# end in its own measure -> glyph); m2 has a lone 0x35 that must render its own trill on m2's note.
# Before the fix the m0 start greedily consumed m2's end into a 2-measure span, leaving m2 bare.
def gen_v0c4_trill_end_far_from_start():
    m0  = ornament_v0c4(0, 0, 0, tipo=0x36)
    m0 += note_v0c4(  0, 0, 0, fv=3, pitch=60)
    m0 += note_v0c4(240, 0, 0, fv=3, pitch=62)
    m0 += note_v0c4(480, 0, 0, fv=3, pitch=64)
    m0 += note_v0c4(720, 0, 0, fv=3, pitch=65)
    m0 += end_marker()
    m1  = note_v0c4(0, 0, 0, fv=1, pitch=67) + end_marker()   # whole note filler
    m2  = ornament_v0c4(0, 0, 0, tipo=0x35)
    m2 += note_v0c4(0, 0, 0, fv=1, pitch=69)                  # the terminal trilled note
    m2 += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), m0), (meas_hdr(4, 4), m1), (meas_hdr(4, 4), m2)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_trill_cross_measure.enc
# FEATURE: TRILL_START with alMezuro=2 must create a Trill spanner spanning
# to the end of the 2nd measure after the start measure.
# ===========================================================================
def gen_v0c4_trill_cross_measure():
    # Measure 0: trill starts on beat 1, alMezuro=2 (spans 2 measures forward)
    e0  = ornament_v0c4(0, 0, 0, tipo=0x36, alMezuro=2)
    e0 += note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e0 += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e0 += note_v0c4(480, 0, 0, fv=3, pitch=64)
    e0 += note_v0c4(720, 0, 0, fv=3, pitch=65)
    e0 += end_marker()
    # Measures 1 and 2 have plain notes (the trill wavy line spans over them)
    e1  = note_v0c4(  0, 0, 0, fv=3, pitch=64)
    e1 += note_v0c4(240, 0, 0, fv=3, pitch=65)
    e1 += note_v0c4(480, 0, 0, fv=3, pitch=67)
    e1 += note_v0c4(720, 0, 0, fv=3, pitch=69)
    e1 += end_marker()
    return assemble(0xC4, [
        (meas_hdr(4, 4), e0),
        (meas_hdr(4, 4), e1),
        (meas_hdr(4, 4), e1),
    ], fill_ts=(4, 4))


def gen_v0c4_technical():
    # Per-note technical markings derived from encore-symbols.enc:
    #   0x44, 0x45 -> thumb-position
    #   0x46       -> open-string (Fingering STRING_NUMBER text "0")
    #   0x0D..0x11 -> fingering 1..5 (Fingering text "1".."5")
    #   0x1E, 0x1F -> harmonic
    e  = note_v0c4_artic(  0, 0, 0, fv=3, pitch=60, articUp=0x44)   # thumb-pos
    e += note_v0c4_artic(240, 0, 0, fv=3, pitch=62, articUp=0x46)   # open-string
    e += note_v0c4_artic(480, 0, 0, fv=3, pitch=64, articUp=0x0D)   # fingering 1
    e += note_v0c4_artic(720, 0, 0, fv=3, pitch=65, articUp=0x0E)   # fingering 2
    e += end_marker()
    m1 = (meas_hdr(4, 4), e)
    e2  = note_v0c4_artic(  0, 0, 0, fv=3, pitch=60, articUp=0x0F)  # fingering 3
    e2 += note_v0c4_artic(240, 0, 0, fv=3, pitch=62, articUp=0x10)  # fingering 4
    e2 += note_v0c4_artic(480, 0, 0, fv=3, pitch=64, articUp=0x11)  # fingering 5
    e2 += note_v0c4_artic(720, 0, 0, fv=3, pitch=65, articUp=0x1E)  # harmonic
    e2 += end_marker()
    return assemble(0xC4, [m1, (meas_hdr(4, 4), e2)], fill_ts=(4, 4))


def gen_v0c4_fermatas():
    # Encore stores the fermata above/below distinction by which slot
    # carries the byte (articUp = above, articDown = below).
    e  = note_v0c4_artic(  0, 0, 0, fv=3, pitch=60, articDown=0x21)  # below -> inverted
    e += note_v0c4_artic(240, 0, 0, fv=3, pitch=62, articUp=0x20)    # above -> upright
    e += note_v0c4_artic(480, 0, 0, fv=3, pitch=64)
    e += note_v0c4_artic(720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_fermata_below_not_in_tuplet():
    # BUG FIX companion: articulationDown=0x21 on a non-tuplet note must create
    # a fermataBelow; on a tuplet note it must be suppressed (same dual-meaning rule).
    # m1: q(articDown=0x21, no tuplet) -> fermata below
    #     q(articDown=0x21, triplet)   -> suppressed
    #     q(triplet) q(triplet)
    def note_artic_tuplet(tick, fv, pitch, articDown, tuplet):
        d = bytearray(25)
        d[0]=28; d[1]=0; d[2]=fv; d[10]=tuplet; d[12]=pitch; d[23]=articDown
        return struct.pack('<H', tick) + bytes([(9<<4)]) + bytes(d)
    e  = note_artic_tuplet(  0, 3, 60, 0x21, 0x00)   # fermata below: tuplet=0 -> keep
    e += note_artic_tuplet(240, 4, 62, 0x21, 0x32)   # tuplet bracket: tuplet=0x32 -> drop
    e += note_artic_tuplet(320, 4, 64, 0x00, 0x32)
    e += note_artic_tuplet(400, 4, 65, 0x00, 0x32)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_tremolo_orn_no_tie():
    # BUG FIX companion: when the "last chord" fallback resolves to a chord that
    # does NOT have an incoming tie, the tremolo must stay on that chord (no
    # spurious walk back via tieBack).
    # m1: quarter Q at tick=0 (no tie). ORN at tick=480 (after Q, no exact match).
    # Fallback finds Q as the last chord. Q has no tieBack -> tremolo stays on Q (tick=0).
    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60)        # Q C4, no tie
    e += ornament_v0c4(480, 0, 0, tipo=0xAF)          # TREMOLO after Q ends
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_fermata_not_in_tuplet():
    # BUG FIX: articulationUp=0x20 on the last note of a tuplet encodes
    # "tuplet bracket placement above" in Encore, NOT a fermata. Only when
    # the note has no tuplet grouping (tuplet==0) should 0x20 produce a fermata.
    # m1: q(artic=0x20, no tuplet) q(artic=0x20, triplet) q(triplet) q(triplet)
    def note_artic_tuplet(tick, fv, pitch, articUp, tuplet):
        d = bytearray(25)
        d[0]=28; d[1]=0; d[2]=fv; d[10]=tuplet; d[12]=pitch; d[21]=articUp
        return struct.pack('<H', tick) + bytes([(9<<4)]) + bytes(d)
    e  = note_artic_tuplet(  0, 3, 60, 0x20, 0x00)   # fermata: tuplet=0 -> keep
    e += note_artic_tuplet(240, 4, 62, 0x20, 0x32)   # tuplet bracket: tuplet=0x32 -> drop
    e += note_artic_tuplet(320, 4, 64, 0x00, 0x32)
    e += note_artic_tuplet(400, 4, 65, 0x00, 0x32)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_tremolos():
    # Encore packs single-note tremolo stroke counts into the artic byte:
    #   au=0x41 -> 1 stroke (8th tremolo)
    #   ad=0x42 -> 2 strokes (16th)
    #   ad=0x03 -> 3 strokes (special form, no 0x40 flag)
    #   ad=0x43 -> 3 strokes (32nd; encore-symbols.enc renders 4 strokes
    #              with this same byte, but the importer caps at 3).
    e  = note_v0c4_artic(  0, 0, 0, fv=3, pitch=60, articUp=0x41)               # 1 stroke
    e += note_v0c4_artic(240, 0, 0, fv=3, pitch=62, articDown=0x42)             # 2 strokes
    e += note_v0c4_artic(480, 0, 0, fv=3, pitch=64, articDown=0x03)             # 3 strokes
    e += note_v0c4_artic(720, 0, 0, fv=3, pitch=65, articDown=0x43)             # 3 strokes
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_ornament_turn():
    # BUG FIX: articulationUp=0x08 (turn ornament) was created as a plain
    # Articulation instead of an Ornament, causing a SIGSEGV in MuseScore's
    # layout code because Ornament-family SymIds are expected to be Ornament
    # instances (which carry cue-note and accidental members).
    # Fix: add ornamentTurn to isOrnamentSymId so it is wrapped in Ornament.
    e  = note_v0c4_artic(0, 0, 0, fv=3, pitch=60, articUp=0x08)   # ornamentTurn
    e += note_v0c4_artic(240, 0, 0, fv=3, pitch=62)                # plain note (control)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def trill_simple_v0c4(tick, voice, staffIdx, xoffset=0):
    """16-byte TRILL_SHORT ornament (tipo=0xB6). Produces ornamentShortTrill glyph."""
    d = bytearray(16)
    struct.pack_into('<H', d, 0, tick)
    d[2] = (5 << 4) | (voice & 0xF)
    d[3] = 16
    d[4] = staffIdx & 0x3F
    d[5] = 0xb6
    d[10] = xoffset & 0xFF  # signed byte stored unsigned
    return bytes(d)


# ===========================================================================
# ornaments_trill_simple_on_note.enc
# FIX: TRILL_SIMPLE (0xB6) 16-byte ORN placed at a note's tick must produce
# an ornamentTrill glyph on that note.
# Measure 1: ORN@tick=0 at same tick as first note → trill on first note.
# Measure 2: ORN@tick=120 (REST tick in 5/8) → snaps forward to next note
#            (the note at tick=240 gets the trill).
# ===========================================================================
def trill_tr_v0c4(tick, voice, staffIdx):
    """16-byte TRILL_TR ornament (tipo=0xB0). Produces ornamentTrill glyph on the chord at tick."""
    d = bytearray(16)
    struct.pack_into('<H', d, 0, tick)
    d[2] = (5 << 4) | (voice & 0xF)
    d[3] = 16
    d[4] = staffIdx & 0x3F
    d[5] = 0xb0
    return bytes(d)


def gen_v0c4_trill_simple_on_note():
    # M1 (4/4): TRILL_SHORT (0xB6) at same tick as first quarter → ornamentShortTrill on that note
    e1  = trill_simple_v0c4(0, 0, 0)
    e1 += note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e1 += note_v0c4(480, 0, 0, fv=3, pitch=64)
    e1 += note_v0c4(720, 0, 0, fv=3, pitch=65)
    e1 += end_marker()
    # M2 (5/8): TRILL_SHORT at REST tick (120) snaps to next note (240)
    e2  = note_v0c4( 0, 0, 0, fv=4, pitch=58)
    e2 += rest_v0c4(120, 0, 0, fv=4)
    e2 += trill_simple_v0c4(120, 0, 0)
    e2 += note_v0c4(240, 0, 0, fv=4, pitch=62)
    e2 += note_v0c4(360, 0, 0, fv=4, pitch=65)
    e2 += note_v0c4(480, 0, 0, fv=4, pitch=67)
    e2 += end_marker()
    # M3 (4/4): TRILL_TR (0xB0) on second quarter note → ornamentTrill on that note
    e3  = note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e3 += trill_tr_v0c4(240, 0, 0)
    e3 += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e3 += note_v0c4(480, 0, 0, fv=3, pitch=64)
    e3 += note_v0c4(720, 0, 0, fv=3, pitch=65)
    e3 += end_marker()
    # M4 (4/4): two TRILL_SHORT ORNs at tick=0 (same tick, same note) → dedup suppresses
    # the second. Only ONE ornamentShortTrill must be placed on NOTE@0.
    e4  = trill_simple_v0c4(  0, 0, 0)   # first TRILL_SHORT
    e4 += trill_simple_v0c4(  0, 0, 0)   # duplicate → must be deduped
    e4 += note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e4 += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e4 += note_v0c4(480, 0, 0, fv=3, pitch=64)
    e4 += note_v0c4(720, 0, 0, fv=3, pitch=65)
    e4 += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e1), (meas_hdr(5, 8, beatTicks=120), e2),
                           (meas_hdr(4, 4), e3), (meas_hdr(4, 4), e4)], fill_ts=(4, 4))


def gen_v0c4_trill_mordent():
    # Encore stores trill-mark and mordent variants in the per-note
    # articulationUp byte: 0x04..0x07 -> trill-mark, 0x0A/0x0C ->
    # inverted-mordent, 0x0B/0x2F -> mordent. Four notes exercise the
    # core mappings; the importer must wrap these in Ornament elements
    # so MuseScore's MusicXML export emits <trill-mark>, <mordent> and
    # <inverted-mordent> under <ornaments>.
    e  = note_v0c4_artic(  0, 0, 0, fv=3, pitch=60, articUp=0x04)  # trill-mark
    e += note_v0c4_artic(240, 0, 0, fv=3, pitch=62, articUp=0x0A)  # inverted-mordent
    e += note_v0c4_artic(480, 0, 0, fv=3, pitch=64, articUp=0x0B)  # mordent
    e += note_v0c4_artic(720, 0, 0, fv=3, pitch=65, articUp=0x2F)  # mordent (variant)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_articulations_combo():
    # Articulation palette is laid out in (below-byte, above-byte) pairs per glyph.
    # Verified in Encore 5 against "Accidentals Marks and others" m11-m16:
    #   0x22/0x23 tenuto-accent, 0x24/0x25 tenuto-staccato, 0x26/0x27 marcato-tenuto
    #   (= tenuto + heavy-accent), 0x28/0x29 staccatissimo,
    #   0x2A/0x2B heavy-accent + staccatissimo, 0x2C/0x2D tenuto + staccatissimo.
    combos = [
        (  0, 0x24, 60),  # tenuto + staccato
        (240, 0x17, 62),  # accent + staccato
        (480, 0x27, 64),  # marcato + tenuto (tenuto + heavy accent)
        (720, 0x15, 65),  # marcato + staccato
    ]
    e = b''
    for tick, byte, pitch in combos:
        e += note_v0c4_artic(tick, 0, 0, fv=3, pitch=pitch, articUp=byte)
    e += end_marker()
    m1 = (meas_hdr(4, 4), e)
    combos2 = [
        (  0, 0x23, 60),  # tenuto + accent
        (240, 0x2D, 62),  # tenuto + staccatissimo
        (480, 0x2B, 64),  # heavy accent + staccatissimo
        (720, 0x24, 65),  # tenuto + staccato again
    ]
    e2 = b''
    for tick, byte, pitch in combos2:
        e2 += note_v0c4_artic(tick, 0, 0, fv=3, pitch=pitch, articUp=byte)
    e2 += end_marker()
    # m3: 0x14 staccato+heavy-accent(∨)=MarcatoStaccatoBelow, 0x26 tenuto+heavy-accent(∨)=MarcatoTenutoBelow.
    combos3 = [
        (  0, 0x14, 60, True),   # staccato + heavy accent (∨) → articMarcatoStaccatoBelow
        (240, 0x26, 62, True),   # tenuto + heavy accent (∨)   → articMarcatoTenutoBelow
    ]
    e3 = b''
    for tick, byte, pitch, use_dn in combos3:
        e3 += note_v0c4_artic(tick, 0, 0, fv=3, pitch=pitch,
                               articUp=0 if use_dn else byte,
                               articDown=byte if use_dn else 0)
    e3 += rest_v0c4(480, 0, 0, fv=2)  # fill remaining 2 beats
    e3 += end_marker()
    # m4: 0x25 tenuto-staccato, 0x2A heavy-accent+staccatissimo (two glyphs),
    # 0x2C tenuto+staccatissimo (two glyphs), 0x22 tenuto-accent.
    combos4 = [
        (  0, 0x25, 60),  # tenuto + staccato (portato)
        (240, 0x2A, 62),  # heavy accent + staccatissimo (two elements)
        (480, 0x2C, 64),  # tenuto + staccatissimo (two elements)
        (720, 0x22, 67),  # tenuto + accent
    ]
    e4 = b''
    for tick, byte, pitch in combos4:
        e4 += note_v0c4_artic(tick, 0, 0, fv=3, pitch=pitch, articDown=byte)
    e4 += end_marker()
    return assemble(0xC4, [m1, (meas_hdr(4, 4), e2), (meas_hdr(4, 4), e3),
                            (meas_hdr(4, 4), e4)], fill_ts=(4, 4))


# ===========================================================================
# text_lyrics_variable.enc
#
# 4/4 measure with 4 quarter notes. LYRIC elements include an empty
# placeholder between non-empty syllables -- Encore's "keep the lyric
# count aligned with chord positions but no syllable sung here" marker.
# The importer must still attach JU, LIO, RO in order (empty consumes
# a slot but produces no Lyrics element).
# ===========================================================================
# ===========================================================================
# notes_tie_start_flag_byte6.enc
#
# 4/4 measure with two notes tied: the TIE element carries
# direction=0x04 (arc-only at +5) and startFlag=0x80 (high bit at +6).
# This is the encoding ~32% of outgoing ties use in the Beethoven
# Plectro corpus, including the m20 dotted-quarter B in
# LaMorenaDeMiCopla. The importer must accept the high bit on either
# +5 or +6 as a tie-start marker.
# ===========================================================================
def gen_v0c4_tie_start_flag_byte6():
    e  = tie_v0c4(0, 0, 0, direction=0x04, startFlag=0x80)
    e += note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += note_v0c4(480, 0, 0, fv=3, pitch=60)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# text_lyrics_two_verses.enc
#
# 4/4 measure with 4 quarter notes. Verse 1 lives on voice 0 LYRIC
# elements (JU, LIO, RO, ME); verse 2 lives on voice 1 LYRIC elements
# (Co, mo, ca, pa). The importer must anchor both verses on the same
# voice-0 chord and tag them with Lyrics::verse() = 0 / 1.
# ===========================================================================
def gen_v0c4_lyrics_hyphenated_words():
    # 4/4 measure with 3 notes at beats 0, 1, 2 (quarter, eighth, eighth).
    # Encore lyric stream for "JU-LIO RO-" mirroring the LaMorenaDeMiCopla
    # m18 layout: each "-" element is a hyphen continuation marker that
    # must NOT consume a note slot. The importer must produce three
    # syllables with LyricsSyllabic BEGIN/END/BEGIN on three notes.
    # Encore PPQ here = 240; 4/4 = 960 raw Encore ticks. Notes go at
    # tick 0 (quarter), 480 (eighth), 720 (eighth).
    e  = lyric_v0c4(  0, 0, 0, 'JU')        # syllable, will get hyphenAfter
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += lyric_v0c4(479, 0, 0, '-')         # hyphen continuation to LIO
    e += lyric_v0c4(480, 0, 0, 'LIO')       # END of JULIO
    e += note_v0c4( 480, 0, 0, fv=4, pitch=62)
    e += lyric_v0c4(720, 0, 0, 'RO')        # BEGIN of next word
    e += lyric_v0c4(721, 0, 0, '-')         # hyphen continuation past bar
    e += note_v0c4( 720, 0, 0, fv=4, pitch=64)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_lyrics_hyphen_across_barline():
    # FIX: a hyphenated word split across a barline ("sof-tly" with "sof" ending one
    # measure and "-" + "tly" opening the next) lost the connecting hyphen. The "-"
    # arrives when the previous measure's queue is already cleared, so the first
    # syllable was never promoted and stayed SINGLE. Expected: "sof" = BEGIN (m1),
    # "tly" = END (m2), so MuseScore draws the hyphen across the bar.
    e1  = lyric_v0c4(0, 0, 0, 'sof')
    e1 += note_v0c4(0, 0, 0, fv=1, pitch=60)   # whole note
    e1 += end_marker()
    e2  = lyric_v0c4(0, 0, 0, '-')             # hyphen opens measure 2 (queue empty)
    e2 += lyric_v0c4(0, 0, 0, 'tly')
    e2 += note_v0c4(0, 0, 0, fv=1, pitch=62)
    e2 += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e1), (meas_hdr(4, 4), e2)], fill_ts=(4, 4))


def gen_v0c4_lyrics_offgrid_nearest_chord():
    # FIX: a continuation syllable whose stored tick sits between notes (further than
    # half a beat from any of them) was dropped when no rest was available to fall back
    # on. A sung syllable always belongs to a note, so it must attach to the nearest
    # chord at any distance instead of being discarded.
    # 4/4 (beatTicks=240, threshold=120): notes at ticks 0 (eighth), 120 (dotted quarter
    # -> gap 360), 480 (half). Lyric "ge" at tick 279 is 159 from note@120 and 201 from
    # note@480, beyond the 120 threshold, and there is no rest. It must land on the
    # nearest chord, the note at tick 120 (pitch 62).
    e  = note_v0c4(0,   0, 0, fv=4, pitch=60)
    e += note_v0c4(120, 0, 0, fv=3, pitch=62)
    e += note_v0c4(480, 0, 0, fv=2, pitch=64)
    e += lyric_v0c4(279, 0, 0, 'ge')
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_lyrics_two_verses():
    e  = lyric_v0c4(  0, 0, 0, 'JU')
    e += lyric_v0c4(  0, 1, 0, 'Co')
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += lyric_v0c4(240, 0, 0, 'LIO')
    e += lyric_v0c4(240, 1, 0, 'mo')
    e += note_v0c4( 240, 0, 0, fv=3, pitch=62)
    e += lyric_v0c4(480, 0, 0, 'RO')
    e += lyric_v0c4(480, 1, 0, 'ca')
    e += note_v0c4( 480, 0, 0, fv=3, pitch=64)
    e += lyric_v0c4(720, 0, 0, 'ME')
    e += lyric_v0c4(720, 1, 0, 'pa')
    e += note_v0c4( 720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_lyrics_variable():
    e  = lyric_v0c4(  0, 0, 0, 'JU')
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += lyric_v0c4(240, 0, 0, 'LIO')
    e += note_v0c4( 240, 0, 0, fv=3, pitch=62)
    e += lyric_v0c4(480, 0, 0, '')        # empty placeholder
    e += note_v0c4( 480, 0, 0, fv=3, pitch=64)
    e += lyric_v0c4(720, 0, 0, 'RO')
    e += note_v0c4( 720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# text_lyrics_latin1.enc
# v0xC4 file whose lyric payload is Latin-1 (one byte per char) instead of
# UTF-16 LE. Real Portuguese files do this (e.g. milesdepartituras/
# Fe_cega_faca_amolada_tk.enc stores "txã" as 74 78 E3 00). Without the
# encoding probe in EncLyric::read, the importer reads two raw bytes per
# QChar and the text comes out as CJK code units (U+7874 + ã).
# ===========================================================================
def gen_v0c4_lyrics_latin1():
    e  = lyric_v0c4_latin1(0, 0, 0, b'tx\xe3')      # 'txã' in Latin-1
    e += note_v0c4(        0, 0, 0, fv=3, pitch=60)
    e += lyric_v0c4_latin1(240, 0, 0, b'n\xe3')     # 'nã' in Latin-1
    e += note_v0c4(        240, 0, 0, fv=3, pitch=62)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# text_lyrics_accent_first_letter.enc
# The syllable "ño" written twice in one file, once UTF-16 LE (F1 00 6F 00)
# and once Latin-1 (F1 6F), so both branches of the encoding probe are pinned
# by the same expected text. A probe keyed on a printable ASCII first byte
# sends the UTF-16 one down the Latin-1 branch, where the high byte of the
# enye reads as the terminator and the syllable comes out as "ñ".
# ===========================================================================
def gen_v0c4_lyrics_accent_first_letter():
    e  = lyric_v0c4(       0, 0, 0, 'ño')            # UTF-16 LE
    e += note_v0c4(        0, 0, 0, fv=3, pitch=60)
    e += lyric_v0c4_latin1(240, 0, 0, b'\xf1o')      # the same syllable, Latin-1
    e += note_v0c4(        240, 0, 0, fv=3, pitch=62)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# text_lyrics_column_says_which_note.enc
# Four quarters in columns 12, 36, 90 and 130. Their syllables are stored in
# the reverse order, two of them at tick 0 and the first one carrying a stale
# tick that points at the last note, which is how a real save comes out.
# Matching by tick reads the phrase as "con flor yer"; the anchor byte holds
# each note's column to the unit and gets it right.
# ===========================================================================
def gen_v0c4_lyrics_column_says_which_note():
    e  = note_v0c4_xoff(  0, 0, 0, 3, 60, 12)
    e += note_v0c4_xoff(240, 0, 0, 3, 62, 36)
    e += note_v0c4_xoff(480, 0, 0, 3, 64, 90)
    e += note_v0c4_xoff(720, 0, 0, 3, 65, 130)
    e += lyric_v0c4(  0, 0, 0, 'flor', kie=90)
    e += lyric_v0c4(  0, 0, 0, 'con', kie=36)
    e += lyric_v0c4(700, 0, 0, 'yer', kie=12)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_accent_sibling_no_spillover.enc
# BUG FIX: ACCENT ORN (0xBE) on staff 0 whose notes are in voice=3 must NOT
# redirect to staff 1.  Without fix: resolver checks track=staffBase+0 (voice=0),
# finds no chord (notes in voice=3 only), falls to sibTrack=staffBase+VOICES
# (staff 1 voice=0), finds E4 → accent lands on staff 1 instead of staff 0.
# With fix: resolver scans all voices of the ORN's own staff first; finds C4
# at track=3 (staff 0, voice=3) and attaches the accent there.
#
# 2 instruments × 1 staff each, 4/4, measure 0:
#   Staff 0 (instrIdx=0): C4 quarter, voice=3
#   Staff 1 (instrIdx=1): E4 quarter, voice=0   ← sibling trap
#   ORN 0xBE on staff 0, voice=0, tick=0
# Expected: accent on staff 0 voice=3 chord; NO accent on staff 1 voice=0.
# ===========================================================================
def gen_v0c4_accent_sibling_no_spillover():
    # Custom 2-staff SCOW header (194 bytes)
    hdr = bytearray(194)
    hdr[0:4] = b'SCOW'
    hdr[4] = 0xC4
    struct.pack_into('<H', hdr, 0x28, 0x0420)    # chuVersio
    struct.pack_into('<h', hdr, 0x2E, 1)          # lineCount
    struct.pack_into('<h', hdr, 0x30, 1)          # pageCount
    hdr[0x32] = 2                                  # instrumentCount
    hdr[0x33] = 2                                  # staffPerSystem
    struct.pack_into('<h', hdr, 0x34, 1)          # measureCount

    # LINE block: 10-skip + start(u16) + measCount(u8) + 2×30-byte staff entries
    def staff_entry(clef_byte, instr_staff_idx):
        e = bytearray(30)
        e[14] = clef_byte   # clef
        e[19] = 1           # showByte = visible
        e[21] = instr_staff_idx
        return bytes(e)

    line_data = (b'\x00' * 10
                 + struct.pack('<H', 0)         # start measure = 0
                 + bytes([1])                   # measureCount = 1
                 + staff_entry(0, 0x00)         # instr 0, staffWithin 0 (treble G)
                 + staff_entry(0, 0x01))        # instr 1, staffWithin 0 (treble G)
    # toSkip = varSize + 8 - 21 - 30*2; for toSkip=0 → varSize = 73
    assert len(line_data) == 73, len(line_data)
    line_block = b'LINE' + struct.pack('<I', 73) + line_data

    # MEAS elements
    def note_raw(tick, voice, raw_staff, fv, pitch):
        d = bytearray(25)
        d[0] = 28; d[1] = raw_staff & 0xFF; d[2] = fv; d[12] = pitch
        return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)

    def orn_raw(tick, voice, raw_staff, tipo):
        d = bytearray(13)
        d[0] = 16; d[1] = raw_staff & 0xFF; d[2] = tipo
        return struct.pack('<H', tick) + bytes([(5 << 4) | (voice & 0xF)]) + bytes(d)

    elems = (note_raw(0, 3, 0x00, fv=3, pitch=60)   # C4 quarter, staff 0, voice=3
           + note_raw(0, 0, 0x01, fv=3, pitch=64)   # E4 quarter, staff 1, voice=0 (trap)
           + orn_raw(0, 0, 0x00, tipo=0xBE)          # ACCENT on staff 0, voice=0
           + end_marker())

    meas = meas_block(meas_hdr(4, 4), elems)
    return bytes(hdr) + line_block + meas + SKELETON_POST


# ===========================================================================
# instruments_bass_enckey0_no_octave_transpos.enc
# BUG FIX: encKey=0 ("sounds as written") must zero the template's octave
# transposition just as it already zeroes non-octave transpositions.
# The acoustic-bass template carries transposeChromatic=-12; without the fix
# the importer keeps that -12 and notes display one octave too high.
#
# File: name "Bajo" (resolves to acoustic-bass via short name + MIDI),
# MIDI 33 (1-indexed Acoustic Bass), F clef, encKey=0 (sounds as written),
# one quarter note A2 (midi=45).
# ===========================================================================
def gen_v0c4_bass_enckey0_no_octave_transpos():
    name = 'Bajo'.encode('utf-16-le') + b'\x00\x00'
    pre  = _patch_tk00(name)
    pre  = _patch_midi_program(pre, 0, 33)     # 1-indexed Acoustic Bass (GM 32)
    # encKey not patched → stays 0 (sounds as written)
    pre  = _set_staff_clef(pre, 0x01)          # EncClefType::F (bass clef)
    elems = note_v0c4(tick=0, voice=0, staffIdx=0, fv=3, pitch=45) + end_marker()
    body  = meas_block(meas_hdr(4, 4), elems)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# ornaments_accent_nonzero_voice.enc
# BUG FIX: ACCENT ORN (0xBE) at voice=0 should attach to the note at the
# same tick even when that note is in a non-zero voice (here voice=1).
# Without fix: resolver finds no chord at track=voice=0, falls back to the
# sibling staff (track+VOICES), and drops the accent when no sibling exists.
# With fix: resolver scans all voices of the ORN's own staff before giving up.
#
# Single staff, 4/4, measure 0:
#   Note C4 (midi=60), voice=1, tick=0, quarter
#   ORN 0xBE (ACCENT), voice=0, tick=0
# Expected: chord in voice=1 at tick=0 has exactly 1 articAccentAbove.
# ===========================================================================
def gen_v0c4_accent_nonzero_voice():
    # ORN 0xBE: size=16 standalone accent ORN at voice=0
    orn = (struct.pack('<H', 0)               # tick=0
           + bytes([(5 << 4) | 0])            # typeVoice: ORN(5) | voice=0
           + bytes([16])                       # size=16
           + bytes([0])                        # staffIdx=0
           + bytes([0xBE])                     # tipo=0xBE (ACCENT)
           + bytes([0] * 10))                  # payload zeros (10 bytes → total 16)
    elems = (note_v0c4(tick=0, voice=1, staffIdx=0, fv=3, pitch=60)
             + orn
             + end_marker())
    hdr  = meas_hdr(4, 4)
    return assemble(0xC4, [(hdr, elems)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_accent_nonzero_voice_offset_tick.enc
# BUG FIX: ACCENT ORN at voice=0, tick=240 must attach to the note in voice=1
# at tick=240, NOT to the note at tick=0.
# Without fix: voice=0 cumTick=0 (no notes in v0), elemTick=measTick,
# so pendingBowings stores measTick → accent lands on the first note (n1).
# With fix: bowTick = measTick + Fraction(240, 960) → accent lands on n2.
#
# Single staff, 4/4, measure 0:
#   Note C4 (midi=60), voice=1, tick=0, quarter
#   Note E4 (midi=64), voice=1, tick=240, quarter  ← accent must land here
#   ORN 0xBE (ACCENT), voice=0, tick=240
# Expected: chord at voice=1, tick=240 (MuseScore tick 480) has the accent.
# ===========================================================================
def gen_v0c4_accent_offset_tick_nonzero_voice():
    def orn16(tick, staffIdx, tipo):
        return (struct.pack('<H', tick)
                + bytes([(5 << 4) | 0])    # ORN voice=0
                + bytes([16])               # size=16
                + bytes([staffIdx & 0x3F])
                + bytes([tipo])
                + bytes([0] * 10))
    elems = (note_v0c4(tick=0,   voice=1, staffIdx=0, fv=3, pitch=60)   # C4 quarter
           + note_v0c4(tick=240, voice=1, staffIdx=0, fv=3, pitch=64)   # E4 quarter (accent here)
           + orn16(tick=240, staffIdx=0, tipo=0xBE)
           + end_marker())
    hdr = meas_hdr(4, 4)
    return assemble(0xC4, [(hdr, elems)], fill_ts=(4, 4))


# ===========================================================================
# notes_dual_rests_same_tick_routing.enc
# BUG FIX: two explicit REST elements at the same Encore tick (120) for
# voices 5 and 6 (both routing to MuseScore voice=0) must not cause a
# cumTick drift that shifts all subsequent notes.
# Without fix: the first REST at tick=120 advances cumTick to 1/4; the
# second REST at tick=120 finds encTickFrac < cumTick, skips gap-snap, and
# lands at cumTick=1/4 (tick=240 in MuseScore), advancing cumTick to 3/8.
# This shifts the following notes by one eighth note (240 MuseScore ticks).
#
# Single staff, 4/4, measure 0:
#   voice=6 (→ voice=0): D3@tick=0(eighth), REST@tick=120(eighth), B2@tick=480(quarter)
#   voice=5 (→ voice=0): F#3@tick=0(eighth), REST@tick=120(eighth), D3@tick=480(quarter)
# Expected MuseScore voice=0: D3+F#3 chord at measTick, rest at measTick+240,
#   B2+D3 chord at measTick+960 (NOT measTick+720 as the buggy code produces).
# ===========================================================================
def gen_v0c4_dual_rests_same_tick_routing():
    # Elements interleaved voice=6 / voice=5 per beat, as Encore would encode.
    elems = (note_v0c4(tick=0,   voice=6, staffIdx=0, fv=4, pitch=50)   # D3 eighth
           + note_v0c4(tick=0,   voice=5, staffIdx=0, fv=4, pitch=54)   # F#3 eighth
           + rest_v0c4(tick=120, voice=6, staffIdx=0, fv=4)              # REST eighth (v6)
           + rest_v0c4(tick=120, voice=5, staffIdx=0, fv=4)              # REST eighth (v5)
           + note_v0c4(tick=480, voice=6, staffIdx=0, fv=3, pitch=47)   # B2 quarter
           + note_v0c4(tick=480, voice=5, staffIdx=0, fv=3, pitch=50)   # D3 quarter
           + end_marker())
    hdr = meas_hdr(4, 4)
    return assemble(0xC4, [(hdr, elems)], fill_ts=(4, 4))


# ===========================================================================
# instruments_instr_bass_midi_tiebreak.enc
# Instrument named "Bass" with MIDI program 33 (1-indexed = GM 32 Acoustic
# Bass). The choral Bass voice template (id="bass") matches the name
# exactly (trackName="Bass"), but its default channel program is 52 (Choir
# Aahs). The acoustic bass template (id="acoustic-bass") only matches by
# shortName="Bass" but ships a pizzicato channel at program 32. The MIDI
# bonus in findEncoreInstrumentTemplate must flip the tie in favour of
# the instrumental bass.
# ===========================================================================
def gen_v0c4_instr_bass_midi_tiebreak():
    name = 'Bass'.encode('utf-16-le') + b'\x00\x00'
    pre  = _patch_tk00(name)
    pre  = _patch_midi_program(pre, 0, 33)   # 1-indexed Acoustic Bass
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_instr_percussion_drumset.enc
# Encore percussion track: name "Percusión" and the throwaway default
# midiProgram=1. Without the drum-kit shortcut the importer falls back to
# program 0 (Grand Piano) and the part plays back as a piano.
# ===========================================================================
def gen_v0c4_instr_percussion_drumset():
    name = 'Percusión'.encode('utf-16-le') + b'\x00\x00'
    pre  = _patch_tk00(name)
    pre  = _patch_midi_program(pre, 0, 1)   # default piano-ish program
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_small_tk_midi49.enc
# TK block with varsize=112 (smallTK layout) and MIDI program 49 stored at
# contentFilePos + offset + 76 = 202 + 112 + 76 = 390.
# Verifies that readMidiPrograms correctly reads MIDI from this position
# rather than the wrong MIDI_IN_CONTENT=60 (which reads within the name area).
# ===========================================================================
def gen_v0c4_small_tk_key6():
    """smallTK file with TK00 varsize=112 and key=+6 semitones at contentFilePos+varSize+53=367."""
    name = b'SmallTkKey\x00'
    pre  = _patch_tk00(name, new_varsize=112)
    pre  = _patch_small_tk_key(pre, 0, 6)
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_small_tk_midi49():
    name = b'SmallTkMidi\x00'
    pre  = _patch_tk00(name, new_varsize=112)
    pre  = _patch_small_tk_midi(pre, 0, 49)
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_total_size_tk_two_instrs.enc
# Two TK blocks in Encore 4.x total-block-size format: varSize = TOTAL block
# size (including 8-byte header), so stride between blocks = varSize (not
# varSize+8 as in standard 5.x layout).  MIDI stored at content[60].
# TK00: MIDI=49, TK01: MIDI=34.
# Verifies that isTotalBlockSizeTkFmt() detects the layout and reads MIDI
# from content[60] rather than the wrong content+varSize+76 formula.
# ===========================================================================
def gen_v0c4_total_size_tk_two_instrs():
    VARSIZE      = 80          # total block size (8-byte header + 72-byte content)
    CONTENT      = VARSIZE - 8 # 72 bytes of content per block
    MIDI_IN_CONT = 60          # MIDI program at content[60] (Encore 4.x layout)
    TK_START     = 194         # header is 194 bytes; TK blocks follow immediately

    # Header: bytes 0..193 from SKELETON_PRE, set instrumentCount=2.
    header = bytearray(SKELETON_PRE[:TK_START])
    header[0x32] = 2

    def make_tk(idx, name_str, midi_1idx):
        magic   = 'TK{:02d}'.format(idx).encode('ascii')
        content = bytearray(CONTENT)
        nb      = name_str.encode('ascii') + b'\x00'
        content[:len(nb)] = nb
        content[MIDI_IN_CONT] = midi_1idx & 0xFF
        return bytes(magic) + struct.pack('<I', VARSIZE) + bytes(content)

    tk00 = make_tk(0, 'InstrA', 49)  # MIDI=49 (String Ensemble 1, 0-indexed=48)
    tk01 = make_tk(1, 'InstrB', 34)  # MIDI=34 (Electric Bass, 0-indexed=33)

    # PAGE+LINE blocks from skeleton: after header(194) + TK00-header(8) + TK00-content(2158).
    LEGACY_TK_END = 194 + 8 + 2158
    page_line     = SKELETON_PRE[LEGACY_TK_END:]

    pre  = bytes(header) + tk00 + tk01 + page_line
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_total_size_tk_key_from_entry_end.enc
# Two TK blocks in Encore 4.x total-block-size format with the entry size a
# real Encore 4.5.x save uses (varSize = 242 = whole block), so the per-staff
# tables sit near the end of the entry: the program table 46 bytes before the
# entry end and the key transposition 23 bytes before that, i.e. 69 bytes
# before the entry end.  TK00: MIDI=66 key=-9, TK01: MIDI=67 key=-14.
# Verifies the key is anchored to the program table rather than read from a
# fixed position inside the content, which lands in the name padding and
# leaves the score in concert pitch.
# ===========================================================================
def gen_v0c4_total_size_tk_key_from_entry_end():
    VARSIZE       = 242            # total block size (8-byte header + 234-byte content)
    CONTENT       = VARSIZE - 8
    MIDI_FROM_END = 46             # program table, measured back from the entry end
    KEY_FROM_END  = MIDI_FROM_END + 23
    TK_START      = 194

    header = bytearray(SKELETON_PRE[:TK_START])
    header[0x32] = 2

    def make_tk(idx, name_str, midi_1idx, key_semitones):
        magic   = 'TK{:02d}'.format(idx).encode('ascii')
        content = bytearray(CONTENT)
        nb      = name_str.encode('ascii') + b'\x00'
        content[:len(nb)] = nb
        content[CONTENT - MIDI_FROM_END] = midi_1idx & 0xFF
        content[CONTENT - KEY_FROM_END]  = key_semitones & 0xFF
        return bytes(magic) + struct.pack('<I', VARSIZE) + bytes(content)

    tk00 = make_tk(0, 'AltoSax', 66, -9)     # Eb alto saxophone
    tk01 = make_tk(1, 'TenorSax', 67, -14)   # Bb tenor saxophone

    LEGACY_TK_END = 194 + 8 + 2158
    page_line     = SKELETON_PRE[LEGACY_TK_END:]

    pre  = bytes(header) + tk00 + tk01 + page_line
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_key_from_run_shaped_table.enc
# Two TK entries in the total-block-size layout whose per-staff tables are not
# provable by a program byte that differs from the channels. The first has a
# channel run and no program at all, which is a staff with no instrument
# assigned; the second carries the same number as channel and as program,
# which a rule reading "the channels must differ from the program" throws out.
# Either way the key went unread and the part came in at concert pitch. In the
# corpus this cost a laud and a guitar their Key=-12, and with it the octave
# clef and the octave of every note they hold.
# ===========================================================================
def gen_v0c4_key_from_run_shaped_table():
    VARSIZE       = 242            # total block size (8-byte header + 234-byte content)
    CONTENT       = VARSIZE - 8
    MIDI_FROM_END = 46             # program table, measured back from the entry end
    KEY_FROM_END  = MIDI_FROM_END + 23
    VOICES        = 8
    TK_START      = 194

    header = bytearray(SKELETON_PRE[:TK_START])
    header[0x32] = 2

    def make_tk(idx, name_str, channel, program, key_semitones):
        magic   = 'TK{:02d}'.format(idx).encode('ascii')
        content = bytearray(CONTENT)
        nb      = name_str.encode('ascii') + b'\x00'
        content[:len(nb)] = nb
        table = CONTENT - MIDI_FROM_END
        content[table - VOICES:table] = bytes([channel]) * VOICES
        content[table:table + VOICES]  = bytes([program]) * VOICES
        content[CONTENT - KEY_FROM_END] = key_semitones & 0xFF
        return bytes(magic) + struct.pack('<I', VARSIZE) + bytes(content)

    tk00 = make_tk(0, 'AltoSax', 3, 0, -9)      # nothing assigned: only the channel run
    tk01 = make_tk(1, 'TenorSax', 5, 5, -14)    # channel and program are the same number

    LEGACY_TK_END = 194 + 8 + 2158
    page_line     = SKELETON_PRE[LEGACY_TK_END:]

    pre  = bytes(header) + tk00 + tk01 + page_line
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_table_the_file_measures_itself.enc
# A fixed-stride entry table of 242 bytes an entry where only the THIRD entry
# carries a TK magic, which is how Encore 4 writes some files, and a plausible
# word planted at the formula position the reader probes first, as the page
# blocks of a real file spell one there. Unmarked entries were read at the
# absolute positions of two other layouts: the name came back as the planted
# word and the key as nothing. Where the one magic sits proves the stride, and
# that places the name 8 bytes into each entry, the tables at its end and the
# key 23 bytes ahead of those.
# ===========================================================================
def gen_v0c4_table_the_file_measures_itself():
    VARSIZE       = 242            # total block size, and the stride between entries
    CONTENT       = VARSIZE - 8
    MIDI_FROM_END = 46
    KEY_FROM_END  = MIDI_FROM_END + 23
    VOICES        = 8
    TK_START      = 194

    header = bytearray(SKELETON_PRE[:TK_START])
    header[0x32] = 3

    def make_entry(magic, name_str, channel, program, key_semitones):
        content = bytearray(CONTENT)
        nb      = name_str.encode('ascii') + b'\x00'
        content[:len(nb)] = nb
        table = CONTENT - MIDI_FROM_END
        content[table - VOICES:table] = bytes([channel]) * VOICES
        content[table:table + VOICES]  = bytes([program]) * VOICES
        content[CONTENT - KEY_FROM_END] = key_semitones & 0xFF
        head = magic + struct.pack('<I', VARSIZE) if magic else bytes(8)
        return head + bytes(content)

    e0 = make_entry(None,      'AltoSax', 3, 66, -9)
    e1 = make_entry(None,     'TenorSax', 4, 67, -14)
    e2 = make_entry(b'TK02',      'Harp', 5, 47, 0)     # the only entry marked

    LEGACY_TK_END = 194 + 8 + 2158
    pre = bytearray(bytes(header) + e0 + e1 + e2 + SKELETON_PRE[LEGACY_TK_END:])
    # A word where the 2158-byte formula looks for instrument 1's name.
    planted = 'ZZTOP'.encode('utf-16-le') + b'\x00\x00'
    at = 202 + 1 * 2158
    while len(pre) < at + len(planted) + 4:
        pre.extend(b'\x00' * 64)
    pre[at:at + len(planted)] = planted

    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return bytes(pre) + body + SKELETON_POST


# ===========================================================================
# instruments_key_when_nothing_is_assigned.enc
# Two TK entries in the total-block-size layout with no channel and no program
# anywhere in the file, which is a score whose staves were never assigned an
# instrument. Nothing proves where the per-staff tables sit and no sibling can
# be measured, yet each key is where this layout keeps it, 23 bytes ahead of
# the tables counted back from the entry end.
# ===========================================================================
def gen_v0c4_key_when_nothing_is_assigned():
    VARSIZE       = 242            # total block size (8-byte header + 234-byte content)
    CONTENT       = VARSIZE - 8
    MIDI_FROM_END = 46             # where the tables would sit, were anything assigned
    KEY_FROM_END  = MIDI_FROM_END + 23
    TK_START      = 194

    header = bytearray(SKELETON_PRE[:TK_START])
    header[0x32] = 2

    def make_tk(idx, name_str, key_semitones):
        magic   = 'TK{:02d}'.format(idx).encode('ascii')
        content = bytearray(CONTENT)          # zeros where the channels and programs would be
        nb      = name_str.encode('ascii') + b'\x00'
        content[:len(nb)] = nb
        content[CONTENT - KEY_FROM_END] = key_semitones & 0xFF
        return bytes(magic) + struct.pack('<I', VARSIZE) + bytes(content)

    tk00 = make_tk(0, 'AltoSax', -9)
    tk01 = make_tk(1, 'TenorSax', -14)

    LEGACY_TK_END = 194 + 8 + 2158
    page_line     = SKELETON_PRE[LEGACY_TK_END:]

    pre  = bytes(header) + tk00 + tk01 + page_line
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_key_from_a_sibling_measured_table.enc
# Two TK entries in the total-block-size layout. The first names a channel and
# a program, which places the tables. The second is a staff with neither, so
# its own table region is a stretch of zeros and proves nothing at all, while
# its key sits the same distance into the entry as its sibling's does.
# ===========================================================================
def gen_v0c4_key_from_a_sibling_measured_table():
    VARSIZE       = 242            # total block size (8-byte header + 234-byte content)
    CONTENT       = VARSIZE - 8
    MIDI_FROM_END = 46             # program table, measured back from the entry end
    KEY_FROM_END  = MIDI_FROM_END + 23
    VOICES        = 8
    TK_START      = 194

    header = bytearray(SKELETON_PRE[:TK_START])
    header[0x32] = 2

    def make_tk(idx, name_str, channel, program, key_semitones):
        magic   = 'TK{:02d}'.format(idx).encode('ascii')
        content = bytearray(CONTENT)
        nb      = name_str.encode('ascii') + b'\x00'
        content[:len(nb)] = nb
        table = CONTENT - MIDI_FROM_END
        content[table - VOICES:table] = bytes([channel]) * VOICES
        content[table:table + VOICES]  = bytes([program]) * VOICES
        content[CONTENT - KEY_FROM_END] = key_semitones & 0xFF
        return bytes(magic) + struct.pack('<I', VARSIZE) + bytes(content)

    tk00 = make_tk(0, 'Harp', 11, 47, 0)       # names both, so it places the tables
    tk01 = make_tk(1, 'AltoSax', 0, 0, -5)     # neither: nothing but zeros where they sit

    LEGACY_TK_END = 194 + 8 + 2158
    page_line     = SKELETON_PRE[LEGACY_TK_END:]

    pre  = bytes(header) + tk00 + tk01 + page_line
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_total_size_tk_key_not_from_channel_run.enc
# Two TK blocks in total-block-size format whose entries are 112 bytes and
# keep their per-staff tables at the other generation's distance from the
# entry end, 44 rather than 46: the channel run covers position 46, so a key
# taken 23 bytes ahead of it picks up an unrelated byte, planted here as +1
# semitone.  The key really sits 23 bytes ahead of the table, holding -3.
# The entry is 130 bytes so that the table falls on no other known position.
# ===========================================================================
def gen_v0c4_total_size_tk_key_not_from_channel_run():
    VARSIZE       = 130
    CONTENT       = VARSIZE - 8
    CHANNELS_FROM_END = 52         # eight channel bytes, covering the usual program position
    PROGRAM_FROM_END  = 44         # the program table really starts here
    TRAP_FROM_END     = 69         # what the program-table anchor would read as a key
    KEY_IN_CONTENT    = 42         # the position that holds the key in these entries
    TK_START      = 194

    header = bytearray(SKELETON_PRE[:TK_START])
    header[0x32] = 2

    def make_tk(idx, name_str, channel, midi_1idx):
        magic   = 'TK{:02d}'.format(idx).encode('ascii')
        content = bytearray(CONTENT)
        nb      = name_str.encode('ascii') + b'\x00'
        content[:len(nb)] = nb
        for v in range(8):
            content[CONTENT - CHANNELS_FROM_END + v] = channel
            content[CONTENT - PROGRAM_FROM_END + v]  = midi_1idx & 0xFF
        content[CONTENT - TRAP_FROM_END] = 1
        content[CONTENT - PROGRAM_FROM_END - 23] = (-3) & 0xFF
        content[KEY_IN_CONTENT] = 0
        return bytes(magic) + struct.pack('<I', VARSIZE) + bytes(content)

    tk00 = make_tk(0, 'InstrA', 1, 75)
    tk01 = make_tk(1, 'InstrB', 1, 75)

    LEGACY_TK_END = 194 + 8 + 2158
    page_line     = SKELETON_PRE[LEGACY_TK_END:]

    pre  = bytes(header) + tk00 + tk01 + page_line
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_instr_perc_clef_drumset.enc
# Instrument with a non-percussion name ("Pandeiro") but EncClefType::PERC
# on its first staff. The primary detection path (clef check) must route it
# to the drumset template regardless of name or midiProgram.
# ===========================================================================
def gen_v0c4_instr_perc_clef_drumset():
    name = 'Pandeiro'.encode('utf-16-le') + b'\x00\x00'
    pre  = _patch_tk00(name)
    pre  = _patch_midi_program(pre, 0, 1)   # midiProgram=1 (would fall back to piano)
    pre  = _set_staff_clef(pre, 0x07)       # EncClefType::PERC
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_instr_drums_name_drumset.enc
# Instrument named "Drums" (English): not in the old hardcoded keyword list,
# so the old code would have fallen through to the MIDI program lookup and
# ended up as Grand Piano. findDrumsetTemplate must match it against
# MuseScore's localized drumset template names.
# ===========================================================================
def gen_v0c4_instr_drums_name_drumset():
    name = 'Drums'.encode('utf-16-le') + b'\x00\x00'
    pre  = _patch_tk00(name)
    pre  = _patch_midi_program(pre, 0, 1)   # midiProgram=1 (would fall back to piano)
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_instr_laud_accent.enc
# Instrument named "Laud" (no diacritic) which should resolve to the Spanish
# folk lute template id="laud" whose trackName ships as "Laúd". The match
# only works once findEncoreInstrumentTemplate strips diacritics before
# comparing.
# ===========================================================================
def gen_v0c4_instr_laud_accent():
    name = 'Laud A'.encode('utf-16-le') + b'\x00\x00'
    pre  = _patch_tk00(name)
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# instruments_tab_template_forced_standard.enc
# Name "Classical Guitar (tablature)" matches a tablature template, but Encore stores a
# normal clef (no TAB). The importer must swap to the standard-notation sibling.
def gen_v0c4_tab_template_forced_standard():
    name = 'Classical Guitar (tablature)'.encode('utf-16-le') + b'\x00\x00'
    pre  = _patch_tk00(name)
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# instruments_tab_clef_keeps_tablature.enc
# Name "Classical Guitar" matches the standard template, but Encore stores EncClefType::TAB;
# the importer must swap to the tablature sibling.
def gen_v0c4_tab_clef_keeps_tablature():
    name = 'Classical Guitar'.encode('utf-16-le') + b'\x00\x00'
    pre  = _patch_tk00(name)
    pre  = _set_staff_clef(pre, 0x08)       # EncClefType::TAB
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ---------------------------------------------------------------------------
# Helper: patch the per-staff tab tuning array. It lives in the 8 slots that
# sit immediately before the first PAGE block: `count` open-string MIDI pitches
# (low->high) followed by pad bytes (0x7F customized, 0x58 default). The importer
# derives the string count from the leading non-pad slots, so overwriting the
# 8 slots is sufficient; the explicit count byte is kept consistent too.
# ---------------------------------------------------------------------------
def _set_tab_tuning(pre, pitches, pad=0x7F):
    pre = bytearray(pre)
    page = pre.find(b'PAGE')
    assert page > 10, "PAGE block missing in skeleton"
    slots = page - 8
    for i in range(8):
        pre[slots + i] = (pitches[i] if i < len(pitches) else pad) & 0xFF
    pre[page - 10] = len(pitches) & 0xFF   # skeleton stores the count here
    return bytes(pre)


# ===========================================================================
# instruments_tab_tuning_mandolin.enc
# A generically-named instrument ("Melody") on a TAB-clef staff. No fretted
# template matches the name, so before the fix the staff stayed a plain 5-line
# STANDARD staff with no StringData, and no fret numbers were drawn. The importer
# must read the per-staff tab tuning stored before the first PAGE block (4
# strings, mandolin GDAE) and set up a 4-line TAB staff with matching StringData
# so the notes are fretted automatically.
# ===========================================================================
def gen_v0c4_tab_tuning_mandolin():
    name = 'Melody'.encode('utf-16-le') + b'\x00\x00'
    pre  = _patch_tk00(name)
    pre  = _set_staff_clef(pre, 0x08)               # EncClefType::TAB
    pre  = _set_tab_tuning(pre, [55, 62, 69, 76])   # mandolin GDAE (G3 D4 A4 E5)
    e  = note_v0c4(0,   0, 0, fv=3, pitch=62)        # D4 (open 2nd string)
    e += note_v0c4(240, 0, 0, fv=3, pitch=69)        # A4 (open 3rd string)
    e += note_v0c4(480, 0, 0, fv=3, pitch=64)        # E4
    e += note_v0c4(720, 0, 0, fv=3, pitch=67)        # G4
    e += end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_tab_tuning_guitar.enc
# Same generic-name TAB staff, but the stored tuning is a standard 6-string
# guitar (the tab-display pitches Encore writes). Exercises the 6-string path
# (6-line TAB staff, 6 StringData strings).
# ===========================================================================
def gen_v0c4_tab_tuning_guitar():
    name = 'Melody'.encode('utf-16-le') + b'\x00\x00'
    pre  = _patch_tk00(name)
    pre  = _set_staff_clef(pre, 0x08)               # EncClefType::TAB
    pre  = _set_tab_tuning(pre, [52, 57, 62, 67, 71, 76])   # guitar E3 A3 D4 G4 B4 E5
    e  = note_v0c4(0,   0, 0, fv=3, pitch=52)        # low E (open 6th string)
    e += note_v0c4(240, 0, 0, fv=3, pitch=64)        # E4
    e += note_v0c4(480, 0, 0, fv=3, pitch=71)        # B4 (open 2nd string)
    e += note_v0c4(720, 0, 0, fv=3, pitch=76)        # high E (open 1st string)
    e += end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# Build a custom 2-staff / N-instrument v0xC4 header + LINE block. Each entry is
# (clef_byte, staff_type, packed_instr_staff_idx) with an optional 4th element show (1=visible,
# 0=hidden; default visible). Mirrors how Encore lays out a notation staff followed by its
# tablature staff (separate single-staff instruments). Returns (header, line_block).
def _tab_header_and_line(entries):
    ninstr = len(entries)
    hdr = bytearray(194)
    hdr[0:4] = b'SCOW'
    hdr[4] = 0xC4
    struct.pack_into('<H', hdr, 0x28, 0x0420)   # chuVersio
    struct.pack_into('<h', hdr, 0x2E, 1)         # lineCount
    struct.pack_into('<h', hdr, 0x30, 1)         # pageCount
    hdr[0x32] = ninstr                            # instrumentCount
    hdr[0x33] = len(entries)                      # staffPerSystem
    struct.pack_into('<h', hdr, 0x34, 1)          # measureCount
    line_entries = b''
    for entry in entries:
        clef_byte, staff_type, packed = entry[0], entry[1], entry[2]
        show = entry[3] if len(entry) > 3 else 1
        e = bytearray(30)
        e[14] = clef_byte
        e[19] = show & 0xFF    # showByte: 1 = visible, 0 = hidden
        e[20] = staff_type     # 0 = melody, 1 = tablature
        e[21] = packed         # bits 0-5 instrument, bits 6-7 staff-within
        line_entries += bytes(e)
    line_data = b'\x00' * 10 + struct.pack('<H', 0) + bytes([1]) + line_entries
    var_size = len(line_data)
    return bytes(hdr), b'LINE' + struct.pack('<I', var_size) + line_data


# Build a real TK (instrument) block whose own tab tuning sits at the end of its content, exactly
# where the importer reads it (content ends with the 8-slot tuning; the block's declared varSize
# includes the next block's 8-byte header, an Encore quirk, so the block occupies varSize bytes and
# the next block starts at block_start + varSize).
def _tk_block(idx, name, pitches, pad=0x7F):
    tuning = bytes([(pitches[i] if i < len(pitches) else pad) & 0xFF for i in range(8)])
    name_b = name.encode('ascii') + b'\x00'
    content_len = 112
    content = name_b + b'\x00' * (content_len - len(name_b) - 8) + tuning
    var_size = len(content) + 8   # +8: next block's header, counted in varSize
    return ('TK%02d' % idx).encode('ascii') + struct.pack('<I', var_size) + content


# instruments_tab_two_tunings.enc
# Two TAB staves, each its own instrument (TK block), with DIFFERENT tunings: a 4-string tab
# (55 62 69 76) and a 6-string tab (52 57 62 67 71 76). Each TK block carries its own tuning at the
# end of its content. Regression: the importer must apply each staff its OWN tuning; the bug applied
# one tuning (the last block's / global) to every tab staff, so the first tab got the wrong tuning.
def gen_v0c4_tab_two_tunings():
    hdr, line_block = _tab_header_and_line([
        (0x08, 1, 0x00),   # instrument 0: tablature staff
        (0x08, 1, 0x01),   # instrument 1: tablature staff
    ])
    tk0 = _tk_block(0, 'TabA', [55, 62, 69, 76])              # 4-string
    tk1 = _tk_block(1, 'TabB', [52, 57, 62, 67, 71, 76])      # 6-string
    meas = meas_block(meas_hdr(4, 4), end_marker())
    return hdr + tk0 + tk1 + line_block + meas + SKELETON_POST


# instruments_tab_hidden_notation.enc
# A HIDDEN notation staff (instrument 0, with notes) plus a visible tablature staff (instrument 1),
# as Encore stores a tab shown over a hidden solfeo. In Ignore mode the tab is dropped, leaving only
# the hidden notation; the importer must reveal it, because an all-hidden score has no playable part
# and crashes playback (PlaybackController::doPause asserts currentPlayer()).
def gen_v0c4_tab_hidden_notation():
    hdr, line_block = _tab_header_and_line([
        (0x00, 0, 0x00, 0),   # instrument 0: notation, HIDDEN
        (0x08, 1, 0x01, 1),   # instrument 1: tablature, visible
    ])

    def note_raw(tick, raw_staff, fv, pitch):
        d = bytearray(25)
        d[0] = 28
        d[1] = raw_staff & 0xFF
        d[2] = fv
        d[12] = pitch
        return struct.pack('<H', tick) + bytes([(9 << 4) | 0]) + bytes(d)

    e = (note_raw(0, 0x00, 3, 55) + note_raw(240, 0x00, 3, 59)
         + note_raw(480, 0x00, 3, 62) + note_raw(720, 0x00, 3, 64) + end_marker())
    meas = meas_block(meas_hdr(4, 4), e)
    return hdr + line_block + meas + SKELETON_POST


# instruments_tab_linked_pair.enc
# A notation staff (instrument 0, with notes) immediately followed by an empty tablature staff
# (instrument 1) that has no notes of its own, exactly as Encore stores a notation+tab pair. In
# Linked mode the two merge into one instrument with the tab linked to the notation; in Separate
# mode they stay two parts; in Ignore mode the tab staff is dropped.
def gen_v0c4_tab_linked_pair():
    hdr, line_block = _tab_header_and_line([
        (0x00, 0, 0x00),   # instrument 0: notation, treble clef
        (0x08, 1, 0x01),   # instrument 1: tablature (TAB clef, staff type 1)
    ])

    def note_raw(tick, raw_staff, fv, pitch):
        d = bytearray(25)
        d[0] = 28
        d[1] = raw_staff & 0xFF
        d[2] = fv
        d[12] = pitch
        return struct.pack('<H', tick) + bytes([(9 << 4) | 0]) + bytes(d)

    e = (note_raw(0,   0x00, 3, 55)   # notation staff only; the tab staff stays empty
         + note_raw(240, 0x00, 3, 59)
         + note_raw(480, 0x00, 3, 62)
         + note_raw(720, 0x00, 3, 64)
         + end_marker())
    meas = meas_block(meas_hdr(4, 4), e)
    return hdr + line_block + meas + SKELETON_POST


# instruments_tab_linked_overfull.enc
# Like the linked pair, but the notation measure overfills a 3/4 nominal bar to 7/8: five eighth
# notes (enc ticks 0..480) followed by a quarter (enc 600). The quarter spans to the bar end, so the
# final eighth slot (mscore tick 1440) has no notation element. The tab staff (instrument 1) carries
# its own explicit rest fill covering the whole bar including that final slot, exactly as Encore's
# tab view stores it. The IrregularMeasure strategy widens the bar to 7/8. In Linked mode the
# notation is cloned onto the tab: without clearing the tab first, its rest in the final slot has no
# notation counterpart to overwrite it, survives the clone, and pushes the tab to 8/8 (regression:
# the score sanity check then reports the tab bar overfull).
def gen_v0c4_tab_linked_overfull():
    hdr, line_block = _tab_header_and_line([
        (0x00, 0, 0x00),   # instrument 0: notation, treble clef
        (0x08, 1, 0x01),   # instrument 1: tablature
    ])

    def note_raw(tick, raw_staff, fv, pitch):
        d = bytearray(25)
        d[0] = 28
        d[1] = raw_staff & 0xFF
        d[2] = fv
        d[12] = pitch
        return struct.pack('<H', tick) + bytes([(9 << 4) | 0]) + bytes(d)

    e = b''.join(note_raw(120 * i, 0x00, 4, 55 + i) for i in range(5))   # notation: 5 eighths
    e += note_raw(600, 0x00, 3, 60)                                      # notation: 1 quarter
    e += b''.join(rest_v0c4(120 * i, 0, 1, 4) for i in range(7))         # tab: 7 eighth rests
    e += end_marker()
    meas = meas_block(meas_hdr(3, 4), e)
    return hdr + line_block + meas + SKELETON_POST


# instruments_tab_standalone_frets.enc
# A tab-only score (one tablature staff, no notation staff). Encore materializes the tab's notes as
# pitch-bearing REST elements: type = REST (8), voice bit 0x8 set, MIDI pitch at element +15, and no
# face value (duration is implied by the tick gaps). The importer must read these as notes so the
# standalone tab shows fret numbers.
def gen_v0c4_tab_standalone_frets():
    hdr, line_block = _tab_header_and_line([
        (0x08, 1, 0x00),   # instrument 0: tablature only
    ])

    def tab_fingering(tick, pitch):
        d = bytearray(15)
        d[0] = 18            # size (rest byte layout)
        d[1] = 0             # staff 0
        d[12] = pitch        # MIDI pitch at element +15
        return struct.pack('<H', tick) + bytes([(8 << 4) | 0x8]) + bytes(d)

    e = (tab_fingering(0,   40)   # E2
         + tab_fingering(240, 45)  # A2
         + tab_fingering(480, 50)  # D3
         + tab_fingering(720, 55)  # G3
         + end_marker())
    meas = meas_block(meas_hdr(4, 4), e)
    return hdr + line_block + meas + SKELETON_POST


# ===========================================================================
# notes_rdur_80_stays_16th.enc
# Two consecutive 16th notes (fv=5) 80 ticks apart in a 4/4 measure. The
# importer-computed realDuration for the first note is 80, which used to
# get mapped to V_EIGHTH via the now-removed triplet table. After the fix
# the face value is the source of truth and both notes stay as 16ths.
# ===========================================================================
def gen_v0c4_transposing_written_tpc():
    # BUG FIX: transposing instruments (Key=+6, like Dulzaina-in-F#) produced
    # double-flat note spellings because computeWindow used the WRITTEN key
    # (F major) for note penalization. Fix: use CONCERT key (B major) so the
    # algorithm penalizes Cb (not diatonic in B major) over B natural.
    # Fixture: instrument MIDI=69 (oboe, common-genre MIDI 68 0-indexed), Key=+6.
    # Written F4 (semiTonePitch=65) -> concert B4 (65+6=71).
    # Expected written TPC: 13 (F natural), not 1 (Gbb) or 7 (Cb).
    pre  = _patch_midi_program(SKELETON_PRE, 0, 69)  # MIDI 69 1-indexed = oboe
    pre  = _patch_key_transpose(pre, 0, 6)           # Key=+6 (sounds as written+6)
    e    = note_v0c4(0, 0, 0, fv=3, pitch=65)        # semiTonePitch=65 (written F4)
    e   += end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_transposing_respell_melody():
    # BUG FIX: a melody on a transposing instrument was respelled with
    # double-flats by score->spell(). spell() is a context/window heuristic that
    # picks enharmonics minimising accidental distance; on a transposing staff
    # whose written key is heavily flat (Eb, 3 flats) but whose concert key is
    # sharp (A major) it drifted concert E/B/G# to Fb/Cb/Ab and the derived
    # written notes to Cbb/Gbb/Ebb. A single note is spelled correctly by the
    # computeWindow concert-key fix, so the regression only shows with a melody.
    # Fix: after spell(), re-derive the TPC of notes on transposing staves from
    # the sounding pitch + concert key (respellTransposingStaves).
    # Fixture: oboe (MIDI 69), Key=+6 (aug4), written key sig = Eb (tipo=3).
    # Written pitches 70/65/62/58 -> concert 76/71/68/64 = E5/B4/G#4/E4.
    # Expected concert TPC 18/19/22/18 (E/B/G#/E), written TPC 12/13/16/12
    # (Bb/F/D/Bb); never double-flats (tpc <= 5).
    pre  = _patch_midi_program(SKELETON_PRE, 0, 69)
    pre  = _patch_key_transpose(pre, 0, 6)
    e    = keychange_v0c4(0, 0, 0, tipo=3)           # written key = Eb (3 flats)
    for i, cp in enumerate((76, 71, 68, 64)):
        e += note_v0c4(i * 240, 0, 0, fv=3, pitch=cp - 6)   # written = concert - 6
    e   += end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST




def gen_v0c4_orn_tempo_3_8_dotted_quarter():
    # BUG FIX: 3/8 files with beatTicks=360 (dotted-quarter beat) had their
    # ORN TEMPO treated as plain quarter BPM, giving 2/3 of the correct speed.
    # Old compound check required numerator > 3; 3/8 has numerator=3 so it failed.
    # Fix: also check beatTicks==360 from the MEAS header.
    # Fixture: 3/8 measure, beatTicks=360, ORN TEMPO=80 at tick=0.
    # Expected: TempoText BPS = 80 * 1.5 / 60 = 2.0 QPS (not 80/60 = 1.333).
    orn = bytearray(ornament_v0c4(0, 0, 0, tipo=0x32))
    orn[30] = 80   # tempo byte (beat-unit BPM = 80)
    e    = bytes(orn)
    e   += note_v0c4(0, 0, 0, fv=3, pitch=60)
    e   += end_marker()
    return assemble(0xC4, [(meas_hdr(3, 8, bpm=0, beatTicks=360), e)], fill_ts=(3, 8))


def gen_v0c4_meas_bpm_suppressed_by_orn_tempo_later_tick():
    # BUG FIX: when an ORN TEMPO element was placed at the first NOTE tick (after
    # an initial rest), the MEAS-header BPM guard only checked the segment at
    # measTick (tick=0, the rest segment). It missed the ORN TEMPO in the later
    # segment, creating two conflicting tempo marks.
    # Fix: widen guard to scan all segments in the measure.
    # Fixture: 4/4, MEAS bpm=160, quarter REST at tick=0, ORN TEMPO=63 at tick=240.
    # Elements in tick order (REST before ORN so segments exist when ORN is processed).
    # Expected: exactly ONE TempoText with BPS = 63/60 (ORN TEMPO wins).
    orn = bytearray(ornament_v0c4(240, 0, 0, tipo=0x32))
    orn[30] = 63   # ORN TEMPO=63 at tick=240 (after initial rest)
    # NOTE before ORN: the note at tick=240 creates a ChordRest segment so the ORN
    # can attach to it (otherwise ORN falls back to measTick and the test is trivial).
    e    = rest_v0c4(0, 0, 0, fv=3)                  # quarter rest at tick=0
    e   += note_v0c4(240, 0, 0, fv=3, pitch=60)      # C4 at tick=240 (creates segment)
    e   += bytes(orn)                                 # ORN TEMPO=63 at tick=240 (finds that segment)
    e   += note_v0c4(480, 0, 0, fv=3, pitch=60)
    e   += note_v0c4(720, 0, 0, fv=3, pitch=60)
    e   += end_marker()
    # Use bpm=0 for fill measures so MEAS-BPM loop skips them (bpm==0 → continue).
    # assemble() would use bpm=100 fill measures creating a second TempoText.
    pre  = set_chumagio(0xC4)
    body = meas_block(meas_hdr(4, 4, bpm=160), e)
    body += b''.join(meas_block(meas_hdr(4, 4, bpm=0), end_marker()) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# text_tempo_orn_xoffset_downbeat.enc
# BUG FIX: an ORN TEMPO is anchored in Encore to a note's tick but drawn (via a
# smaller xoffset) to the LEFT of that note, over the earlier downbeat rest. The
# importer placed the TempoText on the later note instead of the downbeat. It
# must snap the mark to the chord-rest whose xoffset matches its drawn position,
# exactly as dynamics already do.
# Fixture: 5/8 (beatTicks=120). Dotted-quarter REST at tick 0 (xoff 0), quarter
# NOTE at tick 360 (xoff 67). ORN TEMPO=63 stored at tick 360 with xoffset=48
# (left of the note) -> belongs to the downbeat rest. Header bpm=0 so only the
# ORN places a tempo.
# Expected: one TempoText at the measure downbeat (rtick 0), value quarter=63;
# the bug placed it on the note at tick 360 (3/8).
# ===========================================================================
def gen_v0c4_tempo_orn_xoffset_downbeat():
    orn = bytearray(ornament_v0c4(360, 0, 0, tipo=0x32, xoffset=48))
    orn[30] = 63        # ORN TEMPO=63 (quarter BPM in 5/8, a non-compound meter)
    e  = rest_v0c4(0, 0, 0, fv=3)                       # dotted-quarter rest at downbeat
    e += note_v0c4_xoff(360, 0, 0, fv=3, pitch=60, xoff=67)  # quarter note, drawn right of ORN
    e += bytes(orn)
    e += end_marker()
    pre  = set_chumagio(0xC4)
    body = meas_block(meas_hdr(5, 8, bpm=0, beatTicks=120), e)
    body += b''.join(meas_block(meas_hdr(5, 8, bpm=0, beatTicks=120), end_marker()) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# text_tempo_orn_explicit_quarter_unit.enc
# BUG FIX: the tempo mark's beat unit was guessed from the meter (compound -> dotted
# quarter), ignoring the explicit unit Encore stores on the mark in the ORN `noto`
# byte. In a 6/8 a "quarter = 198" mark (noto = 2, a plain quarter) was rewritten as
# the compound default "dotted-quarter = 132" even though both are the same speed.
# The importer must honor `noto`: low 7 bits = note value (2 = quarter), high bit
# 0x80 = dotted.
# Fixture: 6/8 (beatTicks=360), header bpm=198 (quarter BPM), ORN TEMPO=198 with
# noto=2. The ORN value equals the header so it is suppressed and the header renders
# the mark; honoring noto, the display must be quarter=198, not dotted-quarter=132.
# Expected: TempoText "quarter = 198", BPS = 198/60 = 3.3.
# ===========================================================================
def ornament_v0c2_tempo(tick, voice, staffIdx, bpm):
    """36-byte v0xC2 TEMPO ornament (tipo=0x32). Encore 3.x/4.x stores the BPM at
    element +28 (d[28]); the +30 slot that carries the BPM in v0xC4 holds a constant
    unrelated byte here (observed 0x34 = 52), which must be ignored."""
    d = bytearray(36)
    struct.pack_into('<H', d, 0, tick)
    d[2] = (5 << 4) | (voice & 0xF)
    d[3] = 36
    d[4] = staffIdx & 0x3F
    d[5] = 0x32           # TEMPO subtype
    d[28] = bpm & 0xFF    # v0xC2 tempo BPM
    d[30] = 0x34          # constant byte (52); v0xC4 puts the BPM here, v0xC2 does not
    return bytes(d)


# ===========================================================================
# text_tempo_orn_v0c2_bpm_offset.enc
# BUG FIX: v0xC2 (Encore 3.x/4.x) stores a tempo mark's BPM at ORN element +28,
# not +28-is-noto/+30-is-BPM like v0xC4. Reading +30 yielded a constant 52, so a
# "negra = 80" mark imported as "negra = 52". Move +28 into the tempo for v0xC2.
# Fixture: 4/4 v0xC2, header bpm=80, ORN TEMPO with BPM=80 at +28 (and 52 at +30).
# Expected: TempoText quarter=80 (the +28 value), not 52 (the +30 constant).
# ===========================================================================
def gen_v0c2_tempo_orn_bpm_offset():
    orn = ornament_v0c2_tempo(0, 0, 0, bpm=80)
    e  = bytes(orn)
    e += note_v0c2(0, 0, 0, fv=3, pitch=60)
    e += end_marker()
    pre  = set_chumagio(0xC2)
    body = meas_block(meas_hdr(4, 4, bpm=80), e)
    body += b''.join(meas_block(meas_hdr(4, 4, bpm=0), end_marker()) for _ in range(5))
    return pre + body + SKELETON_POST


def ornament_v0c2_tempo_v0c4_layout(tick, voice, staffIdx, bpm, noto=2):
    """38-byte v0xC2 TEMPO ornament that uses the v0xC4-style layout: a beat-unit
    code (noto) at +28 and the BPM at +30. Newer Encore 4.x files store the tempo
    this way (the value at +28 is a small note-value code, e.g. 0x02 = quarter),
    unlike the older layout where the BPM itself sits at +28."""
    d = bytearray(38)
    struct.pack_into('<H', d, 0, tick)
    d[2] = (5 << 4) | (voice & 0xF)
    d[3] = 38
    d[4] = staffIdx & 0x3F
    d[5] = 0x32           # TEMPO subtype
    d[28] = noto & 0xFF   # beat-unit code (0x02 = quarter)
    d[30] = bpm & 0xFF    # v0xC4-style BPM at +30
    return bytes(d)


# ===========================================================================
# text_tempo_orn_v0c2_v0c4_layout.enc
# FIX: some v0xC2 (Encore 4.x) files store the tempo mark the v0xC4 way: a small
# beat-unit code at ORN +28 (noto, e.g. 0x02 = quarter) and the real BPM at +30.
# The earlier v0xC2 rule moved +28 into the tempo unconditionally, so a quarter=158
# mark imported as quarter=2 (the beat-unit code). The reader must keep the +30 BPM
# when +28 is a valid beat-unit code, and only move +28 across when it is not.
# Fixture: 4/4 v0xC2, header bpm=0, ORN TEMPO with noto=0x02 at +28 and BPM=158 at +30.
# Expected: TempoText quarter=158, BPS=158/60; not quarter=2.
# ===========================================================================
def gen_v0c2_tempo_orn_v0c4_layout():
    orn = ornament_v0c2_tempo_v0c4_layout(0, 0, 0, bpm=158, noto=2)
    e  = bytes(orn)
    e += note_v0c2(0, 0, 0, fv=3, pitch=60)
    e += end_marker()
    pre  = set_chumagio(0xC2)
    body = meas_block(meas_hdr(4, 4, bpm=0), e)
    body += b''.join(meas_block(meas_hdr(4, 4, bpm=0), end_marker()) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_tempo_orn_explicit_quarter_unit():
    orn = bytearray(ornament_v0c4(0, 0, 0, tipo=0x32))
    orn[28] = 2          # noto = explicit quarter-note beat unit (0-indexed note value)
    orn[30] = 198        # ORN TEMPO value, in the noto unit (quarter BPM)
    e  = bytes(orn)
    e += note_v0c4(0, 0, 0, fv=4, pitch=60)   # one eighth note (6/8); rest auto-filled
    e += end_marker()
    pre  = set_chumagio(0xC4)
    body = meas_block(meas_hdr(6, 8, bpm=198, beatTicks=360), e)
    body += b''.join(meas_block(meas_hdr(6, 8, bpm=198, beatTicks=360), end_marker()) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# text_orn_tempo_mismatch_suppressed.enc
# BUG FIX: ORN TEMPO ornaments stored in the wrong measure (one system before
# their intended position due to Encore's visual layout) carry a BPM that
# conflicts with the measure's own header BPM. The importer must suppress such
# ornaments so the MEAS-header BPM creates the TempoText at the correct measure.
#
# Pattern: Encore places a tempo annotation visually at the START of a new system
# (measure N) but stores it in the LAST measure of the PREVIOUS system (measure
# N-7) because that measure occupies the same x-column in Encore's layout.  The
# ORN TEMPO carries the BPM for measure N, which disagrees with the header BPM
# of the measure it was stored in.
#
# Fixture: 2 content measures (+ 4 silent fill measures).
#   Measure 1: header BPM=249, ORN TEMPO=80 (BPM conflicts with header → misplaced).
#   Measure 2: header BPM=80, no ORN TEMPO.
# Expected: TempoText BPM=80 at measure 2 only; no TempoText at measure 1.
# ===========================================================================
def gen_v0c4_orn_tempo_mismatch_suppressed():
    orn = bytearray(ornament_v0c4(0, 0, 0, tipo=0x32))
    orn[30] = 80        # ORN TEMPO=80 conflicts with measure-1 header BPM=249
    e1  = bytes(orn)
    e1 += note_v0c4(0, 0, 0, fv=3, pitch=60)
    e1 += end_marker()
    e2  = note_v0c4(0, 0, 0, fv=3, pitch=60)
    e2 += end_marker()
    pre  = set_chumagio(0xC4)
    body = meas_block(meas_hdr(4, 4, bpm=249), e1)
    body += meas_block(meas_hdr(4, 4, bpm=80), e2)
    body += b''.join(meas_block(meas_hdr(4, 4, bpm=0), end_marker()) for _ in range(4))
    return pre + body + SKELETON_POST


# ===========================================================================
# text_orn_tempo_misplaced_multi_measure.enc
# Same misplacement quirk, but the tempo change is SEVERAL measures after the ORN, not just
# one. The header BPM persists on every measure, so the change to 80 only appears at measure 4.
#   Measure 1: header BPM=249, ORN TEMPO=80 (conflicts → misplaced, suppress).
#   Measures 2-3: header BPM=249 (no change).
#   Measure 4: header BPM=80 (the tempo actually changes here).
# Expected: ONE TempoText 80 at measure 4; NO tempo text at measure 1. The old one-measure-ahead
# check missed this and wrongly placed 80 at measure 1.
# ===========================================================================
def gen_v0c4_orn_tempo_misplaced_multi_measure():
    orn = bytearray(ornament_v0c4(0, 0, 0, tipo=0x32))
    orn[30] = 80        # ORN TEMPO=80, four measures before the BPM actually changes to 80
    e1  = bytes(orn) + note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    en  = note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    pre  = set_chumagio(0xC4)
    body  = meas_block(meas_hdr(4, 4, bpm=249), e1)
    body += meas_block(meas_hdr(4, 4, bpm=249), en)
    body += meas_block(meas_hdr(4, 4, bpm=249), en)
    body += meas_block(meas_hdr(4, 4, bpm=80), en)
    body += b''.join(meas_block(meas_hdr(4, 4, bpm=80), end_marker()) for _ in range(2))
    return pre + body + SKELETON_POST


def gen_v0c4_orn_tempo_equals_header_at_start():
    # Initial tempo: measure 1 header BPM=230, plus a redundant ORN TEMPO=230 stored at a LATE tick
    # (tick 240), the same value as the header. The ORN's late/end-of-measure segment does not set
    # the playback tempo, so it must be suppressed and the header must place the TempoText at the
    # MEASURE START (tick 0). Mirrors an initial "= 230" Encore stores at the end of measure 1.
    orn = bytearray(ornament_v0c4(240, 0, 0, tipo=0x32))
    orn[30] = 230
    e1  = note_v0c4(0, 0, 0, fv=3, pitch=60)
    e1 += bytes(orn)
    e1 += note_v0c4(240, 0, 0, fv=3, pitch=60)
    e1 += end_marker()
    pre  = set_chumagio(0xC4)
    body = meas_block(meas_hdr(4, 4, bpm=230), e1)
    body += b''.join(meas_block(meas_hdr(4, 4, bpm=230), end_marker()) for _ in range(3))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_no_tk_name_recovered.enc
# BUG FIX: when a v0xC4 file has no TK blocks, recoverMissingNames() reads
# instrument names from NAME_BASE=202, NAME_STEP=2158. Previously the fallback
# code set name="Part N" *before* calling readInstrumentMeta(), so the guard
# `!name.isEmpty()` in recoverMissingNames() skipped all instruments, leaving
# the user-defined names (e.g. "1º Dulzaina") unread.
#
# Fixture: TK magic zeroed (no TK blocks). UTF-16 LE "Dulzaina\0" written at
# offset 202 (NAME_BASE for instrument 0). After import the part's long name
# must be "Dulzaina" instead of "Part 1".
# ===========================================================================
def gen_v0c4_no_tk_name_recovered():
    pre = bytearray(SKELETON_PRE)
    # Zero TK00 magic so the parser finds no TK blocks.
    pre[194:198] = b'\x00\x00\x00\x00'
    # Write "Dulzaina" as UTF-16 LE at NAME_BASE=202 (instrument 0 name slot).
    name_utf16 = 'Dulzaina'.encode('utf-16-le') + b'\x00\x00'
    pre[202:202 + len(name_utf16)] = name_utf16
    e = note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return bytes(pre) + body + SKELETON_POST


# ===========================================================================
# instruments_no_tk_name_latin1.enc
# Same fix, Latin-1 encoding path: b0 printable, b1 also printable (not 0x00)
# → probeUtf16LE returns false, isLatin1 = true → name decoded as Latin-1.
# "Tamboril" (pure ASCII) stored at NAME_BASE=202. Must be recovered as-is.
# ===========================================================================
def gen_v0c4_unique_name_beats_midi():
    # Instrument named "Dulzaina" with MIDI 69 (1-indexed) = Oboe (GM 68).
    # "Dulzaina" is a substring of exactly one template ("Castilian Dulzaina"), so the
    # name match is unique/distinctive. The importer must keep the dulzaina template and
    # NOT let the Oboe MIDI program override it. (Ambiguous substrings like "Bajo", which
    # hit many templates, still defer to MIDI.)
    pre = bytearray(SKELETON_PRE)
    pre[194:198] = b'\x00\x00\x00\x00'  # zero TK magic -> name recovered from NAME_BASE
    name_utf16 = 'Dulzaina'.encode('utf-16-le') + b'\x00\x00'
    pre[202:202 + len(name_utf16)] = name_utf16
    while len(pre) <= 2278:
        pre.extend(b'\x00' * 64)
    pre[2278] = 69                       # MIDI 69 (1-indexed) = Oboe at large-TK position
    e = note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return bytes(pre) + body + SKELETON_POST


def gen_v0c4_fuzzy_name_match():
    # Name "Clarynet" is a one-substitution typo of "Clarinet" and is NOT a substring of any
    # template name, so the exact/contains search finds nothing. With no MIDI program either,
    # the only way to reach a clarinet is the last-resort Levenshtein (edit-distance) pass;
    # without it the instrument falls back to Grand Piano.
    pre = bytearray(SKELETON_PRE)
    pre[194:198] = b'\x00\x00\x00\x00'  # zero TK magic -> name recovered from NAME_BASE
    name_utf16 = 'Clarynet'.encode('utf-16-le') + b'\x00\x00'
    pre[202:202 + len(name_utf16)] = name_utf16
    e = note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return bytes(pre) + body + SKELETON_POST


def gen_v0c4_no_tk_name_latin1():
    pre = bytearray(SKELETON_PRE)
    pre[194:198] = b'\x00\x00\x00\x00'
    # "Tamboril" in Latin-1 (b0='T'=0x54, b1='a'=0x61 → detected as Latin-1).
    name_latin1 = b'Tamboril\x00'
    pre[202:202 + len(name_latin1)] = name_latin1
    e = note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return bytes(pre) + body + SKELETON_POST


# ===========================================================================
# instruments_no_tk_name_fallback.enc
# When no recoverable name exists at NAME_BASE (b0 is not printable ASCII),
# the "Part N" fallback must still fire so the part has a non-empty name.
# ===========================================================================
def gen_v0c4_no_tk_name_fallback():
    pre = bytearray(SKELETON_PRE)
    pre[194:198] = b'\x00\x00\x00\x00'
    # Force b0=0x00 at offset 202 → fails the printable-ASCII check → skip.
    pre[202] = 0x00
    pre[203] = 0x00
    e = note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return bytes(pre) + body + SKELETON_POST


def gen_v0c4_no_tk_blocks_midi_key():
    # v0xC4 file where the TK00 block magic is zeroed out so the parser finds no
    # instrument TK blocks. The instruments list is populated by fallback ("Part N",
    # contentFilePos=-1). MIDI=69 (oboe) and Key=6 (+6 semitones transposition) are
    # stored at the standard large-TK positions (2278, 2255) as in a normal v0xC4 file.
    # The importer must fall back to the large-TK positions for MIDI and Key when
    # contentFilePos<0 (no TK blocks found), instead of using compact offsets (390/367)
    # that have 0 in this file.
    pre = bytearray(SKELETON_PRE)
    # Zero TK00 magic (bytes 194-197 = "TK00") so isInstrumentMagic returns false.
    pre[194:198] = b'\x00\x00\x00\x00'
    # Store MIDI=69 (1-indexed, oboe) at large-TK MIDI position (base=2278, n=0)
    while len(pre) <= 2278:
        pre.extend(b'\x00' * 64)
    pre[2278] = 69
    # Store Key=6 (+6 semitones) at large-TK Key position (base-23=2255, n=0)
    pre[2255] = 6
    e = note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return bytes(pre) + body + SKELETON_POST


# ===========================================================================
# instruments_no_tk_compact_table_two_instrs.enc
# No TK block magic, two instruments, and the compact per-staff table: the
# programs sit at 390 + n*112 behind their channel runs, the keys 23 bytes
# ahead of them.  Both instruments must get their key, not just the first.
# ===========================================================================
def gen_v0c4_no_tk_compact_table_two_instrs():
    PROG_BASE, PROG_STEP, VOICES = 390, 112, 8
    pre = bytearray(SKELETON_PRE[:194])
    pre[0x32] = 2
    while len(pre) < PROG_BASE + PROG_STEP + 8:
        pre.extend(b'\x00' * 64)
    for n, (prog, key) in enumerate(((66, -2), (67, -9))):
        off = PROG_BASE + n * PROG_STEP
        for v in range(VOICES):
            pre[off - VOICES + v] = 1          # channel run
            pre[off + v] = prog
        pre[off - 23] = key & 0xFF
    LEGACY_TK_END = 194 + 8 + 2158
    pre += SKELETON_PRE[LEGACY_TK_END:]        # PAGE and LINE, well before offset 2278
    e = note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return bytes(pre) + body + SKELETON_POST


# ===========================================================================
# instruments_oversized_varsize_key_from_entry_end.enc
# A 242-byte TK block whose declared size is 0x70000000, as Encore 4 writes
# it, so the size says nothing about the layout: it masks to zero and exceeds
# the block.  The per-staff tables sit at the usual distance from the entry
# end, and the key 23 bytes ahead of them.
# ===========================================================================
def gen_v0c4_oversized_varsize_key_from_entry_end():
    STRIDE, MIDI_FROM_END, VOICES = 242, 46, 8
    header = bytearray(SKELETON_PRE[:194])
    header[0x32] = 1

    def make_tk(idx, name_str, midi_1idx, key_semitones):
        content = bytearray(STRIDE - 8)
        nb = name_str.encode('ascii') + b'\x00'
        content[:len(nb)] = nb
        prog = len(content) - MIDI_FROM_END
        for v in range(VOICES):
            content[prog - VOICES + v] = 2
            content[prog + v] = midi_1idx & 0xFF
        content[prog - 23] = key_semitones & 0xFF
        return 'TK{:02d}'.format(idx).encode('ascii') + struct.pack('<I', 0x70000000) + bytes(content)

    LEGACY_TK_END = 194 + 8 + 2158
    page_line = SKELETON_PRE[LEGACY_TK_END:]
    page_line = page_line[page_line.index(b'LINE'):]   # LINE right after the entry, as Encore writes it
    pre = bytes(header) + make_tk(0, 'Bass', 34, -12) + page_line
    e = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_small_tk_no_cross_entry_tables.enc
# Two 112-byte TK blocks, so the stride is measured between them.  Instrument 0
# keeps no per-staff table of its own, and the position the 5.x formula points
# at for it (content + size + 76) falls inside instrument 1, where this file
# does keep a table.  Instrument 0 must not borrow it: with no table of its own
# it has no program, and falls back to Grand Piano.
# ===========================================================================
def gen_v0c4_small_tk_no_cross_entry_tables():
    VARSIZE, VOICES = 112, 8
    TK_START = 194
    CROSS_IN_NEXT = 84          # where instrument 0's 5.x formula lands inside instrument 1

    header = bytearray(SKELETON_PRE[:TK_START])
    header[0x32] = 2

    def make_tk(idx, name_str, table_at=None, prog=0):
        content = bytearray(VARSIZE - 8)
        nb = name_str.encode('ascii') + b'\x00'
        content[:len(nb)] = nb
        if table_at is not None:                      # channel run then program, inside the content
            for v in range(VOICES):
                content[table_at - 8 - 8 + v] = 1
                content[table_at - 8 + v] = prog
        return 'TK{:02d}'.format(idx).encode('ascii') + struct.pack('<I', VARSIZE) + bytes(content)

    tk00 = make_tk(0, 'NoTable')
    tk01 = make_tk(1, 'HasTable', table_at=CROSS_IN_NEXT, prog=90)

    LEGACY_TK_END = 194 + 8 + 2158
    pre = bytes(header) + tk00 + tk01 + SKELETON_PRE[LEGACY_TK_END:]
    e = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_rdur_80_stays_16th():
    # 1/16 face value, 80 raw ticks apart so calculateRealDurations writes
    # rdur=80 on the first note (the old triplet table would upgrade to V_EIGHTH).
    e  = note_v0c4( 0, 0, 0, fv=5, pitch=60)
    e += note_v0c4(80, 0, 0, fv=5, pitch=62)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# instruments_instr_clarinet_midi72_key0.enc
# BUG: Bb clarinet (MIDI 72, Key=0) was resolved to Grand Piano.
# Root cause: transpCompatibleWith() returns false for any transposing
# template when encKey==0, blocking both name+MIDI (step 2) and MIDI (step 5).
# Fix: step 2 falls back to best name+MIDI match when no compatible match
# exists; step 5 always accepts the MIDI match.
# ===========================================================================
def gen_v0c4_instr_clarinet_midi72_key0():
    name = 'Clarinete'.encode('utf-16-le') + b'\x00\x00'
    pre  = _patch_tk00(name)
    pre  = _patch_midi_program(pre, 0, 72)    # 1-indexed = Bb clarinet
    pre  = _patch_key_transpose(pre, 0, 0)    # Key not set (sounds as written)
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_instr_empty_name_midi_clarinet.enc
# Bb clarinet with no name (TK00 has the default 3-char skeleton name,
# which triggers nameTooShort, so only MIDI step 5 can identify it).
# With Key=0 the old transposition filter rejected the bb-clarinet template
# and fell through to Grand Piano.
# ===========================================================================
def gen_v0c4_instr_empty_name_midi_clarinet():
    pre  = _patch_midi_program(SKELETON_PRE, 0, 72)   # 1-indexed = Bb clarinet
    pre  = _patch_key_transpose(pre, 0, 0)            # Key not set
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_instr_clarinet_midi72_key_neg2.enc
# Bb clarinet with name and Key=-2 (correctly configured in Encore's Staff
# Sheet). The transposition filter should keep selecting bb-clarinet; this
# regression test guards against fixing key0 while breaking the correct case.
# ===========================================================================
def gen_v0c4_instr_clarinet_midi72_key_neg2():
    name = 'Clarinete'.encode('utf-16-le') + b'\x00\x00'
    pre  = _patch_tk00(name)
    pre  = _patch_midi_program(pre, 0, 72)    # 1-indexed = Bb clarinet
    pre  = _patch_key_transpose(pre, 0, -2)   # Bb transposition correctly set
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_instr_recorder_midi75_trackname.enc
# Instrument named "Txistu" with MIDI program 75 (1-indexed = Recorder). The
# name matches no template, so only MIDI step 5 fires and selects the bare
# "recorder" template. That template carries no <trackName>/<longName> in
# instruments.xml (its UI name comes from muse_instruments), so before the fix
# the imported part kept an empty track name and the mixer/instruments panel
# showed a blank instrument name. The importer must backfill the track name
# from the Encore instrument name.
# ===========================================================================
def gen_v0c4_instr_recorder_midi75_trackname():
    name = 'Txistu'.encode('utf-16-le') + b'\x00\x00'
    pre  = _patch_tk00(name)
    pre  = _patch_midi_program(pre, 0, 75)    # 1-indexed = Recorder
    pre  = _patch_key_transpose(pre, 0, 0)    # sounds as written
    e    = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_lyrics():
    e  = lyric_v0c4(  0, 0, 0, 'do')
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += lyric_v0c4(240, 0, 0, 're')
    e += note_v0c4( 240, 0, 0, fv=3, pitch=62)
    e += lyric_v0c4(480, 0, 0, 'mi')
    e += note_v0c4( 480, 0, 0, fv=3, pitch=64)
    e += lyric_v0c4(720, 0, 0, 'fa')
    e += note_v0c4( 720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_multi_measure_hairpin():
    # Measure 0: crescendo start at tick=0, ends 2 measures forward.
    m0  = ornament_v0c4(0, 0, 0, tipo=0x1D, xoffset=5, alMezuro=2, xoffset2=10, speguleco=0)
    m0 += note_v0c4(  0, 0, 0, fv=3, pitch=60, tuplet=0)
    m0 += note_v0c4(240, 0, 0, fv=3, pitch=62, tuplet=0)
    m0 += note_v0c4(480, 0, 0, fv=3, pitch=64, tuplet=0)
    m0 += note_v0c4(720, 0, 0, fv=3, pitch=65, tuplet=0)
    m0 += end_marker()
    # Measure 1: diminuendo start at tick=480, ends 1 measure forward.
    m1  = ornament_v0c4(480, 0, 0, tipo=0x1D, xoffset=20, alMezuro=1, xoffset2=5, speguleco=1)
    m1 += note_v0c4(  0, 0, 0, fv=3, pitch=67, tuplet=0)
    m1 += note_v0c4(240, 0, 0, fv=3, pitch=69, tuplet=0)
    m1 += note_v0c4(480, 0, 0, fv=3, pitch=71, tuplet=0)
    m1 += note_v0c4(720, 0, 0, fv=3, pitch=72, tuplet=0)
    m1 += end_marker()
    # Measure 2: filler notes only.
    m2  = note_v0c4(  0, 0, 0, fv=3, pitch=72, tuplet=0)
    m2 += note_v0c4(240, 0, 0, fv=3, pitch=71, tuplet=0)
    m2 += note_v0c4(480, 0, 0, fv=3, pitch=69, tuplet=0)
    m2 += note_v0c4(720, 0, 0, fv=3, pitch=67, tuplet=0)
    m2 += end_marker()
    return assemble(0xC4,
                    [(meas_hdr(4, 4), m0),
                     (meas_hdr(4, 4), m1),
                     (meas_hdr(4, 4), m2)],
                    fill_ts=(4, 4))


# ===========================================================================
# Per-instrument Key transposition helpers.
# The Encore Staff Sheet stores a signed-int8 semitone offset 23 bytes BEFORE
# the MIDI-program byte in the fixed-offset instrument table; see EncFile::read.
# ===========================================================================
def _patch_key_transpose(pre_bytes, instrument_index, semitones):
    PRG_BASE = 2278
    PRG_STEP = 2158
    KEY_OFFSET_FROM_PRG = -23
    pre = bytearray(pre_bytes)
    off = PRG_BASE + KEY_OFFSET_FROM_PRG + instrument_index * PRG_STEP
    while len(pre) <= off:
        pre.extend(b'\x00' * 64)
    pre[off] = semitones & 0xFF
    return bytes(pre)


def _patch_instrument_name(pre_bytes, instrument_index, name_str):
    """Write the UTF-16 LE name for an instrument at the formula-derived
    position (NAME_BASE + n * NAME_STEP). For n=0 this overlaps the TK00
    name slot; for n>=1 it lets the importer's name-recovery loop pick up
    the name even without an extra TK block."""
    NAME_BASE = 202
    NAME_STEP = 2158
    name_utf16 = name_str.encode('utf-16-le') + b'\x00\x00'
    pre = bytearray(pre_bytes)
    off = NAME_BASE + instrument_index * NAME_STEP
    while len(pre) < off + len(name_utf16):
        pre.extend(b'\x00' * 64)
    pre[off: off + len(name_utf16)] = name_utf16
    return bytes(pre)


# ===========================================================================
# importer_v0c2_multi_stream_drift.enc
# v0xC2 MIDI-recorded measure: voice 0 carries notes at drift ticks that are
# not face-grid aligned and produce an implicit gap larger than
# CHORD_MIDI_THRESHOLD. An earlier version of the implicit-silence gap snap
# (added for the m1 voice-1 reordering case below) snapped cumTick to the
# absolute Encore tick whenever the gap exceeded the threshold, which
# mis-aligned drift positions and produced a zero-length rhythmic gap that
# aborted `populateRhythmicList` during layout:
#   Assertion failed: (rtick2 > rtick1), function strongestSubbeatLevelInRange
# The face-grid gate (snap only when e->tick % faceTicks == 0) makes the
# snap a no-op for drift-shifted ticks, so this file imports cleanly.
# ===========================================================================
def gen_v0c2_multi_stream_drift():
    e  = note_v0c2(  0, 0, 0, fv=4, pitch=60)   # face-grid (gap snap would apply)
    e += note_v0c2( 41, 0, 0, fv=5, pitch=62)   # drift
    e += note_v0c2(105, 0, 0, fv=5, pitch=64)   # drift
    e += note_v0c2(116, 0, 0, fv=5, pitch=65)   # drift
    e += note_v0c2(199, 0, 0, fv=4, pitch=67)   # drift
    e += note_v0c2(280, 0, 0, fv=5, pitch=69)   # drift
    e += note_v0c2(321, 0, 0, fv=5, pitch=71)   # drift
    e += end_marker()
    return assemble(0xC2, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))


# ===========================================================================
# structure_octave_lower_implicit_silences.enc
# Single-staff v0xC4 file with the staff's Encore "Key" set to Octave Lower
# (keyTransposeSemitones = -12). m1 carries the implicit-silence pattern:
#   - voice 0: two quarter notes at ticks 240 and 480 (3/4 measure), NO REST
#     at tick 0 -- the leading silence is encoded only via the tick offset.
#     Without the face-grid gated gap snap, both notes squashed to beats 1-2
#     with the trailing rest pushed past the actual sounding content.
#   - voice 1: a single quarter chord (pitches 64+73) at tick 0 with no
#     further elements. EncMeasure::calculateRealDurations inflates rdur to
#     the gap-to-measure-end (720 in 3/4) which lands on the dotted-half
#     bucket; the inflated-rdur guard keeps the chord a quarter.
# Combined with the Key = -12 transposition, the imported pitches are
#   voice 0: 73 - 12 = 61, 74 - 12 = 62
#   voice 1 chord: {64, 73} - 12 = {52, 61}
# ===========================================================================
def gen_v0c4_octave_lower_implicit_silences():
    pre = _patch_key_transpose(SKELETON_PRE, 0, -12)
    elems = (
        note_v0c4(tick=240, voice=0, staffIdx=0, fv=3, pitch=73)
      + note_v0c4(tick=480, voice=0, staffIdx=0, fv=3, pitch=74)
      + note_v0c4(tick=0,   voice=1, staffIdx=0, fv=3, pitch=64)
      + note_v0c4(tick=0,   voice=1, staffIdx=0, fv=3, pitch=73)
      + end_marker()
    )
    body  = meas_block(meas_hdr(3, 4), elems)
    body += b''.join(empty_meas(3, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# structure_key_per_staff.enc
# Two-instrument v0xC4 file with DIFFERENT Key transpositions per staff:
#   - instrument 0: Key = 0   (Sounds as Written)
#   - instrument 1: Key = -12 (Octave Lower)
# Both staves carry the same quarter chord (pitch 69 = A4) at m1 beat 1.
# Imported pitches:
#   - staff 0: 69  (unchanged)
#   - staff 1: 57  (= 69 - 12, the per-staff offset must be applied
#                  independently to each staff)
# Regression for the per-staff `staffPitchOffset` thread in buildScore.
# ===========================================================================
def gen_v0c4_key_per_staff():
    pre = bytearray(SKELETON_PRE)
    pre[0x32] = 2                  # instrumentCount = 2
    pre = bytes(pre)
    pre = _patch_key_transpose(pre, 0, 0)
    pre = _patch_key_transpose(pre, 1, -12)
    # Give instrument 1 a recoverable name so it lands on a sensible part.
    pre = _patch_instrument_name(pre, 1, 'Lower')
    elems = (
        note_v0c4(tick=0, voice=0, staffIdx=0, fv=3, pitch=69)
      + note_v0c4(tick=0, voice=0, staffIdx=1, fv=3, pitch=69)
      + end_marker()
    )
    body  = meas_block(meas_hdr(4, 4), elems)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# structure_octave_bassa_clef_override.enc
# Single-staff v0xC4 file whose only instrument is named "Laud" (matches the
# laud template, which carries G8_VB as its default concert clef). Encore
# writes the staff with a plain G clef + Key = "Octave Lower" (-12) so the
# user sees a normal treble while the staff plays one octave below. The
# importer must:
#   * apply Key = -12 to every pitch (binary 76 -> m_pitch 64), AND
#   * override the staff clef with the template's G8_VB
# so the resulting MuseScore staff matches both the visual (G8_VB clef with
# m_pitch 64 sits at the same staff position as binary 76 with G clef) and
# the sounding pitch (E4 = 64).
# ===========================================================================
def gen_v0c4_octave_bassa_clef_override():
    # TK00 ships with "Bandurria"; rewrite it to "Laud" so the importer
    # picks the laud template.
    name_utf16 = 'Laud'.encode('utf-16-le') + b'\x00\x00'
    pre = _patch_tk00(name_utf16)
    pre = _patch_key_transpose(pre, 0, -12)
    elems = (
        note_v0c4(tick=0, voice=0, staffIdx=0, fv=3, pitch=76)
      + end_marker()
    )
    body  = meas_block(meas_hdr(4, 4), elems)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# instruments_bass_guitar_transposing_clef.enc
# Single-staff v0xC4 file named "Bass Guitar" (matches the bass-guitar
# MuseScore template, which carries DISTINCT clefs: concertClef = F8_VB,
# transposingClef = F, transposeChromatic = -12). Encore writes the staff
# with a plain F clef + Key = "Octave Lower" (-12). The importer must
# pick the template's transposingClef (plain F) so the resulting score
# preserves Encore's glyph; MuseScore's transposeChromatic still places
# the noteheads at the same staff position the concert clef would render
# them at.
# Encore F clef byte = 0x01 (see EncClefType::F in enc-elements.h).
# ===========================================================================
# ===========================================================================
# instruments_compact_short_header_no_midi.enc
# Compact v0xC4 file (no TK blocks) whose LINE blocks start at offset 194,
# before the compact MIDI formula offset of 390.  The byte at offset 390
# falls inside the first LINE block's content and is set to 0x30 = 48,
# which would map to Timpani if the guard in readMidiPrograms were absent.
# With the guard, no MIDI program is read (midiProgram stays 0) and the
# instrument falls back to Grand Piano.
# ===========================================================================
def gen_v0c4_compact_short_header_no_midi():
    # 194-byte header (v0xC4, no TK blocks)
    hdr = bytearray(194)
    hdr[0:4] = b'SCOW'
    hdr[4] = 0xC4
    struct.pack_into('<h', hdr, 46, 1)   # lineCount = 1
    struct.pack_into('<h', hdr, 48, 1)   # pageCount = 1
    hdr[50] = 1                           # instrumentCount
    hdr[51] = 1                           # staffPerSystem
    struct.pack_into('<h', hdr, 52, 1)   # measureCount

    # LINE block at offset 194; varSize=200 so block spans 194..401 (208 bytes).
    # Offset 390 = content byte 188 (= 390 - 194 - 8); set to 0x30=48 to
    # simulate the spurious Timpani program the old formula would read.
    LINE_VS = 200
    lc = bytearray(LINE_VS)
    lc[12] = 1      # measureCount inside LINE
    SBASE = 13      # staffData starts at content byte 13
    lc[SBASE + 19] = 1   # showByte = visible
    lc[188] = 0x30       # 48 = Timpani; must NOT be read as MIDI program
    line_block = b'LINE' + struct.pack('<I', LINE_VS) + bytes(lc)

    meas = meas_block(meas_hdr(4, 4), b'\xff\xff')
    return bytes(hdr) + line_block + meas + SKELETON_POST


# ===========================================================================
# instruments_compact_tk_ignores_key_byte.enc
# Compact-TK v0xC4 file (TK varsize = 112): the PRG_BASE + n * PRG_STEP
# formula that locates per-instrument data does NOT apply. The byte at
# the formula-derived "Key" offset (PRG_BASE - 23 = 2255) is patched to
# a non-zero value (+8 semitones) to simulate the garbage Encore 5.0.2
# leaves at that position in real compact-TK files. The reader must NOT
# treat it as a valid Key offset; the imported pitch stays at the binary
# value (76 = E5), not 76 + 8 = 84 (C6).
# ===========================================================================
def gen_v0c4_compact_tk_ignores_key_byte():
    name = 'Bandurria'.encode('utf-16-le') + b'\x00\x00'
    pre = _patch_tk00(name, new_varsize=112)
    pre = bytearray(pre)
    # Patch the byte at the Key formula offset to a non-zero garbage value
    # that would shift every pitch by +8 if the guard is missing.
    PRG_BASE = 2278
    KEY_OFF  = PRG_BASE - 23
    while len(pre) <= KEY_OFF:
        pre.extend(b'\x00' * 64)
    pre[KEY_OFF] = 0x08
    pre = bytes(pre)
    elems = (
        note_v0c4(tick=0, voice=0, staffIdx=0, fv=3, pitch=76)
      + end_marker()
    )
    body  = meas_block(meas_hdr(4, 4), elems)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# importer_gap_snap_eighth_meter.enc
# 3/8 v0xC4 measure whose MEAS header carries beatTicks = 120 (the on-disk
# layout for x/8 meters in real Encore files; the synthetic generators
# default to 240 for every denominator, which masks the bug). The single
# staff contains a NOTE at Encore tick 240 with no preceding REST element
# -- the implicit-silence pattern that triggers the gap snap. With the
# wrong wholeTicks formula (4 * beatTicks = 480) the snap pushed cumTick
# to 240/480 = 1/2, half a whole note, well past the 3/8 measure end and
# the voice overflowed. With wholeTicks = beatTicks * timeSigDen (= 960)
# the snap pushes cumTick to 240/960 = 1/4 (= quarter), which fits.
# ===========================================================================
# ===========================================================================
# importer_v0xa6_no_spurious_tremolo.enc
# v0xA6 NOTEs are 10 bytes long but EncNote::read consumes 27 bytes from
# elemStart, so articulationUp and articulationDown end up reading the
# faceValue and grace2 of the NEXT note in the slot stream. Two consecutive
# v0xA6 quarter notes (fv=3) make the first note's articulationUp byte
# resolve to 0x03 -- the same value the tremolo dispatcher treats as
# "tremolo with 3 strokes". Without a size guard the importer creates a
# spurious TremoloSingleChord on every such pair and the playback render
# aborts on chords that lost their tremolo element during layout. The
# importer must NOT create any tremolos for v0xA6 files (the format does
# not carry tremolo data).
# ===========================================================================
# ===========================================================================
# importer_v0xa6_key_transposition.enc
# v0xA6 file with the per-instrument Key transposition byte set at the
# v0xA6 location (TK content + 42). Encore 2.x stores the value as a
# signed int8 in semitones, same UI range as later formats (-33..+24).
# The bazo skeleton's TK00 block starts at file offset 0xC2 (after the
# v0xC4 header layout); the importer reaches it via findNextKnownMagic
# from the v0xA6 header end (0xA6) by skipping the 20 header-tail bytes
# in between. byte 244 (= 0xC2 + 8 + 42) is therefore the Key field for
# instrument 0 in synthetics built off the bazo skeleton.
# ===========================================================================
# ===========================================================================
# importer_v0xa6_header_ends_at_0xa6.enc
# v0xA6 file with the TK00 block placed at file offset 0xA6 -- the proper
# v0xA6 layout. The old EncHeader::read unconditionally skipped to 0xC2,
# which is past the TK00 magic; findNextKnownMagic then walked forward,
# missed the in-progress TK00 content, and ended up reading some later
# block as instr[0]. With the v0xA6 header end at 0xA6, the reader lands
# right on the TK00 magic and parses instr[0] correctly with its Key
# byte (= -12) applied; a single C4 (binary 60) imports as m_pitch = 48.
# ===========================================================================
def gen_v0xa6_header_ends_at_0xa6():
    def mhdr_a6(tsNum, tsDen, bpm=100):
        h = bytearray(0x1A)
        struct.pack_into('<H', h, 0, bpm)
        beatTicks = 240
        durTicks  = beatTicks * tsNum * 4 // tsDen
        struct.pack_into('<HH', h, 4, beatTicks, durTicks)
        h[8], h[9] = tsNum, tsDen
        return bytes(h)
    pre = bytearray(set_chumagio(0xA6))
    # Insert a TK00 block at file offset 0xA6 (the proper v0xA6 location).
    # The 28 bytes between 0xA6 and 0xC2 in the bazo skeleton are header
    # tail; we replace the first 8 with TK00 magic + varsize=0x40, then
    # zero the next 20 bytes (TK00 content prefix), and patch byte +42 of
    # the TK00 content (= file offset 0xA6 + 8 + 42 = 0xD8) to 0xF4 (=
    # -12). The original TK00 at 0xC2 remains as TK01; with the header
    # fix the new TK00 at 0xA6 is read first.
    pre[0xA6:0xAA] = b'TK00'
    struct.pack_into('<I', pre, 0xAA, 0x40)
    # Zero out TK00 content (0xAE..0xC1) before re-patching key byte.
    for i in range(0xAE, 0xC2):
        pre[i] = 0
    # Key byte at content + 42 = 0xAE + 42 = 0xD8.
    # But 0xD8 is past 0xC2 already (overlaps with the legacy TK00 magic
    # area). To keep the layout safe, we limit the patch to bytes within
    # 0xA6..0xC1 by placing the key byte at TK content +42 which is
    # 0xD8. Note this overlaps with the legacy TK00 at 0xC2 -- which is
    # fine because the new TK00 at 0xA6 is read first and consumes the
    # bytes up to its varsize boundary; the importer then walks forward
    # from 0xA6 + 0x40 = 0xE6 which is past the legacy TK00 area.
    pre[0xD8] = 0xF4
    e  = note_v0xa6(0, 0, 0, fv=2, pitch_offset=0)   # C4 -> with key=-12 -> 48
    e += end_marker()
    body = b'MEAS' + struct.pack('<I', len(e)) + mhdr_a6(4, 4) + e
    body += b'MEAS' + struct.pack('<I', 2) + mhdr_a6(4, 4) + b'\xff\xff'
    return bytes(pre) + body + SKELETON_POST


# ===========================================================================
# Helpers for crafting v0xA6 fixtures at the proper on-disk offsets.
# Real Encore 2.x files have TK blocks at strides of 0x40 starting at
# 0xA6 (right after the 174-byte header). The bazo skeleton has a single
# v0xC4-style TK at 0xC2; we overlay a v0xA6 TK block on top of those
# bytes (and beyond) so the importer sees the proper layout.
# ===========================================================================
def _patch_v0xa6_tk(pre, tk_idx, key_semitones, instrument_byte=0x40):
    pre = bytearray(pre)
    pos = 0xA6 + tk_idx * 0x40
    name = f'TK{tk_idx:02d}'.encode('ascii')
    assert len(name) == 4, name
    pre[pos:pos + 4] = name
    struct.pack_into('<I', pre, pos + 4, 0x40)
    content_start = pos + 8
    # Zero the 56-byte content area
    for i in range(content_start, pos + 0x40 + 8):
        pre[i] = 0
    pre[content_start + 42] = key_semitones & 0xFF
    pre[content_start + 48] = tk_idx & 0xFF      # staff index per boda layout
    pre[content_start + 52] = instrument_byte    # MIDI program-like byte
    return bytes(pre)


# ===========================================================================
# importer_v0xa6_boda_like.enc
# Comprehensive v0xA6 fixture that bundles every failure mode the boda.enc
# real-world file exercised. One synthetic, one IT test, every patch in
# the v0xA6 chain (`d320f1e43e` header end, `7b0b87c295` artic-byte zero,
# `4a25465e0d` MIDI pitch at +11, `5d8d355adc` duplicate REST dedupe,
# `ba4e0d5aab` tuplet byte at +7) is exercised end-to-end.
#
# Layout: 4 TK blocks at v0xA6 strides starting at 0xA6, instrument
# count = 4. Keys mirror boda's rondalla: [0, 0, -12, -12] (Bandurria 1,
# Bandurria 2, Laud, Bajo). A single 3/8 measure carries:
#   - Staff 0 (B1, Key=0): eighth + 2 sixteenth-triplet groups -- the
#                          m131 pattern.
#   - Staff 1 (B2, Key=0): three eighth notes (rest + 2).
#   - Staff 2 (Laud, Key=-12): duplicate REST at tick 0 + 2 eighth notes
#                              -- the m107 pattern.
#   - Staff 3 (Bajo, Key=-12): three eighth notes.
# ===========================================================================
def gen_v0xa6_boda_like():
    pre = bytearray(set_chumagio(0xA6))
    pre[0x32] = 4   # instrumentCount = 4
    pre = bytes(pre)
    pre = _patch_v0xa6_tk(pre, 0,   0, instrument_byte=0x43)
    pre = _patch_v0xa6_tk(pre, 1,   0, instrument_byte=0x29)
    pre = _patch_v0xa6_tk(pre, 2, -12, instrument_byte=0x2A)
    pre = _patch_v0xa6_tk(pre, 3, -12, instrument_byte=0x19)

    # 3/8 measure header (v0xA6 layout = 0x1A bytes).
    def mhdr_a6(tsNum, tsDen, bpm=100):
        h = bytearray(0x1A)
        struct.pack_into('<H', h, 0, bpm)
        beatTicks = 120
        durTicks  = beatTicks * tsNum
        struct.pack_into('<HH', h, 4, beatTicks, durTicks)
        h[8], h[9] = tsNum, tsDen
        return bytes(h)

    # v0xA6 NOTE: 20-byte slot, MIDI pitch at +11, tuplet byte at +7.
    def note(tick, staff, fv, midi, tup=0):
        d = bytearray(7)
        d[0] = 10           # size
        d[1] = staff & 0x3F
        d[2] = fv
        d[4] = tup          # tuplet byte at element +7
        pad = bytearray(10)
        pad[1] = midi & 0xFF   # MIDI pitch at element +11
        return struct.pack('<H', tick) + bytes([0x90 | 0]) + bytes(d) + bytes(pad)

    # v0xA6 REST: 14-byte slot, size=7.
    def rest(tick, staff, fv):
        d = bytearray(11)
        d[0] = 7
        d[1] = staff & 0x3F
        d[2] = fv
        return struct.pack('<H', tick) + bytes([0x80 | 0]) + bytes(d)

    e  = b''
    # Staff 0: 8th + 2 triplet groups of 3 sixteenths each (m131 pattern).
    e += note(  0, 0, fv=4, midi=88)                           # E6 8th
    e += note(120, 0, fv=5, midi=88, tup=0x32)                 # E6 16th triplet
    e += note(160, 0, fv=5, midi=89, tup=0x32)                 # F6
    e += note(200, 0, fv=5, midi=88, tup=0x32)                 # E6
    e += note(240, 0, fv=5, midi=86, tup=0x32)                 # D6
    e += note(280, 0, fv=5, midi=88, tup=0x32)                 # E6
    e += note(320, 0, fv=5, midi=86, tup=0x32)                 # D6

    # Staff 1: rest + 2 eighth notes.
    e += rest(  0, 1, fv=4)
    e += note(120, 1, fv=4, midi=76)                           # E5
    e += note(240, 1, fv=4, midi=77)                           # F5

    # Staff 2 (Key=-12): duplicate REST at tick 0 + 2 eighth notes (m107).
    e += rest(  0, 2, fv=4)
    e += rest(  0, 2, fv=4)   # duplicate -- importer must collapse
    e += note(120, 2, fv=4, midi=76)   # binary 76 -> m_pitch 64 (E4)
    e += note(240, 2, fv=4, midi=77)   # -> 65 (F4)

    # Staff 3 (Key=-12): three eighth notes.
    e += note(  0, 3, fv=4, midi=57)   # -> 45 (A2)
    e += note(120, 3, fv=4, midi=60)   # -> 48 (C3)
    e += note(240, 3, fv=4, midi=64)   # -> 52 (E3)

    e += end_marker()

    body = b'MEAS' + struct.pack('<I', len(e)) + mhdr_a6(3, 8) + e
    body += b'MEAS' + struct.pack('<I', 2) + mhdr_a6(3, 8) + b'\xff\xff'
    return pre + body + SKELETON_POST


def gen_v0xa6_key_transposition():
    def mhdr_a6(tsNum, tsDen, bpm=100):
        h = bytearray(0x1A)
        struct.pack_into('<H', h, 0, bpm)
        beatTicks = 240
        durTicks  = beatTicks * tsNum * 4 // tsDen
        struct.pack_into('<HH', h, 4, beatTicks, durTicks)
        h[8], h[9] = tsNum, tsDen
        return bytes(h)
    pre = bytearray(set_chumagio(0xA6))
    # Patch instrument 0's Key byte to -12 ("Octave Lower"). Offset
    # 244 = 0xF4 = TK00 content (at file 0xCA) + 42.
    pre[244] = 0xF4
    # One whole note (pitch_offset = 0 -> C4 = 60) in instrument 0.
    # With Key = -12 the imported m_pitch should be 48 (C3).
    e  = note_v0xa6(0, 0, 0, fv=2, pitch_offset=0)
    e += end_marker()
    body = b'MEAS' + struct.pack('<I', len(e)) + mhdr_a6(4, 4) + e
    body += b'MEAS' + struct.pack('<I', 2) + mhdr_a6(4, 4) + b'\xff\xff'
    return bytes(pre) + body + SKELETON_POST


# ===========================================================================
# importer_v0xa6_duplicate_rest_collapse.enc
# v0xA6 occasionally stores two byte-identical REST elements back-to-back
# at the same tick / staff / voice / faceValue. Encore renders the pair
# as a SINGLE rest; the importer used to consume both and push cumTick
# one beat past the measure end, spilling the next note into a second
# MuseScore voice (a "voice 4" overlap in a 3/8 bar). The fixture
# reproduces the exact pattern (two 8th rests at tick 0, then two
# 8th notes at ticks 120 and 240). The imported measure must hold
# exactly three elements on voice 0 (rest, note, note); voice 1 must
# stay empty.
# ===========================================================================
# ===========================================================================
# importer_v0xa6_triplet_byte_at_offset_7.enc
# v0xA6 NOTE slots store the explicit tuplet byte at element offset +7
# (where v0xC4 has grace2; v0xC4 puts tuplet at +13 which is padding in
# v0xA6). With the wrong offset the importer never sees the 0x32 (3:2)
# marker on real Encore 2.x triplets, the notes collapse to plain
# sixteenths and excess notes spill into a second voice. The fixture
# is a 3/8 measure with 6 sixteenth notes carrying tuplet = 0x32 at
# +7; the imported measure must hold a single voice with two
# Tuplet(3:2) groups.
# ===========================================================================
def gen_v0xa6_triplet_byte_at_offset_7():
    def mhdr_a6(tsNum, tsDen, bpm=100):
        h = bytearray(0x1A)
        struct.pack_into('<H', h, 0, bpm)
        beatTicks = 120
        durTicks  = beatTicks * tsNum
        struct.pack_into('<HH', h, 4, beatTicks, durTicks)
        h[8], h[9] = tsNum, tsDen
        return bytes(h)
    # Build a v0xA6 NOTE with size=10 carrying the explicit-triplet byte
    # at offset +7 (= d[4]) and the MIDI pitch at offset +11 (= pad[1]).
    def note_a6_triplet(tick, fv, midi):
        d = bytearray(7)
        d[0] = 10        # size
        d[1] = 0         # staffIdx
        d[2] = fv        # face value
        d[4] = 0x32      # tuplet = 3:2 at element +7
        pad = bytearray(10)
        pad[1] = midi    # MIDI pitch at element +11
        return struct.pack('<H', tick) + bytes([0x90]) + bytes(d) + bytes(pad)
    # 6 triplet 16ths at ticks 0, 40, 80, 120, 160, 200. Encore tick math:
    # triplet 16th advance = 60 (face) * 2/3 = 40 ticks.
    e  = b''
    e += note_a6_triplet(  0, fv=5, midi=64)
    e += note_a6_triplet( 40, fv=5, midi=65)
    e += note_a6_triplet( 80, fv=5, midi=64)
    e += note_a6_triplet(120, fv=5, midi=62)
    e += note_a6_triplet(160, fv=5, midi=64)
    e += note_a6_triplet(200, fv=5, midi=62)
    e += end_marker()
    pre  = set_chumagio(0xA6)
    # 2/8 measure: 2 8ths = 240 ticks. 6 triplet 16ths = 240 ticks. Fits.
    body = b'MEAS' + struct.pack('<I', len(e)) + mhdr_a6(2, 8) + e
    body += b'MEAS' + struct.pack('<I', 2) + mhdr_a6(2, 8) + b'\xff\xff'
    return pre + body + SKELETON_POST


def gen_v0xa6_note_fermata_size11():
    def mhdr_a6(tsNum, tsDen, bpm=100):
        h = bytearray(0x1A)
        struct.pack_into('<H', h, 0, bpm)
        beatTicks = 120   # 8th in x/8 meters
        durTicks  = beatTicks * tsNum
        struct.pack_into('<HH', h, 4, beatTicks, durTicks)
        h[8], h[9] = tsNum, tsDen
        return bytes(h)
    # v0xA6 NOTE with size=11 (slot = 22 bytes): same layout as size=10 (pitch at +11,
    # tuplet at +7) plus a single articulation byte at +18. 0x20 there is a fermata-above.
    # A decoy 0x7F sits at +15 (where the v0xC4 base read would wrongly pick up the pitch),
    # so a reader that does not special-case size 11 yields MIDI 127 and no fermata.
    def note_a6_fermata(tick, fv, midi):
        d = bytearray(7)
        d[0] = 11        # size -> slot = 22 bytes
        d[1] = 0         # staffIdx
        d[2] = fv        # face value
        # d[4] (= element +7) stays 0 -> tuplet 0, so the fermata is not suppressed
        pad = bytearray(12)   # 3 prefix + 7 d + 12 pad = 22 = size*2
        pad[1] = midi    # element +11 -> real MIDI pitch
        pad[5] = 0x7F    # element +15 -> decoy the unfixed base read would take
        pad[8] = 0x20    # element +18 -> fermata-above articulation
        return struct.pack('<H', tick) + bytes([0x90]) + bytes(d) + bytes(pad)
    e  = note_a6_fermata(  0, fv=4, midi=64)   # E4 + fermata
    e += note_a6_fermata(120, fv=4, midi=67)   # G4 + fermata
    e += end_marker()
    pre  = set_chumagio(0xA6)
    body = b'MEAS' + struct.pack('<I', len(e)) + mhdr_a6(2, 8) + e
    body += b'MEAS' + struct.pack('<I', 2) + mhdr_a6(2, 8) + b'\xff\xff'
    return pre + body + SKELETON_POST


def gen_v0xa6_duplicate_rest_collapse():
    def mhdr_a6(tsNum, tsDen, bpm=100):
        h = bytearray(0x1A)
        struct.pack_into('<H', h, 0, bpm)
        beatTicks = 120   # 8th in x/8 meters
        durTicks  = beatTicks * tsNum
        struct.pack_into('<HH', h, 4, beatTicks, durTicks)
        h[8], h[9] = tsNum, tsDen
        return bytes(h)
    # Element layout (v0xA6 REST = size 7, slot = 14 bytes; NOTE = size 10,
    # slot = 20 bytes). Two duplicate 8th rests at tick 0 + two 8th notes.
    def rest_a6(tick):
        d = bytearray(11)
        d[0] = 7      # size
        d[1] = 0      # staffIdx
        d[2] = 4      # fv = 8th
        return struct.pack('<H', tick) + bytes([0x80]) + bytes(d)
    e  = rest_a6(0)
    e += rest_a6(0)                       # exact duplicate -- importer must drop
    e += note_v0xa6(120, 0, 0, fv=4, pitch_offset=4)   # MIDI 64 = E4
    e += note_v0xa6(240, 0, 0, fv=4, pitch_offset=4)
    e += end_marker()
    pre  = set_chumagio(0xA6)
    body = b'MEAS' + struct.pack('<I', len(e)) + mhdr_a6(3, 8) + e
    body += b'MEAS' + struct.pack('<I', 2) + mhdr_a6(3, 8) + b'\xff\xff'
    return pre + body + SKELETON_POST


def gen_v0xa6_no_spurious_tremolo():
    def mhdr_a6(tsNum, tsDen, bpm=100):
        h = bytearray(0x1A)
        struct.pack_into('<H', h, 0, bpm)
        beatTicks = 240
        durTicks  = beatTicks * tsNum * 4 // tsDen
        struct.pack_into('<HH', h, 4, beatTicks, durTicks)
        h[8], h[9] = tsNum, tsDen
        return bytes(h)
    # Two quarter notes back-to-back; the second's fv=3 byte lands on the
    # first's articulationUp slot and triggers the tremolo dispatcher.
    e  = note_v0xa6(  0, 0, 0, fv=3, pitch_offset=0)   # C4
    e += note_v0xa6(240, 0, 0, fv=3, pitch_offset=4)   # E4
    e += end_marker()
    pre  = set_chumagio(0xA6)
    body = b'MEAS' + struct.pack('<I', len(e)) + mhdr_a6(2, 4) + e
    body += b'MEAS' + struct.pack('<I', 2) + mhdr_a6(2, 4) + b'\xff\xff'
    return pre + body + SKELETON_POST


def gen_v0c4_gap_snap_eighth_meter():
    elems = (
        note_v0c4(tick=240, voice=0, staffIdx=0, fv=3, pitch=72)
      + end_marker()
    )
    custom = [(meas_hdr(3, 8, beatTicks=120), elems)]
    return assemble(0xC4, custom, fill_ts=(3, 8))


def gen_v0c4_bass_guitar_transposing_clef():
    pre = _patch_tk00('Bass Guitar'.encode('utf-16-le') + b'\x00\x00')
    # MIDI program 34 (1-indexed) = Electric Bass picked, the bass-guitar
    # template's default channel program; helps the matcher tiebreak.
    pre = _patch_midi_program(pre, 0, 34)
    pre = _patch_key_transpose(pre, 0, -12)
    # Override the LINE staff 0 clef byte to F (= 0x01). Layout matches the
    # showByte-patch convention used in gen_v0c4_staff_hidden:
    #   LINE_POS + 8 (LINE header) + 13 (per-LINE skip) = staff entry start
    #   + 14 (EncLineStaffData::read skips 14 before clef) = clef byte.
    pre = bytearray(pre)
    line_pos = pre.find(b'LINE')
    assert line_pos > 0, "LINE block missing in skeleton"
    clef_off = line_pos + 8 + 13 + 14
    pre[clef_off] = 0x01    # EncClefType::F
    pre = bytes(pre)
    elems = (
        note_v0c4(tick=0, voice=0, staffIdx=0, fv=3, pitch=45)
      + end_marker()
    )
    body  = meas_block(meas_hdr(4, 4), elems)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def _set_staff_clef(pre, clef_byte):
    """Patch the LINE block's staff-0 clef byte to `clef_byte`."""
    pre = bytearray(pre)
    line_pos = pre.find(b'LINE')
    assert line_pos > 0, "LINE block missing in skeleton"
    clef_off = line_pos + 8 + 13 + 14
    pre[clef_off] = clef_byte & 0xFF
    return bytes(pre)


# ===========================================================================
# Clef-selection fixtures for binary-driven octave-clef rule.
# The rule: Encore_clef + Key == ±12  →  octave-decorated MuseScore clef
#           Encore_clef + Key non-octave →  plain clef (notes shift, not clef)
# Each fixture uses an unrecognised instrument name so template matching
# is skipped, proving the selection is purely binary-data-driven.
# ===========================================================================

def gen_v0c4_g_clef_8va_from_key():
    """G clef + Key=+12  →  G8_VA (octave higher)."""
    pre = _patch_tk00('UnknownClefTest'.encode('utf-16-le') + b'\x00\x00')
    pre = _patch_key_transpose(pre, 0, +12)
    # G clef is the skeleton default; no clef-byte patch needed.
    elems = note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    body = meas_block(meas_hdr(4, 4), elems)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_f_clef_8vb_from_key():
    """F clef + Key=-12  →  F8_VB (octave lower), no template required."""
    pre = _patch_tk00('UnknownClefTest'.encode('utf-16-le') + b'\x00\x00')
    pre = _patch_key_transpose(pre, 0, -12)
    pre = _set_staff_clef(pre, 0x01)   # EncClefType::F
    elems = note_v0c4(0, 0, 0, fv=3, pitch=48) + end_marker()
    body = meas_block(meas_hdr(4, 4), elems)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_f_clef_8va_from_key():
    """F clef + Key=+12  →  F_8VA (octave higher)."""
    pre = _patch_tk00('UnknownClefTest'.encode('utf-16-le') + b'\x00\x00')
    pre = _patch_key_transpose(pre, 0, +12)
    pre = _set_staff_clef(pre, 0x01)   # EncClefType::F
    elems = note_v0c4(0, 0, 0, fv=3, pitch=48) + end_marker()
    body = meas_block(meas_hdr(4, 4), elems)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# Instrument name matching: tokenizer and weak-match handling.
# ===========================================================================
def gen_v0c4_name_trailing_number_stripped():
    """Name "Trumpet-1": the trailing "-1" (separator + ordinal) must be stripped so the
    base name "Trumpet" matches the Trumpet template. MIDI 41 (Violin) only matters if the
    name match fails, so resolving to Trumpet proves the trailing number was removed."""
    pre = _patch_tk00('Trumpet-1'.encode('utf-16-le') + b'\x00\x00')
    pre = _patch_midi_program(pre, 0, 41)   # Violin (the wrong answer if name matching fails)
    body = meas_block(meas_hdr(4, 4), end_marker())
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_name_split_on_separator():
    """Name "French-Horn": words must split on '-' (not only spaces) so the needle "horn"
    matches the Horn template. MIDI 41 (Violin) is the wrong answer if splitting fails."""
    pre = _patch_tk00('French-Horn'.encode('utf-16-le') + b'\x00\x00')
    pre = _patch_midi_program(pre, 0, 41)   # Violin (the wrong answer if the split fails)
    body = meas_block(meas_hdr(4, 4), end_marker())
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def _set_prec(post, paper, orient=1, scale=100, ansi=False, length=None, width=None):
    """Return SKELETON_POST with its PREC (DEVMODE) page fields overridden.
    The skeleton PREC is a Unicode DEVMODE (name 64 bytes); ansi=True instead replaces the
    block with a 32-byte-name ANSI DEVMODE so both parser variants are exercised."""
    post = bytearray(post)
    o = post.find(b'PREC')
    vs = struct.unpack('<I', post[o + 4:o + 8])[0]
    c = o + 8
    if ansi:
        base = 32
        content = bytearray(64)
        content[0:8] = b'AnsiPrn\x00'
        struct.pack_into('<h', content, base + 12, orient)
        struct.pack_into('<h', content, base + 14, paper)
        struct.pack_into('<h', content, base + 20, scale)
        newblk = b'PREC' + struct.pack('<I', len(content)) + bytes(content)
        return bytes(post[:o]) + newblk + bytes(post[o + 8 + vs:])
    base = 64
    struct.pack_into('<h', post, c + base + 12, orient)
    struct.pack_into('<h', post, c + base + 14, paper)
    if length is not None:
        struct.pack_into('<h', post, c + base + 16, length)
    if width is not None:
        struct.pack_into('<h', post, c + base + 18, width)
    struct.pack_into('<h', post, c + base + 20, scale)
    return bytes(post)


def gen_v0c4_prec_page_letter():
    """PREC (Unicode DEVMODE) dmPaperSize=1 (Letter): page size must come from PREC, not WINI."""
    pre = _patch_tk00('PrecLetter'.encode('utf-16-le') + b'\x00\x00')
    body = meas_block(meas_hdr(4, 4), end_marker())
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + _set_prec(SKELETON_POST, paper=1)   # DMPAPER_LETTER


def gen_v0c4_prec_page_ansi_a3():
    """PREC (ANSI DEVMODE) dmPaperSize=8 (A3): exercises the ANSI variant and page size."""
    pre = _patch_tk00('PrecAnsiA3'.encode('utf-16-le') + b'\x00\x00')
    body = meas_block(meas_hdr(4, 4), end_marker())
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + _set_prec(SKELETON_POST, paper=8, ansi=True)   # DMPAPER_A3


def gen_v0c4_prec_landscape_no_wini():
    """PREC A4 landscape (dmPaperSize=9, dmOrientation=2) with NO WINI block. The page becomes
    wider than the default portrait, so MuseScore's default portrait printable width would leave
    the extra width as a lopsided right margin (~4"). The importer must recompute the printable
    width so the right margin equals the left. WINI is the last block in SKELETON_POST, so
    truncating before it drops the margins (PREC/TITL/TEXT stay intact)."""
    pre = _patch_tk00('LandNoWini'.encode('utf-16-le') + b'\x00\x00')
    body = meas_block(meas_hdr(4, 4), end_marker())
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    post = _set_prec(SKELETON_POST, paper=9, orient=2)   # A4 landscape
    post_no_wini = post[:22964]                          # drop the trailing WINI block
    return pre + body + post_no_wini


def _set_wini(post, top, left, bottom_edge, right_edge):
    """Return SKELETON_POST with the WINI margin int32 fields overridden:
    @24=top, @28=left, @32=bottomEdge, @36=rightEdge (see ENCORE_FORMAT.md §WINI)."""
    post = bytearray(post)
    o = post.find(b'WINI')
    c = o + 8
    struct.pack_into('<i', post, c + 24, top)
    struct.pack_into('<i', post, c + 28, left)
    struct.pack_into('<i', post, c + 32, bottom_edge)
    struct.pack_into('<i', post, c + 36, right_edge)
    return bytes(post)


def gen_v0c4_wini_large_margins_a3():
    """A3 (PREC paper=8) with large WINI margins in screen pixels (~84 dpi): top=176, left=209,
    bottomEdge=1191, rightEdge=752. Margins of ~2.1-2.5 inches must survive import, not be clamped
    to a tiny maximum. Mirrors a real A3 score saved with 2.1/2.5/2.7/2.3 inch margins."""
    pre = _patch_tk00('WiniA3Margins'.encode('utf-16-le') + b'\x00\x00')
    body = meas_block(meas_hdr(4, 4), end_marker())
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    post = _set_prec(SKELETON_POST, paper=8)            # A3 portrait, Unicode DEVMODE
    post = _set_wini(post, top=176, left=209, bottom_edge=1191, right_edge=752)
    return pre + body + post


def gen_v0c4_weak_name_defers_to_midi():
    """Name "Contrabass" + MIDI 59 (Tuba, program 58): the substring-only match to the treble
    "Contrabass Bugle" (which shares program 58) outranks the bass-clef GM instrument via the
    MIDI bonus. Because that name match is not exact, the importer must defer to the MIDI program
    and resolve to a bass-clef tuba. Mirrors the Spanish "Bajo" -> "Clarín contrabajo" case."""
    pre = _patch_tk00('Contrabass'.encode('utf-16-le') + b'\x00\x00')
    pre = _patch_midi_program(pre, 0, 59)   # 1-indexed GM Tuba (program 58)
    pre = _patch_key_transpose(pre, 0, 0)
    body = meas_block(meas_hdr(4, 4), end_marker())
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_non_octave_key_keeps_clef():
    """G clef + Key=-7 (perfect fifth, not an octave)  →  plain G."""
    pre = _patch_tk00('UnknownClefTest'.encode('utf-16-le') + b'\x00\x00')
    pre = _patch_key_transpose(pre, 0, -7)
    # G clef is the default; key -7 has no matching octave-decorated variant.
    elems = note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    body = meas_block(meas_hdr(4, 4), elems)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_g_clef_key0_stays_plain():
    """G clef + Key=0 (no transposition)  →  plain G (identity case)."""
    pre = _patch_tk00('UnknownClefTest'.encode('utf-16-le') + b'\x00\x00')
    pre = _patch_key_transpose(pre, 0, 0)
    # G clef is the default; Key=0 triggers early return.
    elems = note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    body = meas_block(meas_hdr(4, 4), elems)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_c_clef_key_keeps_clef():
    """C3L clef + Key=-12  →  C3 (C clefs have no octave variant)."""
    pre = _patch_tk00('UnknownClefTest'.encode('utf-16-le') + b'\x00\x00')
    pre = _patch_key_transpose(pre, 0, -12)
    pre = _set_staff_clef(pre, 0x02)   # EncClefType::C3L
    elems = note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    body = meas_block(meas_hdr(4, 4), elems)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_perc_clef_key_keeps_clef():
    """PERC clef + Key=-12  →  PERC (percussion has no octave variant)."""
    pre = _patch_tk00('UnknownClefTest'.encode('utf-16-le') + b'\x00\x00')
    pre = _patch_key_transpose(pre, 0, -12)
    pre = _set_staff_clef(pre, 0x07)   # EncClefType::PERC
    elems = note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    body = meas_block(meas_hdr(4, 4), elems)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# text_satb_short_names_voice4_lyrics.enc
# Four-instrument v0xC4 file modelled on SATB choir scores: parts are named
# with 1-char labels (S, C, T, B) that fall below the 4-char minimum for the
# name+MIDI matcher. The matcher returns nullptr and the MIDI fallback is
# also skipped (because short labels usually appear in compact-TK files
# where the MIDI byte is unreliable), so every part falls through to the
# neutral Grand Piano template. The original Encore label survives as the
# part's long name.
# The 4th staff (bass) carries the voice >= VOICES path: NOTE elements
# written with voice = 4 in the typeVoice byte, plus matching LYRIC
# elements on voice = 0 at the same ticks. The importer maps voice=4 to
# voice 0 of the same staff, letting the lyric-attachment pass anchor onto
# the chords.
# ===========================================================================
def gen_v0c4_satb_short_names_voice4_lyrics():
    pre = bytearray(SKELETON_PRE)
    pre[0x32] = 4                  # instrumentCount = 4
    pre = bytes(pre)
    pre = _patch_instrument_name(pre, 0, 'S')
    pre = _patch_instrument_name(pre, 1, 'C')
    pre = _patch_instrument_name(pre, 2, 'T')
    pre = _patch_instrument_name(pre, 3, 'B')
    # Soprano: 4 quarters E5 F5 G5 A5
    e_satb  = note_v0c4(  0, 0, 0, fv=3, pitch=76)
    e_satb += note_v0c4(240, 0, 0, fv=3, pitch=77)
    e_satb += note_v0c4(480, 0, 0, fv=3, pitch=79)
    e_satb += note_v0c4(720, 0, 0, fv=3, pitch=81)
    # Contralto: 4 quarters C5 D5 E5 F5
    e_satb += note_v0c4(  0, 0, 1, fv=3, pitch=72)
    e_satb += note_v0c4(240, 0, 1, fv=3, pitch=74)
    e_satb += note_v0c4(480, 0, 1, fv=3, pitch=76)
    e_satb += note_v0c4(720, 0, 1, fv=3, pitch=77)
    # Tenor: 4 quarters A3 B3 C4 D4
    e_satb += note_v0c4(  0, 0, 2, fv=3, pitch=57)
    e_satb += note_v0c4(240, 0, 2, fv=3, pitch=59)
    e_satb += note_v0c4(480, 0, 2, fv=3, pitch=60)
    e_satb += note_v0c4(720, 0, 2, fv=3, pitch=62)
    # Bass: 4 quarters on voice=4 (out-of-range) + lyrics on voice 0
    e_satb += note_v0c4_voice4(  0, 3, fv=3, pitch=48)
    e_satb += lyric_v0c4(  0, 0, 3, 'Lo')
    e_satb += note_v0c4_voice4(240, 3, fv=3, pitch=50)
    e_satb += lyric_v0c4(240, 0, 3, 'rem')
    e_satb += note_v0c4_voice4(480, 3, fv=3, pitch=52)
    e_satb += lyric_v0c4(480, 0, 3, 'ip')
    e_satb += note_v0c4_voice4(720, 3, fv=3, pitch=53)
    e_satb += lyric_v0c4(720, 0, 3, 'sum')
    e_satb += end_marker()
    body  = meas_block(meas_hdr(4, 4), e_satb)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# importer_slur_pixel_span.enc
# Encore stores a SLURSTART ornament with two layout-x fields: `xoffset`
# (slur start anchor) and `xoffset2` (slur end anchor). Each is offset
# from the underlying note's `xoffset` by a per-element constant, but the
# DIFFERENCE (xoffset2 - xoffset) matches the pixel distance between the
# first and last covered notes. The post-pass resolves the end tick by
# finding the note in the start measure whose xoffset is closest to
# (firstNote.xoffset + slurXoffset2 - slurXoffset). Without the pixel
# span heuristic the importer anchored slurs on the last ChordRest of
# the alMezuro target measure and extended them far beyond the user's
# intent (m1 instr 2 of a real legacy file: 3-note slur grew to cover 8
# notes).
#
# Fixture: 8 quarter notes in 4/4 with a SLURSTART at tick=0. Each note
# carries a synthetic xoffset (30, 50, 70, 90, 110, 130, 150, 170 -- a
# stable +20 spacing). The slur's xoffset=70 and xoffset2=110 give a
# pixel span of 40; mapped onto the note ruler that targets a note 40
# units past the first one (= xoffset 30+40 = 70 = note 3 at tick=480).
# ===========================================================================
def note_v0c4_xoff(tick, voice, staffIdx, fv, pitch, xoff):
    """v0xC4 note with an explicit u8 xoffset at element +10.

    body offsets relative to element start:
      d[0]=size, d[1]=staffIdx, d[2]=fv, d[3]=grace1, d[4]=grace2,
      d[5..6]=skip, d[7]=xoffset, d[8]=skip, d[9]=position, d[10]=tuplet,
      d[11]=dotControl, d[12]=pitch, ...
    Element offsets are body offsets + 3 (the leading tick u16 + typeVoice byte).
    """
    d = bytearray(25)
    d[0] = 28
    d[1] = staffIdx & 0x3F
    d[2] = fv
    d[7] = xoff & 0xFF        # body offset 7 = element offset 10 (xoffset)
    d[10] = 0                 # tuplet at +13
    d[12] = pitch
    return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)


def gen_v0c4_slur_pixel_span():
    # Slur with xoffset=70 and xoffset2=110 (pixel_span=40). First-note
    # xoffset=30, so target end xoffset = 30+40=70 -> note at tick=480.
    e  = ornament_v0c4(0, 0, 0, tipo=0x21, xoffset=70, xoffset2=110, alMezuro=0)
    e += note_v0c4_xoff(  0, 0, 0, fv=3, pitch=60, xoff=30)
    e += note_v0c4_xoff(240, 0, 0, fv=3, pitch=60, xoff=50)
    e += note_v0c4_xoff(480, 0, 0, fv=3, pitch=60, xoff=70)
    e += note_v0c4_xoff(720, 0, 0, fv=3, pitch=60, xoff=90)
    e += end_marker()
    custom = [(meas_hdr(4, 4), e)]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# importer_slur_pixel_span_6_8.enc
# BUG FIX: in compound meters (6/8), the slur resolver used
# beatTicks × timeSigDen as the "whole-note tick count" (wt), giving
# 240×8=1920 instead of the correct durTicks×timeSigDen/timeSigNum=960.
# This caused startEncTick to be computed wrong, making the resolver look
# for firstNoteXoff at the WRONG note, causing slurs to end too late.
#
# Example: SLUR at enc_tick=120 in a 6/8 measure (dur=720, wt=960).
#   Wrong: startEncTick = 1×240×8/8 = 240 → firstNoteXoff from note@240
#          targetEndXoff = 50+20=70 → end note at tick=360 (note 4).
#   Right: startEncTick = 1×960/8 = 120 → firstNoteXoff from note@120
#          targetEndXoff = 30+20=50 → end note at tick=240 (note 3). ✓
#
# Fixture: 6/8 measure (dur=720, beatTicks=240, wt=960). Six 8th notes at
# ticks 0,120,240,360,480,600 with xoffsets 10,30,50,70,90,110.
# SLUR at tick=120 (note 2): xoffset=30, xoffset2=50, pixelSpan=20.
# targetEndXoff = firstNoteXoff(30) + 20 = 50 → note at tick=240. ✓
# ===========================================================================
def gen_v0c4_slur_pixel_span_6_8():
    e  = ornament_v0c4(120, 0, 0, tipo=0x21, xoffset=30, xoffset2=50, alMezuro=0)
    e += note_v0c4_xoff(  0, 0, 0, fv=4, pitch=60, xoff=10)  # 8th C4
    e += note_v0c4_xoff(120, 0, 0, fv=4, pitch=62, xoff=30)  # 8th D4 (slur starts)
    e += note_v0c4_xoff(240, 0, 0, fv=4, pitch=64, xoff=50)  # 8th E4 (slur ends)
    e += note_v0c4_xoff(360, 0, 0, fv=4, pitch=65, xoff=70)
    e += note_v0c4_xoff(480, 0, 0, fv=4, pitch=67, xoff=90)
    e += note_v0c4_xoff(600, 0, 0, fv=4, pitch=69, xoff=110)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(6, 8), e)], fill_ts=(6, 8))


# ===========================================================================
# importer_slur_xoffset_unsigned.enc
# BUG FIX: SLURSTART xoffset is stored as quint8 but the struct field is
# qint8. Values > 127 wrap to negative. Using the signed value in the
# pixel-span formula gives a huge spurious span; treating it as unsigned
# gives the correct 1-2 note span.
#
# Fixture: 4/4 with 4 quarter notes. SLURSTART at tick=240 (note 2).
# xoffset=138 (0x8A, read as -118 signed). xoffset2=149. Note 2 xoff=88.
# Unsigned: span=149-138=11. target=88+11=99 → note 3 (xoff=99). ✓
# Signed:   span=149-(-118)=267. target=88+267=355 → no note near 355.
# ===========================================================================
def gen_v0c4_slur_xoffset_unsigned():
    # xoffset=0x8A stored via ornament_v0c4: d[10]=0x8A & 0xFF = 0x8A
    e  = note_v0c4_xoff(  0, 0, 0, fv=3, pitch=60, xoff=60)   # note 1
    e += ornament_v0c4(240, 0, 0, tipo=0x21, xoffset=0x8A, xoffset2=149, alMezuro=0)
    e += note_v0c4_xoff(240, 0, 0, fv=3, pitch=62, xoff=88)   # note 2 (slur start)
    e += note_v0c4_xoff(480, 0, 0, fv=3, pitch=64, xoff=99)   # note 3 (slur end)
    e += note_v0c4_xoff(720, 0, 0, fv=3, pitch=65, xoff=120)  # note 4 (too far)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# importer_dyn_snap_back_by_xoffset.enc
# Encore tags a dynamic ORN at the CHORD-REST AT OR AFTER its rendered
# position but records the visible layout x in the ornament's xoffset.
# When the ornament's xoffset is smaller than the tagged chord-rest's
# xoffset, Encore visually pulls the glyph back under the previous
# chord-rest. The importer used to plant the dynamic on the tagged
# tick, so users saw it one chord later than in Encore.
# Fixture: rest + note A + note B in 4/4. The dynamic ORN is tagged at
# note B's tick but its xoffset matches note A's region. The post-fix
# importer must place the dynamic at note A's tick.
# ===========================================================================
def gen_v0c4_dyn_snap_back_by_xoffset():
    # Rest (eighth) at tick 0 xoff=14.
    # Note A (eighth) at tick 120 xoff=36.
    # Note B (eighth) at tick 240 xoff=61.
    # Dynamic MF tagged at tick=240 with xoffset=46 (between rest@14 and
    # noteB@61, but less than noteB.xoffset so the dynamic snaps back to
    # noteA@120).
    e  = ornament_v0c4(240, 0, 0, tipo=0x84, xoffset=46)   # DYN_MF
    e += rest_v0c4(0, 0, 0, fv=4)   # eighth rest
    # Note helper writes xoffset at body offset 7 (element offset 10).
    e += note_v0c4_xoff(120, 0, 0, fv=4, pitch=60, xoff=36)
    e += note_v0c4_xoff(240, 0, 0, fv=4, pitch=60, xoff=61)
    e += end_marker()
    custom = [(meas_hdr(4, 4), e)]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# importer_wedge_snap_back_by_xoffset.enc
# Same snap-back convention applies to WEDGESTART. A half note + eighth
# pair where the binary tags the wedge at the eighth's tick but the
# wedge's xoffset falls inside the half note's region. The post-fix
# importer must place the hairpin start at the half note's tick.
# ===========================================================================
def gen_v0c4_wedge_snap_back_by_xoffset():
    # Half note (fv=3) at tick 0 xoff=23.
    # Eighth (fv=4) at tick 480 xoff=125.
    # WEDGE tagged at tick=480 xoff=80 (less than 125, so snap back to
    # the half note at tick 0 whose xoff=23 satisfies xoff <= 80).
    e  = note_v0c4_xoff(  0, 0, 0, fv=3, pitch=65, xoff=23)
    e += note_v0c4_xoff(480, 0, 0, fv=4, pitch=65, xoff=125)
    e += ornament_v0c4(480, 0, 0, tipo=0x1D, xoffset=80, xoffset2=200,
                       alMezuro=0, speguleco=0x02)
    e += end_marker()
    custom = [(meas_hdr(4, 4), e)]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# importer_dyn_displaced_to_staff_above.enc
# A dynamic ORN with yoffset > 0 was drawn by the user above its stored
# staff, meaning it visually belongs to the staff ABOVE (staffIdx - 1).
# Encore stores it on staff N but renders it on staff N-1. The importer
# must reroute it so MuseScore places it on the correct instrument.
# Fixture: two adjacent staves; a DYN_MF on staff 1 (instr 2) with
# yoffset=+16. The imported Dynamic must appear on staff 0 (instr 1).
# ===========================================================================
def gen_v0c4_dyn_displaced_to_staff_above():
    # Set up 2-instrument score: patch instrCount=2 in the skeleton.
    pre = bytearray(set_chumagio(0xC4))
    pre[0x32] = 2   # instrumentCount = 2
    pre = bytes(pre)
    # Measure: MF on staff=1 with yoffset=16 (positive -> displaced up)
    # and a regular note on staff=0 so both staves have content.
    # ornament_v0c4(tick, voice, staffIdx, tipo, yoffset=...)
    e  = ornament_v0c4(0, 0, 1, tipo=0x84, yoffset=16)   # MF, staff 1, positive yoffset
    e += note_v0c4(0, 0, 0, fv=3, pitch=60)               # note on staff 0
    e += note_v0c4(0, 0, 1, fv=3, pitch=64)               # note on staff 1
    e += note_v0c4(240, 0, 0, fv=3, pitch=60)
    e += note_v0c4(480, 0, 0, fv=3, pitch=60)
    e += note_v0c4(720, 0, 0, fv=3, pitch=60)
    e += end_marker()
    body  = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# importer_hairpin_barline_clamp.enc
# A cross-measure hairpin with alMezuro=1 and xoffset2 SMALLER than the
# first NOTE's xoffset in the target measure signals that Encore drew the
# hairpin ending right before any note content, i.e. at the bar line. The
# importer must clamp the endpoint to the target measure's start tick so
# the hairpin does not bleed visually into the next measure and overlap
# with a fresh hairpin starting there. Without this clamp the dim from
# m1 extends deep into m2 and overlaps the cresc that starts in m2.
# ===========================================================================
def gen_v0c4_hairpin_barline_clamp():
    # m1: dim hairpin starting at tick 0 with alMezuro=1.
    # Note xoffsets in m1: 30, 60, 90, 120 (using note_v0c4_xoff).
    # xoffset=15 (before first note), xoffset2=5 in m2.
    # m2 notes: xoffsets 30, 60, 90, 120. firstNoteXoff=30 > xoffset2=5
    # -> dim must end at m2.tick (bar line), not extend into m2.
    e1  = ornament_v0c4(0, 0, 0, tipo=0x1D, xoffset=15, alMezuro=1,
                        xoffset2=5, speguleco=0x03)   # dim
    e1 += note_v0c4_xoff(  0, 0, 0, fv=3, pitch=60, xoff=30)
    e1 += note_v0c4_xoff(240, 0, 0, fv=3, pitch=60, xoff=60)
    e1 += note_v0c4_xoff(480, 0, 0, fv=3, pitch=60, xoff=90)
    e1 += note_v0c4_xoff(720, 0, 0, fv=3, pitch=60, xoff=120)
    e1 += end_marker()
    # m2: cresc starting at tick 240
    e2  = ornament_v0c4(240, 0, 0, tipo=0x1D, xoffset=50, alMezuro=0,
                        xoffset2=180, speguleco=0x02)  # cresc
    e2 += note_v0c4_xoff(  0, 0, 0, fv=3, pitch=60, xoff=30)
    e2 += note_v0c4_xoff(240, 0, 0, fv=3, pitch=60, xoff=60)
    e2 += note_v0c4_xoff(480, 0, 0, fv=3, pitch=60, xoff=90)
    e2 += note_v0c4_xoff(720, 0, 0, fv=3, pitch=60, xoff=120)
    e2 += end_marker()
    custom = [(meas_hdr(4, 4), e1), (meas_hdr(4, 4), e2)]
    return assemble(0xC4, custom, fill_ts=(4, 4))



# ===========================================================================
# importer_v0xa6_grace_ongrid_snap_suppressed.enc
# The m87 pattern from real boda.enc: a leading grace (g1=0x20) followed
# by regular notes whose Encore ticks land exactly on the face grid of
# their own face value. Without the stolenTicks snap guard, the snap
# would fire for each of these notes (gap = stolen grace ticks = 30),
# inserting spurious rests BETWEEN the regular notes. The fix suppresses
# the snap for every displaced note in the measure. A trailing rest equal
# to the stolen ticks remains at the measure end (Encore ignores it).
# Binary: 8th(0→120), grace-32nd(120→150), 32nd(150→180), 16th(180→240),
#         8th(240→360). Notes 32nd/16th/8th are all on their face grids.
# ===========================================================================
def gen_v0xa6_grace_ongrid_snap_suppressed():
    def mhdr_a6(tsNum, tsDen, bpm=100):
        h = bytearray(0x1A)
        beatTicks = 120; durTicks = beatTicks * tsNum
        struct.pack_into('<H', h, 0, bpm)
        struct.pack_into('<HH', h, 4, beatTicks, durTicks)
        h[8], h[9] = tsNum, tsDen
        return bytes(h)
    def note_a6(tick, fv, pitch, grace1=0):
        d = bytearray(20)
        struct.pack_into('<H', d, 0, tick)
        d[2] = (9<<4)|0; d[3] = 10; d[4] = 0
        d[5] = fv; d[6] = grace1; d[11] = pitch
        return bytes(d)
    e  = note_a6(  0, fv=4, pitch=78)              # 8th regular
    e += note_a6(120, fv=6, pitch=80, grace1=0x20) # 32nd LEADING grace
    e += note_a6(150, fv=6, pitch=78, grace1=0x10) # 32nd regular (g1=0x10, fv=6 not > 6)
    e += note_a6(180, fv=5, pitch=77, grace1=0x10) # 16th regular
    e += note_a6(240, fv=4, pitch=78, grace1=0x10) # 8th regular
    e += b'\xff\xff'
    pre = bytearray(set_chumagio(0xA6))
    body = b'MEAS' + struct.pack('<I', len(e)) + mhdr_a6(3, 8) + e
    body += b'MEAS' + struct.pack('<I', 2) + mhdr_a6(3, 8) + b'\xff\xff'
    return bytes(pre) + body + SKELETON_POST


# ===========================================================================
# importer_v0xa6_inner_grace_group.enc
# The m57 pattern in real boda.enc: a v0xA6 grace group that contains
# BOTH a leading grace (g1=0x20 = APPOGGIATURA) AND an inner grace
# (g1=0x10, shorter than the leader). Without the inner-grace detection
# the inner note was treated as a regular note, producing a spurious
# gap rest before the leading grace AND a 64th regular chord (not grace)
# afterward. The corrupted structure triggered a SIGSEGV during
# MuseScore GUI layout. sanityCheck() must pass and no spurious rests
# may appear in the measure.
# Layout: 3/8 [8th, 32nd-leader-grace(g1=0x20), 64th-inner-grace(g1=0x10),
#              16th-regular, 8th-regular]
# ===========================================================================
def gen_v0xa6_inner_grace_group():
    """3/8 measure: 8th, grace-32nd(leader), grace-64th(inner), 16th, 8th."""
    def mhdr_a6(tsNum, tsDen, bpm=100):
        h = bytearray(0x1A)
        beatTicks = 120
        durTicks  = beatTicks * tsNum
        struct.pack_into('<H', h, 0, bpm)
        struct.pack_into('<HH', h, 4, beatTicks, durTicks)
        h[8], h[9] = tsNum, tsDen
        return bytes(h)
    def note_a6(tick, fv, pitch, grace1=0):
        d = bytearray(20)
        struct.pack_into('<H', d, 0, tick)
        d[2] = (9 << 4) | 0
        d[3] = 10
        d[4] = 0
        d[5] = fv
        d[6] = grace1
        d[11] = pitch
        return bytes(d)
    # Binary layout mirrors boda.enc m57:
    #   tick=0:   8th  (g1=0x00, regular)
    #   tick=120: 32nd (g1=0x20, LEADING grace, shorter fv=6)
    #   tick=150: 64th (g1=0x10, INNER grace,   shorter fv=7 > fv=6)
    #   tick=165: 16th (g1=0x90, regular; 0x90&0x30=0x10 but fv=5 < 7)
    #   tick=225: 8th  (g1=0x10, regular; fv=4 < 7)
    e  = note_a6(  0, fv=4, pitch=83)               # 8th regular
    e += note_a6(120, fv=6, pitch=84, grace1=0x20)  # 32nd LEADER grace
    e += note_a6(150, fv=7, pitch=83, grace1=0x10)  # 64th INNER grace
    e += note_a6(165, fv=5, pitch=82, grace1=0x90)  # 16th regular
    e += note_a6(225, fv=4, pitch=83, grace1=0x10)  # 8th regular
    e += b'\xff\xff'
    pre = bytearray(set_chumagio(0xA6))
    body = b'MEAS' + struct.pack('<I', len(e)) + mhdr_a6(3, 8) + e
    body += b'MEAS' + struct.pack('<I', 2) + mhdr_a6(3, 8) + b'\xff\xff'
    return bytes(pre) + body + SKELETON_POST


# ===========================================================================
# importer_v0xa6_grace_restores_face_value.enc
# Encore v0xA6 stores grace notes at their actual tick positions. This
# shifts subsequent real notes forward and shortens the last real note's
# gap to the measure end so that realDuration < faceValue. The fix in
# calculateRealDurations detects that the deficit (faceValue - rawGap)
# equals the total face value of grace notes in the group and restores
# the real note to its notated face duration. Without this a 3/8 measure
# of [8th, grace-32nd, 16th, 16th, 8th] produces [8th, grace, 16th,
# 16th, 16th, rest-16th] in MuseScore.
# ===========================================================================
def gen_v0xa6_grace_restores_face_value():
    """3/8 measure with [8th, grace-32nd, 16th, 16th, 8th] in v0xA6."""
    def mhdr_a6(tsNum, tsDen, bpm=100):
        h = bytearray(0x1A)
        beatTicks = 120
        durTicks  = beatTicks * tsNum
        struct.pack_into('<H', h, 0, bpm)
        struct.pack_into('<HH', h, 4, beatTicks, durTicks)
        h[8], h[9] = tsNum, tsDen
        return bytes(h)
    def note_a6(tick, fv, pitch, grace1=0):
        """v0xA6 NOTE: 20-byte slot. Pitch at +11, tuplet at +7, grace1 at +6."""
        d = bytearray(20)
        struct.pack_into('<H', d, 0, tick)
        d[2] = (9 << 4) | 0   # typeVoice
        d[3] = 10              # size
        d[4] = 0               # staff
        d[5] = fv              # faceValue
        d[6] = grace1          # grace1
        d[11] = pitch          # MIDI pitch at +11
        return bytes(d)
    # 3/8 measure: 8th(tick=0), 32ndGrace(tick=120), 16th(tick=150),
    # 16th(tick=210), 8th(tick=270)
    e  = note_a6(  0, fv=4, pitch=81)         # 8th, regular
    e += note_a6(120, fv=6, pitch=83, grace1=0x20)  # 32nd, appoggiatura grace
    e += note_a6(150, fv=5, pitch=81)         # 16th, regular
    e += note_a6(210, fv=5, pitch=80)         # 16th, regular
    e += note_a6(270, fv=4, pitch=81)         # 8th, regular (realDur=90 needs fix)
    e += b'\xff\xff'                           # end marker
    pre = bytearray(set_chumagio(0xA6))
    body = b'MEAS' + struct.pack('<I', len(e)) + mhdr_a6(3, 8) + e
    body += b'MEAS' + struct.pack('<I', 2) + mhdr_a6(3, 8) + b'\xff\xff'
    return bytes(pre) + body + SKELETON_POST


# ===========================================================================
# importer_hairpin_snapstart_at_barline.enc
# A WEDGESTART placed exactly at the measure's durTicks (tick==960 in 4/4)
# has no chord-rest element at that tick; the snap-start lambda used to
# return the default tick (= bar line of next measure, m2.tick) making
# the hairpin zero-span after bar-line clamping. The fix extends the
# backwards-scan to also fire when defaultCrXoff < 0 so the scan finds
# the last note of m1 by xoffset and anchors the start inside m1.
# Fixture: m1 has notes at xoffsets 30/60/90/120 (tick 0/240/480/720),
# a WEDGE at tick=960 xoffset=110 alMezuro=1, and a MF dynamic in m2.
# The hairpin must start at tick=720 (the latest note with xoff<=110).
# ===========================================================================
def gen_v0c4_hairpin_snapstart_at_barline():
    e1  = note_v0c4_xoff(  0, 0, 0, fv=3, pitch=60, xoff=30)
    e1 += note_v0c4_xoff(240, 0, 0, fv=3, pitch=60, xoff=60)
    e1 += note_v0c4_xoff(480, 0, 0, fv=3, pitch=60, xoff=90)
    e1 += note_v0c4_xoff(720, 0, 0, fv=3, pitch=60, xoff=120)
    # WEDGE at tick=960 (= durTicks, bar line), dim crossing into m2.
    # xoffset=110: snap-back in m1 finds latest note with xoff<=110.
    # Note at tick=480 has xoff=90 (<=110) and is the latest.
    # Note at tick=720 has xoff=120 (>110) so NOT eligible.
    # So snapped start = tick=480 (= m1 beat 3, Fraction(1,2)).
    e1 += ornament_v0c4(960, 0, 0, tipo=0x1D, xoffset=110, alMezuro=1,
                        xoffset2=10, speguleco=0x03)
    e1 += end_marker()
    # m2: MF at tick=240 xoffset=70 (> note@240 xoff=60 -> no snap-back;
    # MF stays at its tagged tick=240 = m2 + 1/4).
    e2  = ornament_v0c4(240, 0, 0, tipo=0x84, xoffset=70)   # DYN_MF
    e2 += note_v0c4_xoff(  0, 0, 0, fv=3, pitch=60, xoff=30)
    e2 += note_v0c4_xoff(240, 0, 0, fv=3, pitch=60, xoff=60)
    e2 += note_v0c4_xoff(480, 0, 0, fv=3, pitch=60, xoff=90)
    e2 += note_v0c4_xoff(720, 0, 0, fv=3, pitch=60, xoff=120)
    e2 += end_marker()
    custom = [(meas_hdr(4, 4), e1), (meas_hdr(4, 4), e2)]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# importer_hairpin_endpoint_dynamic_wins.enc
# When a cross-measure hairpin has xoffset2 < firstNoteXoff in the target
# measure AND a Dynamic exists after the start tick within the alMezuro
# window, the Dynamic endpoint must win. The bar-line clamp (xoffset2
# heuristic) must NOT override it. Fixture: dim from m1 (alMezuro=1,
# xoffset2=5) with a MF dynamic in m2 at tick=240. The hairpin must end
# at m2.240, not at the m2 bar line (m2.tick=0).
# ===========================================================================
def gen_v0c4_hairpin_endpoint_dynamic_wins():
    e1  = note_v0c4_xoff(  0, 0, 0, fv=3, pitch=60, xoff=30)
    e1 += note_v0c4_xoff(240, 0, 0, fv=3, pitch=60, xoff=60)
    e1 += note_v0c4_xoff(480, 0, 0, fv=3, pitch=60, xoff=90)
    e1 += note_v0c4_xoff(720, 0, 0, fv=3, pitch=60, xoff=120)
    # dim cross-measure with xoffset2=5 (< firstNoteXoff=30 in m2).
    # Without the priority fix the bar-line clamp would give endTick=m2.0;
    # with the fix the MF dynamic in m2 at tick=240 wins.
    e1 += ornament_v0c4(720, 0, 0, tipo=0x1D, xoffset=100, alMezuro=1,
                        xoffset2=5, speguleco=0x03)
    e1 += end_marker()
    # m2: MF at tick=240 xoffset=70 (> note@240 xoff=60 -> no snap-back;
    # MF stays at tick=240 = m2 + 1/4 = Fraction(1,1) + Fraction(1,4)).
    e2  = ornament_v0c4(240, 0, 0, tipo=0x84, xoffset=70)   # DYN_MF
    e2 += note_v0c4_xoff(  0, 0, 0, fv=3, pitch=60, xoff=30)
    e2 += note_v0c4_xoff(240, 0, 0, fv=3, pitch=60, xoff=60)
    e2 += note_v0c4_xoff(480, 0, 0, fv=3, pitch=60, xoff=90)
    e2 += note_v0c4_xoff(720, 0, 0, fv=3, pitch=60, xoff=120)
    e2 += end_marker()
    custom = [(meas_hdr(4, 4), e1), (meas_hdr(4, 4), e2)]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# ornaments_tremolo_orn.enc
# Encore stores single-chord tremolos as size-16 ORN elements with
# tipo=0xAF (standard triple tremolo for plectro instruments) in addition
# to the articulation-byte encoding. The ORN may appear at the chord's
# tick (common case) or at tick == durTicks (Encore places it after a
# long note at the measure end). Both must attach a TremoloSingleChord /
# R32 to the chord.
# ===========================================================================
def gen_v0c4_tremolo_orn():
    # m1: half note at tick=0, tremolo ORN at same tick (normal case)
    e1  = ornament_v0c4(0, 0, 0, tipo=0xAF)    # TREMOLO_32 at beat 1
    e1 += note_v0c4(0, 0, 0, fv=2, pitch=65)   # half note (480 ticks)
    e1 += note_v0c4(480, 0, 0, fv=3, pitch=65) # quarter note
    e1 += note_v0c4(720, 0, 0, fv=3, pitch=65)
    e1 += end_marker()
    # m2: quarter note at tick=720, tremolo at tick=960 (measure end)
    e2  = note_v0c4(  0, 0, 0, fv=3, pitch=65)
    e2 += note_v0c4(240, 0, 0, fv=3, pitch=65)
    e2 += note_v0c4(480, 0, 0, fv=3, pitch=65)
    e2 += note_v0c4(720, 0, 0, fv=3, pitch=65) # quarter at beat 4
    e2 += ornament_v0c4(960, 0, 0, tipo=0xAF)  # tremolo at measure end
    e2 += end_marker()
    custom = [(meas_hdr(4, 4), e1), (meas_hdr(4, 4), e2)]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# ornaments_tremolo_orn_tied_from.enc
# BUG FIX: When a tremolo ORN appears after the last note in the stream but
# the resolved chord is a tie-continuation (tieBack() != null), the importer
# must walk back to the tie-start chord and attach the tremolo there.
#
# Real-world case: Alborada de Mayo, measure 1, percussion staff.
# Encore shows the tremolo on the quarter note of a quarter->eighth tie.
# The ORN's stream position is after the eighth (cumTick at 3/8), which lands
# past both chords, triggering the "last chord" fallback that resolves to the
# eighth. The fix: if the fallback chord has tieBack(), follow it to the
# tie-start.
# ===========================================================================
def gen_v0c4_tremolo_orn_tied_from():
    # Q(tick=0) tied to E(tick=240); tremolo ORN at tick=360 (after E ends).
    # cumTick after E = 3/8 -> exact match fails -> fallback finds E as last chord
    # -> fix walks back via tieBack() to Q.
    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60)   # Q C4 at tick=0 (tie-start)
    e += tie_v0c4(   0, 0, 0)                    # TIE element at tick=0
    e += note_v0c4(240, 0, 0, fv=4, pitch=60)   # E C4 at tick=240 (tie-end)
    e += ornament_v0c4(360, 0, 0, tipo=0xAF)    # TREMOLO after E
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_tempo_sym_followtext.enc
# BUG FIX: MEAS-header and ORN tempo texts used a raw Unicode note symbol
# (U+2669) in their xmlText instead of <sym>metNoteQuarterUp</sym>.
# TempoText::updateTempo() matches against the TempoPattern list, which uses
# <sym> tags; the plain-Unicode form never matched, so editing the BPM in the
# UI had no effect on playback.
#
# Fix: tempoXmlText() now emits <sym>metNoteQuarterUp</sym> = N (simple meter)
# or <sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = N
# (compound meter). Both paths also set followText=true so MuseScore keeps the
# tempo map in sync when the user edits the displayed number.
# ===========================================================================
def gen_v0c4_tempo_sym_followtext():
    # m0: 4/4, bpm=100 -> simple  -> <sym>metNoteQuarterUp</sym> = 100
    # m1: 4/4, bpm=100 (same as m0 -> no new TempoText; keeps nominalTimeSig=4/4
    #                    so pickup-measure heuristic does not fire)
    # m2: 6/8, bpm=120 -> compound (MEAS header stores quarter-note BPM: 120 qBPM = 80 dotted-quarter BPM)
    #         dottedBpm = (120*2+1)/3 = 80; displayed = dotted-quarter = 80
    #         xmlText: <sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = 80
    #         BPS = 120/60 = 2.0
    # m3: 6/8, bpm=120 (same as m2 -> no new mark)
    pre  = set_chumagio(0xC4)
    custom = [
        (meas_hdr(4, 4, bpm=100), end_marker()),
        (meas_hdr(4, 4, bpm=100), end_marker()),
        (meas_hdr(6, 8, bpm=120), end_marker()),
        (meas_hdr(6, 8, bpm=120), end_marker()),
    ]
    body = b''.join(meas_block(h, e) for h, e in custom)
    return pre + body + SKELETON_POST


# ===========================================================================
# importer_dyn_dedup.enc
# Encore can write the same dynamic twice on the same (staff, voice) at
# the same tick with slightly different xoffsets (e.g. xoff 37 and 38,
# observed as MF duplicates in real plectro band scores). Encore renders
# only one. The importer must drop the second when it targets the same
# segment so MuseScore shows a single dynamic instead of two stacked.
# ===========================================================================
def gen_v0c4_dyn_dedup():
    # Two MF ornaments at tick=0, same staff/voice, xoffset 37 and 38.
    e  = ornament_v0c4(0, 0, 0, tipo=0x84, xoffset=37)   # DYN_MF
    e += ornament_v0c4(0, 0, 0, tipo=0x84, xoffset=38)   # DYN_MF duplicate
    e += note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e += note_v0c4(240, 0, 0, fv=3, pitch=60)
    e += end_marker()
    custom = [(meas_hdr(4, 4), e)]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# importer_slur_cross_measure_fallback.enc
# When the SLURSTART carries alMezuro >= 1 the slur spans across the
# bar line. Encore xoffsets reset at each bar so the pixel-span
# heuristic does not apply (a per-measure pixel calibration would be
# required to bridge them). The post-pass therefore falls back to the
# last existing ChordRest on the same track in the alMezuro target
# measure, which is the behaviour that already worked for the rest of
# the corpus. The fixture exercises this fallback explicitly so a
# future refactor of the pixel-span path does not accidentally take the
# cross-measure case down the same code path.
# ===========================================================================
def gen_v0c4_slur_cross_measure_fallback():
    # Slur starts in m1 with alMezuro=1 (ends in m2). Both measures
    # carry 4 quarter notes; the slur should anchor on the last
    # ChordRest of m2 (tick=720 within m2 = beat 4).
    e1  = ornament_v0c4(0, 0, 0, tipo=0x21, xoffset=50, xoffset2=200, alMezuro=1)
    e1 += note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(240, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(480, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(720, 0, 0, fv=3, pitch=60)
    e1 += end_marker()
    e2  = note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e2 += note_v0c4(240, 0, 0, fv=3, pitch=60)
    e2 += note_v0c4(480, 0, 0, fv=3, pitch=60)
    e2 += note_v0c4(720, 0, 0, fv=3, pitch=60)
    e2 += end_marker()
    custom = [(meas_hdr(4, 4), e1), (meas_hdr(4, 4), e2)]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# importer_hairpin_speguleco_bit0.enc
# Encore 5 stores the WEDGESTART direction in bit 0 of the speguleco
# field. Modern files also set bit 1 on the same byte (so crescendo
# reads as 0x02 and diminuendo as 0x03; the legacy 0x00 / 0x01 pair
# still occurs but is no longer the only encoding). The previous
# `speguleco == 0` check treated every Encore 5 hairpin as diminuendo
# and flipped every cresc/dim pair on disk. Fixture: a 4/4 measure
# with a crescendo (speguleco=0x02) starting at beat 1 followed by a
# diminuendo (speguleco=0x03) starting at beat 3.
# ===========================================================================
def gen_v0c4_hairpin_speguleco_bit0():
    e  = ornament_v0c4(0,   0, 0, tipo=0x1D, alMezuro=0, xoffset2=120, speguleco=0x02)
    e += ornament_v0c4(480, 0, 0, tipo=0x1D, alMezuro=0, xoffset2=240, speguleco=0x03)
    e += note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e += note_v0c4(240, 0, 0, fv=3, pitch=60)
    e += note_v0c4(480, 0, 0, fv=3, pitch=60)
    e += note_v0c4(720, 0, 0, fv=3, pitch=60)
    e += end_marker()
    custom = [(meas_hdr(4, 4), e)]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# importer_hairpin_ends_at_next_dynamic.enc
# Encore renders a `mf<f>mf` chain with each hairpin terminating exactly
# at the next dynamic glyph on the same track, not at the bar line of
# its alMezuro target measure. The pre-fix importer extended every
# hairpin to the end of its alMezuro measure, so adjacent hairpins
# overlapped and the visible end did not match Encore. The post-fix
# resolver scans forward from the WEDGESTART for the first Dynamic
# annotation on the same track within the alMezuro window and stops
# the hairpin there.
# Fixture: 4/4 measure with mf at tick=0, crescendo from tick=240 with
# alMezuro=0, f dynamic at tick=720, diminuendo from tick=720 with
# alMezuro=0, mf-style continuation at the bar line. The crescendo
# must stop at tick=720 (where f sits) instead of extending to the
# bar line.
# ===========================================================================
def gen_v0c4_hairpin_ends_at_next_dynamic():
    e  = ornament_v0c4(0,   0, 0, tipo=0x84)   # DYN_MF at tick=0
    e += ornament_v0c4(240, 0, 0, tipo=0x1D, alMezuro=0, xoffset2=240, speguleco=0x02)
    e += note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e += note_v0c4(240, 0, 0, fv=3, pitch=60)
    e += note_v0c4(480, 0, 0, fv=3, pitch=60)
    e += ornament_v0c4(720, 0, 0, tipo=0x85)   # DYN_F at tick=720
    e += note_v0c4(720, 0, 0, fv=3, pitch=60)
    e += end_marker()
    custom = [(meas_hdr(4, 4), e)]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# text_chord_sym_latin1.enc
# Chord-symbol elements (CHORD, type=7) carry a 36-byte text slot that
# was always decoded as UTF-16 LE. Legacy files store the chord text as
# single-byte Latin-1 (probe byte 0 printable, byte 1 NOT 0x00). The
# reader now applies the same per-element probe used elsewhere so the
# chord name reads as "Am" instead of two Latin-1 bytes merged into a
# single BMP code unit.
# ===========================================================================
def chordsym_v0c4(tick, voice, staffIdx, text_bytes, fretboard=False):
    """50-byte CHORD-symbol element (type=7) with `tipo` low bit set so
    the text payload is included. `text_bytes` is the 36-byte text slot
    inserted verbatim (caller controls encoding and terminator). When
    `fretboard` is true, tipo bit 2 (0x04) is set: Encore draws a guitar
    frame above this chord symbol."""
    assert len(text_bytes) == 36, len(text_bytes)
    d = bytearray(47)
    d[0] = 50              # size
    d[1] = staffIdx & 0x3F
    d[2] = 0               # toniko
    d[3] = 1 | (0x04 if fretboard else 0)  # tipo: bit0 = hasText, bit2 = show fret frame
    # d[4..6] skip3, d[7] xoffset, d[8] skip1, d[9] radiko, d[10] baso
    d[11:47] = text_bytes
    return struct.pack('<H', tick) + bytes([(7 << 4) | (voice & 0xF)]) + bytes(d)


def chordsym_sized_v0c4(tick, voice, staffIdx, text_bytes):
    """CHORD-symbol element sized to its own text, the way real files store it: the name runs from
    element +14 to the end, so the element grows in steps of two with the length of the name. Here
    the text fills its slot with no terminator inside the element, which is what makes a reader
    using a fixed slot carry on into whatever follows."""
    size = 14 + len(text_bytes)
    assert size % 2 == 0, size
    d = bytearray(size - 3)
    d[0] = size
    d[1] = staffIdx & 0x3F
    d[2] = 0               # toniko
    d[3] = 1               # tipo: bit0 = hasText
    d[11:11 + len(text_bytes)] = text_bytes
    return struct.pack('<H', tick) + bytes([(7 << 4) | (voice & 0xF)]) + bytes(d)


def gen_v0c4_chord_symbol_text_bounded_by_element():
    """A chord symbol whose name fills its element exactly, followed by a note. Read with a fixed
    36-byte slot the name swallows the note's bytes and imports as a string of junk."""
    # The symbol sits on beat 2 so the element behind it is a note whose tick low byte is nonzero.
    # A reader running past the element end therefore has real bytes to swallow: on beat 1 the
    # neighbour's tick is 0 and its first byte terminates the string by accident.
    m1  = note_v0c4(0, 0, 0, fv=3, pitch=60)
    m1 += chordsym_sized_v0c4(240, 0, 0, b'Cmaj')
    m1 += note_v0c4(240, 0, 0, fv=3, pitch=62)
    m1 += note_v0c4(480, 0, 0, fv=2, pitch=64)
    m1 += end_marker()
    m2  = note_v0c4(0, 0, 0, fv=3, pitch=67)
    m2 += chordsym_sized_v0c4(240, 0, 0, b'Gsus')
    m2 += note_v0c4(240, 0, 0, fv=3, pitch=69)
    m2 += note_v0c4(480, 0, 0, fv=2, pitch=71)
    m2 += end_marker()
    hdr = meas_hdr(4, 4)
    return assemble(0xC4, [(hdr, m1), (hdr, m2)])


def chordsym_numeric_v0c4(tick, voice, staffIdx, toniko, radiko=0, baso=0, hasBass=False):
    """14-byte CHORD-symbol element (type=7) WITHOUT a text slot (tipo bit0 clear), so the
    chord name is built from the numeric toniko/radiko/baso instead of literal text."""
    d = bytearray(11)
    d[0] = 14                          # size (element bytes 0..13)
    d[1] = staffIdx & 0x3F             # rawStaff
    d[2] = toniko & 0xFF               # element +5: chord quality index
    d[3] = 0x02 if hasBass else 0x00   # tipo: bit1 = bass present, bit0 (text) clear
    d[9] = radiko & 0xFF               # element +12: root
    d[10] = baso & 0xFF                # element +13: bass
    return struct.pack('<H', tick) + bytes([(7 << 4) | (voice & 0xF)]) + bytes(d)


def gen_v0c4_chord_quality_table():
    """One numeric C chord per measure at the toniko values whose quality strings the
    importer used to get wrong: 4 (was dominant 7, is diminished 7), 16 (was blank,
    is maj7#11), 34 (was 11, is 9#11), 40 (was +7, is 13#11), 48 (was 9sus4, is 7sus4)
    and 63 (was blank, is m13). Each measure carries a whole note so the bar is full."""
    tonikos = [4, 16, 34, 40, 48, 63]
    custom = []
    for tk in tonikos:
        e  = note_v0c4(0, 0, 0, fv=1, pitch=60)   # whole note C4
        e += chordsym_numeric_v0c4(0, 0, 0, tk)
        e += end_marker()
        custom.append((meas_hdr(4, 4), e))
    return assemble(0xC4, custom, fill_ts=(4, 4))


def gen_v0c4_chord_sym_latin1():
    # 36-byte slot. Bytes 0/1 = "Am" (Latin-1, probe says ONE_BYTE), then
    # NUL terminator, then padding.
    text_slot = bytearray(36)
    text_slot[0:2] = b'Am'
    e  = chordsym_v0c4(0, 0, 0, bytes(text_slot))
    e += note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += end_marker()
    custom = [(meas_hdr(4, 4), e)]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# text_titl_latin1_small_varsize.enc
# TITL blocks come in two fixed layouts: ONE_BYTE (varsize ~2426) and
# TWO_BYTES (varsize ~21242). The reader used to force TWO_BYTES only
# when varsize >= 10000 but never the inverse, so a TWO_BYTES-from-TK00
# heuristic on a Latin-1 TITL would mis-decode the title. The reader now
# treats varsize < 5000 as authoritative for ONE_BYTE.
# ===========================================================================
def gen_v0c4_titl_latin1_small_varsize():
    # Build a minimal ONE_BYTE TITL block:
    #   2 header bytes + 20 lines * 96 bytes + 504 tail = 2426 bytes.
    # The title is the only non-empty slot; line layout = 30-byte prefix +
    # 66-byte text payload. Latin-1 text "Romería" (with accented í + ñ
    # would be ideal but we keep ASCII-printable bytes that the reader
    # still treats as Latin-1 only if varsize triggers the override).
    title_bytes = bytearray(96)
    title_text = 'Romeria'.encode('latin-1')
    title_bytes[30:30+len(title_text)] = title_text   # NUL-terminator after
    lines = title_bytes + b'\x00' * (96 * 19)         # 19 empty trailing lines
    body = b'\x00\x00' + bytes(lines) + b'\x00' * 504  # 2 + 1920 + 504 = 2426
    titl = b'TITL' + struct.pack('<I', len(body)) + body
    # Suffix the override after the skeleton's TITL.
    pre = set_chumagio(0xC4)
    e  = note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    body_meas  = meas_block(meas_hdr(4, 4), e)
    body_meas += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body_meas + SKELETON_POST + titl


# ===========================================================================
# text_recovered_name_latin1.enc
# Encore 5.0.2 occasionally writes the instrument name at the formula
# offset (NAME_BASE + n * NAME_STEP) without a TK block header. The
# fallback name recovery probed UTF-16 only and discarded every Latin-1
# name silently. The reader now falls back to Latin-1 when byte 1 is
# itself printable instead of 0x00.
# ===========================================================================
def gen_v0c4_recovered_name_latin1():
    # Use the bazo skeleton and overwrite the TK00 name with zeros so the
    # importer's normal name-parsing path leaves instruments[0].name
    # empty and falls through to the NAME_BASE-offset recovery. Then plant
    # a Latin-1 name "Tropa" at file offset 202 (NAME_BASE for instrument
    # 0).
    pre = bytearray(set_chumagio(0xC4))
    # Zero the TK00 name slot (bazo writes the name starting at offset
    # 0x80 within TK00 = file offset 0xA6 + 0x80 = 0x126 -- but bazo's
    # TK00 layout has name immediately after the 8-byte magic+varsize
    # at file offset 0xC2 + 8 = 0xCA). Conservatively clear bytes
    # 0xCA..0xCA+64 so the name reader picks up an empty string.
    for i in range(0xCA, 0xCA + 64):
        pre[i] = 0
    # Plant Latin-1 name at the NAME_BASE offset (202) so the fallback
    # path picks it up.
    NAME_BASE = 202
    plant = 'Tropa'.encode('latin-1') + b'\x00'
    pre[NAME_BASE:NAME_BASE + len(plant)] = plant
    pre = bytes(pre)
    e  = note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    body  = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


# ===========================================================================
# text_text_block_latin1_decoding.enc
# Legacy Encore files (notably Spanish/Portuguese scores) store TEXT block
# payloads as Latin-1, one byte per character, instead of the UTF-16 LE
# encoding modern Encore 5 uses by default. Forcing UTF-16 LE on a Latin-1
# payload turns "la 1ª vez" into "慬ㄠ₪敶" (Chinese-looking gibberish that
# is two Latin-1 bytes reinterpreted as one UTF-16 code unit). The
# importer must detect the encoding per entry (printable ASCII followed by
# 0x00 = UTF-16 LE; otherwise Latin-1).
# ===========================================================================
def text_block_v0c4_latin1(entries):
    """Build a TEXT block whose entries are Latin-1 encoded.

    Same layout as `text_block_v0c4` but with single-byte text.  Real
    Encore files of this kind also end the text with the 4-byte 0x04 0x00
    0x00 0x00 terminator and may carry trailing padding bytes inside the
    payload; the fixture replicates both shapes.
    """
    entries_bytes = bytearray()
    content_size = 0
    for text in entries:
        text_bytes = text.encode('latin-1')
        # 14-byte header + text + 4-byte terminator + 1 padding byte
        # (mirrors sirena.enc's payload tail with garbage bytes after the
        # 04 00 terminator that the reader must NOT consume into the text)
        payload_size = 14 + len(text_bytes) + 4 + 1
        payload = bytearray(14)
        payload += text_bytes
        payload += b'\x04\x00\x00\x00'
        payload += b'\x20'   # trailing padding byte after the terminator
        entries_bytes += struct.pack('<H', payload_size) + bytes(payload)
        content_size += payload_size
    body = struct.pack('<HHI', 0, len(entries), content_size) + bytes(entries_bytes)
    return b'TEXT' + struct.pack('<I', len(body)) + body


def gen_v0c4_text_block_latin1_decoding():
    # m1: a single ORN(STAFFTEXT) at tick 0 with tind=0 -> "la 1ª vez".
    e  = stafftext_v0c4(0, 0, 0, text_index=0)
    e += note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += end_marker()
    custom = [(meas_hdr(4, 4), e)]
    return assemble(0xC4, custom, fill_ts=(4, 4),
                    text_override=text_block_v0c4_latin1(['la 1\xaa vez']))


# ===========================================================================
# importer_two_dynamics_in_one_measure.enc
# Encore can attach two dynamics + two stafftexts to the same measure
# when the measure carries two different repeat directives (e.g. f for
# the 1st volta, pp for the 2nd). The second pair is stored at a tick
# past the measure end (sirena.enc m21: 2/4 with durTicks=480 carries the
# second dynamic + stafftext at tick=960). The previous reader filtered
# them out with `ORN && tick > durTicks` and the user saw only the first
# dynamic in MuseScore. The reader now keeps section-end DYN_* and
# STAFFTEXT ornaments and the per-case placement clamps them to the last
# existing ChordRest segment of the measure.
# ===========================================================================
def gen_v0c4_two_dynamics_in_one_measure():
    # 2/4 measure, two quarter notes; first dyn (f) + text at tick=0,
    # second dyn (pp) + text at tick=960 (past 2/4 durTicks=480).
    e  = ornament_v0c4(0, 0, 0, tipo=0x85)   # DYN_F
    e += stafftext_v0c4(0, 0, 0, text_index=0)
    e += note_v0c4(0,   0, 0, fv=3, pitch=60)
    e += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e += ornament_v0c4(960, 0, 0, tipo=0x81)   # DYN_PP at "end-of-measure"
    e += stafftext_v0c4(960, 0, 0, text_index=1)
    e += end_marker()
    custom = [(meas_hdr(2, 4), e)]
    return assemble(0xC4, custom, fill_ts=(2, 4),
                    text_override=text_block_v0c4(['la 1\xaa vez', 'la 2\xaa']))


# ===========================================================================
# importer_header_measure_count_truncates_ghost_measures.enc
# Real Encore 5 files (e.g. Mamae_eu_quero-Bateria.enc) can carry trailing
# MEAS blocks left over from prior edits that Encore does not render. The
# rendered count lives in the file header measureCount field at offset
# 0x34; trailing MEAS blocks past that count are "ghost" measures the
# importer must drop so the score matches what the user saw in Encore.
# Fixture: 6 MEAS blocks + header measureCount = 2 -> imported score must
# have exactly 2 measures.
# ===========================================================================
def gen_v0c4_header_measure_count_truncates_ghost_measures():
    pre = bytearray(set_chumagio(0xC4))
    struct.pack_into('<h', pre, 0x34, 2)   # measureCount = 2
    pre = bytes(pre)
    e = note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    body  = meas_block(meas_hdr(4, 4), e)
    body += meas_block(meas_hdr(4, 4), e)
    # 4 trailing "ghost" measures still in the file but past measureCount.
    body += b''.join(empty_meas(4, 4) for _ in range(4))
    return pre + body + SKELETON_POST


# ===========================================================================
# importer_volta_coalesce_and_text.enc
# Encore stores the volta-alternative bitmask on EVERY measure inside the
# ending (e.g. m1=0x01, m2=0x01, m3=0x02), not just the first one. The
# importer must coalesce consecutive equal-bitmask measures into a single
# Volta and set the begin-text to "1.", "2.", ... so the number renders.
# Without coalescing the import created one Volta per measure (3 voltas
# total); without setText the bracket appeared with no number above it.
# ===========================================================================
def _meas_hdr_with_repeat_alt(tsNum, tsDen, repeatAlt, bpm=100):
    # repeatAlternative sits at meas-header offset 0x0F (one byte after
    # barTypeEnd 0x0D + a skip of 1). The layout-data block at 0x10..0x35
    # comes AFTER the volta-alt byte, so the helper writes 0x0F directly.
    h = bytearray(meas_hdr(tsNum, tsDen, bpm=bpm))
    h[0x0F] = repeatAlt & 0xFF
    return bytes(h)


def gen_v0c4_volta_coalesce_and_text():
    e = note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    custom = [
        (_meas_hdr_with_repeat_alt(4, 4, 0x01), e),  # alt 1 (m1)
        (_meas_hdr_with_repeat_alt(4, 4, 0x01), e),  # alt 1 (m2) -- same run
        (_meas_hdr_with_repeat_alt(4, 4, 0x02), e),  # alt 2 (m3) -- new run
    ]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# structure_volta_repeat_playback.enc
# A repeat with 1st/2nd endings:  ||: m0  1.[m1] :||  2.[m2]
#   m0: repeat-start barline (start byte 0x0C = 2)
#   m1: 1st ending (repeatAlternative 0x01) + repeat-end barline (barTypeEnd = 4)
#   m2: 2nd ending (repeatAlternative 0x02)
# Regression: the importer left a stale repeat list cached during layout (computed
# before the voltas were anchored), so the 1st ending replayed on the repeat instead
# of being skipped. The expanded play order must skip m1 on the second pass.
# ===========================================================================
def gen_v0c4_volta_repeat_playback():
    def cs(tick, text):
        raw = (text.encode('latin-1') + b'\x00' * 36)[:36]
        return chordsym_v0c4(tick, 0, 0, raw)

    def body(withChord=False):
        e = b''
        if withChord:
            # A chord symbol makes layout call Harmony::ticksTillNext -> repeatList(),
            # which computes the repeat list early (before voltas are anchored). That is
            # exactly the situation the fix must survive.
            e += cs(0, "C")
        e += (note_v0c4(0,   0, 0, fv=3, pitch=60)
              + note_v0c4(240, 0, 0, fv=3, pitch=62)
              + note_v0c4(480, 0, 0, fv=3, pitch=64)
              + note_v0c4(720, 0, 0, fv=3, pitch=65)
              + end_marker())
        return e

    h0 = bytearray(meas_hdr(4, 4))
    h0[0x0C] = 2                       # m0: REPEATSTART at measure start

    h1 = bytearray(meas_hdr(4, 4, barTypeEnd=4))  # m1: REPEATEND at measure end
    h1[0x0F] = 0x01                    # repeatAlternative = volta 1

    h2 = bytearray(meas_hdr(4, 4))
    h2[0x0F] = 0x02                    # m2: repeatAlternative = volta 2

    # A leading multi-measure rest makes the importer enable createMultiMeasureRests,
    # so the repeat list (computed during import) is built over the MM measure list --
    # the same situation as the real-world files where the 1st ending was replayed.
    mrest = rest_v0c4_mrest(0, 0, 0, fv=2, mrestCount=2) + end_marker()

    custom = [
        (bytes(meas_hdr(4, 4)), mrest),
        (bytes(h0), body(withChord=True)),
        (bytes(h1), body()),
        (bytes(h2), body()),
    ]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# importer_to_coda_vs_coda_marker.enc
# Encore distinguishes the source measure of "To Coda" from the destination
# measure carrying the Coda glyph by two distinct coda-byte values in the
# measure header (offset 0x1A low byte):
#   0x85 = CODA1 = "To Coda" text source measure
#   0x89 = CODA2 = Coda glyph destination measure
# Mapping both to MarkerType::CODA loses the distinction and the user sees
# a duplicate Coda glyph where Encore wrote "To Coda". This fixture has
# both bytes present and asserts they import as TOCODA and CODA in order.
# ===========================================================================
def gen_v0c4_to_coda_vs_coda_marker():
    e = note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    custom = [
        (meas_hdr_coda(4, 4, codaByte=0x85), e),  # CODA1 -> TOCODA
        (meas_hdr_coda(4, 4, codaByte=0x89), e),  # CODA2 -> CODA
    ]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# importer_volta_overlapping_bits.enc
# When two volta brackets share some ending numbers (e.g. "1, 2, 3." then
# raw bits {2, 4}), the second bracket must display only the NEW endings
# ("4."), not the full raw set ("2, 4.").  The importer accumulates
# usedVoltaBits from previous brackets in the same repeat block and filters
# rawBits & ~usedVoltaBits before building the endings list.
# Fixture: m1 has repeatAlternative=0x07 (endings 1+2+3), m2 has 0x0A
# (raw endings 2+4).  After filtering: 0x0A & ~0x07 = 0x08 = ending 4 only.
# ===========================================================================
def gen_v0c4_volta_overlapping_bits():
    e = note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    custom = [
        (_meas_hdr_with_repeat_alt(4, 4, 0x07), e),  # volta "1, 2, 3." (bits 0+1+2)
        (_meas_hdr_with_repeat_alt(4, 4, 0x0A), e),  # volta "4." after filtering (raw 1+3, new=3 only)
    ]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# structure_volta_repeat_playcount.enc
# A repeat whose endings span more than two passes: ||: m0  1.-3.[m1] :||  4.[m2]
#   m0: repeat-start barline (start byte 0x0C = 2), the repeated body
#   m1: first ending, repeatAlternative 0x07 (endings 1+2+3) + repeat-end barline
#       (barTypeEnd = 4)
#   m2: second ending, repeatAlternative 0x08 (ending 4) + final barline
#       (barTypeEnd = 5)
# Encore stores no explicit "times played" count; the pass count is implied by the
# highest ending number (4 here). Without deriving it, the end-repeat barline keeps
# MuseScore's default repeatCount of 2 and the section plays only twice, so the "4."
# ending is never reached.
# ===========================================================================
def gen_v0c4_volta_repeat_playcount():
    def body():
        return (note_v0c4(0,   0, 0, fv=3, pitch=60)
                + note_v0c4(240, 0, 0, fv=3, pitch=62)
                + note_v0c4(480, 0, 0, fv=3, pitch=64)
                + note_v0c4(720, 0, 0, fv=3, pitch=65)
                + end_marker())

    h0 = bytearray(meas_hdr(4, 4))
    h0[0x0C] = 2                       # m0: REPEATSTART at measure start

    h1 = bytearray(meas_hdr(4, 4, barTypeEnd=4))  # m1: REPEATEND at measure end
    h1[0x0F] = 0x07                    # first ending, endings 1+2+3

    h2 = bytearray(meas_hdr(4, 4, barTypeEnd=5))  # m2: FINAL barline
    h2[0x0F] = 0x08                    # second ending, ending 4

    custom = [
        (bytes(h0), body()),
        (bytes(h1), body()),
        (bytes(h2), body()),
    ]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ---------------------------------------------------------------------------
# ZBOT stream cipher (Encore 4.x).
# Cipher is its own inverse: zbot_encrypt(zbot_encrypt(data)) == data.
# TABLE_B is base64-encoded from the 39104-byte binary extracted from
# Encore 4.5.4 (VA 0x4C6A00). TABLE_A contains the 17 delta offsets.
# ---------------------------------------------------------------------------
import base64 as _b64

_ZBOT_TABLE_A = [14, 2, 8, 9, 1, 0, 3, 7, 6, 5, 4, 11, 12, 15, 10, 13, 16]
_ZBOT_SUB_OFFSET = [2, 3, 0, 1]
_ZBOT_TABLE_B_B64 = (
    'AAIDBwcACAAHCAgGBQMJAgYEBAcABQEEAwIHCAIECQYFAgUAAwABCAgCBgAACAUFBAkHCQMDCQEACQYB'
    'AAAACAgABwMBBgQBBwUHBgcFCAQGBQcGAQQBBgAEBQEAAwAHCQYJCQkABQQJCAkJCAcEAgUGAQEAAgEF'
    'AwcDBAUACAQDCQAIAAkBAwUFAAMHBgACAggEBwABAAEEBwYEBgIDAgMFAQADBAMCAAkGBggCAgMABQYG'
    'BQICBwkIBgYAAwgBAgEHCQcEAAcGBgEDCAcGAwgIBQEEAAkBBQYICQkHAwcIAwkFBgcGBAcEAwAHBQYA'
    'BwAFAAgBBgYCAQgEAQYABgQJAAABAgAGAAkBBwgABQIIBQQBAwIFCQMICQUHBwkJBAIICQgCCAAEBAEH'
    'AgMIAwQDBwEBBAICBAgJAwUIBwgHCQMABQkHBggDBgADBQMIAQEGBgEDBAgACAcDBwMECQEGAQMICAAC'
    'AgUFAAYBAAkDAgAAAQcAAgIEBAEFCQYICQAACQUGAwYIAAcJBAgGAQIHAAgJBAkJBAYJAgMIAgQIBwAC'
    'BggABAgICQMJBggDBQcDBQcBAwIDCAcBCQUJAwgACQIIBwAAAgAHCAUABwcDAwEJAwYFCQABAgECBwAH'
    'CQEABAABCQQCCAYHCQkBAQcDCAgBAQQGAgIEAQQHBgEAAgUHBwMCAAIECQkBAwgACQgJBQQJBQQHCAkF'
    'CAIEBgcHCQgHBAEGBQkHAwIACQMEBQgIAAkCBQAGBwUCBQgDBgAIAAEIAwQEAwIHAAEGBgkIBgkHCAIC'
    'AgUGCAMFAQQGBgICBAgJBgECAAAIBAUJAgUFAQEEBAgIAgEIBQQDBQUDAAcGAQkABgEAAAkCCQgEBQEE'
    'BgQIAAcCAQcJAwQBAgYHCQMIBgIFCQIGCAMEBwkIAQYBAAEBCAQBBwQHCAMFBgcJCAkABQcBBQQABQUI'
    'BwMFBwgDAQAEBgYAAQkBBgICBgkHBQkCBQUAAQkCBAIDAgcDCQECCQIIAwcBBgUCBgQBAwIABAgFCAID'
    'BgYBBwgDCQIBBgkEAAcBCQgDBwkJAwYDCQUJBAUEAQgHBgcFAwMFCAMDBgMBCQUCBgkJCAQCBAUACAQC'
    'BwgHBAAGBwkJBQMCCQYHBwIDAgkCAAcFBQADAAQIAgAJBQgDBAEEBQcAAQcFCQIEBgACCQACBgAFCQAF'
    'BQcBAAEDBgkCBwgFBQMGBgIFBwkDBgYDCAABAwYABQEDBQgFBAYAAQUIAgMFAgYIBgICAgICCAUAAQUA'
    'CAYDAAIABwIABQcABwgJBwkHAAgFBgEGBAAAAQIFAAcECQEIAAkBAAMDBwUHAQIGCAMECQAHBgMCAwkE'
    'BgICBAQDAAEBCAkGAwYABAEBAAICBQgBBAgAAQEJCAYHCQkCAQEDAAABAgAFAwYABgcBBwgFBAcCBQAE'
    'BwQIAwkBBQUJCQcCBwkHAwkABAEHAAEGCAkECQIEBAUGAgMDAQAEAAMBBQgBAwYDBAEBBggGCAgBCAIH'
    'CAIJCAIDBgEHCAEFBgAFBgMFBAYABwgEAQkJBQYHBQIHCAIEAgEBAQIECAABAAgGBQMGBwAAAQcECQIA'
    'BwAIAQUBBQUGCQAJBAIEBgcACQAABAkDAgkCAQAJBgEEBwEFAQYJAgAABAYGCQMBAQMEAAUCAQQDAgMG'
    'AAMABwcFAgIJBwIDAgkCAAkFBgcFBQQHBQkABwEBCQYCBAcAAgQABAcAAwICCQkBAQYHAgkBAAQEAgQG'
    'BQQCAQUHBwgCBggGAAEGCQYDAwMGAQIEAAYDAQYECQMFBAIEAQQABQgCAQAAAAMDBgMBCAkEBAMDBwYF'
    'BwYABAcHCQMCAgIECQEBCAIEAAEDBQUCBgEABQUABAEHBAAEAQcEBQMABQQECAQFAQcCBwUGBwMDAgYC'
    'CQQFBgQGBgYAAwMCAwEABgQHBQYBAAcABgMIBwYECAUDAgUEAAYBAwUHBwYAAAgEAAQAAAYABAIBAQkJ'
    'BgMABwYIAQIAAgkCBggIAwcABQEHAQABAAQJBggGCQIJBQcIAAUDBggABAAGAQMFAwIEAQUCBAMFCQcC'
    'AAQDCAMBAAcIAQAHAAIACQAGAwgGBwYBBwcEAQMJBAMGBQkECAUJBQIIAAgEBAADBAYIBQgGBAkGAgIA'
    'CAAJAAUDAwYHCAAAAgEGBgYEAwABAgcGBAAEBQgJBQUJCAIBBQcJAgEIBAkHAAAABwUAAgMCAAQGBwQA'
    'BQIAAwAAAAgEAQUBCAQBBwQFBwQJBwgECQEHBQYECQIDCAAJBwgABwUBBgkHAwUFBAYHCQACBgIABwMI'
    'BAAIBAIABwAIAwkGBQkBBwkIBgUFBwkFAQYCBgkFAQQFBgIFBwcHBQIIBwAICQQDCAcGBQICAAADBQEA'
    'AQIFAAUDCQgABwMHBgMFBQIEBgYIBgQABAcIBgYJCQgDAQQEAwQHBQcABgYEBAQHAwgFBQcEBggCAwkI'
    'BQgJAgQBAgADBwkBAQQDBQEIBwQGAQUABQkEAQcBBQUBCQIDCQYDCQcFBgUCAgUEAQEJAwAJAQUBBAUD'
    'CAkGCAgGBgMAAwQHAgIIAwcEAgQCCQYAAAIAAgAJCQIDCAcGAQQFAQYECQEHAwQIBAUBCQUBCQkCCQMC'
    'BwcIAgUJBgQHAgkEBAYCAAMBAgkFBgUEAQQAAAgFBAQCAAkEBQQJAQEECQQCBQADAgAACQcJAwEABgQG'
    'BQYAAQMEAQcCAwAGBwcABgkFAAEHBwgBBAgCCAkAAwkFCQEGBgkJBAIFBgEJAwEDCQIFAQQJCQUHCQgB'
    'BAYGAQEFBwAFAwIHAgYICAEGBgQABgUIAgQABwYJBggACQcFAAQJAwYACQIDBAcACAEBCAgIAwMICAcH'
    'BgIGBQcGCAUAAAYFBwYJAwUEAgACAgkECQMECQcGCAgFBwkFBAUBAwQJAgQJAAMECAUCBwgJAAcEBQEC'
    'AQEBAwYHAAYABAcHBAYIBwcFAQcGAAYABwQEBwUHCQkFBAUDCQUFBAgCAwEJBAUABAgJCAUGAwMHBAcC'
    'CQIGAgcEAQQFAwIIAQkGCAgHAwgDBgMBCAIBBgkHAgYEAggDBgEABAYGAAgHAAYBCAkABwgCBwgECAUF'
    'AwEDCQYCAgkEAgMJCQUGBQMFBQcDAQEFBgQFBwcIAwQIAgQDBAMBAwQEBgYIAQQFCAAJAwkACAECAwgJ'
    'BgIGBAEABQQBAQcBBgEAAgEGCQIBAAYEAAIDCQMAAQIHCQMABgYIBAUGAQMFAwAAAQgHBgQEAwEAAAAB'
    'BgUGCQcGAAcFBgkCBwEIBQUFBAQFCAYCAwYFAQQECQUHAAUCCQEBBgQDBgAGCAgCBAIGBgEHAgMJBgYD'
    'AQcGAgEFCQIJAwAFCQgBBQMBAwgJAQYIAwEECAUFAgMDBAgIAAkGAQEHBgQJBwMCBwQHAwQDAwgFBgQE'
    'AAIIBAYJBwMHCQUAAAcIAgcCBgEJAAEJBgkFAQIGAgYFAgADCQgICAcECAYFBwEEAwAAAgYCAAQGBAgC'
    'CAQAAQgJBgQEAgMAAAkABwcGAAYJBAcHAAEHAgcCAQUCAAYABggABggCBgYGBgYDCQEABQgACQgIAwgD'
    'AAAAAgIBCQABCQYIAwgBCAIABAgJBgEGBQMACQYCAQMGAwADBwMBBgEBCAYECQYCCAcJCQYCCQEIAgUD'
    'CQkIAwYBAAcCCAkBAgIABQMCCQIAAwkHAQgCBgYFBAMJCAABBAcAAwcGAgEBCAcDCQQECQYHAgcCCAQE'
    'AAIJBAAICAMHAQADBgQFAwECCQkABAcFAQkHAgUHBwUGCAcHCQICAAUDAgQFBwIECQkGCQkGAgYIAwcI'
    'BAgEAQcECQMBBQgDBQUDBgIBCQADBgkAAwgFAQUEAwAIAQcAAgUBBAcFAwQFBgAEAAYHAAAJAgUEAgQH'
    'AQgGBwIABQUHAgUICQQEAwcHBQkBAQkEBQICBAMFBAIFAQMCAwYAAAABCAEIBAAFCQUHAQMACAUHAQkH'
    'BwYDCAYAAAEBBgMBBgYAAAYFBwEFCQQABgYHAwEBBwcCBQIJAgADBwkACQcAAgcGCQkIBQkIAQQCBgcH'
    'AQEGAQIIAQEJCQgJBQkCCAUCAAkGBQYBAgQBCQkEBgMCBgAFAwAABQgGCQYABwkCAgcABQcCBwEJBQgB'
    'AQEAAwcHAgUJAgIECAEFCAEBAwUJBQMGAAYIBwMCAQMCBwMGAwgJAAAFCAkBAwgAAQIFCAkFBQQDCAQH'
    'BQUFCQMHBQEGBgUDAAgGAwIIBAYHCAcABwQCAQMAAQUFAQMGAQgFCQQDBAAICAQHBAEGCQUBBAkFCAUA'
    'BgUFBQkEBwkGCQIJBQkHBwIJCQAHAgUFCQUDAggBBQIDAgMABggGBgUJAAYCBQUCAQMEBAkEBQkAAgQH'
    'BwkABwcFAQkBBgIHAwUEAgAFCQgJAAgCAAEFAwcGCQcGBgYDCQUDAQICBQcGAAcCAwIDAgkIBQIJBgAF'
    'CAQDBgcAAQMEBAUHBgYABgcFAwAEAAICBgAAAwICBAAECQgDBwYIBAIFCQIHBQIDCQYHCQIJBQQGAAcG'
    'BQYHAAUFBgEACAkCBQAFBwgCBwUIBQYEAQkABgYJAgABCAMEBQAFCAcJBQgHAwQJAAQGAAAGBAQBAgEC'
    'CAAGBQUIAAYJBAIFBgMBBQgCAwgECAQFBwgDBAACBwMJBgEABgEBAwIDBAAEBgUAAQAGAAcIBQcFCAYD'
    'BQECBwcIAAUECAYBAAgHAAIJAgMGBAYAAwYBAwkCAQECAgcHAgUHBwMFBQIBBQcAAwgDAQMBAggFBAAG'
    'CQkHBwIGBAUAAgIGAAYJAQMGAQMCCQEAAwMABgUIBgkGAQIGBgYBAwQDCAYDBggIAgYGAQMBAAUEBQgD'
    'CQMHCAQEAAMDAgEFAQIFBQYFBgMFCQIBBwQHCAAAAwUHBQYHCQkDBgkCAAUCBAAIAwcEBgcFBQAJAQUI'
    'BQkEAwkDAAMEBggIBAEHAAUIBwEAAAIFAgYICQgGBAIAAwEIBgcJAgAIAAQCBgcBBgUABwECBwUBAgYB'
    'BgADCAYDCQIGCAIGAgICAgIJCQcEBgEIAwUDBgkFAgcCAwcCCQcAAwEEAQcGBwYCAQkGCAEBCAYJAwgB'
    'AQUGBgMAAAEAAQAHAAcGAgYIAgEFAQAEAQMIAQYJAQcIAwAEBAUDAgUCCAYABQEABwUJBQgBCQQFBQgG'
    'CAQBBgUGBAMIAgkABQYJCAEGCQkBBwkCBAgABAUDAQgCCQIABgkCBwAEAQIFBwMGBQkABQQJCQEGAQAF'
    'BQABCAcEBAUHCQEAAQcJBAYHAwkDAgQDCQYFCAgJAQAABAECAAEEBQQGCQADCAAJCAQCCAkAAAYIAQIH'
    'CQQGBwgAAAgEBAkFAwkBCQkIBwQBAwMBBwcACAgABAcFAwAICQgBAAUIAgIAAQEHCQgDCQIBCQAABQgE'
    'CQIDAgUFBAACBQgJCAYJAAAIAQkJBAQACAMEAAUBBAgIBQUEAQAFAQIACQAJAAcAAQYAAwkGBAYFCQUI'
    'AgkGCQcCCAIHAwcEAQYCBAkHAwgCAAAHCQgGBAYGBgMEAAcGCQcGBwkDAAcEBAUJAAMDBgUIBQIHBwkH'
    'BwcABAMJCQYICAIAAwgABQEHAggHBQMECAcFAgIBAwYJAgcCCAgJBAAFCAMHAgkFAAYAAQAGAgUJBwgC'
    'AQcJBQkEBQAHAAkFCAkABwEIBgEIBgEFBQEEAAIHAAUCBwcFAgUGAwMAAAEDCAMGBAgHBAABBAkABgQJ'
    'BQYFBgkGAwYGBgIDAggDAAIAAAYFCQEHAQcEAQQAAgUECAACBwIGCQMFCAQFAQEBAgADCQQGBgYGCQcA'
    'AQkJBgEIBAIJBgcCCQQBCQABBQkAAAYGAwUEBwgDAgEDCQMBAQkEAwkIAAcABAkGAwcFAwkBCQQBCQIE'
    'AAADBgkACAIIAwMAAAMEBQgHAwEJAwMCBAEGBQIHBAIIBQQCAAUCCAAJBAUIBwkABAgGAQUJCAkHAAQF'
    'AAMGBggEBgYFAgAGCAEBAgcBBQEEBQgFBgEIBAABAAkAAQUABgAEAAcHAAQEAwUABQkDAggABwUGAwAB'
    'AgAIAgADCQEFAwMICAEFCQAFBwkBBwQIBAQFCAUBAwkABQQDCAUAAQcECAMHBAQJAAYEBQECCQAEAwkB'
    'CQcCAAkIBQQCBwQCBAkCBQcGBwcGAAUCBgkHCQIIBQgGBwcECAIGBQQAAggEBQAEBgcDBAcJBQADBAkH'
    'AQAIAAQCCQcFAQAACQkFCQABBwYICQcAAAMBCQkBAQMGCAYJAAUDBQIHCAcACQgABAcIBQgJBQQEAAUA'
    'BgQFAAEGAwUGBgEHCAAACQUGAgAHAgcIBgAFCQkIBwAECQACAAUICQEGCAIGAwQDBQkECQMJAgQCBwkA'
    'AAQJCQUFCQUGCQgFAAAGCQgFAgQJBQkCBAkGAgUJAAkAAQEECAcGBgAICQcCCQcABQcACQMECAAJBwgA'
    'BQcJCQkGBQUHBAkAAgMGCQQACQgGAwMCAwYDAAcICAIHBAACCAkHAQcIBwcHBgkBAwkIBwAEBgQDAgkJ'
    'BQkABwEECAIEAAgCBgcACQUCAggBAQgBAAQFAwUEAQkIBQkDCQIAAAMBCQIHCQcGBAkBBAUABAUBAwUD'
    'BQEEAQkABwQECQIABggBAQMBAwAJAQUCCQAEBQAJCAUCAgEJAQMFBQcDBQYCCQMHAQMGBQcACQMHBQcH'
    'AAUAAgQJAwQAAQYDBwAGBQQAAggEBwUGBgQBCQAIAgcCBwAICQUGAwACAwcFBgYHAAMCAgEABwUIBgQB'
    'AgEJBQAFBAQABQUJCQQGAwQGBQgFBQYDBQIFCQcJAwcFBgkJCAIIAgIJBAIJBgUIAwkACQMACAgJBAkH'
    'BAYCAgYICQUICAYAAQAEAwkDBAcGCQQAAwEDCQkFCAkACAgFCQEEBQAHBAMBAAIGBAkIAwkBBwECBgAH'
    'BQYIBQcGBgIIAQYHBQMIAgMCAAkGAAgEAAUAAgUDBAMDBQkEBgUEBAABAwgIBQcFCAYIAQgDBQcJAwkF'
    'BAUIAAgJBgACAgkJAAYGAQUCBAQBAwYCCAIDCAgABwAJBAkHBQQABwIDBAQECAQFAAcIAgkFBwgHAAAB'
    'BwIAAgEDAQYGCAUABAIAAgMFCQQFAAcJBwcEAAAGCQMAAAcAAgQHBwcEBwUHCAUDBQcABwcEAgAIAQUG'
    'CAICAwIAAQkAAwYDBgQEBwcEAAACBAYJBQcAAAYBBAUFBgcGCQgGAgcDAAIDBQMGBAkBAggCBAcIBgAJ'
    'CAgICAkACAUEAgQAAggFAwkEAQUJBgUDAAEIAgIHAAUCCAcABAcEBAgBBwYIBAMABAgECQMFBAQJBAYC'
    'AgYAAQgFAgICBwMDBwEFCAQBBQQEBQAIBgcAAggABgQEBgMHAAUCBwEAAwgAAgEIAgQAAwgBBQcGAQMJ'
    'BgUIAAMIAAAFCQIFAgQEAwgFBAcICQkCBwAFAwEFAgYAAAMABQEBBwEIAwEBAAMFAAQCAAEAAQAGBgkC'
    'CQAFAgIGCAUGBQICAAkIAgQHBgQACQEDBgAAAAgCAQIJBQQIAAYGAAcIBAEJBQcEAwEGCAABCQUAAwkE'
    'BQYABgEFBAEFBgECBwcGAAMJAQgBAgcFBQUGBwYAAAYHBwADAwkICAIJAAIACQQABAAIAgAABgcIBwcH'
    'AgUEBAAFBgIGAAIFAAUIAQQAAgAEAQMIAQYABQABAgcACAAHAAIJAwcABwcHCQIGBQYAAwcHAAYGAwcF'
    'CQUCAwcEBggIBQYAAwUJCAYFAQEBAAAIAAcBCAIACQYGBgAIAAABBwEEBAYFAgYAAwAAAAQICAYFBwcD'
    'AQgJCQUECQQBBwQFBAAAAAUJCAQECQQBAQQCAgQJBQUHBQMIAgcFBwUCAwAEBgYJAwIBBwQECAYFAwMD'
    'BAUFAQAEAQIEBwUCBwUEBwAAAAgICQIACQIIAAMBBwcBBwIIBAUFAgIJBgQGBgYCBAEEBAcCBgMFAgUI'
    'BwkHAQIIBQIBCAYEBwQAAgQGBQkABwYECAkDBwEEAAIBCQcIBAQIAwYIBwAGBwUDCAYIBwMJBwgCBwQB'
    'CAYHAQUBCAYABwYBAgABCAUFBQQDAQgJAQIFAwYABgIGAgcGAgkDBQUJBgMBBQUCCAEIAgEHBgEHBgIE'
    'AwAJCAcJBQUJAgUBBwYEAAYACAUIAwQCBgAFAgAGCAkBBwgABggIBQMJAgIJCAIEBgEJAwMEAwAJAwcC'
    'CQMAAQYJBwUGCAUDCAUFBQUCBAMCBwEAAgQAAQYJBAgAAQgGAgAACQQCAwkIBwQCBAADAwQHCQEABgED'
    'BAcABwYCAgUJAgQDBAYBCQAHAAQFBAgEAAkCAgkHAQQAAAQHAgEBAAYACAMDBQkDBQQFBAcJBAkCBAIC'
    'BwkDAwACBgAGAQcCBQYCCAEAAAgJCAACBgICBQMDBQIABwAIBQUECAQEAQgABwcFAQgBBQMFAwkIBQUE'
    'AwUDAgQIAQACAwQFAQIGCQgECAICBgkGAQkICQUBBgIGAQcFBwkGAQEGBgQEBAIIAQMAAggEBAEHAgYB'
    'CQYGAAYCBwkDCAQFCQEJAQcGBAEBCAQBBAcJBwUJBAgBBAQBAQcJBAYGAgEFAgQCAwkAAggDAAQECAAA'
    'AwUBBQkGCQECAgYJBwMHCAIJAQcEBAcEBAIJAQIAAwcIAgEDBAkHCQcFAQYGCQUBBwQHBgQBCAgJBwME'
    'BwUAAgUDBwUIBwIDCAQEBAADAAYHBQYCCQECCQIAAAgCBwcICQICAQYJAwUDAwIECQcBAwMABQEJBgEC'
    'CQUICAMACAEJBAAHAAcJCAcABQIDAQkFAQcFBgEDBAQDBAMDBwAABQkCBwYJCAEJBwAHAgQBBggDCAQF'
    'BgQJCQQCBwcABwIGBAEFBQkBAQMFBwYBAQgGAQkJCAEHBQcIAggJBwkDCAcHCQgABQYAAAAAAAEHBgcD'
    'CQIECQUDBAMIAgUFCAABCAkIBQYGAAcBAgcIBQEFCQEBBgMJAQEJCAADAggGAAMCAgMDBAAGCQEABQAC'
    'AwYIAQMIAQMBAgIJAQcJCQYGAggAAQUAAAkGAwEBBwUDBgAABQEEAQQABwABBwUHBAUIAAMHAwEDAwAH'
    'BwQIBAYIAwYHCQgCAQcACAEFBgAJBAQGCAcABAQDCQMBAAUCBwMBAQgECAUDAgQJCQkGBQkDAQgHBQYE'
    'AwYFBgAGBwQDBgYDBAUBBwYFAgAHAwcFCQkDBggACQQFBQIHBQgCAwcIBQgFCQQJCQkGBAYBAwYFBQIF'
    'CQUIAQkHBwgEBwABBQEEBAcIAggACQMGCAYGCAAEAgYAAgYFAQcHAwcFAwACAggACAQJAQgEBgQFBwID'
    'BgkABQUDAQYBBgEFAwQGCAYABgADBwQACAIFAAQABgMBAAMFCQYABwIDAgEGAgMABwUFBgUIAwUDBQUH'
    'BAYIBQQCAAEBBwYFCAAEAQABAwcJBgAJBAcHBwkHBAgGBQkJBggIBgcABwcICAEABwAEBgEICAMJAgIG'
    'AQcDCAAIAQgBBwEJBQECAwEEAgkABgAAAAcICQkCAgcEBQMEBQIBCQMHBQkIBgcICAMAAggJCAgJCAQE'
    'BgQHCQYHBAcCAAEIAgYCBwQBBQMACQIBCAYDCQgIAgIABQEBAAkEBwYIAwYAAwIHAggIBwgACAIEBgAH'
    'BggEAgEDCAkHBgkJAwkJAQYCAAECCAQFBwADCQkDBAgCAgMFAAAIBggDBwgECQADCAcFBwcACAQACAkB'
    'AQgDBgEGBQgEAQUEBwMIBwYIAgQBBAAABQMBBgEBBAcBCQYCAgMJCQkICAMCCQYJCQAACQUABAADAAAB'
    'AwgBAwECCAMHBwcFAwkJBwgBAggJAgIABwQDAwgGAAcICAECBwACCAUFAwQGCAkCBQUBAwgHBwgJCQcA'
    'BQEABwkCAgUJAAcAAwQICAUJAggDBwEIBAgIAAAHAAIGBAQACAEGAQgCAAADBAEDBAQDBQEEBAcJCAkE'
    'CAIIBAgDCAQEAwQGBAcAAgkHAQgBBQQACQcBAgkICQIBAQkCAwUJAgIGBggIAwABBwMDAgkHCAEICQQH'
    'AwUFCQEDCAIGAAMEBAYGBQYAAwkFBAMJBgcGAwACAAIIAwMFAQEDCAUEBwMFBQMEAwkGAAIFAwMEBQAG'
    'BQcJAAYACAAABQEFAAkGBQgBAAIABwgIAwMHBAgGAgUCBAIBCAcDBgIDBgQHCQIIAwAHAAEIBAgJBwIE'
    'AgEAAgYGAwcIBgMDCQkEAgcBAwQBBAgFAQAGCQACBgUGBwUACQIACAcDBwUBBAYAAgkDBgAABgMCAQYF'
    'BQQHAgADCQcBBwIECQEIBwIIAQcBBQYFBgQIBAgFCQgDCQgABAQIBwYHBgYICQECAwMHBQYIAAIEAwAE'
    'AwAGAQECCAUGAQcHAAUGAgYFBgQJBwIABgQHAgAJAwcEBAkHBAUABggGBQgJBgQIAAcCBQYABgkJCQEB'
    'AAYHAgEHBQkAAQAAAAYJCQgBAwEHAQkGAwkFBQkGCAkBBQEHBAkIAQQABgYCAgABBgIDAQcJAwMIAgIG'
    'BQAHBAIFBAkHAwEHCAYBBQACBgUEBwAFBgUHAgIICQQBBwgFAQAAAQkDAwUAAQAEAwEDAAkCAAQHAAMF'
    'CAUABwcIAwUFBQUJAAIGBgYFBgUEAwYIAwEIBQAGAgIECQYCCAkGAQUBBgUHBgUIBQQCBwQIAQkCAwIH'
    'AgcGBwAJCAgGBAAEBwQBAAACBwIJBgUABwIDBwUHBQMCAQUICAgDAwIDBQEIAQgDBgkHAQMCAQABAAQA'
    'BwkCBwYHBQMAAwEAAAkEAQUEBQECBAEGCQcBBwUABQIAAQcJAQgDCAUJBQQHBgQCAAUFBwcGBQAEBgEF'
    'BgEABAgDBwgCBQkJAAMDBAMECQEIBAQHBQAJAQMICQIIBQUJAgQIAQkJBgYJAwQCAAMGAQEABgMJBwcJ'
    'AAcHAQAACAYHBAcJBwIDCAUEBgYBBgMFAQIEBgIDBwMIAQQJAwkEBgIAAAcDBQMIAwYAAQkGCAgBBgcF'
    'BwkJAAEIAAgAAwIDBwQIAAkFAQYFAQAGBwYCBQQDBgUDCQkCAQgEBAIGCAAFAQYDBgABAwQECQcDCAAF'
    'CQcGBQMIAwkCBQQCAAUCAQkFBAAEAAYBBAQEBQYCAggJAAUFAgIDAgcDAgAJAgYBBAAFAAcHAwcDAQgH'
    'CAIIBwkFBQAFAwEGAgkABgIFAwgJAAcAAQgAAgEDCAkGAQUGBQkBAAQDAAcJCQkJCAkFCAcFAQIIAwYJ'
    'BQQBBwcJAwAIAQIJBgEBAAgCBgUCCQUBAQgAAAQHCAkHAQIECAUDAgABAAgGAgcEAQEFCQEGBwYJBgMF'
    'BQMGAAEACAUEAAYCCAUCAAkHCQYFAggAAwYFAwkACAgGCAkFBQABBAYABQUAAgkHAgYBBgYABwAACAEH'
    'AAkDBwADBAQEAwIBAAAGAwMGBQkBCAgACQQCBAcCAwkAAAIBCAUABQUEAQQJAAkCCQYHAwYJBAACBgAH'
    'CQAJBQYABQAABwEDAQYBBAYBCAIHBwcHBgADBAAHBwMFBgUCAQkABgUJBwUHAAYIAwEHBAYJCQIAAwgD'
    'AQIBAggJCAYJCAUFBAQJAwUAAAAAAQUFBwUGAAkFAwABBQEGAAgJBAcGAgIIAwQABAQBAgMFAwMBBwcI'
    'BwEFAQYJAAYJAQIHAgEDAAQAAggGAQYGCAUABAUHBwUEBQYFBQcFBwYCAAcGBQYBCAUABAADBQUABwQF'
    'AwkJBwMABQkDAggABwQECAgFBgIHAgUICQQACQUEBQIHBQECBQkFBwkEAQkDAQEFAwcGBQMAAgUFCQEA'
    'AgcJBAcABAQDAwIFAgAFAgABCAMHAgcGAAQHBQIDBQQIAgEHBQQAAQcAAwkBCQYGAAMJCQYFCAMAAwUJ'
    'AgkCBQkJCAYGBAYCAgUDBgQBAAIJBAkJBQEJAAMDAwIDBQQBBAEAAAEBAgIIBAIBBAIIBgEDCQkBBQcG'
    'BQYIAQYIAAQHCAYCAQEDAgAIBQEEAwQEAQYBCAEGBAUEAwABBAUIBwMDAgkGBAQGBQAJCQIABQACAQUF'
    'BwkBAQIHAwMAAAQACAQJAQkCAwcEAQUBBQcCBAIABwEAAwgJAQEEBwQHBgEDBQQIBwQBCAkJBwkCBQUD'
    'AgMDAQUBBAQEBAkDAQcABAQEBQECAAEJCQkAAwAJCQEFCAAFAgAEAgUHAAMEAgkBAgkHCAMGBQMIBgEC'
    'AgUABAkJAwIICAIHBgEGAAYACQgBAAkJBQMGCAgCCAcDCAMFAQEGBAQIBwcACQAFAwEGAQIJBQEHCQYH'
    'BwUBAwIJAgEDCAgFBAcBBwMAAQYACQgFAwcHAgkEAwcHBAQHAwUGCAQFBgUHAwYHAQYBAwIJBQAABQYE'
    'AgUIAAcGAQIDCAACBgADAwcIBwIFBQcDCAkHCAYGBQMJBwAHCQMFCAcGBQADBQAEAwEICAYIAgMJBwAB'
    'AQkEAwMEAwUEAwcFBgcBBggJAQkEAQIACQcJBwABCQIEBwgAAgAIAAIAAggGAwYDBgUCAwkAAwkFAwQC'
    'AQMCAwgCBwgICAQDAwcICQYIBQYFBAQDAwUEBAgBAgAIBQUIAwgAAgMJBgcDBgEHAQIABAICBAYCAQgB'
    'AQgEBggFAQgEAQQECQAFBQECCQQDAgIFAQEIBwYBAgYJBgcBCAgHBAkABQEABwUAAAIICQgGAwMEBgMC'
    'AAQIAAYGAwQJAAYFAwECAAMHBwkBAQIDAAcDBAUBAQQCCQcHBgEECQMHAwUAAAkCCAYFBQUHBAgABggG'
    'CQcBAwEHCQUJAwMIBQQHBAAIAgYEBQgECQkBCAgEAQEACAUDCQEDBwYEBgMIAwEDCQMDCAMJAgACAgcF'
    'BgkHBQUJCQUJCQcDCAAICAgJAQICBgAFAwkDAQkEBwYEAggCBwkIAgAHBQUHAgcHBwgGBQUBAAMFAwkA'
    'AAYAAgQGBgMDAAIEBQkJAgADAwAACAcFAwMEAAMGCQMAAQcCBgMDBwYGBgQIAAkEBggCAwcDBggGAQgJ'
    'AwYAAQMIAAMECQcGAQkBBQEHAAIABwIEBwcJAwMDAwIFBgUGAQcCCQMIAgQEBQAEAggEBQUJAgQBBAQC'
    'CQEABgYAAwEGCAQCCQcGAQYJAwIFCQUIAAYFAAIIAAcACQUDBwMBBAcJBQMIBAkAAQkEAgIIBwYHBQkI'
    'AQcDBwgGAwkACAIIBwYHAQkGBQAHAQUBBwMICQcEBQQHAQgECQAIBgMIBQIFBwkEAQMBBAMEAAUJCQIJ'
    'AQcABQMHBAcCAAIHAQYDAAQEAAkJBgcBBQIIAwIHBwAHBwgFBQIICQMFBgYDAgEHAQEDBgcGBAUGCQAI'
    'BggGAgUHBQgDAAkHBwEGBQIGBQMECAIABwgGAwMDCQkDCAQCBgkGCAMEBwEJAAMABQYEAQgFCAgAAQMH'
    'BggHAAgDAwYBAQYDBQYAAQAIAwgBCQcFBAIACAgHAgMABgYIBwEAAQAGBQUCBQQHBwkCCQcCBwgIBAEH'
    'AwQGCAIDBggDBgIJBAQBBwQEBwADBgcAAwkICQkAAAAFBggHBAMGBwYICQQCAgMDCAEDAQQDAAcJAwkD'
    'AgkCBQMAAQgJCAUCBQcJBwECCAYIAgUDAAkGBAIJCQYACQYIAAEGBAMABwQECQkHAwkDBwYDCAcIBQkA'
    'AgQCCQgEBQcBCQUACQYBCQkIAQgCCQUFBQgCAgYEAwEIAgABCAUIAgcCCAMDAAkBAQYHAQgFBAUFAAIG'
    'AwkHCQcJCQcFBwMJBQgHAAUGBgYHCQAJAgkGAAUACAQJCAADAAYJCAAJCAMECQYACAgDAAADBQIABQkH'
    'CAQDBQkGAwkFAAUIBAUIAwEHAwkCBgQFBwcDBAIBAwIBBwgHAQQACAUJAgAAAgEJBgYAAAUBCQQECQAI'
    'AggJAQEAAwECAAcICAgDAQgGBAYGAAMIAwgACAcFBgMJAwMFBAgJBwMHAQIDBgcDBQECBQUABQABAAEI'
    'AwUJBwkFAwMBCQQBAwMFCQQJCAgGCAkGAggCBgUJAgYDAQYJAAQFBwAABwcBAgEBAAYFAAkJCAkABQkC'
    'AwEIBwMHBggEBgYFBQMAAAgABgQAAgEIAQkABwgFAwkCAwICBQcHAwIEAQMFAwIABQYFBwQBAQECBQgD'
    'CQcICAkDAAQCAwYJBwkIBwEEBAYCAAEDBgQGBQMEBAYCBgUCCAMJBAcBAwABBQEDAAMJBQUBAAYCBgEI'
    'AAYEBQgBCQgABQEAAwAEBQQHAwMAAwQFAgUEBggABgEICAIJBAkFCQUACQIJAQEGCAQHAwAACAUCBgEC'
    'BAQDAQYGAgYFBQEABwYABwgDBwAIAwQBBwcIBAgDAAYDAQAFAgEJBQMHAAQIBggHAgUFBQkJBwgABwEA'
    'AAQJCQQAAAMHCAMEBQQJAQYABQMIAAYGBggICQgAAgYCBwIEAgADCQkGCQMFAgAEAQcHAgMACQUIAggB'
    'CQUFAAYGBAkFAQYEBAQGBAEGBgAAAwcHBQgCAQIIAwcDAAcCAQQGAgEBCAkGAAAAAAEGBwICAgEFAgkA'
    'CQAFBAMEAQgDBAQABgUJBgkHAgQCBwQIBgIEBwcFAgkEBgMBCQAJBgcEAgUGBQIFAgQFAQQABwEGBAMB'
    'AwkEAQAIAgQFCAYHBQUBBAQDBwEBBgQCAgAAAQACBQYFBwMAAwkJBwQJAAUACQAJCAcDBwQFAQQIBQUJ'
    'CQEBBwAFBgcECAUDAwkEAAgJCAMCAwcBBgUJBAECBAcJBQkEBwIAAggAAgYEAgQACQgGAwcGAQkIBwMC'
    'AAcECAcCCAcDBAAFCAUCCQAGCQUAAAcICAEDAQQIAwAHAwIDBAQHBAkAAQUECAYCAgkBCQEAAAcGBAkH'
    'CQMFAwEGAgMJBwYGAAIIAQUFBQEDAgIFBgUJAAcCBQkHBQQFBgAHAAEBAwUABwUGCQgEAggEAwgDBQQA'
    'BQUHAAkCBAkBBgcDAwICBgcHAwYFBQgHBwcHBAAEAgMEBgcDAAMDAwgIBQIJBwYJCQAACQkIAgcGBwEI'
    'BwIIBQAHAggIBQYEBgABBwYECAYDAQgCAgIDBgkCAgUHBwYJAAEJAwEJAAUBCAgDAQEFAwABBQEGAAQA'
    'AgkHBQQBAAACBgIHBAAECQcIAwQDBQYABAkEAQEIBAYCBQQGAQYGBAcHBwEECAAJBgEAAwkIBQgABQQE'
    'AAIACQkBBgkBAwcJAwkGAQQBAQkJBAEJCAkGBwAFCAEGAAQEBQQHBwADBQYIBwIJCQgHBAUHAAgDCAgA'
    'AQIDAQUAAwgDAQIIBQIECQEFBwcEBgECBAYHCQkDBQUBAAQEAQcEAgkDCAYBBQYBAwQBAgUDBwIBAQAC'
    'AggHBAQEBAYAAwkJAQcABAIFAwUBBgQGBQEEAAcFBQgIAQYEBAMDCAcBAgMCAgQAAAUFCAgCCAEGCQED'
    'CAkHBgAFBAMAAwgJBAcFBAcHBgUICQYDAAgHCQUGBAQCCQYIBAYAAwACAgkGCQcICQcJAwEFBAYDBwME'
    'AwgHAwEDBgkGCQYFAgkIBwkAAQMCBwcAAAgJBQMIAAcGAQcDBwIHBgIIAwEJAwABBgcBCQEABgcJBwgG'
    'AQcAAAYEBwUEBwkACAQHCQkECAAFAAMDAQYAAggEAgMICAIHCQUBBggCAgECBQcBBAAFCQUJAwYJCQEI'
    'CQMJAQIIAQAABggJAAcHAwYBAQQACAgHBwEDAwkABAMCAwcHBAQDBQIHCAMFAAcEBgIJBAYHCQgEBwUE'
    'BQQGBQEHAAQCBAICCAMGAggCBAYIBwEGCAMJBgEBCAQABQICAwgEAQADCQIBAwMFAwcEBQYEBgQIAggB'
    'BQcHBwUCAQQIAwQGBAQFBQUCBgcBAgQICQcDAQUDBwgABQcCCQUDAwUEBgEJBQMFBgkFBwYJAAcAAQQJ'
    'BAYFBwUJCAQEAgICBwQHCAcFCAgABwQBCQEFBwgHAQMJBwAFAQQDCAQCBQgBAwEAAwcECAMHCAUGBgcA'
    'BAYBAAIIBwgFCAgEAAYJBAIJBgEBBAIACQYAAAQJBAICBwIHAwUICAcDAgIABAIGBAEACAQJBQAJCAgG'
    'CAYDBQUHBgEJAQkDBwIEBgMFAwADCQMGBQcJBQACAQICAAgJAwIHCAMFCQYDAgYBBgQGCQAABQIJBggD'
    'CAMCCAgABgQIBAEGAQYICAQDBwIEBwUJAAgHBwgJBAcGAQIJBwkGAAQABwEBAQAGAwMFBgYGAgYABAEI'
    'AgEHCQEIAQMDBgYACQkFCQABBgAAAgYEAQkFBwQHBAUEBQgFAwUGAgAIBAIHBwEFAQYBBwcAAAUFAgUJ'
    'CAUDAAUACQgFBwQFBwEDAAQABwUAAQEIBwIDAgEEAAUCAAECCAIDBAEICAgHBAgCAAMHAwgFCQQDBQkB'
    'BgkGAQgICQAHCQMABgkHBAMDAgIEBQEIBAACCAIBAQUJCQUEAQkBBgQJBAAHBgQBBgYDAwcFCQECCAMA'
    'BQYAAwYFBQIHCQAGBgYABQUHCQcAAAAIAQgDCQYCBAMBAwgHAQAHCQMJBgcBAwYGBwkGAAcJAwgHCQgD'
    'CAMACQMEAAgBAQMJAwMEBQQAAgUGBQcECAgHCQQJBgAFAgMFAQEEBwICBQYICAAAAgUBBwcBBgUICAQG'
    'CQcEAQQIAQgFAwAGAQkIBQQEBgMAAwMEBgYIBAEAAQICCQUJBgMECQcABwcEBQkECAgEAwEBBwkAAQUA'
    'CQADCAIJAgIGAwUIAgAEBAQIAAUGBQQEBgUCBQIBBAYJAgAABQYHAQcHCAQIBAcJAQMBAwgECQIGCAAG'
    'AAcIAQEAAQEHAwgHBQEEAwUECQIJAgQJCQkIAwUHAwgHAQgABQMJBAgDAQIJCAUCCAgBBAkGAgEGBAMA'
    'BwYHCAcIBgkBCAMJBgYCBAEDBQEJAQkGCAQICQkIAAYGAAIECAYABgAJAgICBgcFAgMBAgMDBAgGAAIJ'
    'AQMIAwkGBAkEAgMBBQEFCQIIAgAFBAIEBQIABgMGCQcDCAcHBwQDCQQACAQDAgMGBAkCCQgBAwIEBAII'
    'CQgDAAMCCAkECAUHAAYFCAkBBAAFBwMEBgMCAQMBAQgJAQkDAAAECQACBwEAAQQEBwIHAwUACQYAAgIH'
    'CQMJCQUDCQUBAwcHAgMABgUGBgAHBQIECQgDAAACAwACBgkEAAcGBAMEBAQIBAYJAwECAQQEBwkAAgQF'
    'CAQIAwEIAgcDCAkJBAIJCAYIAwcGCAAFAgUGCQcJCQEJCAcECAMECQQBCQMBBggFBQYAAQcACAUGAwMA'
    'BggACAcFAQQCBwUCAwYHAwIGBQAIBgYFBwIJAAcDBwIDAQUFAgUFAAQEAgEBBwAIBwIGBgIIBwIHCAUF'
    'CQcABwMGBAMACQgCAgEEBQEACQEGCAAHBgkHBAUBCQECBwMEBAQJCQQACQEHCAADAgQHBQUBBgkHBAEI'
    'AQYHBwYAAgcBBAUFAQIBBwECAQEICAYFAAQICAUEBQkFAQEHCAIDAAkFBAcFBgEEBAMABAYAAwUABwEB'
    'BgAGBwADAQUAAQAHAQgGAwkHAgkEBQkAAggACQEAAgMBBAYFCQIJBwMJBAgCAgIGBgAJBQcACAEFCAgB'
    'BAEACQACBwkDCAECCQMJBgMBBgEIAQgHCQYJAwEABgUHBwEBBQUIAQgGBQECBAYCAAAHBAEEBwYCBwEJ'
    'BAgJAwAIAAIFBQABCAkHAQQEBAEDCAIAAwQJBgQIBQYJAgMGBggDAQkICAcDAgQJAQYHAAUCAgQDBwEJ'
    'BAECCQIIAQcICQABCQUFAwIBCAAICQkCBQYABwcDAAYHAwcBAwADBAcHBgkJBgQFBAACCQkGBwUEBwEG'
    'AAUHBQgEBwQFBAUFBQAEAQkDCQMJBwcHBwAABgAAAAcBAwIDBQgDBgMAAwEABAIDCAYICQUHCQMIAAgC'
    'AQEABgYFAgYABwEGCAUGAAcFBgAEAgEGAgMHCAADAAQAAAACAAYGAAkIAwYHBwkFBAkGBAgHCAABAgYJ'
    'AgUGCQAJBwIDBAgCAggCAwEJAQAIAgAIAgYCAAgEAAMGAAAACQQGCAgGAQQBCQkJAQkCBwIDBwgECQkB'
    'AQMHAgEHAgcHBwgCAAYBAQYFAwUHAAMCBQgGBgQCAQEABwADBggGAgIBCAMDBgQACAQAAAMIAAcDAwAC'
    'AAEHAQkFBgkEAwYJAAgFAAEDCAYDCAUIBQUDBQEEBQMHCQcFBQgAAAcFCAIGBQIBCAYACQQBAQcDBAQH'
    'AgAHCAkDAQQEAggFCAQGBAIAAwUDAQkAAwIHBQAJCQcIAgcJBAkJCAcJBgIFCAQBAgUIBAQABwEHAwYC'
    'CQUFCQUCCAIIBwgFBQkGAQcEAQgIAQkJAQAEAgQAAAgEAwADAwUCCQgABgkICQQHAQIDBwkDBQYABAkE'
    'AQIABAUIAwkIAwgHAQUEBgIGBAQDBQcAAwEEBwAFBAYGCAkIBQcABwMJCAIFBAMABQEIAgUIBAcDAAUB'
    'BgMFBAACCQkGAQcJCQgEBgYFBgMCBQIJCQABAwQFAgQJAgQDAAECCAUHCAQGAgQFCQgCBgUJAwUCAQEE'
    'AgcICAYJAQEGBAEAAwQGAgYCBgkIBQkECAEGAQIACAYAAQMFBgQHCQcICAMHBwUHBQcFAwkJBgcGBAgH'
    'BgECCAIHBgEJCQYFAAUACAMHCAgFAwAEBgQEAQcABgACAwMBBwgHAwAFBQMGBgYBCAEBAAcBBggJBwIE'
    'CQIABAIBCQUABgQDCAcDBgQABwIIAgACBwcFCAIDAgQECQADBQgJCAEGAAgFCQEHAAQJBgkGBwkAAQYC'
    'BgUGBwcFAgQICQcBBgMCAwcCAAgABwcGAwkJAAMIAgYGAQAFAgIFAgcABgQHAQUDAwgIAAkFBQcFCQMA'
    'BQcDBwUIBQIHBwYBAggEBAgHBgUFAggFAQEFBgAACQMGBQkFCAUCBQEGBQUDBQAEAQEFCAkDAwIIAQQC'
    'AQgDAAIBCAcBAQYIAQgIBgEAAwMAAwcDBAIJAQQIAwEGAgQACQIJAgcBCQUFAwkJAAMHAAQHBQcIAwUG'
    'BwECCQMGAQQDAgAFCAEAAgEDBQMCBgkAAQQAAwgGAwEEAggGCAEACQMGBgMDAQgCAAEAAAMCBgcHBwUD'
    'AQAFBAAJBwIEAwQHAwYFAQgECAcHBAMCAQcGBwIHAQEHAwMCBgcEBgAABAMHAwMJAwAGAQIIAgYBAQMJ'
    'CQUABAQACAMBBAAGBQYFAgMFAwMCAAEACAgECAUHCAcABgEGCQYFBgAIAAcABgkCAQgGBwAHAQMFAwIA'
    'BQUCAgEJBgYIBwYGBgEEAwEGAwQBAwAFAwIACAUDBggDBQEIBQAFAgEFAwUGBAcBCQMGBgIAAAUHAAAI'
    'BQEFBwcEBwQEAwgGBQkEBwAJBAMGAAkDBgcDCAgDAgQGAAkABQgBAgMBCQECAQECAQQCBQYJCQQGCAIA'
    'AgEGCQcHBggEAAMFCQgJAAQHCQEBCAIECAQJAQYABgMJCAcBCAcCBgACBQIIBgUDAwkGAQcECAYEAAYJ'
    'AQEAAwABBAUFAAIHBAkHCAEEBwkGBQcGBwcHCAABBAQCBQIJBAkHAwYICAYBCQQBBgYICAQCAwUFAwgA'
    'BAkDCQYAAQQGBQEBCAUIAwMEBwYCCAcFCQUJAwUDAQgFCQIDBAEBBgQDCAEHAAIHAwcFBwYFAQMBAQED'
    'CAcFCQAJBwAACQUGBgMJCAcHCQQFAgIEBwUDCQQFAwUGAQMEAwgAAAQCBQgHBwADBggCAQUGCAgABAgF'
    'BQUBAwEABQUHAgUICQYEBgAGAQICBwcJAQkJAQAECQICBAcEBQMCBQQAAwkGBAQHAwUFBAgDCQEDBwAF'
    'AwgCBAcHCAMGCAgDAwYGBQYABwkIAQYACAYHBwMICAUAAwUJAAcEBAgFAAEABgMIAgkGAwQIBQQFAgAH'
    'AQgDAwcHAQkECAIFAwQBBAgCCQQGAQAGAAgACQkABAIEAQcABgEJCQUICAQJBAIGCQYHAAMGCQkFBgkI'
    'CAIDAgAJBQUIAwkCCAUFCAUIBwkGBQgGBQYJBAcAAwgBCAQFAQkCAAEBCQkHAgkBCQgGBAUABgAFCAkC'
    'AAMBAgAFAQIDBQMJAAEFBgAFAwkICQkJAAgICAMIBgUABgQHAAMBCQQCCQEJCQAHAwAAAQYHBAYICQkD'
    'BgkECQMCAAIFAAYEBQUDAAgDBQkDAgIDCQEEBAcIBwEICAcAAgEFCQYJAgMCAAcDAgEAAQEBBQEDBAYF'
    'AwIFBgEGBwMFAQECBwQAAAMECQgHAwIGCAQGCAEJCQUCBwgJBAkBAAUDBwEEAwkACAEFCQIIAgADAwQJ'
    'AQkEAgcBCQcFAQkEBQEGBAcEBwQBAwcJBQAHAgMIAAAHCQYFBgUGAgAECAAIAgEHBwkAAgEIAgAEBwUF'
    'CAEEAAMBCAcHAQIHCAYFBgIHCQgCAgIGAwAAAwYFBggCBwcIAgQJBQYDAwgJAQEBAQIEBAMAAwQFAQcG'
    'BwQCBQADAgMFAwMJBQECBwQJCQgHAwQEBQcAAAQJCAUHBQEIBQAIAAAICQkJAwAEAgMGAQYCBAQAAwkI'
    'BwIBBwQCBggFAgIGBgIJAQAHCAQACAAGAgQDAwgEAgEIAwEFAAcCAgIBAAIBCAMJBAQEBQcABAQECQEA'
    'AAUICAgIAAUGBwAFAQgECAQAAwEIBQQDAQICAgQBCAkFAQIFBAYEAQQFAwgIBQcABQEFAQYJCQEGCAQB'
    'BQYIBgYGAgcIBQcAAQECBgIJCAkABggBBQkEBgEJBgEFAwEIAwkBCAIFAAcACAEIAQIBAgMEBgQEAwUF'
    'BgADAQEFCAUEAQQJBQQABAIAAQkGAAcBAAEGAQkEBwUGAgQHBwIBBgMJBAYGBgYCBwYFAAUIBAEGBwAA'
    'BgcCCQIFBQEGAAkEAQADBwQAAwkJAQUFBgMFAwQIAQgGCAQCBwcABQMCBQIJAQABBQYGBgUIAggCAgEF'
    'BwMEAwgIAQgABwECAwYGBwkABgUABAEDCQcCBgQHCQkEBAYDBAcIBAIDBwYDBQUGCQACBQQIBAYFAwIF'
    'AAEGCAEFAAgCBwUBAwEBBwQAAggJAQcFBAEFAwQDAAACAgUCAAMIAAIDBAYJAQkGCAYFBggHBAkHBAQC'
    'BAAIBwYJAQQFAQkIAAYHBAIGAQIBBAUDCQUFBgEFCAEBCAkACQgFBwUJCAcJAgIDAwEGAgUJCAEDAAAC'
    'AQcABwIBBwAECAkDAgkEBgkBBQACAggCBAMIBgIEAQQBBwUHAAcABgYEBAAECQUCAgYDCAMEAQkBAgII'
    'BAEDAgMBBwEDAgcFAgEFBQMBBAYICQMECQUIAwkGAAEABQEBBgQACQcABwIBAwUFAgMHCAQHCAQJBAUF'
    'CQMJCAcEAgcJAgIGAwgEAgIJAgQGCQYABQcACQkEAQABBQIDAAUAAQcHBQAAAAUDBAYGAAQEBgkEAAcJ'
    'AAcFAgMHAQEBAQgFAAADBwYCAQAEBQQABwUGBwgICQEJAAMJAwcCAgAHCAkEBQkFAwIBAwAGAAMDCQYJ'
    'BgUCBAEEBAkJAQQHAQABAgcECAYCBgkAAAAJBAMFBQgFBwQABAAHBwgJAAABAQIFCAcEAQIEAgcFAAIA'
    'CAcACQAFBAAHCAgABQAHBwMECAMCBQEICQgDAQEIBQEBCAIIAQYBAQAHAAQICAgABwYJAwkAAggJCAkD'
    'AwUACQcCBgAHAgUFBQcJAQACAQYBBgMICQgFAAcICAkIAAAIAgkCCAMGAAgHBQICAwQEBgMEAwIEBgYB'
    'BgMECAUACQMABAkFBgYDBQgDCQYEAwcBBAAECQkEAAgJBgcABwMABQQDAQECBwIIAgUAAAMGAwYBCQMG'
    'BgEJCQcDCQEABQkHBQgFCAYGAQMICQYDBQAAAwMGCQEHAAYDAAEAAQEHAgYICQMAAwQFCAQBBQIDBggI'
    'AAECBAQJAQICBgQABgYDBQcCAwkJBwYEBgYDAQUACAMIAwUABgQCAgMDBgEECAMJBgQHBAQCAQcECQAC'
    'BQcDCQQHAwcABQQEAQYHAAgHBgUECAgAAQQCAwQDAwYGAQgDBQMICQUGAgIHAAcCBgcFCQIJCQQIAgkI'
    'CAQFCQMBBgMCAgcCCAMGAQQEBgkGCQECAQQAAAUBBAEIBgcAAAQEBwkFCQIIAgMJAAEABQkECAMDAAID'
    'CQIBBQECBwMIAwIHAQUABAIJCAIGBgYICQQEAQUABwYABwQHCQUCAAkAAgMIBAMCBQUBAQMDAAUEBwAD'
    'AAkGAQMBBwIEAwQBBQEGAAkIBAQCCQcFBgYGBwYABwkABgEFBQcHAgkFBQYACQcFBQcBBQQGAwEJBQUF'
    'BQYFAQAHCQcJAgcJBAYACQgCCQEJAQAJBAAJAAkEAgQFAwcCAAgIAwkEAgIBBQcICQIDAAMDCQYJBAcG'
    'BwIJCQQDAwkIBQcABAMFBgIDBAcDCAMGAwUABAMHAAQIBwIEBAQAAwAIBgUDBggACQUHAAYGAwYEBQUE'
    'BAIFAAQDAgQAAQAAAAECCAMHBgAJBAUBBwcDAgkABQgEAQEBBwQDAQIDAgADAgQDBgAABwYFCQIGAwAB'
    'CQgBCQICAQEJBgQGAwYBAwIGAwQIBAIFBwUBCQUJBQMHBAgGCAQHBwACBgQIBwkGBgIABwIABQQJBQQH'
    'AQQFCAQBBQgHCQUDAgYJAgQICQcJAgkGAwgCAggIBAQBCAUFCAIIAgYGAAQFCQEJBwgGAwQBCQgABgEC'
    'BgEICQUCCQgJAQgHAAUHAgcCAAgIBAkIAwcDCAYIAwgCCQQBBQIHAAkBAQMAAQAJAwYBCAMCBgMFAQkC'
    'BQYEBgMDCQQFBwkGAwIFCQUIBwgFAwcBAwcCCAEBCQAEAgcCCAUECQAFBAYBAwQJCAcAAwgEBwQICAEH'
    'BgcABAgIBwMIAwUACQEJCAYCBgkGBAYGAgkEAQIBAAUBAggDBQUBAQAJBwgFBAkCAQEBAwQHBwAEBQMJ'
    'AwEEAwMGBgMFBQUFAwYACQAIBAYEAAMHBQYIAwQHBQUHBQIEBAABCAcABwABBgIJAQICBQYECQMJAggD'
    'CAUGAAIJBwAFBwQFBgEHCQcAAQUIBwcJAwEFBwECBwUJBAQACQcBBAkJBgADAAUEBAIEBQABAgMDBgkE'
    'BAQAAQcJAgEABgcDBQIGBQUIBAYACAADBwAHAQQGAwQHCAYJBAIFAgkGCQECCAcEBQQICQcCCQkABwYB'
    'CQICAgUJAAgJBAgBCQQHBgUIAAEAAAYFAAIFCQQIAQAABwAEBwYHBgUDCQkABQEGCAYGBgMBCAcICAID'
    'CQgGCAMACAcBCAIABgkFBQUJCQQAAAMBBQUCAQUIBAICCQQABgACBgYGAgUACQQHAggCCQAAAQcFBwIF'
    'CQIGAQQFBAUFBQUFBAIICQEIAAkGCQYAAAADCQkHAgEIBgcBAgMABwEFAwADBQAICAYJAQcBCQUJAQUF'
    'CAYBCAMGAAEEAAMIAwcHAAYBBwAEBAkJBwcAAgEFCAIABQkCAgcHBAgACQADAgYHAgQFBgQFBAkEAAAI'
    'CAkIBwkJCAEIBQYCAAUEAgAJCAAHBgcCBgMBAwMEAgABAAIIBwkFBQUBAQUAAgEEAAYAAQIHAgMICQAH'
    'CQkDCAAHAwEACQgHBAkHAgEEAwYIBAgJCQEJCQADBAABAwQJAgUDAQQABwkGBgcCBQkIAAMFBAEABAIH'
    'BQYAAwQICAEEAgQFCQkHAwMBAwEDBAkJAAgJAAAHBwcABQAACAAIBgMGBwAIAAMGCAcFCQcFBAcGBQgA'
    'BAcCAAEJCAkEAwMIBQADAwAFBwQFBQcEBgMHCQgJAQADBwUGAAUJBgQCAAYDAgMJAgEABAEAAgQAAwIG'
    'BQQAAQcECQUGAQIIBggHCAYGAwkECAQDAQYCAwEGCAgHBgUGAgQGBQkAAQUGCAQEBgEECQMCBAYCCQME'
    'AAMIBwEABAcBBAcFBAEIAAcIAQMBCQEAAQEIBQMFBQcJAAgHAQcFBAgCCQgAAQEBBAIBCQYDBQEEAAMB'
    'AAkGBAYJCQEABgYCBQgFBAACBwgCAQkGBAUHCQEFAQEGBAMIAAQECQIIBgQIAgMCAAUHCAIFAgABAgYG'
    'BwYFBwYIBAEICAkDBAIGCQIJCQgDBAUACAEGBQkGAgcDAAUHBwgIBQkBCAQCBQADCQkECQAEBQQICAIC'
    'BwMHAAUDCQAAAwQHAQIIBgkAAAEIAAcABwAAAAgBBAEHBAUHCAYBAgkEBgcAAwMICAABAAIEAAABAAAH'
    'AAADBAYFAAEBCAIJBgQBAQMEAwMBBwAHBQMAAQgCBwMHCQUJBQMJCQcJCAUHCQMJAAYEBgICCQICAAMF'
    'AwQDAAQABgkBAwkEAQIGAAABBAECAwQJAgkFAQIABwEJBggDBgIJAwYFAAMGBggFAAcDAQIEAgQBCQIC'
    'AAYCBgYHCAYHAAgFAAMBCQcJCAIEAgkHBgkDBQAFBQQACAMDAgkHBQICBAMCBAAACQIEAQIIAwkFBAgI'
    'BgMDBQUHCAcHAQgBAAIFAggDBQAEBgAJAQMJBAEJCAcFAgMEBAkECQIDBgIEAgAFCQUIAAgFCAYFBQYG'
    'AgYBAwYGCQQDCQIIBwgJCQMGCQIEAAkHBAkABQgEAQEIAgMIBgYCCAIBAAQACQQABgQEAQMIBQQCAgcC'
    'AQYDCQMJCQUHAQYJCAgEBgkCCAIBCQkCBwUFCQgGBAIFAgEJBwkFBgMIBgEHBAIEBAcABQAFCAcFAgYB'
    'BQAEAwMABwkECAUGCAYHBAAHCAEJAAgIBAgIAAUICAkGCQMJBAUHAgUJBwMABQkHAgcAAQQAAQkJBgcD'
    'AwgJAQMDBAMIAwIBBAADAwkDAwUICAcEBQgGBAAFAgkCAQQJBwEBAAkEAAYJBwQACAMIAAgABgEAAwIF'
    'AwYFAQkCBAUABgUIBgMACAEBBwQHBAEGAQUIBAcHAQIFAggFBAMABAkDBwUHAwQJCAkBBgYIAAkCBgIH'
    'AgYEBgAGCQQHCAgBCQAGBwEJBAgABAABBQADAwQJBgYCBAMJCQgHAQUAAgIBAQMIAAkBAgUAAAQHAQcD'
    'AQYAAQIGAQkBCAEACAIHAAIECQMFAQkGBgIEBwIECQYHAQgDBgICCQUBAwkFBwMJBAkGBQkEBgcECAgI'
    'AQAHBQUICAUCCQMEAggABwICBQIDCAYBAQYCAgACBAcJAAUFBQMICAkGAAEFBAUHCAcDCQQCCQAICQgH'
    'BwIJBwgGBAcIAAMDBwQFAQUAAQYHBwgECQAFCAUHBgcDBQYIAQQECQYABgIFAAAJAAkEAwgAAwkFBwQF'
    'AAEHAwQEAAAFAwgFBwAGCQACBQIAAQYHAwAAAwEJAwUFCAIGAAEFCQMBAAAFBQEIAwEABAgECQkFAAcA'
    'BwkGCAgFAQcAAwgEAwMEAwEACQEHBQcDCAAAAgMCAwUHAwMIAQkCAQIHBwkGBQcEAQEDBwUEBQYFCAcA'
    'CQEAAAQEBQAIBwQABAUCAwABBwkBBQQECAcBAAYBCQMEBgMABgQDBAMGAgUBAAQGAwECBgcHBgYFAQUD'
    'BAEGBwQIAQAJAAcIAwQABQIFBgcIAwkCCAkEBwgDCAMHBgkAAwYAAQEAAwcHCAQDAQIACQAAAQQFAQIG'
    'AAcICAEBAwEACAUIBwYFBwMAAgkBCQcFBAUIAgABAAIDCAkJBQEDBAgABAQFAQACBQgFBggFCAkDCQYD'
    'AgkHAQgHCQQDBAYHBwkEBQQABQQEAAUJCAgIBwYJAQAAAggBBAYABQEGAwMAAgYJAAkHBAICAwIJAAYD'
    'CAABBgkECQIFBwcHBQEBBgAGBggHAAYHAAABBgQEBAMIBggGAAcCBgMIAAYEBwgIAwcECQQEBgIACQgG'
    'BwYCAAMBBwACAwEABAMFAQAEBQIAAQAJCQEGBAEJBwEIAwEBBwYFAwEHBAcHCQYAAwcJCQUCBAABAwUA'
    'AwYABgcFAAcEAQgFCQkICAEGBQIIBgIJAwIIBgYAAgEDAQUACQgAAwgJAAUHAQcBBQUHAQAHAwMFBQgA'
    'BQAACQkGBQkEAAECCAAABQMJAQMDAQIBBQQIBwQHCAMECAABBAUHBwUIBQYDAwgHBwkIBQcCAQQGBwAG'
    'BwcHBAMABQAIBwcBAwgBCQYIAAcAAwQHAQgGAAkDCQUABQMDBgMICAgDBAAEBwAJCQUAAQEFAgYGAAYF'
    'BwYHBQkFAQkABQgDBAgBAwYGAAEJAgYICQYFBwUJAwAGAAAEBwQHAAgHBgABAwIEBQYBAwACBAUDAwEC'
    'BwYGBcahsgABCAABCAAGCAcFBggHAwgJAgIDBwgDAgIABwkCCQADCQIECAkECAMJAwQDCQIGBQMFCAMI'
    'BwkHBQAIBwMEBQcIBgkBCAgBCAAFCAgAAQQCAgUCCQQABwkGBAYBBwkAAwcHBwkGAAgEAAcECQkJAQgC'
    'AQQJBwgBAAUGBQkJAQMBAwkJBAYEBAAIAQIACQYJAQMCBQYABQAFAAYHBggFAgQCAQECCAgDBAIHAAEA'
    'AgkABwIBCQcCBwYDAgMJBAMFBwAAAwAFBggCAwcACAQIBwMECQcCBwEACQMJCAMDBwcDBAcDBgUCCQgG'
    'CAMJAQQIAAIFCAcHAwkABgQHAwQHCQACBgMCBgQFBQMABgIFBQcDCQAGCAEHBggJAAEFAAEFAwkDAwMC'
    'BwMCAQkHAQgCAgEIAQEEBQMHBgQJBwAFAgMBAAQFAAMAAQQBAgkFBQUBCQIFCAkDBwcHAwIEBAgHAwgI'
    'AgIABAcABwABAAcCBgQHCAIIBgQFAgYDCAgFBAUCAQYIAggDBQIJCAEAAgkFAgUACQUBAgUIAwQFCAQC'
    'BAQBBwEBAwQGBwIDBAkJBQkGAgAIAAICBQkBCAgBAwcFBAgBBAYEBgYDBgUFBggIAQYABwQBBggBAwgH'
    'AAEEAgAACQEHAwcHAQkBCAgHAgkDAAACBQUGBgkGAAQJBgEFAQgAAQAJBQQDBQQHBwYHAwgAAAUBBQYB'
    'BQQFAwAFBQQDBwEDBwcFCAYBAAQHAgkGCQAHAQUCAgUABwADBAMDAwAEAQAACQYHAgYHBQUAAAgABgkH'
    'AQkFBwACCQMFAwEEBQEFCAYCAgcGBgEHCQUABQcDBgUABwMIBAQDAgAFAwUCCQMICAkJBwAAAQgDAwcD'
    'CAIBCAYICQYDAwAEAwUABAIHAAgGAgAIAQUDCAUFCAUDCAYHAQYJBwEHBAYCAgcDBwAEAggEAAQACAQG'
    'AQkIBggJAwUJAgMAAgcBAwgDCQAFBAkCCAYDBgkFAAACCAgFCQECBwAFAgUCBAYFAAEJAwQBBQMFAAQF'
    'CAEAAwkFAwAHBAYGBAAABQgJBwQFAQUGBwQHAAQJCQEABwIJAQkJBwIGAQYEBwcGAQcEBwQBCQMIBAEH'
    'AAEGAAQCAAAHBQIJBQgICQMAAAcCAwQGAgEJAQIJAwMJCQUEBQcEBwcABgEHAQIDBwQCAQYIBQkCCQMA'
    'AwEFBwIDBwUFAwcABAMBAwgGAQcABQkHBAQHCQYIAwcEAAUEBwkBAwIHCQkFAAEFBwABBAICAQMCCAgA'
    'BQYEBgECAwUFAQUEAgQABgEHBgQCAwUCBQAHCQMBAAIGCQEABgEAAgQBAgICBQQFCQkEBQcFCQkBCQEC'
    'BQMJAQUDCAYFBAAJCQgFAgMFAwQCAQYCAgkIAwUGBAkDAAAAAggACAkDBwIAAAAEBAIHAwQACAkJBQkA'
    'BQECBwIHBAkFAQIJBgcIBQUGAgcJAQkBAAUEBwMEAwgCBQgEAAAABAkEBgcDAQAIBQMBBwkIAQAABAUE'
    'BQUDCAYHAAkCBQIFAgIDBQIHBwEABAQDBQcBAwkGAgcGAQUCBgAFBQAABAAHAAMDCQgFAAIGCAcGAwkE'
    'CQUIAQIACAUJCAkDAggHAAUIBgYABAkGAwcFBwEBBgkBCAMCAwcGBwcJBgEAAwAFAwcBCAEJCQkGBggC'
    'AgcFBgUGAAEIAwYFAggEAQkECAUJAAcGBwABAwUEBwYJAwgHAgQJAgIHBwYBAAMHBQIDAQYFBQYIBgEF'
    'AAcGBQMCCQMIAAIABgUIAQcHAQQIBgEGAQQJBgIBAwkGAQYJAgAFBAkFAAkBAAcABgQEBAYJAAIACAIA'
    'BQIFCAcCCQQHAgUDAAQJBAcFBwIHBwMEBAkJBQQHAQMGBAAFBwYDAAEGCQYJBgkAAwMGCQEHBwYHCQYI'
    'AwUABwAEAgIFAAgCBQgGBwEJBwQCAgAGAwMEAAUGBwQFAwUHBggEAwYGBgYABgkJCAUGCAIJAgkDBQkB'
    'CQkAAgEABQcFAAgIAgUBAgEBAwUCCQAFAwIAAAgIBAgBBQIEBwgGBggABAYGAQABBQMJCAkBCAAEAAAA'
    'AQIBAAEJBwQABwIIBQABAQMHAwkDAgMFBAACCAkJAwAGAAIICAcGBgkGBwMCCAUIAAkBAQIIAAkHAQkC'
    'BAQHBwEHAgYIBwkJAAMCCQMACAUECQQFBQAEAwUFAAkHBwkGAAMEBQAHCQEIBQYIBAYABwMJAAMIBQUB'
    'BAYABgYEBQMDAgQEBgkAAgIDBgAICQQAAQIDAwQIAQUECAICAAgCAAUGBggIAgUEAAIFAgABAQYEBAQC'
    'BAMJBQUDAQkIAAgBCQQIBgMHAAEEBQgGAAUABgMGBgEEBgIACAMIAgUICAgCAgYBCAMGAAEEBAYFAQYE'
    'BgEGBQYDAwMFAwkABAgABgkAAwAFAAkEBQIFAAgAAwYFAgIEAwYHAQEEBQQBBQUABAkGBQgEBQcJCQYA'
    'BgYFCQIDAgQBBggDBQkHCAQCAwAJBggHBAEFBQkCAwMAAAUEAwQFCQgCAgcEAAcEBAcGAQQBCQgEAwgH'
    'CAIJAAEIBAQECAUECQEBAwgFAgEJBgAIAQcCCQYDAQkJBwkJAgUHCQIJCQEFBwEFBwkFAgECBwQDCQcD'
    'AQQJBAUECQkHAQAABAIDBAEBBwkIBgAAAgYABAIEAAACAQABBAQFBgQHAwQDCAYAAQYECQcDBwAFAwcA'
    'AAIAAwICBgYIAQEGAwYGBgQDAAcBBAMECAcDCQkCBAUGBwMGCAkEBAAECQgJBwYECAAABwMCBwMHBAkD'
    'BggJCAUHAAgIAwUGAAQGBwAFAgkIBgcFBQgIAAgGAAYJAgMCAwIIBwkDAwYCAQYBAwEHBwgABgIJBgYF'
    'AAMGCAAHBAMDAwkCBQUECQYDBAkBAgYCBgIIAgIGCQIDCAgHBAUEAQEIAwMJBAkCBAMDAwgEBwEABAQH'
    'AgYJBwkEAwAAAwAIBwgCBQEFAgABBQYDCAUCBwQGCAEFAgkEBAQDCQQIBwEGCQYGCAMDCQMIBQcHCAkB'
    'CAYECQcHBQACBQAEBwkJCAEIBQkJBgkHBAAFAwkFAQgEAwIACQcCBwEABwgDAwUEBwIFBwgGAQkDAwkD'
    'CQMDCQMIAQQGBwMAAQkEAQMJAgcDAwUIAwcGAQgJCAkDBQUDCAIGAQMCBgQJAQMEAgUFAwcBBwIEBwQC'
    'CAkECQUJBQUABQAGAwAABwEFCQMAAgcABQEFBAAABggABwEIAQgHAwcFAAUFBQQAAwMFAwYBBgMGBQIB'
    'AggABgIFAAIBBQcCBwUGAgUAAwgGAQEJAAYBBwABAwcACQQCAAIFBgAEBQcDBwMAAwMCBgAHAAUIAQYH'
    'BgUJBAMEAwAEAgEHCQEBAgkACAEABwAGAAcIAgEBAwABAQAEBgEGBQAHBwUDCQAEAQkIAAEFAwUECAII'
    'AgIABQMGCQAABggEAwAJAQcFBQQJBQgEAgQGCQYCBAcABwIABAYHAggCBgkBAQQGAQkGBgEBCAUBCAIH'
    'BAIEBwAAAwMJCQgCBgYCAwUAAggGBAYCAQUGAgAABQYJAQMABAEAAAIDAAMJAQUEAgAFCAQIAQkEBwMD'
    'CQcEAgIICAEGCAgEBgUDBAUECAkCBgcCCQMJAwcEAwACAwMGBQcACAEGAwAFAAMCBAICBggJAQgEAgMF'
    'CQcBBAEBBgQCBAEEBwEIBAcDBQcABAQAAAkACQYJCQAEAAYGBQUCBgkAAQUACAUBCQMAAwkIAQcHAwQI'
    'BQgBBgEHBwMBBgcGCQQFBQEGAggCBggDAwAAAgIFBgQCBAAEAAMICQUAAQYABQMIBgcIBAgFCQcDCAkI'
    'CQUDCQYCAQYHAgkBAwEDAwgIBAIHAgMJCAgDBQQCBgcABwEIAAMBAQQBBgcCBQIIAQABCQUIBwgFBwEI'
    'BwUGBAkBCQAFBwYFCQAFBQcGBQcJAQgIBwECBAUBCAUBAggHAgMJAwEACQYFAwkJAgQCCAgDBQIBAQcA'
    'BwQJAgUBBAIJBgIAAgMACQMEBQUEAgMCBggEAwIGAQQACAUGBQIDBQMJAgMEBQEECQkDBAEDBAgIBgcE'
    'CAkAAgcBCAgIAgkHBgYABgQIAAIDAAYJBwMCBAUEBAQAAwIBAgQABwgIBgYDBAMECAQJBgIGAwEDAQMG'
    'BAcACAcAAgEBAAgEAQQBCAMEAggIAgYBBAQFBgIHCQcFAwQCBgIBBwMIAAYIAQADAQgABAYACQEEAQUD'
    'AwMBBAgEBQEAAwYHCAIEBAIJBQIECQADBAAIBgMACAgDAgIHBgUIBAQIAwIDBAEICQQGBgAGAAcBAQcA'
    'AggDBAADBAAIAgYHCAMEAgIHBAADBgUCBwMAAgEHAggGBAkBCAgDBgMGAwEFAgkHBwEJAwIGAAQBAQgJ'
    'AQEJAgAIAAEBBwMCAQMHAgIHCAIACAUAAgkDAwgHAQgHBwQDAQIEBAcHAgQABAUDAwEGAAUFCAAHCAED'
    'AQAAAAkJBwADCAMAAgkJBgIDCAIEBAAFCAYABAQEBAAAAQgGBgAFAAUCCQcGBQIIAwEBAgECAQkJBgEG'
    'AQICBwABBQMFAgUHBAgJCAkHBAkJAwcJAAkEAwkABQQFCAkHAwMJAgQFBwMBCQMCCQUFAAkHBwcGCAMH'
    'AQUABwQICAUCBwcABgUEBgcJAgIECQQJCAYEBwgJAgIABAMIAggDCAkJBwgBBwMFCAECBAAECAEHBQEB'
    'AQYFBAkGAQcDCQYIBQUIAQcGAwgFBwIHBgAEBgEGAQMFAggIAAUIBgYBBgcIAAQDBAIIBgMIAQQECAgC'
    'AwEIBAEBAAICCQkEAgkJCAUJBQcFBwIGBAMDCAUEBwYBAgMGBQYHCAgHAAMFCQQABgAHAQIFCAcEBwEA'
    'AgAJBQMABwYBBQAGCAIABwgAAQUBAgkHCAUCBwgBCAQBBgEIBQIACQMHAAYJBQUFBAIABAcHCAkGBQAD'
    'BwMFCQMABgUIBQYBBwgEBQEHBgUJAgcDAAQBBgQGAgkEBAgCBAYACAkCBgQJBQMBAAgIBwkAAgMFAwAB'
    'BQgHAQUHBwMCCQMBCAIABQcGBAgECAICAQMGCAIDBgAEBwQFBgAFAgkACQAEAAAJBggJBwcECQUAAQAI'
    'CQYCAggIAwgCAAcDCQcIAwIFAQAJCQQEAgkDCQABBgIIAQQDCQQDCAAIBgkFCQEDAwkEAQcJAgEBCAYC'
    'CAUEAQUFAwYGBwQBAQAIBAIICAYJAwkGBggEBQUGAAkJBgIDAAMJBgYGAQEAAAMDBQQHBQkCAQYJCQEF'
    'BgcJAgMBCQUCCQcABwYGBQIAAwIHBAUEAgQHAgQBCAYICQcCAAgEBQcDBwEHAAIABwkABgkFCAADAwkI'
    'BQUHAwAACQAFBgIDCQcAAgIBBAkDAwkBCAEEBQEIAQkCBQEEAwMIAgAACAcDBQICAQQCAAAFAQcDBgcH'
    'BwkAAAIEAgAEBQEHBwMGBwgACQUFBwUABgUFAgQBAwgICQAAAAYEBwEIAwgEAAEEAAUCCQEIBQYGAAkH'
    'AAkBBwUBBgMDAQUEBAACBgQGBgIABwQGCAIJBwIJBQYCCAkICQEACAYJAgQJAAABBQAFCQgECQYGBAcB'
    'CAYBAQcFBAcBAQQFAAkIBAQHAwQHBwEHAAUEAwIGBwAIBwgFBwMDBQAIAQUGAggHAgMEAQkCBwIFAAAI'
    'CAMDAQcJBQIBAwUFAgAEBQYDBgEFAgUGCQYCCQcFAgAJAgEABQgHBwUEBQkIAQcFBQMECQAIAwQABwgF'
    'BwUGBwUHBwUDAwQGAgMABgQECQEJBQIJBgkHBQMHAgUDAAUICQkABAAIBwkEBQgHBgcHAAAFBAIJCAgJ'
    'CAUFBgMCBAUEBwIBAwQCCAQAAwACBgUEBQECBQkFCQEBCQEABQgABgMACAIGAAACAwAGCAkAAwgFAQAH'
    'BwQHCAkABQYEBwQGAgcIAQkEAAgGBQIDCQgDBgkBAQcABAMCBQACBgAIAwkACQEECAIDBAUFCAYDCAgB'
    'BAEHAgMFBAMDCAgEBQMICQICAAUFCAIHAQIGBQIBAwgCAAQDAgADBgADBQMICQYHCQQDBgMJBQkHBQIE'
    'BgMHAAUECAgABgEDAQAIAgYBBwUGBgMJAwAIBwQGBgcIAgkGCAkFAgQEBwMFAAYBBgUDBgQIBQAEBgkA'
    'BwEJBwIJAQcABgcGCQkJBAAFAwcJBAUIBwkJBwkEBwcFAAMJBAkBAwIEBgIFBgMFBgAHCQgJBQkBAgcD'
    'AgAACQYEBQIEBggGAAEBBgQIBgMBBQgCAwEGAwYHAwcHBQECBQcGBQADBQgJAgADBAQBCAkFCAgICAYD'
    'AAAIBQUAAwEBAAAEBwUBBwICBgAABAcIBAEHBwUJBgYAAgIABAkIAgEJAgkEBgEFCAYJCAMJBwAFAQQF'
    'BQQHCQIIAgIABwkCBQIGBgYIAgUFCAYJBwQICQcCAAAGAQgJBAgICAcIAwYGAwEBBgcABwYFAQkEAAII'
    'BgEIAwcHCQMECAcFBgEJCAkIAgMHAggGBQUFBwQFBQEHBgIIBwYCBgcJCAkBAQYFAAcABQcEAgIECQMA'
    'AAIHBwICAQAGBAgEAQkGCAAEAQUHBgkACQQEBwQFBgYACAEIBAQDAgEDCAMAAwgDBwMJAwAGBAYHAwAJ'
    'AwUAAwkCAgYGBQMGCQEBBgQGBwcHAQQJBwcAAAIGCAUDBAQGCQAIBAkJAAkFAwYBBggHCQkBAwABCAcA'
    'BwcGAwgAAgMICAEFBAEAAAkDBQYHBgMJBQAEAAgAAwEEAwcHCQMBBQYECAUECQkICQUCAAMGBQgHCAAB'
    'AwkEAwEGAQQABgQIAQcICAUCAwYABgQBBAcHCQUHBwgAAAUJAQYECQQBBQcBBwcIAQAACQUDAwUABQQH'
    'BQMEAwQABgAAAgYGAgIJAAIGAAYFAwYCAAYGAgYBBwcBBAgEAggABQAGBQUHCAIBAQcGBgUEAQkEAAkG'
    'BQQDAgMJBwcCBwMHCAMHAwMCBwIEAAAHBggHAQAICAAJBAkCBwEDAgcJAgQIAQIAAgAJBQkDAAcJAQEH'
    'BQMJAgADAgcJBQACAwAFCAACAgEFBAkFBgUBCAYFBAcFBgADAwkBCAAJBAIIAwkJCAAABQEBCAYBBwIB'
    'BgMIBwIEAQUBBgUEBwYDAQQBBAQACAEDAwACAAYGCAQDCQEEAQcBAgQIBgYGBgIGBQICAAUACQIJAQAA'
    'AAgJBwUBBAgHCAQGBAUJCAcAAAEFAwYBAwcABQQEAwYICQIICAABAwMJAwQHAgkBBwAACQkDBQAACAEH'
    'AAcIAQYABQQBBAgIAwIBBwAJBgMGBAYIAAcJCQACBAADBgEFAQICAAQEAgMIBwcBAwYJCAUECQgGBwQC'
    'BQQGBwkGCQYBAQMDAgAGCQcCAgkBBQEIBwEBAAgHBwgACAgGBQMEBwkCCAQGAgAEAgMCCQEDAAgEAwYH'
    'CAQHBAIACQMJAgMAAQgABgQIBQQDBgMHAQEGCQIHAwMDAQEHAAYHCAMFBAUFBgUEAAgACQMEBwECAAIF'
    'BQcEAgkACAkABAcACQEAAwQBAwcABAkAAQQEAQMFAgYFAAEFBggHAwADCQADBAcCAgUHBQkJAgQABwUI'
    'AAQGAwMDCQQJAgIBAgMJBgQACAgGAAIHCQAABAIGBgUGAAEDBgkIAgUDBQAGBAIJBwMABgcGBAgJBQMJ'
    'AgUIBAUJBAMGBQMJBAYFBAgHAwcAAgMJCQIJAwMJCQkGAwQDBQEBAQMAAwkICQYEAgACAgYAAAEDAgYA'
    'AAAIBgUJBQcHCQMICQIEBwAFAQQFAAkJCAUJCQUJBQgHCQIHAwECCAMIBwIIBAMCAgkJBQMCCAAECQYG'
    'AAUIBwYDAAcCAwcDCAQEAQcBBgYAAAQBCQEIBAIHAgIAAQUABQkEAQEDAwIDCAIGBQgGCAIHCQQGBgkB'
    'CAMABgUJBgQHBwMFBgkFCAEBCAgDBAEDBQcFAgYCAwQIBQMABwMABwcECQgACQkGCQgACAICAgYACAYF'
    'AQgCCAMJBAYGBAQJAwQDAQMJAggCBQYEBQgIAgEGCQQHBAgJBQUBBAkAAgcECQEIAwkHAgQICAEGAwUI'
    'BAMCBgUDBAkDAgEFAgcEBAEJCQgABwUABwgJAgcEAgMABQcIBQIBAQEEBgEABQcAAgECAQcCAwEGBgQB'
    'AwEIAwMGBwIGBQUHAQIDCQcJAgAIAwgDAAcCBAEECAIDBQUIAwQEBAgABQYJAgYIAAECBAcACQQFCQEC'
    'AAcBBwgECAkBBgACAQAICAQJAAEGCQIAAQAACAABBAgHAwgHCQkGBgEGBAQIAgMGBAQGAAEEAggGAwIA'
    'AwMGBAYIAwQCBgYIAwYCCQAHAwAECAgHBwgBAAQFCAcECQIDBQgJBAUHBAUFBgIHAAcABgUFAwIGBgEF'
    'BgMABAMACAQICQECBQUIAQUFAwgDBgcIAQMDAQkBCQAJAgkHCQkJBAMIAQICAwgECQYHCAAABAEAAgYC'
    'AgYHCAUAAgYDAgMAAAgICQECAgkJBQEBCAUAAQYJAgICBQkBBAUDAwgABwIJAwMJAgQHAwYDAQgFAwYD'
    'AAYFBAgAAQUABwYGBQUIBAIBAAcFBAQBCQUBAwUDBwAGBQEBCAAEAQQBAAkCCQYEAQAACAcCCQgIBAMJ'
    'AAYJBQUCAAABBwEFAQYHAQQIBwkACAEDAAEFBAQABggDBgUFAAABAAgJBgIGCAMHCQIFBgQABAMHCQMD'
    'BAUFAAgCBggFAgcBAQMFCQcHAQEDBwcABAAGAgQCAgUHAgcCCQkEAwEEBAUEBwkHCAEGAwcEAwkJAwQJ'
    'BgcHBAkCBAUECQAEAwMJCAQEAgQDAAEGCAQABQABBQMDCAMEAwEABQEBAQQJAwIEBwQHBAIDAQAAAQQJ'
    'AwcBAwEECQEACQgAAAYFAQQDAwMGAgcEBgYABgEACQMJAwEECQECAggEAgkJAwgJCAABAQMFCQQHAwEA'
    'AAMCAwMFCQYEAgQABwYIBwIABwIIBwMHBQYIAgMDAgUABAUICQUHAwQHAAYABwcBBAYGCQIJAwYIAAII'
    'AwAJBwIABgcECAEJBQQBAQkCAQACBgMCAQkBBAcIAQEACAcHCAQIAQcJAQYJAQQBCQUBAwQBBggHCQAG'
    'AggBAQQECQYBBgkGAQUHAgkDBAQHBwEFBQgACQUFCQIFBAMGBAMDBgAJBQMDAggABAUIBwEHBAYBCAcJ'
    'CAEFBQYJCAMECQECAAQFAwEEAwgBCAYHAQgFAwQBBAYBCAgCAgIEBwMFBgYFBwcAAwcABAcBBQMCAAEI'
    'AgQFBQkBCQUHCQkFAQYEAggIBgcFBAADBgMEAggACAIIAwEFBAcABQYCBgkIAwUHAAUCCAkABgABAQcC'
    'AgkJBQcCAggFBAMCAAEHAgMBBAgHBAMCAAkHBwMFAgUDBQYHAgUGBgEDBQYFAwEDBQkHAQYIBQUABQID'
    'BQcGAgQBBQgJAwcEAgcFBwABAQIFCAgGBwIGCAkJAwICBQgFBQYJBwkIAQMCCAEEAgIJCAYBBQcJBQAE'
    'AwIBBQQIAggEBQcDBAYICAMDAQMGAAEBBwABBgQDAQEEBQkFBQIBBAUCBggJAAQIBggICQgGCAQICAkA'
    'CAkCAgMDAAcHBgIAAQMBAwEECAYJCQMFAQkJCAgBAwMEAQEJBAACAAQJBgcGAwkFBQUICQMICAEBCAYA'
    'AAcHBgMDBgYIAAkFCQQAAQMGCQMAAQIHAAgEAAEDAgUCAgICAwEFAwYJAAMDBQQCAAkGBwQIBAEDAgkJ'
    'BAgDAwcICAIFAwEJBgcHCAUACAIDAQQFCAECCAAECAADAgUABwUEBwIEBwcGBwcEAAICAgcJAgYIBwIG'
    'BwEABQYDBQYEAQgJAQMCBQMFCQQABAQIBAgJBgQDCAkEAwAABAYFAQgABAQDAgcJBwkEAwQDAwkDCQgJ'
    'AwYECAcIAwQFAQcGAQkFAAgEBwcABwMHBQUEAgUAAQQDBwUGAAQIAgkIAQEECAgCAgUIBQUDAAQGBwkF'
    'CAIGAAgEAQEGAAgDAwgIBgIJBAUJCAMBBAACBQAHCQIBCAgAAAAGBwcEAwYJAAAJAgYEAgACCAACCAEA'
    'BAAFCQcABAMGAQEDCQEGCQYCBwIBBAYDAAMABgEJCAYHCAcHAgUABAcJCQAFBwICBQkDAwUFAgcIBwAG'
    'BQUHBgECAwIEBAcHBQUABgcDAQkJBQgBCAMHBAYICAYCBgEFBwUBCAYAAwUAAQICBQAICQEDBggFAAkI'
    'BAAIBwcIBgEHCAQBAgYHBQQHAwYGAAMDBwYGBVcBAAABAQUIAgICCAgBBQUABgcAAgMCCQcHBAIJAwEA'
    'BAUAAQgACQIEAwUIBwUCAwcJCAcHBwYGAwYHAwkAAQYIBQYABwQHAwcCCAUEAAkGAwkBAAgFBAMDBAAF'
    'AQIHCAMBBQgHCAcABwMGAwAEBQAEBgkFBAkEAgUABQkHBAgFBwkIBQAGBQgEBAQCCQIGAwkCBQQEBwYA'
    'CQYDAwAFAgIICQUIBAEICQkHBwkHBQkHBAYABAIDAwYABAUDBgIIAQEABAcHAgMHBAMCAAcFBwkBBgQB'
    'BAcAAAECCQQDAAYHAQMAAQAGBgkCCQECBAEJBQgABAAFAgkICAAIAQcECQkBAgIICAAFAQABBwABBAAG'
    'AQcFBgEABQUIAwYFBQYJAQcEBgcFAgUAAwgJCQUHBggJAgYIBAcDCAgBBgkGAgYBAQkDBgQCCAUFBQUF'
    'AwQJCQkBCAgABQUACAYDBwcEAQIBBAUDBQgDAQEECQIDBwAAAwYCCAMHBwkIAwIFAwkBAAcGBAUIBAIC'
    'BwEJAgUJAQgHAQcBCAkFBQECBwAAAQYHBgMBBAQJAQYHBQgCBAQBCAAEAggJBAIABQIDCQABCAACAwMI'
    'CQMHCQMAAAYGBgcBCQYDAwEDCAEAAwkABwkDAgcACQQCAgkCAAUBAAIJCQEFAAYIBAAEAAEDBgIHCAAE'
    'CAkCBAIEAAcDBQQABwgDBAACCAkECAQEBwEABgQBAgcGBwQDAgMFAAAHBwcHBwUIBQcAAgUBBgIJAgMB'
    'AwcHAQYGAQcIAwkICAEEAwIAAQAIBgAAAwQICAcABAQJAAQECAUABQEHBAIDCAgFAQAIBAIFBgAJAAUH'
    'BAkJBwgFCQUHBgIFBgUHAQIBBAcFBQEHBAgHCQgACAUABgQHBwMCCQYECQYABggFCQcACQIGBAkBAgYB'
    'AwIIAAIGAQkJAggECQkGBwkDBwkHAAQFCAYABAIDAwcGBAIIBAcAAwUBBwgHBAQDAggDBgEBCAkGBAEA'
    'BAUJAgAFBwkDAgMICAQFAggGAwUJAAUIAQUIAAYGAQcBCQIIAAABBggECAEFAgEAAAUDCAQFAwMEBAkH'
    'BgAHCAABAAEECQkABQUCAwQCBwcABAcGBgYEAAkGBQcICAQJCAgFAwMDBgICAQAJBAEACQgJAgcJAAMC'
    'AgECAAgDCQgCAAkEBwEDAwgJBwIFCAAJBgEGBwgJCQkGAgUABwcHAAADBgQCAwAFAwEIAgAABgAEAwEI'
    'AQkIBgUDAAUDBQUGCQQABAYIAQcGCQUFBwAHCAkABQEDAwMCAQYJCAMHAQcJCQYGAwIECQIEAgQAAQYE'
    'BQMFBgcGAwEBCQUJBAQJBQIEAwMJBwIFAggBBgAHAwYGAwIJAggFAAMIBgUJCAYGBgkDAQkGAwYIBAAF'
    'CQAEBwkEAAcHAAQHBgMIAQEFBAMIBAAEAgQJCQUACQICAAYCBAIJBAUJBwQIAQkDAgkBAQEEBggECAkG'
    'AgMCAgkJBwUFBAIABAUIBwEEAAMICAgGAwMIAAMCAAgGCAYDCAUHBQQHAQQGAwQABgkBBwIBBgMCBQkJ'
    'CQEBAAUIAAQGAAQGBgAAAQUGAAEJBAgIBQUIBwECCAQEAwIABgQGCQkGAQkBBwMCBwcJBAAGBQkEBwIG'
    'CQIECAAIAQkFAQUHCQIHBAUAAwUJAgQHBgEECAYJCQIDBAYJBQIEAwMIAwEJBwAIAwMGBwcAAAYGBgcG'
    'BQYGBwACCAICCAUIAgQACAYDAgYABwgCBQYIBgYJAQcEBAYEAAkJBwcJAgQDAwkEBwUDAwQDAwEHCAEC'
    'AAAABwMDBAcCBwMGAgkGCQQGAAYHCAgGAwMFBQgEBgcCBwkCBwMJAQcFAQgBBAICAggBBAgCBQMCCAEA'
    'AAkCCQYDBAMBCAMHCQMJBwkABAYJBgQDBQQFAgEIBAgJAgEACQkHBwcEAgQECAAAAQQGBAgHAwAEBAMH'
    'AAEEAwQICAcHAgkHAAMCCQQDAggIAwQGBAkJCAYFBAYFBQQEBQkAAAADBwUGBAQJBAMGAAgIBQQHBgQF'
    'CAYBAgcCCQEABwYIBQkCBAgBAwkECQIIBgkEBggIAQEICAQIAwAGCQYAAAkDAwQGCQcFAgICBgEEBAEG'
    'BwQDAAMHAgIICAkBCAAIBQUJBwQDAQcBCAEBAQIHAAcGAAIDCAkFAwcAAAEABwUEBgMIBAQEBAMBCQUE'
    'CQUGBQUFCAUFBAIIBQACCQkJCAEHBAMCBAIHAgMCCQIHBAQCBgkDBwEEBAAABAQGAQUFCAEBAwgEBQYF'
    'BgEBCQABAwcEBgQICAIIBwcFAAUGBwMCAQgCAwUFAQcIAQYJCAUFAQQBBwYJAggJCAgEBgcDAAkHAgUA'
    'BwgGAwMHCQAFBgQCAgYGAwIDAAYBBggEBwgABwECAgQABwMFCAUDCQMEAQADAgUABQEJAwAFBQMJBAQI'
    'AgkABQgCBAEHBAECAQgGAAgHBgYEAQYGBgQHAAMIAAQJBwEDAwQFBQMGBgUJBAMGBQYCAwkHCAEJBAgG'
    'CAgAAAgACQcJCAQIAgkFBQcGAwAHCQEFCQkFAwMDCAcACAEICAEHBwYICAQCBwgBCAcECQYFBwYFBwME'
    'AAUACQIHCAkAAgUFCAkFAQEABgQJAQYJAAIHBwcGAQkIBAkABggBCAQDBwYCAwIFAgQIAAACBQQHCAgH'
    'BAAHBgQAAgIBCQUABAAGAggCBAEABAUICAcGAAgBCQEDAAAAAwUHAggBBwYBBQkHAgYCCAcHBwQICAQB'
    'AAIIAQUFBAEAAwEBAAcHBQMFBwUHCQQABQQDCAQFCAgBBQICCQIJBQQEAQAEBQgDAQQBAAgGCAMGAwQD'
    'BAMHBQkFCQIJCAYFAgQHBAUBCQUCCAIACQQABQYDBwQDCAEJAgYJBgEFAwAEBwcHAgMBBQUFBAICAAYH'
    'AwUEBQQDAgMECAQJBwADCQcBCAUGCAMAAQUFBgIABAgCAwgGCQIHAwMACAUHBQMEBwAABgkBBQQGBAMF'
    'CAgIBAMEBAgFAQMACQkJBAYEAgUGAwgGCQABAgkEBAAABQUCCQQDAgYABgIIBgQFBQEGAQgBBAcDBQUH'
    'CAkBAwMDCQUDBQEACQEFCQEHAQkCBgAHBAABBQABAAQHBgMDBAEDBgcABgkABgcCCQkFAQMJAwcHBgUG'
    'AAcJBgMEBwgJAgMJAwYDBAIDCQEAAgcGAAgBBQcGBAMGCQUEBwQBAQADCAIIBAAABgIEBwQIBQUJBgAE'
    'AAIDAAEBBAEJAQEBAwMIAggDBQAABQIBBAMFBQYHCQcEAQkICQgGCQYHAgMGBAUFBAcACQkDCQkHAwkC'
    'BwgJBQIGAAQABgQEBgIABwMDCQUCCQIBAQEJCAAJAgcJAwQGCQgIAAYJBAQICQkFAAkCAQIAAwMIAwMA'
    'CQMGCAAICQYHBAEBBwMAAAEJBgUICAcJBgAFBAIJCQYHBAIGAAUGAQcCAwcJBQAFAAkDCQMEAAcACAUJ'
    'AgMBCQIJBgMCBAcFCAcJCAYIBAkBBQUIAQYHBAIFBQYBBAEJBgIHAwYGBwYBCQgDBQABAgcCAQYHAAIH'
    'AAUIAQMFBgUDCAUHCAMEAAcIBAMCAAgBBgAHBAACBgkEAgcHBAICBgUAAAYGAgEBAQcBAAAEBAUFAAMA'
    'BAkJAAYFAgACAwEACQUBCAkBAAIDAgMHAwADAAgJBgYBBgYEBQEHAgQACAYGBAUEAwUFAAAIAwAGBgIJ'
    'CQcDCAgJBwIDBwcDBwMAAQAEBgQIAAEFBQMBBAUBBQUJAQQEBQgBAQIFBwIDBgcEBQMEAQQEAgAFAQcB'
    'AQAIBgkFCQEGAQMIBAgHCQMAAwYIBAcECAkJCAEFCQkDCAkBBggIBgQDCQkIBgMABwADAQkHBgcFAAEH'
    'BAMEBAMHBgQEBQkGAAgGBgYHCAcABAADAAYFAAcCBgIIAQcIAgUABQMBBwACAQMFAAEIBwYEAAMGAwkE'
    'BwQIBAYGBAYFCQYEBwcFAggDAwIIAgQJAwAHAQUGAQkJBgkHBwcEBwcBBAAFAgEEAgUBAgkGBQIHCQgC'
    'AgIBCAQDBQUCAwAABAMABgYGAQAEAQYJBAcJCQECBAgDCAcABAUABgUAAgEBAQADAgIJCAUHAwkDAgMI'
    'BAAIBgMIBwACBgICBQABCQECAgYGBAIHAgkECAAECQYAAQgAAQcACQMFAQcCCAMGAgUHCQABAQEDBggI'
    'CAgJAwYDCAcABQMHBQEAAwcIBQEGAAcDBwUGBgcHBAkGCAUIBwEEBAEABAgBAwAHCQAHAAgEBgEHAgAA'
    'AwcDAQEJCAMFBAAFBgEJBAkEBAUHBAACCQkHAgkIAwMCBAkGBQAABQQIBQMAAAABBwQEBgIHBwADBgAI'
    'BwEGBQEFAQAHBAgICAMJBAQFAwgBCQACBQYJBwUEBQYEAQEDBAkDBAYBAQQGAwAJBgEHCAMHAgABBAIH'
    'AwUFBwUCAgcFBwYCAwYDBAEBAgYGBAABBwIGAQQCBAkEBgQDAwYGCAkIBwQDCAUHAwMGAQcFAQcBCQUA'
    'CQcCBQEIAQcJBgYDAQUCBgkGCAIFAAYDCAgEAQYFAgEDBAcBBwQDBwMGBgcJCAAAAAMACQEABwQIBwYG'
    'AwEAAgIBAAQFBgMBAQAACQkICAAFBQIEAwYJCQUAAwkABgIAAAcJBAYDAAYCAwMFBAYCAAQHAwEHBwYI'
    'AgAHBwcAAAYEAQcBAgcJBAECAwAFAwMICAIAAwQBBwAABwgECAQAAAEEAggHCQAABAAHCAYJCAYHCAgE'
    'AggICQMFBQgABgUJAgkABAQBAAAGBgMCCQkEAQYHCAcFAwgEAAAHAAYDBgIFCAMGBAACCQUGCAcDAQYH'
    'BAMBAAAHBQcGAgQCAgkBCQkBBAcHCQkHAQQECAkJBQgJAQYBAgEABQMBBwAHBwMIAAAACQcAAQcJCQUF'
    'AwMABQQECQIFBgYAAQYCBgMJAwUHBAMAAQgABQgIBQkJCAUDCAMIAwAAAwgFAwEAAwMIBwYGBwcACAMI'
    'CQcHBAEHAAgGBAEGBAEGAwcJAAAGCQMJBgQABgkDBwMACAYJCQcCAQUIAQAGCQYBBwEFAwgACQQFBAQE'
    'AQEHCAAJBwIEBgMHBwYJBAYCAgQFAQAABgMGAAEABwcGBgUCBAEBAQkJCAkFAwQBBgEGAQMJCQIDAAEG'
    'BgkHBAgJCQIDCAkGAwABBQgGBgIAAQgCAwIICAMCAAEABQYICAEEBwYBAwYEAgQIAwQHBgEDBgAGAgMH'
    'AQYEAQMHBQkDAwQHAQkGBQIGAwMCBAYCAAkBCAIBBwQDBAIFBwMFAAEABgAAAQcABwUABAEFAwMDBggH'
    'BAAEBAYJAQQGCAcHAAIDCAQJBAEEAQMAAQQDAwkDAgMEBQICAAQCBgAFBgABCAUABQgIAwcICQcBAgME'
    'BQcAAwMIAAYJCAIDAgMBAwYFAwQAAQgBBgUGBgcDAQUEAwIJBQYEBAgAAgUDBAIFAgUACAIFAwgJCAMD'
    'BgIEBQICCQUFBgYJAQACAQMHCAAJBggFAgIABgkAAAAGCQkABQABAQkHAwICAQIEAwcBAQMABgkGCQEE'
    'CAQHBQYCCAACAAAICQYBCAgDCQAEAwgCAQkAAgAFAwYAAgUGAAcHAgADCAIAAAEIBAAIBgUBAwcABQUF'
    'BQcAAwgEBwAJBAIEAgkCCQkAAAMEBAUCBQUECQYABQMGBAIIAAYHBggBCAgCBgMGBAcIBwEGAwEEBgMC'
    'BgIECAkCAwUHBAQCBAcHBAkIAAICAwEIBwQFBwIHAAgEBQMEBQEGAQcJBQgGCQIJAAYHBwIHCAEABAEJ'
    'AAcEBgIGAAQBCQQABggJBQgGCAQACQAFAwUHAQcBBAYFBAEDBQAIAAAHAQIJCQkAAAACBAkABwUBBwEB'
    'AAAIBwIDCQYJBgYECQQAAQAGAAcIBAYICQMFAAMCAwUCAwACBAkECQUJAwQABwIFAgQCBQQBBwYDAAkF'
    'BQUAAwAGAwIHCAgBBQYIBAEABQMICQYJAgUEBgEHAQkBAAAEBQcFAwcIBAYBBAAGBgAACAMIBQAHCQII'
    'AQUGCQUHAQECBwkDCAkBBwYDBAEAAAECAwcABAAGAwQAAQkEBwUBBwQHBAYGAwEFBgACAQMBBAkIBQME'
    'BAEGBAIJAQYBBgAACAEDBwIGBAIDAAIBBgEDBQcJBgMHBAEDAQUAAgAGBAYBAgQGCAACBAIICQkHCAIG'
    'BQgGAAEHCAUFBggABwkFAQkGBwIECQIEBggCAwAEBAUDCAAACQkDAQQFAgUBAQMJAQIECQAACQAFAgEC'
    'AggGBAUDAwAGBQcHCQUJAAABAgYGAwIFAwICAggAAQcIAQUCBQEABgkDCQcAAwAICQMBBAUFAQMGAQYC'
    'AQMEBwUHCQEGBAUHAgEICQkJCAkFBwACBwQEBAMFBwUGCQcFBQMBAQYABwAFAAkHBQMJAQgJAAcEAAUI'
    'AwMEAwkDBgIGAAMCAwUEAgkBBwcHAQgDCAIHAwIEAwEAAAIIAgEHBQgHBAYAAgMJCQgFAwAABgcCAgAF'
    'AgIBAQAGBgYJBgYABAgHBggFAwEBAQYJAQUFAQUECAEHAgkDAAQFBgMBAgEGAQIDAAcIBwcJAgkFAwgG'
    'CAMFCAMEAwAGAAECCQYIBwUABAUABQYFBgMHAgQCAwgBAgIHAgACBgUEBgMGAwUFBQMDCAQECQUABgUA'
    'BgQIAAADAwUCAQgJCQMJBwcAAgcDCQEBAggFBQAGCAAJAwgEBAQHBAIIBgIGAQMHCQEFAgYDCQYEAAgF'
    'BwgABAMDAAQEBgYBAwcCBgAHAAYCAQMEAQgGAQEGBQkIBQQIAAABCAIIBAcIBwgACQkBBQABBQUBBAcI'
    'AwMBAQMACQgHBgEDCAcBAgkDBwUFCAYHBAAEAAIABAcGAQgHBAAFAwkGCQkJAAgJAQUBBgkHBAMEBwUG'
    'AQcFBwIBBAEAAggEAAcJAQgCBgcABgYDBAQBCQQFAwcGBwIBBQcDBgEECQgHAwkFCAcBBAUICAYHBAQC'
    'AgUCAgIBBwYACAMHAwYBCQYCBgADAAAAAAMJBAICAQAFAQUBCAcCCQYIBgcGAAEEBwkGBQgECQAIBgAJ'
    'AwcFAQIABgEBAQkHBgcBBQAFBgADAQEABQUJAAEBAAkJAAUFAAQJAwYECQUDCQAEAgEABwMECAIFAwAG'
    'CQcACQUFBQAJBQUGBwIFCAMJCQcBAgMGAwcACAUFBgkDAQIHBQAHBQQCAAQBBgMABgUCCAcFAAYHBAMJ'
    'AQYBAQECBQQFAAEJCQcAAQkACQQCBwgCAAUDCQkACQMCAwgFCAIEAQcJAwIAAQkFAAAEAwEDCAcCAgUD'
    'CQMICQgEAQkEAgkBBQYDAgUIBgIGBgIICAcHBQIGBAkACAUGBAEGBAIBBgUEAggABgQEAwkEBQQIBwQB'
    'BQgBAQMJBgAABgUI5SEAAAkDBgQBBgEFBwIFBQgFBggCCAMBBwkEAgQJCQcFAAcGBwkACQUCBgkFBAMF'
    'AAkHAgkBBAMHBgcEAgUJBAEJAgADAAEHBwEBBgECAgkIBQMIBAUCBggJCQkCAwMAAgkHBQMDAAEIAAIB'
    'AAECCQYCCAMCAwcDAwUHAgYBAwkGBgQAAgMEAAYIAQUBAwYACQgJBgcEAAABBwMACQcAAgYJBgICAAgD'
    'AQIDCQQGBwcAAgADBQgJAwkDAwQCAwABBQkABQAHBwcDBQUACAAACQYFCAUFAAAFCAYGAAgABwEECAED'
    'CAQDAQQHBAEJCAMIBwkAAwEABQUHAAcECQcDAQEBCQcJBwgHCQQJBQECAAEGBgkGBQUICQQFAgEAAwcB'
    'CAAJBgMCBgEJAgIHBgMDCAMFAAUBBQMBAAUHCQIGBwkDAAcJAAMHAQIFBgkCCQkEAgcGBAADCAkGAwkH'
    'AQMIBAUEAgEJBQUABgMFBAAECAgCAwkEBAkBAgUCAQgDCAgICQMBCAQCAQUIBQUGBAEGAQICBQECBAMA'
    'CQcECQkAAwIICAgAAQEFAwAABwEFCAgHCQcJAwYHAAYDAAcICQYFBgkFBAYBCQkBBwMIAgAFCAcBBAIA'
    'AAgIBAABAwEBBAADAQYABgACAAIFBQgGAQAEBwcCBgMCAAUDAggJAAMBCQQJAwkCAwMABwQGBgUCBgQC'
    'AgYJCQYFBggIBQAFBwYBBwcABgYDBgcEBAcGBgMBBwAIBAcABwYEAAQCCQUJAgABCAQIAAYIBgAHCAIB'
    'AQMJAgkHAQEIBwABBQAFBwMEBAMACAAFBQAAAggDAQYEAwUJCAMJAQIIAAEFAAgECQQIAAYIAwkFAAEA'
    'BAgHAgYHAwkGBAQCBQkEBQEBBwUJCQEBCAQJAwUIBQAJBAMHBgUFCAQJBwMIAQUECQkABggJAwADAwgB'
    'BQgJCQMGBQIGBwcBBgkCCAMCBAgICAcHCQkDBQQDBQcICAMAAAUAAgADBgECBwQBAgMHBgMGBwYFAAUC'
    'CAIEAwMEBgUEBQkHBQAGAgMIBwcHBAMJAAUBAAgCCAgFCAgHBwkABAQHCAQHBAkHAwcEBAYIBAcHAgkB'
    'BwMJAQYDBQMICAUAAQEBAAQBAwkFBgkBBwkAAQgFAwAJBQUAAgQCCQQACAkJAgIDCQAEBwcDCQcIBAAC'
    'AwcAAQYAAAQHBQYJCQAHAQIEAAEEAAECAgkDAgQCAwkFAQEIBgYDCAgBBgEABwUCAwQBCAIFCAUGCAIA'
    'AwcICQYEBQUAAAMJCQgCBwMIAwYJAQkJBwgGBwIICAMGBwYFAgEGCQMEAwIFAgUBAAQBBQIFAgUDAAAD'
    'BwcDAQkJAgICBQkIBgkJAgEBAgMBCAYGAgQAAwQFCAAICQADCAkEBQIHAAcBCAkBAgcDAgMBBgAGAQQA'
    'CQECBwYABgQJAQcEAgYCCQADBAkHBgQGBgUEBQAICAQJAwkEAgYDAAEEAAgEBAMFBAkCCAcCAwYHBwMH'
    'CAcABgYHBAgAAwAHBwgJBgcCAQIHBwQEBAEBCAkCBgcEBwEEBAcJAwMIBgcAAgQACAgBBgEEAQMACQYJ'
    'CAEIBgMACAIFBwMFBgAHAgcEBwkGBAkIAAYEAgcBCQgIAgkBAAYBBwcDAQEJCQcJCAQDBAYCBwYGAgAC'
    'AAAIAgcIAQEABwcIBwkBAwMGAAQHAwYGBwAFBAIJCAIJCAMDAgIFAQkGBgAEBQQABgEFAQMFAgYECQEA'
    'CAIBBwQDBQEDCAkDBQIJAAcAAwcABgYCAQACAgMIAQcHAQIGAAkHBgMGCAgGAAMDBQIHBQcIBgYDCAIE'
    'CAkIBgcCCQUJCQEFBQYCBQAGBQIAAggAAQQGCAQHBQACBAEGAgAGBQACBAMJBAcBBQQBAgYJAwIDBwUA'
    'BQAGBggEBQIEBAQBAAIIBQAICQkCAAQFAgMABQQGAQUDCQYCCQIABQcEBAgDBAAEAwcJBQcJBQEBAQcB'
    'BQMGBwUCAgIHBAADBgAJBgACBgUEBQIDBQcGCQEACAcGAwgIAwIEBwQIAgEDAQQGBAcHAQEBBgIHBgAC'
    'AggEAAECBgcIAwAIBgcHBAMECAQFBgYGBgYDAAMEAQcACQIJCAkABggAAQgHAwQFCAgIAQQDAQgCAwUJ'
    'BQIBBQgFAAYIBAYCAAUIAwAFCAUEBwMHBwUEBgcDBgMICAMECQYACQAACAcBBAIECAQBBwICCQEFCQUF'
    'AgEIAQQDBwAFBwgCAgMEBQEEAgYAAAIBAwIJBQQICQcFCQUGBAcDCQQDAAgIAAUABwcEBwYCBgcGBAUF'
    'CQIEBgMFAAcEBgUABQIDBQYCCQcHAQgEBgIABgICBwQFBwYBAgQCBwMCAAIIBAIGBgcACAYCBwcJAAcJ'
    'BQcJAgkJAwAFCQIAAAUHAQIACAICBAgHAwYDBwEBAwcICQIHAAcFCQMEAwMECAgEBwQCAggABAMGAwcE'
    'AgEEAgUHBQcGAAEFAwIHAwcABQQAAAYICAYGBgUGAwYJBwQECQQFBwAGAgQBAwUAAgUHAgAEAAEAAgEC'
    'BgcIBQABAAUJBQMABgkDAAEEAQIJAwADBwEBBQUECQUGBAUIBgEJBgYDBAUBAQIGCAYHBAIDBQEHBQQJ'
    'AQMCCQkEAQIEBwQCAQEHBQcEAAQAAAABAQkBBQQHCQQJBAkHCQYDAAgFAQgFBgcCBwgACAAFCQkHCAQJ'
    'BAQJCAUJBwICAwYACQUFBgYGAQUFBgAEAQgAAAEFAAgJBAMDBQcHBQQHAQEHCQYIAwEBAAkACQQHAwUD'
    'BgAACAUBAwMGCAQEAAQAAgYJCQcIBgQFBQIBBAAFBAMJBgUBBwkFBgEFBgQJBQQDBgcGBwIBAwgIBAAF'
    'CAQBBwUGAQkGBQkEBAICBQMABgMFBgICCQAHBwQGAgQEBgEABwQEAgIGAgAJBQcFBgQACAQBAAcEBQkH'
    'BAMCAgEDAgAGBAMBCAYIAQgJBQcACAQGCAYAAAkIBgcGBQMCBgYCAAcGBgkFAgYFCQEFBQMIBAEABwYE'
    'AAQJBwAHCAkGBwkECAgEAAIIAwcHAAEBAAEBCAcFBQcIAgkIAAgICAYFBgIFBQkIBQAGCQIFBQMJCQQC'
    'BAAABwkJBwAGBgICBQQGAgcDAwAIBQMJBQMABQIHAQkABwgAAwgFAAUHAAAEBQkFCAcBBAgCAQABBAcH'
    'BAgGBwAABAAHBgACBwcEBQgICQQECAQFBAIABAgGCAIECQADAAEAAQgABAIGAAAHBAUJBwkGAQAFBgAD'
    'AQYHBAYCAgcEBwUCCQAFAQUFBAkJAQcCBgkDBwUBAwIACQUDAAQGCAEACAIDCQEFAwIJAQgFCQMEAAcJ'
    'BwkEBAEFAgQFAgUDCQkGAAgJAAcEBgUGCAMJAgUJBQICAQcCAwEAAQECBAUABQEAAQMECQcAAAIBAwgD'
    'CQgIAwYDCAkHBAcGBAcJBAMDAwYFBwEACAgGAggGCQcBBwMABAMGBQcJCQkICAgAAQUIAwUACQAFAwQH'
    'AQgGAwAECAABAwEBAQEGAwUCBAQABwEBBgQFBAUCBggDAgcJBgMGAgQJCQQEAgEGCQUIAAkJAgIGBQgA'
    'AgIDBgYDAgkDBwQFBgUGAQYGAgEDAQAABwQACAAACAkJBQEGAwYCBgcDCQICBAkFBQEHCQcJCQEGBgcI'
    'BwEABgYIBgQBAAYBBQAECAYJAAgHAgYFCAYFCQAGAgQHAAEAAQkFBgUHBwEJCAQHAgYHCQQABgAABAYJ'
    'BAAIAgAAAgMGAgQAAgkFCAgIBgAGCQgIBgIIAwEABAkGCAQGCAUJBgACAgAEAAMBAgEIBgYGAQQEAAgE'
    'CAAFAAcAAQcACQMFBwQEBgAEAwAGAwMHBQUFBwIEAwcDAgUJCQAIBggJBwcICQIFAgABBwkDBAEGBQgH'
    'AQQBAgUHCAQIAwUEAAUGAwYFAgAHBgECAQAJBAkIAwgICAcDAwgDBwUCBQQBBwICAQEEAQEBBQgCBQQA'
    'AQgJBQkFCAYGBQkBAwIFBQgIAAAHCAUABQcHBAMHAwMDBAYGBgYJBwEHAQkGCQcAAwIBBwIAAQcGAAEE'
    'AQQGCQcFAwEFBAUJBQcABQEDBwcDAwUGAwIABQkHAwIJCQEFAwUEBAEEBgUBAwUEBAECAAgABQEGAQQB'
    'BwcICQQJAAYAAQUBAQkGBgIHAQUDAQMFAQkHAAQAAAYACAcCAgkJBgMABAICBQYGBQgGBQEHBwICAQEF'
    'AQEABQADCAcDCAcFAAcAAQcEAwIBAwIGAQEHBwgGBwcABQQBCAUJCQUEBQcFAwgHBQcIBgcAAQAFCQED'
    'AQQJBAAFAAcGCAIABAQJAAQFAwUHAAcIAgAECAcHBQUFCQMDBwYJBgAHAQkBCQgDBgMFAAkIAQUBBAUF'
    'AAADAggIBwICAQcABwUGAQcHBAkAAAUCAgkACQQDBAQBBAEFBgIEBwQGBgUJCQQAAAUCBAMEBgIFBQQG'
    'BwIFBgkCBAQFBgkGBgYEBAMIBgkABwQBAwYCCAgAAgIFAwUJAwIDCQACCQgHAQAAAQcAAgEFAQcJCAYG'
    'AwAHCQUACQYJAwYFBgMICQEJAgkHAAACBQMCAQQIAQYEBQYABAAFBgQFAgUAAgMABgcGAQkACQMDBAYI'
    'BAYGAwgIBAMBAAAFBwAEBQECCQQABwMACQACBwkEBQkJBQMCBgUEAAgGCAYDCAYHBgMFAAECAQQABwMH'
    'BwIDBQQCBgIIAAcDBAYEBwQFBgMCBgMCAwMCAAkDBQUJBwAGBgcJAAcDAwcBBQYIAggGBQMFBQgHBgED'
    'BAQIBwMGCQQBAAEFAQEIAwcDBgQFAgQJAgkHBAYHAAIGCQMIBQgBBAcABAMIBgAHAAIEAQEHAgMBCQQD'
    'AQgHBAMEAQkDBAMABgYBAgUICAQFCQUEBgQHAgMICAUCAAgECQECBQEGAQACBgcCBwQEAwkCBQIJCQkH'
    'AgMHAgMGBQMIAwkEAwcABgYIAwECCQEEAAgACAAFBAUDAwUJAwgIBQYDBQICBAkEBwIEAgMGBAIGAwQI'
    'BwYCBwMGCQAEBQMDBQcABgQABQIDBgICBwkJBwUHAQEABwcFAAcBBQgIAQIIAgUEBwAFCQIGBQABBgUD'
    'AgUABAQGAwEECAkCCAYCAAICBAkACQQHBAYJAQUGBwkJAAEGBwIGAQQCAgcCBQICAAgEBwAACAYHBwgG'
    'AwkBAAIACQEDBgkDAQcFCAYDBQECBQUBBAUIBwMIBQEEAAYGAggBBAYAAAADBQAGBQIBAAMIBQEEAAcI'
    'BQADBAAGCAQHBgEBBggABwECBgMGCAADAAYBBwQIAAADAAUABgUAAAYBAAUABwQGAQMABwcHCQUHBAYB'
    'CQQHBQIECQUFCQQGCQAJBQMBBggCBAgIAQUEAAUGAgABBggIAgIABgIDBwEBBgECAQMCBQgEBAgEBAkB'
    'CQcAAgYGBwABBgcIAwcDCAgGAgUDCQIIAggIAwIIAQEIAQkDBAQJAAYAAQIDBAgAAAADCAAHAwgIAAgB'
    'BQkCCAYDBAIEBAcGBQkDCAYEBwAGAwIGBwMCBwQCBQQIBgYIBQkFCQIJAAAABwgABAMDBAMGAQIEAQEA'
    'AwgIAAcFBgQGAQQICQEFCQIDBQcGAAkFBQgACQADBQUIAgQAAAkGBQcECQkJBAIEBwEGBQgEAwkGBgAE'
    'AAkCBgMIBwABBgcJAAEBBwYGAQUJAgUIBwMJBgYICAEFCAgJAwEBBgQCCAcBAQQBAwcDAgkACQcFBQkA'
    'CAgDAwcEAAYGBgkIBAIIBwEJAgUACAgCAgQCBAQBAwABAAIIAAAJAgECBwEBAwIJCAMBBgEEAwMAAAMG'
    'AAcABgIIAAgCAwYFCAECAgIIAgAEAgEEBQAIAQkGCQcABAYJBAMABQgFCQIFAwAAAAYEBQAHAAQBAgQA'
    'AwgJBAkAAAcFCQMEAwcDBwEFBwIEBwMCCQIEAgMFBwkIAgMGAQEECQIEBgABBAQFAQgJAQMHCQkFCAQE'
    'AQUFAAEGCQAACQcEBwUBAgcACQUFAwACAgAHBQIFCQQABQAICAcGCAEIAAEDCQYCAQkGAwYHCAIFBAcE'
    'CAYBBgICAQgABQQCBAkBAQcDBAYABgQAAwIDAwgIAgQFBwYIBgAHBQYICAMFAAkDAAkEBAMIBgUHAQcH'
    'BgQGAAYBBQgDCAUDBwkFAgUEBAgFCQAAAAMJAAEDBQAGBgIIBwICAAQEAAcEAQMECAADBAcEAAcHAQEA'
    'AAkJAgUGBwMDBgIGAwYABAMCCAIABwEDAgEACAgIAwEHAQMJCQIDAgEHBAYEAgEBAwYDAQkDCAgJAQQF'
    'CAgAAQQABwYIAgcJBQcBAgABBQEEBAQFBgQDBgEACQAABAgCBgYDAgcACQUEBQYBBQgACAUIBQEHBwID'
    'CAcCCQACAQYBBQUHBQkBBAQCAAUBCAMIAgcDAwACAAEABAAFAAIICQAGCQgECAYGBQkHBgUCAAgCBQAD'
    'CQkGCQkHAwkDBAQACQMEBAQCAQEJBQADAQEIAAcABwgJAAgAAAkABAUCCAcDBQcBAAgABggACQcGBgUG'
    'AAMDCQUJCAMBCQQCCAgDAwEABwYEAwQFAwAHAQEEAwgDCAEEAQIEAAMBBggDBgUAAAIHAwkFAAMECQcF'
    'BAMFAAUBCQgBBAEEAAcDAAAIBAcECAEDBggBCQQABwUHBggDAQkFCAUIBAgEBwAEAgYAAgIJBQMHAwgA'
    'AAUABAMABwMICAIBCAADAgUACAAIAAcJAQIECAUBAQIBBgYFAwkABAAEAQEFAwgABgAJCAMAAAcABAUB'
    'BwICAQkGBgMEBwAABQEGCQIDCQIDAwUGBQgIBAQDBwMCAAIIAgAAAggCBQcIBQcCCAkGBQECCAUABQUI'
    'BwMICQQFCQQBBwkIAgkIAgABAwIHAQUDAgICBwQAAQUJBwgJAwMEAwIJBgMIAgcDAwUDAwEEBAYEBwgA'
    'AwgCAAkDAgcGBgACAAUACAYHAAUDBgMJAQcFAAIJBAQAAgkJAQYGBQQJAwcFBAMIAQcDBgUFAgECAwID'
    'BwYDBgMEBAEBAQQACQEHBwAICQUBAwkIAgkDAAMIBQkHCQAAAQgBBwQDAwkGBwgGAQcJAAYGAQYIAQMA'
    'BQgBAAQJBgkBAAEACQAABgkIBQQIAwcJAAkDAQgBAgEFCQMACAgDAgYFBgAAAwAACQEGCQYECAEFCAQH'
    'CQMBCAMECAMDAgADBAUIBwMAAgYGBAUDBAMIAQkJBgUHBAcCBwYHAQAJCQkHCAAIAgcJCAMJAQQCAgQJ'
    'AQECAwIIBQEGCQYHAgYBBAkDAwgABwcACQgABwkHBQIGAwEJBAUCAAgCAQQHCQMICAYJAwMEBQgGCAQI'
    'BQMHCQIACAIIBAkGAwEABQUFCQkACAgIAQICAgECBAgCCAMJCQEJBQIEBAQDCAgDAQQECAkGAwIIAAkD'
    'BgUDAgEGAwQBAAgFCAYIBAEBAAAGCAEGBggGAgIHAgAGAAQABwMCAQYABQUECQgCBAQEAQkDBQMAAAAF'
    'BAIEBAcDAQcEAgkDCAYEAwMHAAUABwkHBAkIBAABCQYGCAEIBwEAAAEFAwQCCQMEAwkCAQgHAgEBBQAG'
    'AQcGAQEIAwgJBQUJBAYHAQgICAQFBgcIAwYBAgcJBQEHAgADCQkAAQMABAgDAgMGAwMAAQUGAAkFBgUH'
    'CQkFAgcGAwYGBggFBgkECQMACAACBwQJAgEIBAYECQIEAQUCCAAEAQIGAgYABwIBAQQHBgkAAQEGBAIE'
    'BAMAAggFBwkIAAgHAgIDAwkEBAgEAwYBBgEFBQUGBAYEAAcCBQMGBwEGBAMDAAkJAwgGBgABBQEIAAED'
    'CQMDBgEJAAIGBgQJCQABBwcBBAUBBAECAwQEBQkECAkHAQgBCQMDBgYDCAAEBwIFAQEDAwcBCAEEAAkA'
    'AgQGAgkGBgQFCQIFCQEDAAAEBQUFAgIAAQMBAQMAAQUHBwMCBAMECQkCBwMFAAQBAQACBgYJAgYGAQAF'
    'AQIABwcDAAIACQMGBgkBBwgJBAUCCQMHAAADCQQHAAgDAwgCAAcABQkHAAcCBQMDAgAHAQAIAQUJAAgJ'
    'BgAAAwcACQIAAgAFCAcCCAEIAgQCCQYCAQkHBgMCBAAABQgGBQIFBgMDCQYDAQYFAQEEAAMAAQAABwMC'
    'BQACBgQBCQEACAQGBgcIAgIGCQEDBwAJAgIHAgYICQECAwEJBwkGAs3MmQA='
)
_ZBOT_TABLE_B = _b64.b64decode(_ZBOT_TABLE_B_B64)


def zbot_encrypt(scow_bytes):
    """Encrypt/decrypt a SCOW buffer using the ZBOT stream cipher."""
    pos, sub, ctr = 0xAB, 0, 0
    result = bytearray()
    for b in scow_bytes:
        if ctr == 17:
            ctr = 0
        delta = _ZBOT_TABLE_A[ctr]
        ctr += 1
        idx = (pos + delta) % 9776
        key_byte = _ZBOT_TABLE_B[idx * 4 + _ZBOT_SUB_OFFSET[sub]]
        result.append(b ^ key_byte)
        sub += 1
        if sub == 4:
            sub = 0
            pos = (pos + 1) % 9776
    return bytes(result)


def gen_sintetico_all_features():
    """Comprehensive synthetic v0xC4 fixture exercising every reader/importer feature.

    Two instruments (Melodia MIDI=42, Acompa MIDI=25), 20 measures.

    Features covered by measure:
      m0 : ORN TEMPO (bpm=120) + all note values: 64th 32nd 16th 8th Q H whole
      m1 : dotted notes (dotted-H dotted-Q dotted-8th) + rests (Q-rest 8th-rest 16th-rest)
      m2 : ties (Q-Q Q-8th)
      m3 : 8th triplets (3:2 groups of three)
      m4 : grace notes, appoggiatura (grace1=0x20/0x04) + acciaccatura (grace1=0x30/0x04)
      m5 : articulations above, fermata(0x20) accent(0x12) marcato(0x13) staccato(0x1D)
      m6 : articulations, tenuto(0x1C) staccatissimo(0x29) up-bow(0x18) down-bow(0x19)
      m7 : technical markings, harmonic(0x1E) thumb(0x44) open-string(0x46) fingering 1 to 5
      m8 : ornaments, trill(ORN 0x36) mordent(0x0B artic) inv-mordent(0x0A artic) turn(0x08)
      m9 : tremolo ORN (0xAF) + per-note tremolos artic 0x41/0x42/0x43 (1/2/3 strokes)
      m10: dynamics pp p mp mf (ORN 0x81 0x82 0x83 0x84)
      m11: dynamics f ff fff sfz sffz fp fz sf + hairpin crescendo
      m12: hairpin decrescendo + slur (SLURSTART/SLURSTOP)
      m13: arpeggio (ORN 0x22) + chord symbols
      m14: staff text "Allegro" (TEXT block tind=0) + lyrics "do re mi fa"
      m15: MEAS BPM change (bpm=80) + segno marker (ORN 0xA2)
      m16: coda marker (ORN 0xA6) + to-coda marker (ORN 0xA5)
      m17: repeat-start barline + volta 1 bracket
      m18: repeat-end barline + volta 2 bracket
      m19: 6/8 compound meter (bpm=120 quarter-note = 80 dotted-quarter)

    TEXT block entries (tind index):
      0: "Allegro"   1: "Moderato"   2: "Ritardando"
    """
    # ---- helpers for TEMPO ORN (tipo=0x32, tempo byte at element offset 30) ----
    def orn_tempo(tick, staffIdx, bpm):
        d = bytearray(33)
        struct.pack_into('<H', d, 0, tick)
        d[2] = (5 << 4)          # typeVoice: ORN | voice=0
        d[3] = 33                # size
        d[4] = staffIdx & 0x3F
        d[5] = 0x32              # tipo = TEMPO
        d[30] = bpm & 0xFF
        return bytes(d)

    # ---- note with artic (combined helper) ----
    # dc = sounding duration in Encore ticks (for dot detection).
    # If dc <= 255 it also goes into dotControl; otherwise only playbackDurTicks
    # (d[13..14]) is set so calcDotsSnap can still identify the dots.
    def n(tick, si, fv, pitch, au=0, ad=0, tup=0, dc=0):
        d = bytearray(25)
        d[0]=28; d[1]=si&0x3F; d[2]=fv; d[10]=tup
        if 0 < dc <= 255:
            d[11] = dc               # dotControl (1 byte)
        if dc > 0:
            struct.pack_into('<H', d, 13, dc)  # playbackDurTicks (2 bytes)
        d[12]=pitch
        d[21]=au; d[23]=ad
        return struct.pack('<H', tick) + bytes([0x90]) + bytes(d)

    def r(tick, si, fv):
        d = bytearray(15)
        d[0]=18; d[1]=si&0x3F; d[2]=fv
        return struct.pack('<H', tick) + bytes([0x80]) + bytes(d)

    def g(tick, si, fv, pitch, grace1, grace2):  # grace note
        d = bytearray(25)
        d[0]=28; d[1]=si&0x3F; d[2]=fv; d[3]=grace1; d[4]=grace2; d[12]=pitch
        return struct.pack('<H', tick) + bytes([0x90]) + bytes(d)

    def o(tick, si, tipo, xoff=None, alm=0, xoff2=0, yoff=0):
        # Auto-compute xoffset from tick so Encore distributes ornaments across
        # the measure visually (not piled at beat 1).  Encore's measure width is
        # ~200 screen units (0xCC=204 in meas_hdr layout byte 0).  Formula maps
        # tick 0..960 → xoffset 5..195.  Pass xoff explicitly to override.
        if xoff is None:
            xoff = max(5, min(195, 5 + int(tick * 190 / 960)))
        return ornament_v0c4(tick, 0, si, tipo, xoffset=xoff, alMezuro=alm,
                              xoffset2=xoff2, yoffset=yoff)

    def tie(tick, si):
        return tie_v0c4(tick, 0, si)

    def lyr(tick, si, text):
        return lyric_v0c4(tick, 0, si, text)

    def cs(tick, si, text):
        # chordsym_v0c4 needs a 36-byte text slot (Latin-1, zero-padded)
        raw = text.encode('latin-1') + b'\x00'
        raw = (raw + b'\x00' * 36)[:36]
        return chordsym_v0c4(tick, 0, si, raw)

    def st(tick, si, tind):
        return stafftext_v0c4(tick, 0, si, tind)

    # Instrument 1 (staffIdx=1) pattern: simple quarter notes C4 per beat
    def perc(ticks_list, si=1):
        return b''.join(n(t, si, 3, 60) for t in ticks_list)

    Q = 240   # quarter note ticks in Encore (960/4)

    measures = []

    # m0: ORN TEMPO + all note values (two staves)
    e0  = orn_tempo(0, 0, 120)
    e0 += n(  0, 0, 7, 60)        # 64th C4
    e0 += n( 15, 0, 6, 62)        # 32nd D4
    e0 += n( 45, 0, 5, 64)        # 16th E4
    e0 += n(105, 0, 4, 65)        # 8th F4
    e0 += n(225, 0, 3, 67)        # Q G4
    e0 += n(465, 0, 2, 69)        # H A4
    e0 += r(945, 0, 4)             # 8th rest (fill to 960)
    e0 += perc([0, Q, Q*2, Q*3])
    measures.append((meas_hdr(4, 4, bpm=120), e0 + end_marker()))

    # m1: dotted notes + rests
    e1  = n(  0, 0, 2, 60, dc=720)  # dotted-H C4 (720 ticks)
    e1 += n(720, 0, 3, 62, dc=360)  # dotted-Q D4 (360 ticks)... but runs over 960
    # actually dotted-H(720) + dotted-Q(360) = 1080 > 960; use dotted-Q + dotted-8th + 8th rest
    e1  = n(  0, 0, 2, 60, dc=720)  # dotted-H C4 (720 ticks)
    e1 += n(720, 0, 4, 62, dc=180) # dotted-8th E4 (180 ticks)
    e1 += r(900, 0, 5)              # 16th rest (60 ticks) -> total 960
    e1 += perc([0, Q*2])
    measures.append((meas_hdr(4, 4), e1 + end_marker()))

    # m2: ties Q→Q, Q→8th
    e2  = n(  0, 0, 3, 60)          # Q C4 (tie-start)
    e2 += tie(  0, 0)
    e2 += n(Q,   0, 3, 60)          # Q C4 (tie-end)
    e2 += n(Q*2, 0, 3, 64)          # Q E4 (tie-start)
    e2 += tie(Q*2, 0)
    e2 += n(Q*3, 0, 4, 64)          # 8th E4 (tie-end)
    e2 += n(Q*3+120, 0, 4, 67)      # 8th G4
    e2 += perc([0, Q, Q*2, Q*3])
    measures.append((meas_hdr(4, 4), e2 + end_marker()))

    # m3: 8th triplets (tup=0x32 = 3:2)
    T = Q * 2 // 3   # tuplet 8th tick spacing = 160 ticks
    e3  = b''.join(n(T*i, 0, 4, [60,62,64][i%3], tup=0x32) for i in range(6))
    e3 += perc([0, Q*2])
    measures.append((meas_hdr(4, 4), e3 + end_marker()))

    # m4: grace notes (appoggiatura grace1=0x20|grace2=0x04, acciaccatura grace1=0x30|0x04)
    e4  = g(  0, 0, 4, 64, grace1=0x20, grace2=0x04)  # appoggiatura 8th E4
    e4 += n(  0, 0, 3, 67)                              # Q G4 (main note after appog)
    e4 += g(Q,   0, 4, 69, grace1=0x30, grace2=0x04)   # acciaccatura 8th A4
    e4 += n(Q,   0, 3, 71)                              # Q B4
    e4 += n(Q*2, 0, 3, 72)                              # Q C5
    e4 += n(Q*3, 0, 3, 74)                              # Q D5
    e4 += perc([0, Q, Q*2, Q*3])
    measures.append((meas_hdr(4, 4), e4 + end_marker()))

    # m5: articulations above, fermata(0x20 no tuplet), accent, marcato, staccato
    e5  = n(  0, 0, 3, 60, au=0x20)   # fermata above
    e5 += n(Q,   0, 3, 62, au=0x12)   # accent
    e5 += n(Q*2, 0, 3, 64, au=0x13)   # marcato
    e5 += n(Q*3, 0, 3, 65, au=0x1D)   # staccato
    e5 += perc([0, Q, Q*2, Q*3])
    measures.append((meas_hdr(4, 4), e5 + end_marker()))

    # m6: articulations, tenuto, staccatissimo, up-bow, down-bow (below slot)
    e6  = n(  0, 0, 3, 60, au=0x1C)   # tenuto
    e6 += n(Q,   0, 3, 62, au=0x29)   # staccatissimo
    e6 += n(Q*2, 0, 3, 64, au=0x18)   # up-bow
    e6 += n(Q*3, 0, 3, 65, ad=0x19)   # down-bow (below slot)
    e6 += perc([0, Q, Q*2, Q*3])
    measures.append((meas_hdr(4, 4), e6 + end_marker()))

    # m7: technical, harmonic, thumb-position, open-string (Fingering "0"), fingering 1-5
    e7  = n(  0, 0, 4, 60, au=0x1E)   # harmonic
    e7 += n(Q//2, 0, 4, 62, au=0x44)  # thumb-position
    e7 += n(Q,   0, 4, 64, au=0x46)   # open-string
    e7 += n(Q+Q//2, 0, 4, 65, au=0x0D)  # finger 1
    e7 += n(Q*2, 0, 4, 67, au=0x0E)   # finger 2
    e7 += n(Q*2+Q//2, 0, 4, 69, au=0x0F)  # finger 3
    e7 += n(Q*3, 0, 4, 71, au=0x10)   # finger 4
    e7 += n(Q*3+Q//2, 0, 4, 72, au=0x11)  # finger 5
    e7 += perc([0, Q, Q*2, Q*3])
    measures.append((meas_hdr(4, 4), e7 + end_marker()))

    # m8: ornaments, trill ORN 0x36, mordent artic 0x0B, inv-mordent artic 0x0A, turn artic 0x08
    e8  = o(  0, 0, 0x36)              # trill start ORN
    e8 += n(  0, 0, 3, 60)            # Q C4 (carries trill)
    e8 += n(Q,   0, 3, 62, au=0x0B)   # mordent
    e8 += n(Q*2, 0, 3, 64, au=0x0A)   # inverted-mordent
    e8 += n(Q*3, 0, 3, 65, au=0x08)   # turn
    e8 += perc([0, Q, Q*2, Q*3])
    measures.append((meas_hdr(4, 4), e8 + end_marker()))

    # m9: tremolo ORN (0xAF) + per-note tremolos artic 0x41/0x42/0x43
    e9  = o(  0, 0, 0xAF)              # tremolo ORN at beat 1
    e9 += n(  0, 0, 2, 60)            # H C4 (carries tremolo ORN)
    e9 += n(480, 0, 3, 62, au=0x41)   # Q D4: 1 stroke (8th tremolo)
    e9 += n(Q*3, 0, 3, 64, au=0x42)   # Q E4: 2 strokes (16th tremolo)
    e9 += n(Q*3, 0, 4, 67, ad=0x43)   # 8th G4 chord extension: 3 strokes in articDown
    e9 += perc([0, Q*2, Q*3])
    measures.append((meas_hdr(4, 4), e9 + end_marker()))

    # m10: dynamics pp p mp mf
    e10  = o(  0, 0, 0x81)             # DYN_PP
    e10 += n(  0, 0, 3, 60)
    e10 += o(Q,   0, 0x82)             # DYN_P
    e10 += n(Q,   0, 3, 62)
    e10 += o(Q*2, 0, 0x83)             # DYN_MP
    e10 += n(Q*2, 0, 3, 64)
    e10 += o(Q*3, 0, 0x84)             # DYN_MF
    e10 += n(Q*3, 0, 3, 65)
    e10 += perc([0, Q, Q*2, Q*3])
    measures.append((meas_hdr(4, 4), e10 + end_marker()))

    # m11: dynamics f ff fff sfz sffz fp fz sf + hairpin crescendo
    # WEDGESTART must come FIRST in the stream (before notes) for correct elemTick.
    # speguleco=0: bit0=0 -> CRESC_HAIRPIN.
    e11  = ornament_v0c4(0, 0, 0, 0x1D, xoffset=5, xoffset2=90, speguleco=0)  # CRESC hairpin
    e11 += o(  0,   0, 0x85)           # DYN_F
    e11 += n(  0,   0, 4, 60)
    e11 += o(Q//2,  0, 0x86)           # DYN_FF
    e11 += n(Q//2,  0, 4, 62)
    e11 += o(Q,     0, 0x87)           # DYN_FFF
    e11 += n(Q,     0, 4, 64)
    e11 += o(Q+Q//2,0, 0x88)           # DYN_SFZ
    e11 += n(Q+Q//2,0, 4, 65)
    e11 += o(Q*2,   0, 0x89)           # DYN_SFFZ
    e11 += n(Q*2,   0, 4, 67)
    e11 += o(Q*2+Q//2, 0, 0x8A)        # DYN_FP
    e11 += n(Q*2+Q//2, 0, 4, 69)
    e11 += o(Q*3,   0, 0xAA)           # DYN_FZ
    e11 += n(Q*3,   0, 4, 71)
    e11 += o(Q*3+Q//2, 0, 0xAB)        # DYN_SF
    e11 += n(Q*3+Q//2, 0, 4, 72)
    e11 += perc([0, Q, Q*2, Q*3])
    measures.append((meas_hdr(4, 4), e11 + end_marker()))

    # m12: hairpin decrescendo + slur
    # speguleco=1: bit0=1 -> DIM_HAIRPIN.
    e12  = ornament_v0c4(0, 0, 0, 0x1D, xoffset=5, xoffset2=90, speguleco=1)  # DECRESC
    e12 += n(  0, 0, 3, 60)
    # Slur: SLURSTART(0x21) with alMezuro=0 (same measure), xoffset=20, xoffset2=100
    e12 += o(Q,   0, 0x21, xoff=20, xoff2=100)
    e12 += n(Q,   0, 3, 62)
    e12 += n(Q*2, 0, 3, 64)
    e12 += n(Q*3, 0, 3, 65)
    e12 += o(Q*3+Q//2, 0, 0x41)  # SLURSTOP
    e12 += perc([0, Q, Q*2, Q*3])
    measures.append((meas_hdr(4, 4), e12 + end_marker()))

    # m13: arpeggio + chord symbols
    e13  = o(  0, 0, 0x22)             # ARPEGGIO ORN at beat 1
    e13 += n(  0, 0, 3, 60)            # C4 (bottom of arpeggio chord)
    e13 += n(  0, 0, 3, 64)            # E4 (chord extension voice)
    e13 += n(  0, 0, 3, 67)            # G4 (chord extension voice)
    e13 += cs(Q,   0, 'Am')            # chord symbol Am
    e13 += n(Q,    0, 2, 69)           # H A4
    e13 += cs(Q*3, 0, 'G7')
    e13 += n(Q*3,  0, 3, 67)           # Q G4
    e13 += perc([0, Q, Q*2, Q*3])
    measures.append((meas_hdr(4, 4), e13 + end_marker()))

    # m14: lyrics "do re mi fa" (STAFFTEXT omitted from sintetico, Encore 4.x
    # does not support STAFFTEXT ORN tipo=0x1E in MEAS elements; it crashes with
    # a null-pointer when looking up the tind in its TEXT table.
    # STAFFTEXT is covered by tst_text.cpp tests (text_staff_text.enc fixture).
    e14  = n(  0, 0, 3, 60)
    e14 += lyr(  0, 0, 'do')
    e14 += n(Q,   0, 3, 62)
    e14 += lyr(Q,  0, 're')
    e14 += n(Q*2, 0, 3, 64)
    e14 += lyr(Q*2,0, 'mi')
    e14 += n(Q*3, 0, 3, 65)
    e14 += lyr(Q*3,0, 'fa')
    e14 += perc([0, Q, Q*2, Q*3])
    measures.append((meas_hdr(4, 4), e14 + end_marker()))

    # m15: MEAS BPM change (bpm=80) + segno marker
    e15  = o(0, 0, 0xA2)               # SEGNO marker
    e15 += n(  0, 0, 3, 60)
    e15 += n(Q,   0, 3, 62)
    e15 += n(Q*2, 0, 3, 64)
    e15 += n(Q*3, 0, 3, 65)
    e15 += perc([0, Q, Q*2, Q*3])
    measures.append((meas_hdr(4, 4, bpm=80), e15 + end_marker()))

    # m16: to-coda (0xA5) + coda (0xA6)
    e16  = o(Q,   0, 0xA5)             # TO_CODA marker
    e16 += o(Q*2, 0, 0xA6)             # CODA marker
    e16 += n(  0, 0, 3, 60)
    e16 += n(Q,   0, 3, 62)
    e16 += n(Q*2, 0, 3, 64)
    e16 += n(Q*3, 0, 3, 65)
    e16 += perc([0, Q, Q*2, Q*3])
    measures.append((meas_hdr(4, 4), e16 + end_marker()))

    # m17: repeat-start (barTypeStart=2 at h[0x0C]) + volta 1 (repeatAlt=0x01 at h[0x0F])
    e17  = n(  0, 0, 3, 60)
    e17 += n(Q,   0, 3, 62)
    e17 += n(Q*2, 0, 3, 64)
    e17 += n(Q*3, 0, 3, 65)
    e17 += perc([0, Q, Q*2, Q*3])
    h17 = bytearray(meas_hdr(4, 4))
    h17[0x0C] = 2       # REPEATSTART barline at measure start
    h17[0x0F] = 0x01    # repeatAlternative = volta 1
    measures.append((bytes(h17), e17 + end_marker()))

    # m18: repeat-end (barTypeEnd=4 at h[0x0D]) + volta 2 (repeatAlt=0x02 at h[0x0F])
    e18  = n(  0, 0, 3, 67)
    e18 += n(Q,   0, 3, 69)
    e18 += n(Q*2, 0, 3, 71)
    e18 += n(Q*3, 0, 3, 72)
    e18 += perc([0, Q, Q*2, Q*3])
    h18 = bytearray(meas_hdr(4, 4, barTypeEnd=4))   # REPEATEND barline at measure end
    h18[0x0F] = 0x02    # repeatAlternative = volta 2
    measures.append((bytes(h18), e18 + end_marker()))

    # m19: 6/8 compound meter (bpm=120 quarter-note BPM = 80 dotted-quarter BPM)
    # In 6/8: beatTicks=120 (eighth), durTicks=120*6=720
    e19  = n(  0, 0, 4, 60)  # 8th C4
    e19 += n(120, 0, 4, 62)  # 8th D4
    e19 += n(240, 0, 4, 64)  # 8th E4
    e19 += n(360, 0, 4, 65)  # 8th F4
    e19 += n(480, 0, 4, 67)  # 8th G4
    e19 += n(600, 0, 4, 69)  # 8th A4
    e19 += n(  0, 1, 4, 48)
    e19 += n(120, 1, 4, 48)
    e19 += n(240, 1, 4, 48)
    e19 += n(360, 1, 4, 48)
    e19 += n(480, 1, 4, 48)
    e19 += n(600, 1, 4, 48)
    measures.append((meas_hdr(6, 8, bpm=120, beatTicks=120), e19 + end_marker()))

    # ---- Load PALOTEOS base (real 2-instrument Encore file) ----
    # PALOTEOS uses the compact TK format (112 bytes per instrument, stride=112).
    # Instrument 0 block: bytes 202..313 (name at 202, MIDI at 262)
    # Instrument 1 block: bytes 314..425 (name at 314, MIDI at 374)
    # Using PALOTEOS as base guarantees the file opens in Encore.
    PALOTEOS_PATH = os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', '..', '..', '..', '..', 'Scores', 'Issues', 'PALOTEOS DE MONCALVILLO LA SEMANA 7x8 2.enc')
    PALOTEOS_PATH = os.path.normpath(PALOTEOS_PATH)
    if not os.path.exists(PALOTEOS_PATH):
        # Fallback: search near common paths
        import glob
        matches = glob.glob(os.path.expanduser("~/Scores/**/PALOTEOS*7x8*.enc"), recursive=True)
        PALOTEOS_PATH = matches[0] if matches else None

    if PALOTEOS_PATH:
        with open(PALOTEOS_PATH, "rb") as f:
            pal = bytearray(f.read())
        # Bytes 0..451: header + TK00 (name+config for inst0) + gap (inst1 data)
        pre_base = pal[:452]
        # Patch instrument 0 name: "Dulzaina 1ª." → "Melodia" (Latin-1, null-padded)
        name0 = b'Melodia\x00' + b'\x00' * (13 - 8)
        pre_base[202:202+13] = name0
        # Patch instrument 0 MIDI: 89 → 42 (Viola) at byte 202+60=262
        pre_base[262] = 42
        # Patch instrument 1 name: "Dulzaina 2ª." → "Acompa" at byte 314
        name1 = b'Acompa\x00' + b'\x00' * (13 - 7)
        pre_base[314:314+13] = name1
        # Patch instrument 1 MIDI: 17 → 25 (Guitar) at byte 314+60=374
        pre_base[374] = 25
        # Patch measureCount only. Keep PALOTEOS nSystems (6) and 0x2A (13)
        # unchanged, we use PALOTEOS's 6 LINE blocks, so nSystems must stay 6.
        # Encore uses nSystems to iterate LINE blocks; mismatching it causes crash.
        struct.pack_into('<H', pre_base, 0x34, len(measures))
    else:
        # Fallback to old approach (may not open in Encore)
        pre_base = bytearray(set_chumagio(0xC4))[:452]
        pre_base[0x32] = 2; pre_base[0x33] = 2
        struct.pack_into('<H', pre_base, 0x2E, 5)
        struct.pack_into('<H', pre_base, 0x34, len(measures))
    pre_base = bytes(pre_base)

    # ---- Build LINE blocks (86 bytes each, 2 staves per system) ----
    # Layout bytes copied from a real 2-instrument Encore file (PALOTEOS);
    # Encore re-flows on open so the exact positions don't matter, but
    # non-zero layout data avoids "Program Error" on load.
    # Staff data layout (14 bytes each) from real file:
    STAFF0_LAYOUT = bytes.fromhex('000000010000000000d0069709020000')  # 16 bytes? no
    # From PALOTEOS LINE[0]: staff0 layout bytes [13..26] = 14 bytes:
    STAFF0_LAYOUT = bytes.fromhex('000000010000000000d006970e020000')[:14]
    STAFF1_LAYOUT = bytes.fromhex('00000000000000008b00a300fcf8f4f0')[:14]

    # Use PALOTEOS's exact pre-MEAS section (header + TK + gap + 6 LINE blocks)
    # as the base, patching only instrument names, MIDI, and measureCount.
    # Our custom LINE blocks crashed Encore due to missing layout bytes.
    # PALOTEOS's 6 LINE blocks cover 22 measures; we only provide 20 MEAS blocks
    # and set measureCount=20, Encore ignores measures beyond measureCount.
    pre_base_final = bytearray(pre_base)
    # Also restore PALOTEOS's original 6 LINE blocks (already in pre_base since
    # pre_base = PALOTEOS bytes 0..451, but we need the LINE blocks too).
    # Pre_base only covers 452 bytes; add PALOTEOS LINE blocks back.
    pal_path = PALOTEOS_PATH
    if pal_path and os.path.exists(pal_path):
        with open(pal_path, 'rb') as f:
            pal_full = f.read()
        # Find where PALOTEOS LINE blocks start and where MEAS starts
        i = 0
        pal_line_start = 0
        while i < len(pal_full) - 4:
            if pal_full[i:i+4] == b'LINE':
                pal_line_start = i
                break
            i += 1
        i = 0
        pal_meas_start = 0
        while i < len(pal_full) - 4:
            if pal_full[i:i+4] == b'MEAS':
                pal_meas_start = i
                break
            i += 1
        pal_lines_bytes = pal_full[pal_line_start:pal_meas_start]
        # pre_base covers 452 bytes (header + TK + gap, before LINE blocks)
        # Append PALOTEOS's exact LINE blocks
        pre_full = bytes(pre_base_final) + pal_lines_bytes
    else:
        pre_full = bytes(pre_base_final)

    body = b''.join(meas_block(h, e) for h, e in measures)

    # Post-MEAS: PALOTEOS PREC+TITL+TEXT (2-instrument, proven to open in Encore)
    post_bin = os.path.join(os.path.dirname(__file__), 'paloteos_post.bin')
    PALOTEOS_POST = open(post_bin, 'rb').read() if os.path.exists(post_bin) else SKELETON_POST

    return pre_full + body + PALOTEOS_POST


# ===========================================================================
# ornaments_v0c2_orn_c4_accent.enc
# v0xC2: two 4/4 measures with 4 quarter notes each (8 total), every note
# paired with ORN tipo=0xC4.  In v0xC2, ORN 0xC4 maps to articAccentAbove
# (not stringsUpBow as in v0xC4).  Replaces personal file BN-COLET.ENC.
# ===========================================================================
def gen_v0c2_orn_c4_accent():
    """Stamped format 3.05, the generation in which 0xC4 is the accent. The version byte is 0xC2
    for both generations, so only the format version tells them apart; from 3.07 on the same code
    is a genuine up-bow, which ornaments_v0c2_post40_articulation_codes.enc covers."""
    def one_measure():
        e = b''.join(
            note_v0c2(t, 0, 0, fv=3, pitch=60) + ornament_v0c4(t, 0, 0, tipo=0xC4)
            for t in [0, 240, 480, 720]
        )
        return e + end_marker()
    hdr = meas_hdr(4, 4)
    return set_version(assemble(0xC2, [(hdr, one_measure()), (hdr, one_measure())], fill_ts=(4, 4)), 773)


# ===========================================================================
# ornaments_v0c2_cross_measure_slur.enc
# v0xC2: two 4/4 measures with one quarter note each and a SLURSTART in
# measure 0 (alMezuro=1) whose end resolves to measure 1 via addSpannerEnds.
# All slurs must be cross-measure (>0), no same-measure slurs (==0).
# Replaces personal file XEQUEABU.ENC.
# ===========================================================================
def gen_v0c2_cross_measure_slur():
    # Reproduces the XEQUEABU.ENC pattern: a v0xC2 slur whose spanning measure-count
    # (element byte +18 = alMezuro = 1) marks it as ending one bar later. Encore draws
    # these note-1 -> note-1 arcs between bar starts, and their xoffset2 is unreliable, so
    # the importer anchors the endpoint to the downbeat (first chord) of the target measure.
    # Measure 0: note@0 + SLURSTART(alMezuro=1) + three more quarter notes (fill 4/4 so
    # adjustPickupMeasure does not shrink m0).  Measure 1: note@0 = the downbeat endpoint.
    m0 = (note_v0c2_xoff(  0, 0, 0, fv=3, pitch=60, xoffset=3)
          + ornament_v0c4(  0, 0, 0, tipo=0x21, xoffset=1, xoffset2=5, alMezuro=1)
          + note_v0c2_xoff(240, 0, 0, fv=3, pitch=62, xoffset=2)
          + note_v0c2_xoff(480, 0, 0, fv=3, pitch=64, xoffset=3)
          + note_v0c2_xoff(720, 0, 0, fv=3, pitch=65, xoffset=4)
          + end_marker())
    # Measure 1: note@0 is the downbeat endpoint of the cross-measure slur.
    m1 = note_v0c2_xoff(0, 0, 0, fv=3, pitch=67, xoffset=7) + end_marker()
    hdr = meas_hdr(4, 4)
    return assemble(0xC2, [(hdr, m0), (hdr, m1)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_v0c4_orn_be_accent.enc
# v0xC4: two 4/4 measures with 4 quarter notes each (8 total), every note
# paired with ORN tipo=0xBE.  In v0xC4, ORN 0xBE maps to articAccentAbove.
# Replaces personal file BN-COLET5.enc.
# ===========================================================================
def gen_v0c4_orn_be_accent():
    def one_measure():
        e = b''.join(
            note_v0c4(t, 0, 0, fv=3, pitch=60) + ornament_v0c4(t, 0, 0, tipo=0xBE)
            for t in [0, 240, 480, 720]
        )
        return e + end_marker()
    hdr = meas_hdr(4, 4)
    return assemble(0xC4, [(hdr, one_measure()), (hdr, one_measure())], fill_ts=(4, 4))


# ===========================================================================
# lyrics_v0c2_compound_meter.enc
# v0xC2, 6/8 (beatTicks=360, durTicks=720): 3 identical measures of 6 eighth
# notes each with lyrics.  Syllable set: "La","ro","sol","es","mi","do"
# (each >=2 chars so wrong-offset decoding gives single-char artifacts).
# Tests: v0xC2 lyric text-offset (+18 not +20) and compound-meter lyric
# matching (encTicksPerQuarter = beatTicks*2/3 = 240 when beatTicks=360).
# Replaces personal file jronda_68_lyrics.enc.
# ===========================================================================
def gen_v0c2_compound_meter_lyrics():
    syllables = ["La", "ro", "sol", "es", "mi", "do"]
    ticks = [0, 120, 240, 360, 480, 600]  # 6 eighth notes in 6/8 (120 ticks each)
    def one_measure():
        e = b''
        for t, syl in zip(ticks, syllables):
            e += note_v0c2(t, 0, 0, fv=4, pitch=60)  # fv=4 = eighth note
            e += lyric_v0c2(t, 0, 0, syl)
        e += end_marker()
        return e
    # beatTicks=360 = compound beat (dotted quarter); durTicks=720 = 6 eighths.
    hdr = meas_hdr(6, 8, beatTicks=360, durTicks=720)
    return assemble(0xC2, [(hdr, one_measure()), (hdr, one_measure()), (hdr, one_measure())])


# ===========================================================================
# lyrics_rest_does_not_shift_notes.enc
#
# v0xC2 6/8 with REST@0 and NOTEs@[120, 240]. Regression test for two bugs
# in attachPendingLyrics:
#
# Bug 1: REST elements consumed noteTickList entries, shifting all note
# encTick assignments. Without fix: REST gets encTick=120 (first note's),
# and NOTE@120 gets encTick=240 (second note's). LYRIC@140 then matches
# NOTE@240 instead of NOTE@120.
#
# Bug 2: Lyric proximity matching used pure absolute distance without
# preferring notes where note_tick <= lyric_tick. Lyric closer to a later
# note by absolute distance would match the wrong note.
#
# With both fixes: LYRIC@140 correctly matches NOTE@120.
# ===========================================================================
def gen_v0c2_lyrics_rest_does_not_shift_notes():
    e = rest_v0c2(0, 0, 0, fv=4)      # REST at tick 0 (eighth rest)
    e += note_v0c2(120, 0, 0, fv=4, pitch=60)  # NOTE1 at tick 120
    e += note_v0c2(240, 0, 0, fv=4, pitch=61)  # NOTE2 at tick 240
    e += lyric_v0c2(140, 0, 0, "ma")  # LYRIC at 140, should match NOTE1@120
    e += end_marker()
    hdr = meas_hdr(6, 8, beatTicks=360, durTicks=720)
    return assemble(0xC2, [(hdr, e)])


def gen_zbot_single_note():
    """Minimal ZBOT file: one measure with a quarter-note C4 (SCOW v0xC4 encrypted)."""
    n = note_v0c4(tick=0, voice=0, staffIdx=0, fv=0x03, pitch=60) + end_marker()
    return zbot_encrypt(assemble(0xC4, [(meas_hdr(4, 4), n)]))


def gen_zbot_family_40x():
    """The Encore 4.0 element family inside an encrypted container. Encrypted files are a quarter of
    the corpus against two fixtures, and neither of those carries more than a note, so this one runs
    a whole element family through the decryption and then through the generation-dependent
    offsets."""
    return zbot_encrypt(gen_family_40x_c2())


# The macOS side of the encrypted wrapper. Encrypting a SCO5 document turns its magic into ZBO6 by
# itself, since the keystream carries 'S' to 'Z' and '5' to '6', so the fixture is the plain SCO5
# page-setup document run through the same cipher. Seventeen real files of this shape exist.
def gen_zbo6_from_sco5():
    return zbot_encrypt(gen_sco5_macos_page_setup())


def gen_zbot_from_bazo():
    """ZBOT-encrypted version of bazo.enc.  Decrypting must yield the same score."""
    bazo_path = os.path.join(OUT_DIR, 'bazo.enc')
    with open(bazo_path, 'rb') as f:
        scow = f.read()
    return zbot_encrypt(scow)


# ===========================================================================
# ornaments_multiinstr_slur_routing.enc
# Regression for resolvers-slur.cpp staffIdx mismatch in multi-instrument files.
#
# ps.staffIdx = routed LINE slot (0-3); em->staffIdx = raw compact instrIdx (0-1).
# Without fix: staves 1-3 have no notes matched → strategy-3 last-chord fallback
# picks note3 instead of note2. With fix: emLineSlot() maps raw byte to LINE slot.
#
# 2 instruments × 2 staves, 3 quarter notes per staff in measure 0:
#   staff 0 (piano treble, rawStaff=0x00): C4(60) → E4(64) → G4(67)
#   staff 1 (piano bass,   rawStaff=0x40): C3(48) → E3(52) → G3(55)
#   staff 2 (organ treble, rawStaff=0x01): A4(69) → B4(71) → C5(72)
#   staff 3 (organ bass,   rawStaff=0x41): A3(57) → B3(59) → C4(60)
# SLURSTART on each staff at tick=0 (xoffset=3, xoffset2=4).
# Expected slur endpoint = note2 (pitch E4/E3/B4/B3). Without fix: note3.
# ===========================================================================
def gen_v0c4_multiinstr_slur_routing():
    def note_raw(tick, voice, raw_staff, fv, pitch):
        """28-byte v0xC4 note with full raw_staff byte (no 0x3F truncation)."""
        d = bytearray(25)
        d[0] = 28; d[1] = raw_staff & 0xFF; d[2] = fv; d[12] = pitch
        return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)

    def orn_raw(tick, voice, raw_staff, tipo, xoffset=0, alMezuro=0, xoffset2=0):
        """33-byte ORN with full raw_staff byte."""
        d = bytearray(33)
        struct.pack_into('<H', d, 0, tick)
        d[2] = (5 << 4) | (voice & 0xF); d[3] = 33; d[4] = raw_staff & 0xFF
        d[5] = tipo; d[10] = xoffset; d[18] = alMezuro; d[20] = xoffset2
        return bytes(d)

    # Notes: tick=0 (note1), tick=240 (note2, slur endpoint), tick=480 (note3, last-chord decoy)
    # voice=4 (>=VOICES) for bass staves to trigger case-A routing to the bass LINE slot
    elems = b''
    for raw, voice, n1, n2, n3 in [
        (0x00, 0, 60, 64, 67),   # staff 0: piano treble
        (0x40, 4, 48, 52, 55),   # staff 1: piano bass
        (0x01, 0, 69, 71, 72),   # staff 2: organ treble
        (0x41, 4, 57, 59, 60),   # staff 3: organ bass
    ]:
        elems += note_raw(  0, voice, raw, fv=3, pitch=n1)
        elems += orn_raw(   0, voice, raw, tipo=0x21, xoffset=3, alMezuro=0, xoffset2=4)
        elems += note_raw(240, voice, raw, fv=3, pitch=n2)
        elems += note_raw(480, voice, raw, fv=3, pitch=n3)
    elems += end_marker()

    # Reuse the 2-instrument × 4-staff header + LINE block from gen_v0c4_multiinstr_compact_routing.
    hdr = bytearray(194)
    hdr[0:4] = b'SCOW'; hdr[4] = 0xC4
    struct.pack_into('<H', hdr, 0x28, 0x0420); struct.pack_into('<H', hdr, 0x2C, 0xF000)
    struct.pack_into('<h', hdr, 0x2E, 1); struct.pack_into('<h', hdr, 0x30, 1)
    hdr[0x32] = 2; hdr[0x33] = 4; struct.pack_into('<h', hdr, 0x34, 1)

    def make_staff_entry(clef, key, instr_staff_idx):
        e = bytearray(30); e[14] = clef; e[15] = key; e[19] = 1; e[21] = instr_staff_idx
        return bytes(e)

    line_data = (b'\x00' * 10 + struct.pack('<H', 0) + bytes([1])
                 + make_staff_entry(0, 0, 0x00) + make_staff_entry(1, 0, 0x40)
                 + make_staff_entry(0, 0, 0x01) + make_staff_entry(1, 0, 0x41))
    line_block = b'LINE' + struct.pack('<I', len(line_data)) + line_data

    meas = meas_block(meas_hdr(4, 4), elems)
    return bytes(hdr) + line_block + meas + SKELETON_POST


# ===========================================================================
# text_lyrics_grandstaff_routed_notes.enc
# Regression test for lyric-to-note matching on a routed (grand-staff) staff.
#
# Two instruments x 2 staves (4 staves). Lyrics belong to instrument 0's BOTTOM
# staff, whose notes reach MuseScore staff 1 via the voice>=VOICES (case A) path.
# The lyric also routes to staff 1 (voice 4). Instrument 1's TOP staff has raw
# staffIdx == 1, so the old note-tick collection (keyed by RAW encStaff == lyStaff)
# grabbed those wrong notes (ticks 0, 240) instead of the bottom-staff notes
# (480, 720). With the wrong ticks the syllables matched no chord and fell back to
# rests in reverse order ("ve Sal"). Routing the collected notes the same way the
# note loop does fixes it ("Sal ve" on the bottom-staff notes).
# Expected: bottom staff (MuseScore staff 1) shows "Sal-" then "ve" on the notes
# at ticks 480 and 720, in order.
# ===========================================================================
def gen_v0c4_lyrics_grandstaff_routed_notes():
    hdr = bytearray(194)
    hdr[0:4] = b'SCOW'
    hdr[4] = 0xC4
    struct.pack_into('<H', hdr, 0x28, 0x0420)
    struct.pack_into('<H', hdr, 0x2C, 0xF000)
    struct.pack_into('<h', hdr, 0x2E, 1)         # lineCount = 1
    struct.pack_into('<h', hdr, 0x30, 1)         # pageCount = 1
    hdr[0x32] = 2                                # instrumentCount = 2
    hdr[0x33] = 4                                # staffPerSystem = 4
    struct.pack_into('<h', hdr, 0x34, 1)         # measureCount = 1

    def make_staff_entry(clef, key, instr_staff_idx):
        e = bytearray(30)
        e[14] = clef
        e[15] = key
        e[19] = 1
        e[21] = instr_staff_idx
        return bytes(e)

    line_data = (b'\x00' * 10
                 + struct.pack('<H', 0)
                 + bytes([1])
                 + make_staff_entry(0, 0, 0x00)      # instr 0 treble
                 + make_staff_entry(1, 0, 0x40)      # instr 0 bass (staffWithin 1)
                 + make_staff_entry(0, 0, 0x01)      # instr 1 treble (raw staffIdx 1)
                 + make_staff_entry(1, 0, 0x41))     # instr 1 bass
    assert len(line_data) == 133, len(line_data)
    line_block = b'LINE' + struct.pack('<I', 133) + line_data

    def note_compact(tick, voice, raw_staff, fv, pitch):
        d = bytearray(25)
        d[0] = 28
        d[1] = raw_staff & 0xFF
        d[2] = fv
        d[12] = pitch
        return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)

    elems = b''
    elems += note_compact(0, 0, 0x00, fv=1, pitch=72)     # instr0 treble: whole note
    # instr0 bass (MuseScore staff 1) via voice=4: quarter notes at 480 and 720.
    elems += note_compact(480, 4, 0x40, fv=3, pitch=55)
    elems += note_compact(720, 4, 0x40, fv=3, pitch=57)
    # instr1 treble (raw staffIdx 1): the "wrong" notes at ticks 0 and 240.
    elems += note_compact(0,   0, 0x01, fv=3, pitch=64)
    elems += note_compact(240, 0, 0x01, fv=3, pitch=65)
    # Lyrics for the bottom staff, voice=4 -> routes to MuseScore staff 1 (case A).
    elems += lyric_v0c4(490, 4, 0, "Sal")
    elems += lyric_v0c4(491, 4, 0, "-")
    elems += lyric_v0c4(730, 4, 0, "ve")
    elems += end_marker()

    meas = meas_block(meas_hdr(4, 4), elems)
    return bytes(hdr) + line_block + meas + SKELETON_POST


# ===========================================================================
# notes_multiinstr_compact_routing.enc
# Regression test for multi-instrument compact staffIdx routing.
#
# Two instruments x 2 staves each (4 staves total). Notes use compact
# rawStaff encoding where the raw staff byte = (staffWithin<<6)|instrIdx,
# identical to the LINE block's instrStaffIdx format.
#
# Bug: before the fix the importer treated staffIdx as a LINE slot index,
# so organ notes (instrIdx=1) were placed on piano-bass (LINE slot 1).
# After fix: notes land on the correct staves.
#
# Staff 0 (instr 0 treble): C4=60, rawStaff=0x00, voice=0
# Staff 1 (instr 0 bass):   C3=48, rawStaff=0x40, voice=4 (out-of-band)
# Staff 2 (instr 1 treble): E4=64, rawStaff=0x01, voice=0
# Staff 3 (instr 1 bass):   E3=52, rawStaff=0x41, voice=4 (out-of-band)
# ===========================================================================
def gen_v0c4_multiinstr_compact_routing():
    # Build SCOW header (194 bytes = headerEnd 0xC2 for v0xC4)
    hdr = bytearray(194)
    hdr[0:4] = b'SCOW'
    hdr[4] = 0xC4               # version byte
    struct.pack_into('<H', hdr, 0x28, 0x0420)   # chuVersio
    struct.pack_into('<H', hdr, 0x2C, 0xF000)   # fiksa1
    struct.pack_into('<h', hdr, 0x2E, 1)         # lineCount = 1
    struct.pack_into('<h', hdr, 0x30, 1)         # pageCount = 1
    hdr[0x32] = 2                                # instrumentCount = 2
    hdr[0x33] = 4                                # staffPerSystem = 4
    struct.pack_into('<h', hdr, 0x34, 1)         # measureCount = 1

    # Build LINE block with 4 staff entries (instrStaffIdx = 0x00,0x40,0x01,0x41)
    def make_staff_entry(clef, key, instr_staff_idx):
        e = bytearray(30)   # 30 bytes per EncLineStaffData
        # bytes 0-13: layout zeros (skipped by parser)
        e[14] = clef        # clef type
        e[15] = key         # key signature
        # e[16] = pageIdx = 0
        # e[17] = skip0 = 0
        # e[18] = skip1 = 0
        e[19] = 1           # showByte = 1 (show staff)
        # e[20] = staffType = 0 (MELODY)
        e[21] = instr_staff_idx  # the key field: (staffWithin<<6)|instrIdx
        # bytes 22-29: zeros (skipped)
        return bytes(e)

    line_data = (b'\x00' * 10                       # skipRawData(10) in EncLine::read
                 + struct.pack('<H', 0)              # start = 0
                 + bytes([1])                        # measureCount = 1
                 + make_staff_entry(0, 0, 0x00)      # instr 0, staffWithin 0 (treble G)
                 + make_staff_entry(1, 0, 0x40)      # instr 0, staffWithin 1 (bass F)
                 + make_staff_entry(0, 0, 0x01)      # instr 1, staffWithin 0 (treble G)
                 + make_staff_entry(1, 0, 0x41))     # instr 1, staffWithin 1 (bass F)
    # toSkip = varSize + 8 - 21 - 30*4 = 0 when varSize = 133
    assert len(line_data) == 133, len(line_data)
    line_block = b'LINE' + struct.pack('<I', 133) + line_data

    # Build MEAS block: one whole note per staff using compact rawStaff encoding.
    # Raw staff byte = (staffWithin<<6)|instrIdx -- same format as instrStaffIdx.
    def note_compact(tick, voice, raw_staff, fv, pitch):
        """28-byte v0xC4 note with full rawStaff byte (no 0x3F truncation)."""
        d = bytearray(25)
        d[0] = 28
        d[1] = raw_staff & 0xFF   # full byte preserving staffWithin in high 2 bits
        d[2] = fv
        d[12] = pitch
        return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)

    # voice=4 (>= VOICES=4) triggers the case-A out-of-band routing path for bass staves,
    # matching the encoding used in real multi-staff Encore files (e.g. SALVEDOL.ENC).
    elems = b''
    elems += note_compact(0, 0, 0x00, fv=1, pitch=60)   # instr0 treble: C4
    elems += note_compact(0, 4, 0x40, fv=1, pitch=48)   # instr0 bass:   C3, voice>=VOICES
    elems += note_compact(0, 0, 0x01, fv=1, pitch=64)   # instr1 treble: E4
    elems += note_compact(0, 4, 0x41, fv=1, pitch=52)   # instr1 bass:   E3, voice>=VOICES
    elems += end_marker()

    meas = meas_block(meas_hdr(4, 4), elems)

    return bytes(hdr) + line_block + meas + SKELETON_POST


# ===========================================================================
# structure_sco5_macos.enc
# SCO5 is the big-endian macOS Encore 5 format. Its PREC block is a macOS
# NSPrintInfo XML plist (paper/orientation/scale), and it does not store document
# margins anywhere importable. The importer reads page size + orientation from the
# plist and forces zero margins (Encore lays these scores edge to edge).
# Fixture: minimal SCO5 file, Letter portrait plist, one 4/4 measure.
# Expected: page 8.5 x 11 in, all margins 0.
# ===========================================================================
def gen_sco5_macos_page_setup():
    def be(fmt, *a):
        return struct.pack('>' + fmt, *a)

    h = bytearray(194)
    h[0:4] = b'SCO5'
    h[4] = 0                                  # chuMagio -> default v0xC4 reader
    struct.pack_into('>H', h, 0x28, 0x0420)   # chuVersio = Encore 5
    struct.pack_into('>h', h, 0x2E, 1)        # lineCount
    struct.pack_into('>h', h, 0x30, 1)        # pageCount
    h[0x32] = 1                               # instrumentCount
    h[0x33] = 1                               # staffPerSystem
    struct.pack_into('>h', h, 0x34, 1)        # measureCount
    h[0x52] = 4                               # scoreSize (default)

    # LINE: skip10 + start(u16) + measureCount(u8) + one 30-byte staff entry.
    line = bytearray(10) + be('H', 0) + bytes([1])
    staff = bytearray(30)
    staff[19] = 1                             # show staff (clef@14=0=G, instrStaffIdx@21=0)
    line += staff
    line_blk = b'LINE' + be('I', len(line)) + bytes(line)

    # MEAS: 0x36 header (4/4, beatTicks=240) + one quarter note + end marker.
    mh = bytearray(0x36)
    struct.pack_into('>H', mh, 0, 100)        # bpm
    struct.pack_into('>H', mh, 4, 240)        # beatTicks
    struct.pack_into('>H', mh, 6, 960)        # durTicks
    mh[8] = 4
    mh[9] = 4
    nd = bytearray(25)
    nd[0] = 28        # element size
    nd[2] = 3         # faceValue = quarter
    nd[12] = 60       # pitch
    note = be('H', 0) + bytes([0x90]) + bytes(nd)   # tick=0, typeVoice=NOTE|voice0
    elems = bytes(note) + b'\xff\xff'
    meas_blk = b'MEAS' + be('I', len(elems)) + bytes(mh) + elems

    plist = (b'<?xml version="1.0" encoding="UTF-8"?>\n<plist version="1.0"><dict>\n'
             b'<key>com.apple.print.PageFormat.PMOrientation</key><integer>1</integer>\n'
             b'<key>PMTiogaPaperName</key><string>na-letter</string>\n'
             b'</dict></plist>\n')
    prec_blk = b'PREC' + be('I', len(plist)) + plist

    return bytes(h) + line_blk + meas_blk + prec_blk


# ===========================================================================
# instruments_sco5_tk_names.enc
# SCO5 (big-endian macOS Encore 5) with two instruments, each carrying its name in
# a TK block. Every block frames its size big-endian EXCEPT the TK blocks, whose
# size field is little-endian ("70 00 00 00" = 112). Read big-endian and masked to
# 16 bits, that size becomes 0, so the name-scan loop reads nothing: only the first
# instrument name is later recovered by position and the rest are lost. The importer
# must undo the big-endian read of the TK size so both names import.
# ===========================================================================
def gen_sco5_tk_instrument_names():
    def be(fmt, *a):
        return struct.pack('>' + fmt, *a)

    def tk_block(idx, name):
        # 112-byte TK block: magic(4) + LITTLE-endian size(4)=112 + 104 content
        # bytes (Latin-1 name, NUL terminator, zero padding), mirroring real SCO5.
        content = bytearray(104)
        enc = name.encode('latin-1')
        content[0:len(enc)] = enc            # name at content+0, NUL already present
        magic = ('TK%02d' % idx).encode('ascii')
        return magic + struct.pack('<I', 112) + bytes(content)

    h = bytearray(194)
    h[0:4] = b'SCO5'
    h[4] = 0                                  # chuMagio -> default v0xC4 reader
    struct.pack_into('>H', h, 0x28, 0x0420)   # chuVersio = Encore 5
    struct.pack_into('>h', h, 0x2E, 1)        # lineCount
    struct.pack_into('>h', h, 0x30, 1)        # pageCount
    h[0x32] = 2                               # instrumentCount
    h[0x33] = 2                               # staffPerSystem
    struct.pack_into('>h', h, 0x34, 1)        # measureCount
    h[0x52] = 4                               # scoreSize (default)

    tk0 = tk_block(0, "CORNETA 1")
    tk1 = tk_block(1, "TROMPETA 2")

    # LINE: skip10 + start(u16) + measureCount(u8) + two 30-byte staff entries.
    # Emitted BEFORE the TK blocks so the instrument name fields do not line up with
    # the position-based name-recovery offsets (which would otherwise mask the bug);
    # the names must then come from the TK block read itself.
    line = bytearray(10) + be('H', 0) + bytes([1])
    for si in range(2):
        staff = bytearray(30)
        staff[19] = 1                         # show staff (clef@14=0=G)
        staff[21] = si                        # instrStaffIdx -> instrument si
        line += staff
    line_blk = b'LINE' + be('I', len(line)) + bytes(line)

    # MEAS: 0x36 header (4/4, beatTicks=240) + one quarter note + end marker.
    mh = bytearray(0x36)
    struct.pack_into('>H', mh, 0, 100)        # bpm
    struct.pack_into('>H', mh, 4, 240)        # beatTicks
    struct.pack_into('>H', mh, 6, 960)        # durTicks
    mh[8] = 4
    mh[9] = 4
    nd = bytearray(25)
    nd[0] = 28        # element size
    nd[2] = 3         # faceValue = quarter
    nd[12] = 60       # pitch
    note = be('H', 0) + bytes([0x90]) + bytes(nd)   # tick=0, typeVoice=NOTE|voice0
    elems = bytes(note) + b'\xff\xff'
    meas_blk = b'MEAS' + be('I', len(elems)) + bytes(mh) + elems

    return bytes(h) + line_blk + tk0 + tk1 + meas_blk


# ===========================================================================
# notes_sco5_tie_arc_bigendian.enc
# SCO5 (big-endian macOS Encore 5): two half notes of the same pitch joined by a
# tie whose only forward-tie signal is the arc span. The arc endpoints are uint16,
# so in this byte order their significant byte is the second one: a reader taking
# only the first byte sees arcX1 == arcX2 == 0, reads that as an intra-chord
# decorative arc and drops the tie. Both flag bytes are clear so nothing else can
# rescue it.
# ===========================================================================
def gen_sco5_tie_arc_bigendian():
    def be(fmt, *a):
        return struct.pack('>' + fmt, *a)

    h = bytearray(194)
    h[0:4] = b'SCO5'
    h[4] = 0                                  # chuMagio -> default v0xC4 reader
    struct.pack_into('>H', h, 0x28, 0x0420)   # chuVersio = Encore 5
    struct.pack_into('>h', h, 0x2E, 1)        # lineCount
    struct.pack_into('>h', h, 0x30, 1)        # pageCount
    h[0x32] = 1                               # instrumentCount
    h[0x33] = 1                               # staffPerSystem
    struct.pack_into('>h', h, 0x34, 1)        # measureCount
    h[0x52] = 4                               # scoreSize (default)

    line = bytearray(10) + be('H', 0) + bytes([1])
    staff = bytearray(30)
    staff[19] = 1                             # show staff (clef@14=0=G)
    line += staff
    line_blk = b'LINE' + be('I', len(line)) + bytes(line)

    mh = bytearray(0x36)
    struct.pack_into('>H', mh, 0, 100)        # bpm
    struct.pack_into('>H', mh, 4, 240)        # beatTicks
    struct.pack_into('>H', mh, 6, 960)        # durTicks
    mh[8] = 4
    mh[9] = 4

    def note(tick, pitch):
        nd = bytearray(25)
        nd[0] = 28        # element size
        nd[2] = 2         # faceValue = half
        nd[12] = pitch    # +15
        return be('H', tick) + bytes([0x90]) + bytes(nd)

    def tie(tick, arcX1, arcX2):
        # 18-byte TIE: d[0]=size, d[2]=+5 direction, d[3]=+6 start flag, both clear.
        # arcX1 at +10 and arcX2 at +12 are uint16 and so are written big-endian here.
        d = bytearray(15)
        d[0] = 18
        struct.pack_into('>H', d, 7, arcX1)    # d[7] = element +10
        struct.pack_into('>H', d, 9, arcX2)    # d[9] = element +12
        return be('H', tick) + bytes([0x30]) + bytes(d)

    elems = note(0, 60) + tie(0, 20, 96) + note(480, 60) + b'\xff\xff'
    meas_blk = b'MEAS' + be('I', len(elems)) + bytes(mh) + elems
    return bytes(h) + line_blk + meas_blk


# ===========================================================================
# ornaments_sco5_bigendian.enc
# SCO5 is the big-endian macOS build, and the corpus holds 16 of them against three fixtures that
# between them touch no ornament at all and no rest. This one carries the three ornament kinds the
# importer treats differently, an articulation, a dynamic and a fermata, plus a rest, all in the
# byte order that separates this container from every other.
# ===========================================================================
def gen_sco5_ornaments_and_rest():
    def be(fmt, *a):
        return struct.pack('>' + fmt, *a)

    h = bytearray(194)
    h[0:4] = b'SCO5'
    h[4] = 0                                  # no chuMagio: the magic selects the reader
    struct.pack_into('>H', h, 0x28, 0x0420)
    struct.pack_into('>h', h, 0x2E, 1)
    struct.pack_into('>h', h, 0x30, 1)
    h[0x32] = 1
    h[0x33] = 1
    struct.pack_into('>h', h, 0x34, 1)
    h[0x52] = 4

    line = bytearray(10) + be('H', 0) + bytes([1])
    staff = bytearray(30)
    staff[19] = 1
    line += staff
    line_blk = b'LINE' + be('I', len(line)) + bytes(line)

    mh = bytearray(0x36)
    struct.pack_into('>H', mh, 0, 100)
    struct.pack_into('>H', mh, 4, 240)
    struct.pack_into('>H', mh, 6, 960)
    mh[8], mh[9] = 4, 4

    def note(tick, pitch, fv=2):
        nd = bytearray(25)
        nd[0] = 28
        nd[2] = fv
        nd[12] = pitch
        return be('H', tick) + bytes([0x90]) + bytes(nd)

    def rest(tick, fv=2):
        rd = bytearray(15)
        rd[0] = 18            # the rest size SCO5 uses, untouched by any fixture before this
        rd[2] = fv
        return be('H', tick) + bytes([0x80]) + bytes(rd)

    def orn(tick, tipo):
        d = bytearray(13)
        d[0] = 16
        d[2] = tipo
        return be('H', tick) + bytes([0x50]) + bytes(d)

    elems  = note(0, 60)
    elems += orn(0, 0xC9)     # staccato
    elems += orn(0, 0x85)     # dynamic f
    elems += orn(0, 0xCC)     # fermata above, on the note like Encore writes it
    elems += rest(480)
    elems += b'\xff\xff'
    meas_blk = b'MEAS' + be('I', len(elems)) + bytes(mh) + elems
    return bytes(h) + line_blk + meas_blk


# ===========================================================================
# ornaments_v0c2_same_measure_slur_no_cross.enc
# Regression: v0xC2 slur starting mid-measure must end within the same measure,
# not cross to the next. The cross-measure extension must not fire when there is
# a note after the slur start in the current measure.
#
# Pattern reproduces SALVEDOL.ENC measure 3:
#   firstNoteXoff=9, slurXoffset=11, slurXoffset2=12, pixelSpan=1,
#   targetEndXoff=10, maxXoffInMeas=9 → cross-measure extension fires without fix.
#
# Measure 0: C4 (xoff=9) | SLURSTART (xoff=11, xoff2=12) | E4 (xoff=5)
# Measure 1: G4 (xoff=9) -- decoy (dist=1 from targetEndXoff=10, beats same-measure dist=5)
# ===========================================================================
def note_v0c2_xoff(tick, voice, staffIdx, fv, pitch, xoffset=0):
    """22-byte v0xC2 note with explicit xoffset (= EncMeasureElem.xoffset at es+10 = d[7])."""
    d = bytearray(19)
    d[0]=22; d[1]=staffIdx&0x3F; d[2]=fv
    d[7]=xoffset    # note xoffset at es+10
    d[10]=pitch     # semiTonePitch stored in tuplet slot for size=22 (postProcessElement swaps them)
    return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

# ===========================================================================
# ornaments_v0c2_slur_firstnote_xoff_mismatch.enc
# Regression for targetEndXoff = slurXoffset2 fix.
# When firstNoteXoff << slurXoffset, the OLD formula (firstNoteXoff + pixelSpan)
# produces a target far from the intended endpoint, and a later "decoy" note with
# xoffset close to that low target incorrectly wins.
#
# Pattern reproduces SALVEDOL.ENC organ bass slurs:
#   note1 tick=0   xoff=2  (slur start, firstNoteXoff=2)
#   note2 tick=240 xoff=9  (correct endpoint)
#   note3 tick=480 xoff=3  (decoy: dist=0 from OLD target=3)
# SLURSTART at tick=0: slurXoff=10, xoffset2=11.
#   OLD target = 2 + (11-10) = 3  → note3 wins (dist=0) ✗
#   NEW target = 11               → note2 wins (dist=2) ✓
# ===========================================================================
def gen_v0c2_slur_firstnote_xoff_mismatch():
    e0 = (note_v0c2_xoff(  0, 0, 0, fv=3, pitch=60, xoffset=2)   # note1: C4, small xoff
        + ornament_v0c4(    0, 0, 0, tipo=0x21, xoffset=10, alMezuro=0, xoffset2=11)
        + note_v0c2_xoff(240, 0, 0, fv=3, pitch=64, xoffset=9)   # note2: E4, correct end
        + note_v0c2_xoff(480, 0, 0, fv=3, pitch=60, xoffset=3)   # note3: C4 decoy (xoff≈OLD target)
        + end_marker())
    hdr = meas_hdr(4, 4)
    return assemble(0xC2, [(hdr, e0)])


def gen_v0c2_same_measure_slur_no_cross():
    e0 = (note_v0c2_xoff(  0, 0, 0, fv=3, pitch=60, xoffset=9)    # C4, firstNoteXoff=9
        + ornament_v0c4(    0, 0, 0, tipo=0x21, xoffset=11, alMezuro=0, xoffset2=12)
        + note_v0c2_xoff(240, 0, 0, fv=3, pitch=64, xoffset=5)    # E4: slur endpoint, dist=5
        + end_marker())
    e1 = (note_v0c2_xoff(0, 0, 0, fv=3, pitch=67, xoffset=9)       # G4: cross-measure decoy, dist=1
        + end_marker())
    hdr = meas_hdr(4, 4)
    return assemble(0xC2, [(hdr, e0), (hdr, e1)])


# ===========================================================================
# ornaments_v0c2_unreliable_slur_count.enc
# Some v0xC2 files store noise in the slur measure-count field (+16): plausible small
# values mixed with out-of-range sentinels (0xFE/0xFF), even though every slur is a
# within-bar arc. When any slur's count points past the last measure, the whole file's
# +16 field is unreliable and every slur must resolve within its own bar.
#   slur A @0:   +16 = 255 (0xFF, out of range) -> marks the file's counts unreliable
#   slur B @480: +18 = 1   (plausible, would wrongly span to m1 if trusted)
# Measure 1 has a note so a wrongly-trusted count would form a real cross-measure slur.
# Expected: both slurs stay inside measure 0.
# ===========================================================================
def gen_v0c2_unreliable_slur_count():
    e0 = (note_v0c2_xoff(  0, 0, 0, fv=3, pitch=60, xoffset=9)
        + ornament_v0c4(    0, 0, 0, tipo=0x21, xoffset=11, xoffset2=12, alMezuro=255)  # sentinel
        + note_v0c2_xoff(240, 0, 0, fv=3, pitch=62, xoffset=20)
        + note_v0c2_xoff(480, 0, 0, fv=3, pitch=64, xoffset=50)
        + ornament_v0c4(  480, 0, 0, tipo=0x21, xoffset=50, xoffset2=70, alMezuro=1)     # plausible
        + note_v0c2_xoff(720, 0, 0, fv=3, pitch=65, xoffset=70)
        + end_marker())
    e1 = (note_v0c2_xoff(0, 0, 0, fv=3, pitch=67, xoffset=9)
        + end_marker())
    hdr = meas_hdr(4, 4)
    return assemble(0xC2, [(hdr, e0), (hdr, e1)])


# ===========================================================================
def gen_v0c2_multiinstr_compact_routing():
    def note_raw(tick, voice, raw_staff, fv, pitch):
        """22-byte v0xC2 note with full raw_staff byte."""
        d = bytearray(19)
        d[0]=22; d[1]=raw_staff&0xFF; d[2]=fv; d[10]=pitch
        return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

    # Whole notes per staff. pitch values: C4=60, C3=48, E4=64, E3=52
    elems = b''
    elems += note_raw(0, 0, 0x00, fv=1, pitch=60)   # staff 0 piano treble: C4
    elems += note_raw(0, 4, 0x40, fv=1, pitch=48)   # staff 1 piano bass:   C3
    elems += note_raw(0, 0, 0x01, fv=1, pitch=64)   # staff 2 organ treble: E4
    elems += note_raw(0, 4, 0x41, fv=1, pitch=52)   # staff 3 organ bass:   E3
    elems += end_marker()

    hdr = bytearray(194)
    hdr[0:4]=b'SCOW'; hdr[4]=0xC2   # v0xC2
    struct.pack_into('<H',hdr,0x28,0x0420); struct.pack_into('<H',hdr,0x2C,0xF000)
    struct.pack_into('<h',hdr,0x2E,1); struct.pack_into('<h',hdr,0x30,1)
    hdr[0x32]=2; hdr[0x33]=4; struct.pack_into('<h',hdr,0x34,1)

    def staff_entry(clef, key, isidx):
        e=bytearray(30); e[14]=clef; e[15]=key; e[19]=1; e[21]=isidx; return bytes(e)

    line_data=(b'\x00'*10+struct.pack('<H',0)+bytes([1])
               +staff_entry(0,0,0x00)+staff_entry(1,0,0x40)
               +staff_entry(0,0,0x01)+staff_entry(1,0,0x41))
    line_block=b'LINE'+struct.pack('<I',len(line_data))+line_data
    meas=meas_block(meas_hdr(4,4), elems)
    return bytes(hdr)+line_block+meas+SKELETON_POST


# ===========================================================================
# ornaments_v0c2_multiinstr_slur_routing.enc
# Combined regression for emLineSlot fix + targetEndXoff fix in v0xC2 format.
# Reproduces the SALVEDOL organ-bass pattern (firstNoteXoff=2, slurXoff=10,
# xoffset2=11, decoy note3 at xoff=3) on all 4 staves of a 2-instrument file.
#
# Without emLineSlot fix: staves 1-3 have no notes matched → strategy-3
#   last-chord picks note3 (wrong).
# Without targetEndXoff fix: target=3, note3(dist=0) beats note2(dist=6) (wrong).
# Both fixes needed: staves found correctly AND target=11 so note2(dist=2) wins.
#
# Expected slur endpoints (pitch of note2):
#   staff 0 piano treble: 60  staff 1 piano bass: 52
#   staff 2 organ treble: 71  staff 3 organ bass: 59
# ===========================================================================
def gen_v0c2_multiinstr_slur_routing():
    def note_raw(tick, voice, raw_staff, fv, pitch, xoff=0):
        """22-byte v0xC2 note with full raw_staff and explicit xoffset."""
        d = bytearray(19)
        d[0]=22; d[1]=raw_staff&0xFF; d[2]=fv; d[7]=xoff; d[10]=pitch
        return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

    def orn_raw(tick, voice, raw_staff, tipo, xoffset=0, alMezuro=0, xoffset2=0):
        d = bytearray(33)
        struct.pack_into('<H',d,0,tick); d[2]=(5<<4)|(voice&0xF); d[3]=33
        d[4]=raw_staff&0xFF; d[5]=tipo; d[10]=xoffset; d[18]=alMezuro; d[20]=xoffset2
        return bytes(d)

    # Per staff: note1(xoff=2) + SLUR(xoff=10,xoff2=11) + note2(xoff=9) + note3(xoff=3 decoy)
    # Pitches: n2 values are the expected endpoint pitches used in the test.
    elems = b''
    for raw, voice, n1, n2, n3 in [
        (0x00, 0, 55, 60, 50),   # staff 0 piano treble: n2=C4
        (0x40, 4, 45, 52, 48),   # staff 1 piano bass:   n2=E3
        (0x01, 0, 67, 71, 62),   # staff 2 organ treble: n2=B4
        (0x41, 4, 57, 59, 54),   # staff 3 organ bass:   n2=B3
    ]:
        elems += note_raw(  0, voice, raw, fv=3, pitch=n1, xoff=2)
        elems += orn_raw(   0, voice, raw, tipo=0x21, xoffset=10, alMezuro=0, xoffset2=11)
        elems += note_raw(240, voice, raw, fv=3, pitch=n2, xoff=9)
        elems += note_raw(480, voice, raw, fv=3, pitch=n3, xoff=3)
    elems += end_marker()

    hdr = bytearray(194)
    hdr[0:4]=b'SCOW'; hdr[4]=0xC2   # v0xC2
    struct.pack_into('<H',hdr,0x28,0x0420); struct.pack_into('<H',hdr,0x2C,0xF000)
    struct.pack_into('<h',hdr,0x2E,1); struct.pack_into('<h',hdr,0x30,1)
    hdr[0x32]=2; hdr[0x33]=4; struct.pack_into('<h',hdr,0x34,1)

    def staff_entry(clef, key, isidx):
        e=bytearray(30); e[14]=clef; e[15]=key; e[19]=1; e[21]=isidx; return bytes(e)

    line_data=(b'\x00'*10+struct.pack('<H',0)+bytes([1])
               +staff_entry(0,0,0x00)+staff_entry(1,0,0x40)
               +staff_entry(0,0,0x01)+staff_entry(1,0,0x41))
    line_block=b'LINE'+struct.pack('<I',len(line_data))+line_data
    meas=meas_block(meas_hdr(4,4), elems)
    return bytes(hdr)+line_block+meas+SKELETON_POST


# ===========================================================================
# structure_pickup_casea_sparse.enc
#
# Case A pickup regression: timeSig[0]=2/4, timeSig[1]=4/4.
# Measure 0 only has 2 eighth notes: rdur[0]=120 → V_EIGHTH (advance=1/8),
# rdur[1]=360 → V_QUARTER (advance=1/4). cumTick = 3/8 < measure->ticks()=2/4.
# Without the Case A guard, Case B would double-shorten to 3/8 and shift all
# subsequent measures incorrectly. With the guard (timesig=4/4 != ticks=2/4),
# Case B is skipped and measure 0 stays at 2/4.
# ===========================================================================
def gen_v0c4_pickup_casea_sparse():
    e0  = note_v0c4(  0, 0, 0, fv=4, pitch=60)   # 8th at tick=0: rdur=120 → V_EIGHTH
    e0 += note_v0c4(120, 0, 0, fv=4, pitch=62)   # fv=4 at tick=120: rdur=360 → V_QUARTER
    e0 += end_marker()
    e1  = note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(240, 0, 0, fv=3, pitch=64)
    e1 += note_v0c4(480, 0, 0, fv=3, pitch=67)
    e1 += note_v0c4(720, 0, 0, fv=3, pitch=72)
    e1 += end_marker()
    return assemble(0xC4, [
        (meas_hdr(2, 4), e0),   # Case A: timeSig=2/4 < nominal 4/4; sparse content (3/8)
        (meas_hdr(4, 4), e1),
    ], fill_ts=(4, 4))


# ===========================================================================
# structure_pickup_caseb_hairpin.enc
#
# Case B pickup + hairpin regression: 4/4 measure with only 2 eighth notes
# (cumTick=1/4 → shortens to 1/4, shifts all subsequent measures back 3/4).
# A WEDGESTART ornament at tick=0 with alMezuro=1 (extends to measure 1).
# Without the maxEndTick fix, the stale search boundary (3/4 too large) could
# cause the hairpin resolution to search past the correct endpoint in measure 1.
# The test verifies: (a) hairpin exists, (b) it ends within measure 1.
# ===========================================================================
def gen_v0c4_pickup_caseb_hairpin():
    e0  = note_v0c4(  0, 0, 0, fv=4, pitch=60)   # 8th: rdur=120 → V_EIGHTH
    e0 += note_v0c4(120, 0, 0, fv=4, pitch=62)   # 8th: fallback → V_EIGHTH; cumTick=1/4
    e0 += ornament_v0c4(0, 0, 0, tipo=0x1D, alMezuro=1)  # WEDGESTART, span 1 measure
    e0 += end_marker()
    e1  = note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(240, 0, 0, fv=3, pitch=64)
    e1 += note_v0c4(480, 0, 0, fv=3, pitch=67)
    e1 += note_v0c4(720, 0, 0, fv=3, pitch=72)
    e1 += end_marker()
    return assemble(0xC4, [
        (meas_hdr(4, 4), e0),   # Case B pickup: 2 eighths, cumTick=1/4
        (meas_hdr(4, 4), e1),
    ], fill_ts=(4, 4))


# ===========================================================================
# structure_pickup_measure.enc
#
# Pickup (anacrusis) first measure: timeSig[0]=1/4, timeSig[1]=4/4.
# The importer detects the mismatch and should produce a full-duration
# first measure (4/4) with a leading invisible gap rest (3/4) and the
# pickup note placed at the end of the measure (offset 3/4).
# ===========================================================================
def gen_v0c4_pickup_measure():
    e0  = note_v0c4(0, 0, 0, fv=3, pitch=60)   # C4 quarter at tick=0 (pickup note)
    e0 += end_marker()
    e1  = note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(240, 0, 0, fv=3, pitch=64)
    e1 += note_v0c4(480, 0, 0, fv=3, pitch=67)
    e1 += note_v0c4(720, 0, 0, fv=3, pitch=72)
    e1 += end_marker()
    return assemble(0xC4, [
        (meas_hdr(1, 4), e0),   # pickup: timeSig=1/4
        (meas_hdr(4, 4), e1),   # full measure: 4/4
    ], fill_ts=(4, 4))


# ===========================================================================
# structure_pickup_measure_same_ts.enc
#
# Pickup where timeSig[0]==timeSig[1]==4/4 but measure 0 only fills 1/4
# (Case B: same timesig, fewer notes). Two 8th notes at ticks 0 and 120:
#   note0 rdur = 120 (tick spacing) -> V_EIGHTH, advance=1/8
#   note1 rdur = 840 (no successor, falls back to fv=4=V_EIGHTH), advance=1/8
# cumTick after both = 1/4; gap = 3/4.
# The importer must shift both notes to the END (offset 3/4 and 7/8).
# ===========================================================================
def gen_v0c4_pickup_measure_same_ts():
    e0  = note_v0c4(  0, 0, 0, fv=4, pitch=60)   # 8th at tick=0
    e0 += note_v0c4(120, 0, 0, fv=4, pitch=62)   # 8th at tick=120
    e0 += end_marker()
    e1  = note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(240, 0, 0, fv=3, pitch=64)
    e1 += note_v0c4(480, 0, 0, fv=3, pitch=67)
    e1 += note_v0c4(720, 0, 0, fv=3, pitch=72)
    e1 += end_marker()
    return assemble(0xC4, [
        (meas_hdr(4, 4), e0),   # pickup: same timeSig=4/4, 2 eighth notes
        (meas_hdr(4, 4), e1),   # full measure: 4/4
    ], fill_ts=(4, 4))


# ===========================================================================
# notes_implicit_trailing_gap.enc
#
# Measure 0: fully filled 4/4 (4 quarter notes), no pickup detection.
# Measure 1: two eighth notes filling 1/4 of 4/4, trailing 3/4 should
# be invisible gap rests, not visible rests.
#   m1 note0: rdur = 120 (tick spacing) -> V_EIGHTH, advance=1/8
#   m1 note1: rdur = 840 (fallback V_EIGHTH), advance=1/8
#   cumTick after m1 = 1/4; trailing gap = 3/4 → invisible gap rest.
# ===========================================================================
def gen_v0c4_implicit_trailing_gap():
    e0  = note_v0c4(  0, 0, 0, fv=3, pitch=60)   # full 4/4 in measure 0
    e0 += note_v0c4(240, 0, 0, fv=3, pitch=64)
    e0 += note_v0c4(480, 0, 0, fv=3, pitch=67)
    e0 += note_v0c4(720, 0, 0, fv=3, pitch=72)
    e0 += end_marker()
    e1  = note_v0c4(  0, 0, 0, fv=4, pitch=60)   # measure 1: only 2 eighths
    e1 += note_v0c4(120, 0, 0, fv=4, pitch=62)
    e1 += end_marker()
    return assemble(0xC4, [
        (meas_hdr(4, 4), e0),   # full measure (avoids pickup detection for m0)
        (meas_hdr(4, 4), e1),   # partially filled: trailing 3/4 must be invisible
    ], fill_ts=(4, 4))


# ===========================================================================
# structure_pickup_caseb_reduces.enc
#
# Case B pickup: same timeSig=4/4, 8 32nd notes from tick=0.
# No gap-snap (each note at cumTick*960 = correct Encore tick).
#   notes 0-6: rdur = 30 (tick spacing) -> V_32ND, advance=1/32 each
#   note 7 (tick=210): rdur = durTicks-210 = 750 -> fallback V_32ND
# cumTick after all = 8/32 = 1/4 < 4/4 -> shortens to 1/4 (pure cumTick, no barline).
# ===========================================================================
def gen_v0c4_pickup_caseb_reduces():
    e0 = b''.join(note_v0c4(i*30, 0, 0, fv=6, pitch=60) for i in range(8))
    e0 += end_marker()
    e1  = note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(240, 0, 0, fv=3, pitch=64)
    e1 += note_v0c4(480, 0, 0, fv=3, pitch=67)
    e1 += note_v0c4(720, 0, 0, fv=3, pitch=72)
    e1 += end_marker()
    return assemble(0xC4, [
        (meas_hdr(4, 4), e0),   # partial: 8 32nds = 1/4 of 4/4
        (meas_hdr(4, 4), e1),   # full measure
    ], fill_ts=(4, 4))


# ===========================================================================
# structure_pickup_caseb_no_reduce_full.enc
#
# Case B: same timeSig=4/4, whole note (fv=1) at tick=0 fills the measure.
# cumTick after whole note = 1 = measure->ticks() = 4/4 -> NOT shortened.
# The second note at tick=480 is dropped (voice full).
# Expected: measure 0 keeps its full 4/4 duration.
# ===========================================================================
def gen_v0c4_pickup_caseb_no_reduce_full():
    e0  = note_v0c4(  0, 0, 0, fv=1, pitch=60)   # whole note: cumTick = 1 = measure->ticks()
    e0 += note_v0c4(480, 0, 0, fv=3, pitch=62)   # quarter (dropped, voice full)
    e0 += end_marker()
    e1  = note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(240, 0, 0, fv=3, pitch=64)
    e1 += note_v0c4(480, 0, 0, fv=3, pitch=67)
    e1 += note_v0c4(720, 0, 0, fv=3, pitch=72)
    e1 += end_marker()
    return assemble(0xC4, [
        (meas_hdr(4, 4), e0),   # whole note fills measure → cumTick = 1 = m->ticks() → no reduction
        (meas_hdr(4, 4), e1),
    ], fill_ts=(4, 4))


def gen_v0c4_16th_rdur112_no_triple_dot():
    """BUG regression: a 16th note whose MIDI rdur happens to equal 60*15/8=112
    (integer truncation of the triple-dotted value 112.5) was falsely assigned
    dots=3, advancing the cursor by 15/128 instead of 1/16 and corrupting the
    rest of the measure.

    Fixture: two notes in 4/4. NOTE@0 (16th) → NOTE@112 (half).
    The rdur of NOTE@0 = 112-0 = 112 ticks.
    After fix: dots=0, durationType=16th.
    """
    e  = note_v0c4(  0, 0, 0, fv=5, pitch=60)   # 16th at tick=0 (rdur=112 from next)
    e += note_v0c4(112, 0, 0, fv=2, pitch=62)   # half
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_timesig_change_2_2_to_4_4():
    """BUG regression: Fraction(2,2)==Fraction(4,4) via cross-multiplication
    (2x4==4x2=8), so the 2/2→4/4 change was silently swallowed just like 6/8→3/4.
    Fix: use Fraction::identical() in buildInitialSignatures.
    Fixture: 2 measures 2/2, 2 measures 4/4, 2 measures 2/2.
    """
    h22 = meas_hdr(2, 2, beatTicks=480, durTicks=960)
    h44 = meas_hdr(4, 4, beatTicks=240, durTicks=960)
    em  = b'\xff\xff'
    measures = [
        (h22, em), (h22, em),   # M0-M1: 2/2
        (h44, em), (h44, em),   # M2-M3: 4/4
        (h22, em), (h22, em),   # M4-M5: 2/2
    ]
    return assemble(0xC4, measures, fill_ts=(2, 2))


def gen_v0c4_triplet_orphan_prior_complete_group():
    """BUG regression: orphan sandwich with a prior complete group in the measure.
    seenCompleteGroup=True after group 1 must not block the sandwich heuristic
    for group 2.

    Fixture: 4/4 (dur=960):
      Group 1 (complete):  tick=0(0x32), tick=80(0x32), tick=160(0x32)  <- 3:2
      Regular Q:           tick=240
      Group 2 (orphan):    tick=480(0x32), tick=560(0x00), tick=640(0x32) <- 3:2
      Regular Q:           tick=720
    """
    def note_tup(tick, pitch, tuplet):
        d = bytearray(25)
        d[0] = 28; d[2] = 4; d[10] = tuplet; d[12] = pitch
        return struct.pack('<H', tick) + bytes([(9 << 4)]) + bytes(d)

    e  = note_tup(  0, 60, 0x32)   # group 1, note 1
    e += note_tup( 80, 62, 0x32)   # group 1, note 2
    e += note_tup(160, 64, 0x32)   # group 1, note 3
    e += note_v0c4(240, 0, 0, fv=3, pitch=65)   # regular Q
    e += note_tup(480, 67, 0x32)   # group 2, note 1
    e += note_tup(560, 69, 0x00)   # group 2, note 2, ORPHAN
    e += note_tup(640, 70, 0x32)   # group 2, note 3
    e += note_v0c4(720, 0, 0, fv=3, pitch=72)   # regular Q
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_gm_perc_range_taiko():
    """BUG FIX: instrument with MIDI program in the GM Percussive range (113-128)
    and a name that matches no standard template must be imported as drumset
    with a percussion clef (not the LINE-block clef C3L/C4L/F).
    Fixture: TK00 name = "A. Marazuela 335" (no template match), prg=116 (Taiko Drum).
    Without fix: falls back to Grand Piano, clef taken from LINE block.
    With fix: Step 1b detects prg>=113 → drumset; buildInitialSignatures forces PERC clef.
    """
    name_utf16 = 'A. Marazuela 335'.encode('utf-16-le') + b'\x00\x00'
    pre = _patch_tk00(name_utf16)
    pre = _patch_midi_program(pre, 0, 116)   # 116 = Taiko Drum (1-indexed GM)
    e   = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_gm_perc_chord_notes():
    """BUG FIX: chord notes on a percussion staff were dropped by the MIDI artifact
    filter even though they are genuine simultaneous chord tones.
    The filter condition 'rdur > CHORD_CLUSTER_THRESHOLD && not_in_tuplet' fired on:
      (a) the first note on a staff when its tick-diff rdur is short because the
          next chord note starts a few ticks later (savedPrevMidiTick < 0 → bypass);
      (b) chord extensions (isChordExt=true → bypass).

    Fixture: one percussion instrument (prg=116), measure with two H notes at
    tick=0 (pit=60) and tick=5 (pit=64).  calculateRealDurations gives note@0 a
    rdur of 5 ticks (5-0=5), which triggers the filter without the bypass.
    With fix: both notes survive → chord has pitches 60 AND 64.
    """
    name_utf16 = 'A. Marazuela 335'.encode('utf-16-le') + b'\x00\x00'
    pre = _patch_tk00(name_utf16)
    pre = _patch_midi_program(pre, 0, 116)
    # Two H notes at ticks 0 and 5 on the same staff/voice.
    # note@0 → rdur = 5-0 = 5 (from calculateRealDurations tick diff) → triggers
    # old artifact filter; bypassed by savedPrevMidiTick<0 in the new code.
    # note@5 → isChordExt=TRUE (delta=5 < CHORD_MIDI_THRESHOLD=8) → bypassed by !isChordExt.
    e  = note_v0c4(  0, 0, 0, fv=2, pitch=60)   # H note, pit=60
    e += note_v0c4(  5, 0, 0, fv=2, pitch=64)   # H note, pit=64 (chord ext)
    e += end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_2_2_beatticks480_correct_encoding():
    """Regression guard: 2/2 with the CORRECT beatTicks=480 (half-note beat).
    This encoding gives beatTicks*timeSigDen = 480*2 = 960 = wholeTicks, so
    the old formula was coincidentally correct.  Verify it still works after
    the fix (wholeTicks hardcoded to 960).

    Same note layout as gen_v0c4_2_2_beatticks240_gap_snap but with correct
    beatTicks=480.  All 7 elements must appear at the same tick positions.
    """
    e  = rest_v0c4(  0, 0, 0, fv=4)
    e += note_v0c4(120, 0, 0, fv=3, pitch=60)
    e += note_v0c4(360, 0, 0, fv=4, pitch=62)
    e += note_v0c4(480, 0, 0, fv=4, pitch=64)
    e += note_v0c4(600, 0, 0, fv=4, pitch=65)
    e += note_v0c4(720, 0, 0, fv=4, pitch=67)
    e += note_v0c4(840, 0, 0, fv=4, pitch=69)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 2, beatTicks=480, durTicks=960), e)],
                    fill_ts=(2, 2))


def gen_v0c4_2_2_beatticks240_gap_snap():
    """BUG FIX: For 2/2 with beatTicks=240 the gap-snap formula computed
    wholeTicks = beatTicks * timeSigDen = 240 * 2 = 480 instead of 960.
    A note at Encore tick=360 mapped to encTickFrac=360/480=3/4, which was
    greater than cumTick=3/8 after placing rest+Q, so gap-snap fired and
    advanced cumTick to 3/4. Subsequent notes were dropped (measure overflow).
    Fix: always use wholeTicks=960.

    Fixture: 2/2, beatTicks=240, durTicks=960.
      REST  tick=0   fv=8th  (cumTick: 0 -> 1/8)
      NOTE  tick=120 fv=Q    pitch=60  (cumTick: 1/8 -> 3/8)
      NOTE  tick=360 fv=8th  pitch=62  <- false gap-snap triggers here with old formula
      NOTE  tick=480 fv=8th  pitch=64
      NOTE  tick=600 fv=8th  pitch=65
      NOTE  tick=720 fv=8th  pitch=67
      NOTE  tick=840 fv=8th  pitch=69
    Total: 120+240+5*120 = 960 ticks. All 7 elements must be placed.
    """
    e  = rest_v0c4(  0, 0, 0, fv=4)              # 8th rest
    e += note_v0c4(120, 0, 0, fv=3, pitch=60)    # quarter
    e += note_v0c4(360, 0, 0, fv=4, pitch=62)    # 8th (false snap target)
    e += note_v0c4(480, 0, 0, fv=4, pitch=64)    # 8th
    e += note_v0c4(600, 0, 0, fv=4, pitch=65)    # 8th
    e += note_v0c4(720, 0, 0, fv=4, pitch=67)    # 8th
    e += note_v0c4(840, 0, 0, fv=4, pitch=69)    # 8th
    e += end_marker()
    # Use beatTicks=240 explicitly (non-standard for 2/2, the bug trigger).
    return assemble(0xC4, [(meas_hdr(2, 2, beatTicks=240, durTicks=960), e)],
                    fill_ts=(2, 2))


def gen_v0c4_triplet_orphan_missing_tup():
    """BUG FIX: Live-recorded v0xC4 files occasionally have the tup byte missing on
    one note in the middle of a triplet (sandwich pattern: tup=3:2, tup=0, tup=3:2).
    Without the fix, the group breaks at the orphan, the surrounding notes are treated
    as isolated explicit notes (plain), overflow occurs and the last note is dropped.

    Fixture: 4/4 (dur=960).
      tick=0   fv=8th tup=0x32  pitch=60  ← triplet note 1
      tick=80  fv=8th tup=0x00  pitch=62  ← ORPHAN: missing tup byte
      tick=160 fv=8th tup=0x32  pitch=64  ← triplet note 3
      tick=240 fv=Q   tup=0x00  pitch=65  ← regular quarter
      tick=480 fv=H   tup=0x00  pitch=67  ← regular half
    Total: triplet(240) + Q(240) + H(480) = 960 ticks.
    """
    def note_orphan(tick, pitch, tuplet):
        """Build a v0xC4 note with a specific tuplet byte (may be 0)."""
        d = bytearray(25)
        d[0] = 28; d[1] = 0           # size=28, staffIdx=0
        d[2] = 4                       # faceValue=8th
        d[10] = tuplet                 # tuplet byte at element offset +13 (d[10] in 25-byte payload)
        d[12] = pitch                  # semiTonePitch at element offset +15 (d[12])
        return struct.pack('<H', tick) + bytes([(9 << 4) | 0]) + bytes(d)

    # Note: in note_v0c4 the layout is:
    # d[0]=size=28, d[1]=staffIdx, d[2]=faceValue, d[10]=tuplet, d[12]=pitch
    # matching offsets: elem+3=size, elem+4=rawStaff, elem+5=fv, elem+13=tuplet, elem+15=pitch
    e  = note_orphan(  0, 60, 0x32)   # 8th, tup=3:2
    e += note_orphan( 80, 62, 0x00)   # 8th, tup=MISSING (orphan)
    e += note_orphan(160, 64, 0x32)   # 8th, tup=3:2
    e += note_v0c4(240, 0, 0, fv=3, pitch=65)  # quarter
    e += note_v0c4(480, 0, 0, fv=2, pitch=67)  # half
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_timesig_change_6_8_to_3_4():
    """BUG FIX: 6/8 and 3/4 have the same total duration (durTicks=720), so
    Fraction(6,8)==Fraction(3,4) via cross-multiplication.  buildInitialSignatures
    used operator== and silently swallowed the change.  Fix: identical() comparison.
    Layout: 2 measures 6/8, 3 measures 3/4, 2 measures 6/8 (all empty).
    """
    h68 = meas_hdr(6, 8, beatTicks=360, durTicks=720)
    h34 = meas_hdr(3, 4, beatTicks=240, durTicks=720)
    em  = b'\xff\xff'   # empty measure body
    measures = [
        (h68, em), (h68, em),           # M0-M1: 6/8
        (h34, em), (h34, em), (h34, em), # M2-M4: 3/4
        (h68, em), (h68, em),           # M5-M6: 6/8
    ]
    return assemble(0xC4, measures, fill_ts=(6, 8))


# ===========================================================================
# ornaments_new_artic_types.enc
# Six newly-decoded standalone ORN tipos, one per beat across two measures:
#   M1 beats 1-4: MARCATO(0xBF), MARCATO_BELOW(0xC6), MARCATO_STACCATO_BELOW(0xC0), TENUTO(0xC8)
#   M2 beats 1-2: THICK_STOPPED(0x30), DOUBLE_MORDENT(0xB8)
# ===========================================================================
def gen_v0c4_new_artic_types():
    e1  = orn16_v0c4(  0, 0, 0, tipo=0xBF)   # MARCATO ^
    e1 += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e1 += orn16_v0c4(240, 0, 0, tipo=0xC6)   # MARCATO_BELOW v
    e1 += note_v0c4( 240, 0, 0, fv=3, pitch=62)
    e1 += orn16_v0c4(480, 0, 0, tipo=0xC0)   # MARCATO_STACCATO_BELOW (v with dot)
    e1 += note_v0c4( 480, 0, 0, fv=3, pitch=64)
    e1 += orn16_v0c4(720, 0, 0, tipo=0xC8)   # TENUTO -
    e1 += note_v0c4( 720, 0, 0, fv=3, pitch=65)
    e1 += end_marker()
    e2  = orn16_v0c4(  0, 0, 0, tipo=0x30)   # THICK_STOPPED + bold
    e2 += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e2 += orn16_v0c4(240, 0, 0, tipo=0xB8)   # DOUBLE_MORDENT (3-wave mordent)
    e2 += note_v0c4( 240, 0, 0, fv=3, pitch=62)
    e2 += note_v0c4( 480, 0, 0, fv=2, pitch=60)  # half to fill measure
    e2 += end_marker()
    custom = [(meas_hdr(4, 4), e1), (meas_hdr(4, 4), e2)]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# ornaments_staccatissimo_orns.enc
# Four standalone staccatissimo ORN tipos, one per beat in a 4/4 measure.
#   Beat 1: 0x28 STACCATISSIMO  -> articStaccatissimoAbove (1 symid)
#   Beat 2: 0x29 TENUTO_STACCATISSIMO -> articTenutoAbove + articStaccatissimoAbove (2 symids)
#   Beat 3: 0x2A TENUTO_STACCATISSIMO_2 -> same 2 symids
#   Beat 4: 0x2B MARCATO_STACCATISSIMO -> articMarcatoBelow + articStaccatissimoAbove (2 symids)
# ===========================================================================
def gen_v0c4_staccatissimo_orns():
    e  = orn16_v0c4(  0, 0, 0, tipo=0x28)
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += orn16_v0c4(240, 0, 0, tipo=0x29)
    e += note_v0c4( 240, 0, 0, fv=3, pitch=62)
    e += orn16_v0c4(480, 0, 0, tipo=0x2A)
    e += note_v0c4( 480, 0, 0, fv=3, pitch=64)
    e += orn16_v0c4(720, 0, 0, tipo=0x2B)
    e += note_v0c4( 720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_tremolo_r8_r16_r64.enc  (name kept for backwards compat)
# Confirmed standalone tremolo ORN tipo: 0xEE = TREMOLO_16 (R16, 2 slashes).
# 0xE6 and 0xE9 are now known to be string-number ORNs (2 and 5), not tremolos.
# The fixture now places:
#   Beat 1: 0xE6 STRING_NUMBER_2  -> Fingering "2" (string number, not tremolo)
#   Beat 2: 0xEE TREMOLO_16       -> TremoloType::R16 (2 slashes)
#   Beat 3: 0xE9 STRING_NUMBER_5  -> Fingering "5" (string number, not tremolo)
#   Beat 4: rest
# ===========================================================================
def gen_v0c4_tremolo_r8_r16_r64():
    e  = orn16_v0c4(  0, 0, 0, tipo=0xE6)
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += orn16_v0c4(240, 0, 0, tipo=0xEE)
    e += note_v0c4( 240, 0, 0, fv=3, pitch=62)
    e += orn16_v0c4(480, 0, 0, tipo=0xE9)
    e += note_v0c4( 480, 0, 0, fv=3, pitch=64)
    e += rest_v0c4( 720, 0, 0, fv=3)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_graphic_line_skipped.enc
# ORN tipo 0x1C (GRAPHIC_LINE, Encore Graphics palette) is a 28-byte spanner-
# shaped element with no musical meaning. The importer must skip it silently
# without crashing and without adding any articulation to the chord.
# ===========================================================================
def gen_v0c4_string_num_orn_no_dup():
    # Regression: standalone string-number ORN (0xE6 = string 2) must not duplicate
    # the string number already placed by the per-note hasScaleStringAnchors path.
    #
    # n1 tick=0:   artUp=0x39 (string 1 direct) → sets mc.hasScaleStringAnchors=true
    # n2 tick=240: options bit 0 set, position=1 (D4) → options-bit-0 path creates "2"
    #              AND ORN 0xE6 at same tick tries to add "2" again → dedup must drop it.
    e  = note_v0c4_artic(  0, 0, 0, fv=3, pitch=60, articUp=0x39)  # string 1 anchor
    e += orn16_v0c4(240, 0, 0, tipo=0xE6)                           # string 2 ORN
    e += note_v0c4_opts(240, 0, 0, fv=3, pitch=62, options=0x01, position=1)
    e += rest_v0c4(480, 0, 0, fv=2)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_graphic_line_skipped():
    # 28-byte 0x1C element: tick(2)+typeVoice(1)+size(1)+staff(1)+tipo(1)+22 zeros
    d = bytearray(28)
    struct.pack_into('<H', d, 0, 0)        # tick=0
    d[2] = (5 << 4) | 0                   # type=5 (ORN), voice=0
    d[3] = 28                              # size=28
    d[4] = 0                               # staffIdx=0
    d[5] = 0x1C                            # GRAPHIC_LINE tipo
    graphic_line = bytes(d)
    e  = graphic_line
    e += note_v0c4(0, 0, 0, fv=2, pitch=60)   # half note on beat 1
    e += note_v0c4(480, 0, 0, fv=2, pitch=62) # half note on beat 3
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_cross_measure_slur_precision.enc
# Cross-measure slur (alMezuro=2) where slurXoffset2=15 matches the SECOND
# note of the target measure, NOT the last. Verifies that Fallback 1
# (xoffset2 direct comparison in resolvers-slur.cpp) selects the correct
# endpoint note and does not fall through to "last ChordRest".
#   M0: SLURSTART(alMezuro=2, xoffset=5, xoffset2=15) + 4 quarter notes
#   M1: 4 filler notes (no slur)
#   M2 (target): 4 notes with xoffsets 5/15/25/35, pitches C4/D4/E4/F4
#     slurXoffset2=15 → D4 (MIDI 62), the 2nd note.
# ===========================================================================
def gen_v0c4_cross_measure_slur_precision():
    e0  = ornament_v0c4(0, 0, 0, tipo=0x21, xoffset=5, alMezuro=2, xoffset2=15)
    e0 += note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e0 += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e0 += note_v0c4(480, 0, 0, fv=3, pitch=64)
    e0 += note_v0c4(720, 0, 0, fv=3, pitch=65)
    e0 += end_marker()
    e1  = note_v0c4(  0, 0, 0, fv=3, pitch=67)
    e1 += note_v0c4(240, 0, 0, fv=3, pitch=69)
    e1 += note_v0c4(480, 0, 0, fv=3, pitch=71)
    e1 += note_v0c4(720, 0, 0, fv=3, pitch=72)
    e1 += end_marker()
    # Target measure: distinct xoffsets so Fallback 1 can pick the right note.
    e2  = note_v0c4_xoff(  0, 0, 0, fv=3, pitch=60, xoff= 5)   # C4 xoff=5
    e2 += note_v0c4_xoff(240, 0, 0, fv=3, pitch=62, xoff=15)   # D4 xoff=15 <- target
    e2 += note_v0c4_xoff(480, 0, 0, fv=3, pitch=64, xoff=25)   # E4 xoff=25
    e2 += note_v0c4_xoff(720, 0, 0, fv=3, pitch=65, xoff=35)   # F4 xoff=35
    e2 += end_marker()
    custom = [(meas_hdr(4,4), e0), (meas_hdr(4,4), e1), (meas_hdr(4,4), e2)]
    return assemble(0xC4, custom, fill_ts=(4, 4))


def write(name,data,layout=True):
    # `layout=False` skips the Encore display-layout pass for fixtures whose
    # importer regression depends on notes carrying a zero xoffset (the pass
    # would change spanner-anchor snapping). Those fixtures test importer logic,
    # not Encore rendering, so leaving them un-laid-out is intentional.
    if layout:
        data = apply_encore_layout(data)
    path=os.path.join(OUT_DIR,name)
    with open(path,'wb') as f: f.write(data)
    print(f"  {name}  ({len(data):,} bytes)")


# ===========================================================================
# instruments_c2_no_tilde_compact_names_midi.enc
# v0xC2 file without a ~~~~ block: 3 instruments in the linear compact table
# at NAME_BASE(202) + n*112 (name) and 262 + n*112 (MIDI).
#   [0] no name, MIDI=49 -> Cello  (midiProgram-1=48, 0-indexed)
#   [1] "Guitarra", MIDI=25 -> Classical Guitar (24 0-indexed, name+MIDI match)
#   [2] no name, MIDI=57 -> Trumpet (56 0-indexed)
# Before the fix: recoverMissingNames used NAME_STEP=2158 (wrong, falls into
# LINE data) and COMPACT_NAME_BASE=314 (off by one entry), so instrument [0]
# got the name "Guitarra" and [1] got garbage; MIDI was similarly shifted:
# instrument [0] received entry[1]'s value instead of its own.
# ===========================================================================
# ===========================================================================
# notes_v0c2_full_measure_no_false_dot.enc
# v0xC2 4/4 fully-filled measure: 8th + 16th + 16th + 8th + 8th = 480t.
# The 8th at tick=0 is followed by a 16th at tick=120, which matches the
# fixDottedEighthPattern trigger (8th rdur=120 + 16th rdur=60 at tick+120).
# faceSum(480) + 60 = 540 != 480 = durTicks, so the faceSum guard blocks the
# fix.  Without the guard the first 8th becomes a dotted-8th (90t), causing
# measure overflow and reshaping of all subsequent notes.
# ===========================================================================
def gen_v0c2_full_measure_no_false_dot():
    e  = note_v0c2(0,   0, 0, fv=4, pitch=81)   # 8th
    e += note_v0c2(120, 0, 0, fv=5, pitch=78)   # 16th
    e += note_v0c2(180, 0, 0, fv=5, pitch=80)   # 16th
    e += note_v0c2(240, 0, 0, fv=4, pitch=83)   # 8th
    e += note_v0c2(360, 0, 0, fv=4, pitch=85)   # 8th
    e += end_marker()
    return assemble(0xC2, [(meas_hdr(4, 4), e)])


def note_v0c2_with_dotctrl(tick, voice, staffIdx, fv, pitch, dotCtrl=0):
    """22-byte v0xC2 note with an explicit dotControl byte."""
    d = bytearray(19)
    d[0] = 22; d[1] = staffIdx & 0x3F; d[2] = fv; d[10] = pitch; d[11] = dotCtrl
    return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)


# ===========================================================================
# notes_v0c2_plain_sixteenth_no_spurious_dot.enc
# v0xC2 4/4 measure: 2 × 16th (dotControl=0x39, bit 0 spuriously set in the
# binary) + 3 × 8th.  Before the fix, the bit-0 fallback in computeDotCount
# turned both 16ths into dotted-16ths (90t each), overflowing the measure by
# 60t and truncating the last 8th.  After the fix the guard blocks bit-0 when
# rdur == faceTicks, preserving all 5 notes as plain durations.
# Trigger condition: dotControl has bit 0 set in the binary (e.g. 0x39) on a
# note whose realDuration == faceValue2ticks(faceValue) (exact plain match).
# ===========================================================================
def gen_v0c2_plain_sixteenth_no_spurious_dot():
    # Two plain 16th notes with dotControl bit-0 set coincidentally (as seen in
    # tapada.enc m28 bandurria staff), followed by three plain 8th notes.
    # Total: 60+60+120+120+120 = 480 = 4/4 durTicks.
    e  = note_v0c2_with_dotctrl(0,   0, 0, fv=5, pitch=71, dotCtrl=0x39)
    e += note_v0c2_with_dotctrl(60,  0, 0, fv=5, pitch=73, dotCtrl=0x39)
    e += note_v0c2(120, 0, 0, fv=4, pitch=75)
    e += note_v0c2(240, 0, 0, fv=4, pitch=75)
    e += note_v0c2(360, 0, 0, fv=4, pitch=75)
    e += end_marker()
    return assemble(0xC2, [(meas_hdr(4, 4), e)])


# ===========================================================================
# structure_wini_screen_pixel_a4.enc
# bazo.enc with the WINI block replaced by screen-pixel A4 coordinates.
# Encore versions running on ~85 PPI monitors store WINI offsets in screen
# pixels rather than typographic points: top=28, left=28, bEdge=962, rEdge=672.
# These values exceed A4 in pts (595×842), so the old code produced wrong
# margins (R=0.03", B=0.10").  The fixed code detects the screen-pixel format,
# identifies A4 (210×297mm), and computes correct margins (~0.33" symmetric).
# ===========================================================================
def gen_wini_screen_pixel_a4():
    bazo_path = os.path.join(OUT_DIR, 'bazo.enc')
    data = bytearray(open(bazo_path, 'rb').read())
    # Find and patch the WINI block content at the margin offsets.
    # WINI content layout (after magic+varsize, 42 bytes):
    #   +0..+23  screen/window data (leave unchanged)
    #   +24..+27 top margin (int32 LE)
    #   +28..+31 left margin (int32 LE)
    #   +32..+35 bottomEdge (int32 LE)
    #   +36..+39 rightEdge  (int32 LE)
    wini_off = data.find(b'WINI')
    assert wini_off >= 0, 'WINI block not found in bazo.enc'
    content_off = wini_off + 8   # skip magic + varsize
    struct.pack_into('<i', data, content_off + 24, 28)   # top
    struct.pack_into('<i', data, content_off + 28, 28)   # left
    struct.pack_into('<i', data, content_off + 32, 962)  # bottomEdge  (A4 @ ~84.7 DPI)
    struct.pack_into('<i', data, content_off + 36, 672)  # rightEdge
    return bytes(data)


def gen_v0c2_no_tilde_compact_names_midi():
    hdr = bytearray(512)
    hdr[0:4] = b'SCOW'
    hdr[4] = 0xC2
    struct.pack_into('<H', hdr, 0x28, 0x0420)
    struct.pack_into('<H', hdr, 0x2C, 0xF000)
    struct.pack_into('<h', hdr, 0x2E, 1)
    struct.pack_into('<h', hdr, 0x30, 1)
    hdr[0x32] = 3
    hdr[0x33] = 3
    struct.pack_into('<h', hdr, 0x34, 1)
    # [0] no name at 202, MIDI=49 at 262
    hdr[262] = 49
    # [1] name "Guitarra" at 314, MIDI=25 at 374
    hdr[314:323] = b'Guitarra\x00'
    hdr[374] = 25
    # [2] no name at 426, MIDI=57 at 486
    hdr[486] = 57

    def staff_entry_c2(isidx):
        e = bytearray(30); e[19] = 1; e[21] = isidx; return bytes(e)

    line_data  = b'\x00' * 10 + struct.pack('<H', 0) + bytes([1])
    line_data += staff_entry_c2(0x00) + staff_entry_c2(0x01) + staff_entry_c2(0x02)
    line_block = b'LINE' + struct.pack('<I', len(line_data)) + line_data

    elems  = note_v0c2(0, 0, 0, fv=1, pitch=60)
    elems += note_v0c2(0, 0, 1, fv=1, pitch=64)
    elems += note_v0c2(0, 0, 2, fv=1, pitch=67)
    elems += end_marker()
    return bytes(hdr) + line_block + meas_block(meas_hdr(4, 4), elems) + SKELETON_POST


# ===========================================================================
# instruments_c2_tilde_primary_block_midi.enc
# v0xC2 file WITH a ~~~~ block: single instrument with printable ASCII at
# NAME_BASE(202) triggering hasPrimaryBlock(0)=true. MIDI is at 202+60=262
# (value=25 -> Classical Guitar, 0-indexed 24).
# Before the fix: hasPrimaryBlock instruments were skipped by the compact MIDI
# loop and kept midiProgram=0, falling back to Grand Piano.
# ===========================================================================
def gen_v0c2_tilde_primary_block_midi():
    hdr = bytearray(400)
    hdr[0:4] = b'SCOW'
    hdr[4] = 0xC2
    struct.pack_into('<H', hdr, 0x28, 0x0420)
    struct.pack_into('<H', hdr, 0x2C, 0xF000)
    struct.pack_into('<h', hdr, 0x2E, 1)
    struct.pack_into('<h', hdr, 0x30, 1)
    hdr[0x32] = 1
    hdr[0x33] = 1
    struct.pack_into('<h', hdr, 0x34, 1)
    # ~~~~ block at offset 100 with varSize=50
    struct.pack_into('BBBB', hdr, 100, 0x7e, 0x7e, 0x7e, 0x7e)
    struct.pack_into('<I', hdr, 104, 50)
    # Primary block marker at NAME_BASE=202: 'V' triggers hasPrimaryBlock(0)
    hdr[202] = 0x56
    # MIDI at NAME_BASE+60=262: 25 (Classical Guitar 1-indexed, 0-indexed=24)
    hdr[262] = 25

    def staff_entry_c2(isidx):
        e = bytearray(30); e[19] = 1; e[21] = isidx; return bytes(e)

    line_data  = b'\x00' * 10 + struct.pack('<H', 0) + bytes([1])
    line_data += staff_entry_c2(0x00)
    line_block = b'LINE' + struct.pack('<I', len(line_data)) + line_data

    elems  = note_v0c2(0, 0, 0, fv=1, pitch=60)
    elems += end_marker()
    return bytes(hdr) + line_block + meas_block(meas_hdr(4, 4), elems) + SKELETON_POST


# ===========================================================================
# importer_mrest_single_block.enc
# 7 MEAS blocks: notes, notes, notes, mrest=3, notes, notes, notes.
# 2 LINE blocks: system 1 covers MEAS[0..3], system 2 covers MEAS[4..6].
# Expected: 9 MuseScore measures (blocks 0-2 = notes, 3-5 = empty from mrest,
# 6-8 = notes). createMultiMeasureRests must be TRUE.
# ===========================================================================
def gen_v0c4_mrest_single_block():
    n = note_v0c4(0, 0, 0, 3, 60) + end_marker()          # C4 quarter
    mrest = rest_v0c4_mrest(0, 0, 0, 1, 3) + end_marker() # mrestCount=3
    # assemble() with 7 blocks; use set_chumagio + manual meas blocks to avoid
    # the minimum-6 pad and to get exactly 7 content blocks + the 2-LINE layout.
    pre = bytearray(set_chumagio(0xC4))
    struct.pack_into('<h', pre, 0x34, 7)   # measureCount=7
    pre[2406] = 4                           # LINE[0].measureCount=4
    struct.pack_into('<H', pre, 2468, 4)    # LINE[1].start=4
    pre = bytes(pre)
    body = (meas_block(meas_hdr(4, 4), n)
          + meas_block(meas_hdr(4, 4), n)
          + meas_block(meas_hdr(4, 4), n)
          + meas_block(meas_hdr(4, 4), mrest)
          + meas_block(meas_hdr(4, 4), n)
          + meas_block(meas_hdr(4, 4), n)
          + meas_block(meas_hdr(4, 4), n))
    return pre + body + SKELETON_POST


# ===========================================================================
# bazo_left_100.enc
# bazo.enc with WINI left margin = 7 (test name is legacy; actual encoded value
# is 7 pts). top=18 left=7 bEdge=824 rEdge=577.
# Expected: pageOddLeftMargin = 7/72, pagePrintableWidth = 570/72.
# ===========================================================================
def gen_bazo_left_100():
    bazo_path = os.path.join(OUT_DIR, 'bazo.enc')
    data = bytearray(open(bazo_path, 'rb').read())
    wini_off = data.find(b'WINI')
    assert wini_off >= 0, 'WINI block not found in bazo.enc'
    content_off = wini_off + 8
    # top stays 18 (unchanged); set left=7, keep bEdge=824 rEdge=577.
    struct.pack_into('<i', data, content_off + 28, 7)    # left = 7 pts
    return bytes(data)


# ===========================================================================
# bazo_top_100.enc
# bazo.enc with WINI top margin = 100 pts (unusual value to differ from default).
# Expected: pageOddTopMargin = 100/72 != defaultTop.
# ===========================================================================
def gen_bazo_top_100():
    bazo_path = os.path.join(OUT_DIR, 'bazo.enc')
    data = bytearray(open(bazo_path, 'rb').read())
    wini_off = data.find(b'WINI')
    assert wini_off >= 0, 'WINI block not found in bazo.enc'
    content_off = wini_off + 8
    struct.pack_into('<i', data, content_off + 24, 100)   # top = 100 pts
    return bytes(data)


# ===========================================================================
# importer_hairpin_xoffset2_snap.enc
# dim hairpin in m1 (alMezuro=1) whose xoffset2=70 lands between xoff=60 and
# xoff=90 of m2 notes. The snap must pick the note with largest xoff <= 70,
# which is xoff=60 at m2 enc-tick=240 = MuseScore Fraction(5,4).
# ===========================================================================
def gen_v0c4_hairpin_xoffset2_snap():
    # m1: 4 quarter notes with xoffs [30,60,90,120]; dim hairpin at tick=0,
    # alMezuro=1, xoffset2=70.
    e1  = ornament_v0c4(0, 0, 0, tipo=0x1D, xoffset=15, alMezuro=1,
                        xoffset2=70, speguleco=0x03)   # dim
    e1 += note_v0c4_xoff(  0, 0, 0, fv=3, pitch=60, xoff=30)
    e1 += note_v0c4_xoff(240, 0, 0, fv=3, pitch=60, xoff=60)
    e1 += note_v0c4_xoff(480, 0, 0, fv=3, pitch=60, xoff=90)
    e1 += note_v0c4_xoff(720, 0, 0, fv=3, pitch=60, xoff=120)
    e1 += end_marker()
    # m2: 4 quarter notes with xoffs [30,60,90,120]. xoffset2=70 → snap to xoff=60.
    e2  = note_v0c4_xoff(  0, 0, 0, fv=3, pitch=62, xoff=30)
    e2 += note_v0c4_xoff(240, 0, 0, fv=3, pitch=62, xoff=60)
    e2 += note_v0c4_xoff(480, 0, 0, fv=3, pitch=62, xoff=90)
    e2 += note_v0c4_xoff(720, 0, 0, fv=3, pitch=62, xoff=120)
    e2 += end_marker()
    custom = [(meas_hdr(4, 4), e1), (meas_hdr(4, 4), e2)]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# ornaments_breath_and_caesura.enc
# 4/4, one measure, 4 quarter notes.
# ORN tipo=0xA8 (BREATH_COMMA) placed before note 2 (at the note's tick so it
# snaps to the preceding note, note 1).
# ORN tipo=0xA7 (CAESURA) placed before note 4 (snaps to note 3).
# Expected: 2 Breath elements, one breathMarkComma and one caesura.
# ===========================================================================
def gen_v0c4_breath_and_caesura():
    # The ORN tick equals the following note's tick; the importer searches
    # backward for the preceding chord.
    def breath_orn(tick, tipo):
        d = bytearray(13)
        d[0] = 16
        d[1] = 0
        d[2] = tipo
        return struct.pack('<H', tick) + bytes([(5 << 4) | 0]) + bytes(d)

    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e += breath_orn(240, 0xA8)                 # BREATH_COMMA at note 2's tick
    e += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e += note_v0c4(480, 0, 0, fv=3, pitch=64)
    e += breath_orn(720, 0xA7)                 # CAESURA at note 4's tick
    e += note_v0c4(720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_new_artic_bytes.enc
# Three quarter notes, each carrying a new articulation byte:
#   note 0: articUp=0x1B → brassMuteClosed
#   note 1: articUp=0x2E → ornamentTurnInverted
#   note 2: articUp=0x30 → brassMuteHalfClosed
# ===========================================================================
def gen_v0c4_new_artic_bytes():
    e  = note_v0c4_artic(  0, 0, 0, fv=3, pitch=60, articUp=0x1B)
    e += note_v0c4_artic(240, 0, 0, fv=3, pitch=62, articUp=0x2E)
    e += note_v0c4_artic(480, 0, 0, fv=3, pitch=64, articUp=0x30)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_standalone_trill_end.enc
# 4/4, single quarter note at tick=0.
# TRILL_END ORN (tipo=0x35) at the same tick with no prior TRILL_START.
# Expected: one Trill spanner covering >= quarter note (>=240 Encore ticks).
# ===========================================================================
def gen_v0c4_standalone_trill_end():
    def trill_end_orn(tick, staffIdx=0, voice=0):
        d = bytearray(13)
        d[0] = 16
        d[1] = staffIdx & 0x3F
        d[2] = 0x35       # TRILL_END
        return struct.pack('<H', tick) + bytes([(5 << 4) | (voice & 0xF)]) + bytes(d)

    e  = trill_end_orn(0)
    e += note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_measure_repeat.enc
# 4/4, two quarter notes.
# REPEAT_MEASURE ORN (tipo=0xA3, size=16) at tick=0.
# Expected: one MeasureRepeat element.
# ===========================================================================
def gen_v0c4_measure_repeat():
    def repeat_meas_orn(tick, staffIdx=0, voice=0):
        d = bytearray(13)
        d[0] = 16
        d[1] = staffIdx & 0x3F
        d[2] = 0xA3       # REPEAT_MEASURE
        return struct.pack('<H', tick) + bytes([(5 << 4) | (voice & 0xF)]) + bytes(d)

    e  = repeat_meas_orn(0)
    e += note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_trill_with_accidentals.enc
# Four quarter notes, each with an articUp byte encoding a trill variant:
#   note 0: au=0x04 → ornamentTrill with no accidental (AUTO)
#   note 1: au=0x05 → trill + flat → MINOR
#   note 2: au=0x06 → trill + sharp → AUGMENTED
#   note 3: au=0x07 → trill + natural → MAJOR
# ===========================================================================
def gen_v0c4_trill_with_accidentals():
    e  = note_v0c4_artic(  0, 0, 0, fv=3, pitch=60, articUp=0x04)  # trill AUTO
    e += note_v0c4_artic(240, 0, 0, fv=3, pitch=62, articUp=0x05)  # trill + flat
    e += note_v0c4_artic(480, 0, 0, fv=3, pitch=64, articUp=0x06)  # trill + sharp
    e += note_v0c4_artic(720, 0, 0, fv=3, pitch=65, articUp=0x07)  # trill + natural
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_open_string_and_stick.enc
# Two quarter notes:
#   note 1: articUp=0x46 → open string → Fingering "0" (FINGERING style, no circle)
#   note 2: articUp=0x47 → stick (unmapped) → no fingering
# Expected: exactly 1 Fingering with text "0" and style FINGERING on note 1 only.
# ===========================================================================
def gen_v0c4_open_string_and_stick():
    e  = note_v0c4_artic(  0, 0, 0, fv=3, pitch=60, articUp=0x46)  # open string
    e += note_v0c4_artic(240, 0, 0, fv=3, pitch=62, articUp=0x47)  # stick (unmapped)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_trill_alt_standalone.enc
# TRILL_ALT ORN (tipo=0x37, size=16) on a quarter note with no prior TRILL_START.
# Expected: exactly 1 Trill spanner covering >= 240 Encore ticks.
# ===========================================================================
def gen_v0c4_trill_alt_standalone():
    def trill_alt_orn(tick, staffIdx=0, voice=0):
        d = bytearray(13)
        d[0] = 16
        d[1] = staffIdx & 0x3F
        d[2] = 0x37       # TRILL_ALT
        return struct.pack('<H', tick) + bytes([(5 << 4) | (voice & 0xF)]) + bytes(d)

    e  = trill_alt_orn(0)
    e += note_v0c4(0, 0, 0, fv=3, pitch=60)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_artic_dedup_trill_on_chord.enc
# Two-note chord at tick=0 where BOTH notes carry articUp=0x04 (ornamentTrill).
# Without dedup the chord would accumulate two identical Ornament(ornamentTrill).
# Expected: exactly 1 Ornament(ornamentTrill) on the chord.
# ===========================================================================
def gen_v0c4_artic_dedup_trill_on_chord():
    e  = note_v0c4_artic(  0, 0, 0, fv=3, pitch=60, articUp=0x04)  # C4, trill
    e += note_v0c4_artic(  0, 0, 0, fv=3, pitch=64, articUp=0x04)  # E4, trill (dup)
    e += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_accent_tick0_xoffset.enc
# 3/4 measure, 3 quarter notes at ticks 0/240/480 with xoffsets 8/8/8.
# Two ACCENT ORNs (0xBE) at enc-ticks 0 and 480 (ornXoffset=11 each).
# Without fix: Phase 1 of correctBowingTickFromXoffset clusters the tick-0 ORN
# with the tick-480 ORN (|11-11|=0 <= BOW_XOFF_CLUSTER) and moves it to 480 →
# both accents land on note 3. With fix: pre-check finds note at tick=0 with
# noteXoffset=8 near ornXoffset=11 (|11-8|=3 ≤ 6) → ORN stays on note 1.
# Expected: note1 has 1 accent, note3 has 1 accent, no chord has 2 accents.
# ===========================================================================
def gen_v0c4_accent_tick0_xoffset():
    def orn_be_xoff(tick, xoff, staffIdx=0, voice=0):
        """16-byte ACCENT ORN (tipo=0xBE) with xoffset."""
        d = bytearray(13)
        d[0] = 16
        d[1] = staffIdx & 0x3F
        d[2] = 0xBE    # ACCENT
        d[7] = xoff & 0xFF
        return struct.pack('<H', tick) + bytes([(5 << 4) | (voice & 0xF)]) + bytes(d)

    e  = orn_be_xoff(  0, 11)
    e += note_v0c4_xoff(  0, 0, 0, fv=3, pitch=60, xoff=8)
    e += note_v0c4_xoff(240, 0, 0, fv=3, pitch=62, xoff=8)
    e += orn_be_xoff(480, 11)
    e += note_v0c4_xoff(480, 0, 0, fv=3, pitch=64, xoff=8)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(3, 4), e)], fill_ts=(3, 4))


# ===========================================================================
# notes_chord_strum_xoffset.enc
# 3/4 measure with eight four-note chords (B3 D4 G4 B4 = 59/62/67/71). Each chord
# is recorded with staggered playback ticks (a per-chord "strum" / live-recording
# drift) but all four members share one notated column stored in the note xoffset:
#   chord 1 (eighth): ticks 5/7/15/20   xoffset 14
#   chord 2 (16th):   ticks 122/127/132/137 xoffset 38
#   chord 3 (16th):   ticks 182/187/192/197 xoffset 57
#   chord 4 (eighth): ticks 242/247/252/257 xoffset 75
#   chord 5 (16th):   ticks 362/367/372/377 xoffset 100
#   chord 6 (16th):   ticks 422/427/432/437 xoffset 118
#   chord 7 (eighth): ticks 482/485/485/487 xoffset 137
#   chord 8 (eighth): ticks 602/605/605/607 xoffset 162
# Chord 1 has an 8-tick step between its second and third members (7 -> 15): a
# tick-gap chord-grouping threshold splits it into separate short notes, and the
# split notes then trip the incomplete/overfull rhythm checks. With the xoffset
# column fix the four members collapse onto the anchor tick and form one chord.
# Expected: exactly 8 chords, each pitches 59/62/67/71, durations
# eighth,16th,16th,eighth,16th,16th,eighth,eighth (fills the 3/4 bar).
# ===========================================================================
def gen_v0c4_chord_strum_xoffset():
    chords = [
        (4, 14,  [(5, 59), (7, 62), (15, 67), (20, 71)]),
        (5, 38,  [(122, 59), (127, 62), (132, 67), (137, 71)]),
        (5, 57,  [(182, 59), (187, 62), (192, 67), (197, 71)]),
        (4, 75,  [(242, 59), (247, 62), (252, 67), (257, 71)]),
        (5, 100, [(362, 59), (367, 62), (372, 67), (377, 71)]),
        (5, 118, [(422, 59), (427, 62), (432, 67), (437, 71)]),
        (4, 137, [(482, 59), (485, 67), (485, 62), (487, 71)]),
        (4, 162, [(602, 59), (605, 67), (605, 62), (607, 71)]),
    ]
    e = b''
    for fv, xoff, notes in chords:
        for tick, pitch in notes:
            e += note_v0c4_xoff(tick, 0, 0, fv=fv, pitch=pitch, xoff=xoff)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(3, 4), e)], fill_ts=(3, 4))


# ===========================================================================
# notes_diff_column_no_merge.enc
# 3/4 measure. Two eighth notes are recorded only 5 ticks apart (0 and 5) but in
# clearly different notated columns (xoffset 10 vs 40), then a quarter at tick 240.
# The 5-tick gap is below the chord-extension window, so a tick-only rule merges
# the two eighths into one chord. Because their columns differ by more than the
# minimum column separation, they must stay two separate eighth notes instead.
# This is the inverse of notes_chord_strum_xoffset (same column -> one chord).
# Expected: note 1 = eighth [60], note 2 = eighth [64], note 3 = quarter [67].
# ===========================================================================
def gen_v0c4_diff_column_no_merge():
    e  = note_v0c4_xoff(0,   0, 0, fv=4, pitch=60, xoff=10)
    e += note_v0c4_xoff(5,   0, 0, fv=4, pitch=64, xoff=40)
    e += note_v0c4_xoff(240, 0, 0, fv=3, pitch=67, xoff=70)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(3, 4), e)], fill_ts=(3, 4))


# ===========================================================================
# notes_tuplet_diff_column_keeps_members.enc
# Faithful reproduction of the staff-6 corruption seen in a real live-recorded
# score: an eighth-note 3:2 triplet (three positions in three columns) whose
# second and third members were played only 5 ticks apart (ticks 80 and 85) but
# sit in different notated columns (xoffset 40 vs 70). A tick-only chord rule
# merges those two into one chord, leaving the triplet with 2 of 3 members; the
# bar then no longer sums to a clean value and imports incomplete. Keeping the
# columns distinct preserves all three members and the bar stays complete.
# Layout: 3/4 bar = eighth triplet (a quarter) + a half note filling the rest.
# Expected: a 3:2 tuplet with exactly 3 notes (60, 64, 67); sanityCheck clean.
# ===========================================================================
def gen_v0c4_tuplet_diff_column():
    def note_tup_xoff(tick, pitch, xoff, tup=0x32, fv=4):
        d = bytearray(25)
        d[0] = 28
        d[2] = fv
        d[7] = xoff & 0xFF   # xoffset at element +10
        d[10] = tup          # tuplet ratio at element +13
        d[12] = pitch
        return struct.pack('<H', tick) + bytes([(9 << 4) | 0]) + bytes(d)

    e  = note_tup_xoff(0,  60, 10)   # triplet position 1
    e += note_tup_xoff(80, 64, 40)   # position 2
    e += note_tup_xoff(85, 67, 70)   # position 3, only 5 ticks after position 2, different column
    e += note_v0c4_xoff(240, 0, 0, fv=2, pitch=72, xoff=100)  # half note fills the rest of the 3/4 bar
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(3, 4), e)], fill_ts=(3, 4))


# ===========================================================================
# ornaments_bowing_tick0_xoffset_mismatch.enc
# 4/4 measure, 4 quarter notes at ticks 0/240/480/720 with note xoffsets
# 9/33/57/81 (increasing across the measure). Two bowing ORNs:
#   UPBOW   (0xC4) at enc-tick=0   with ornXoffset=69
#   DOWNBOW (0xC5) at enc-tick=480 with ornXoffset=120
# This mirrors real Encore files where the ORN xoffset and the note xoffset use
# different horizontal origins (here the up-bow's xoffset 69 is far from note 1's
# xoffset 9, even though the up-bow belongs to note 1 by its enc tick=0).
#
# Without fix: the tick-0 up-bow has a non-zero xoffset and the |69-9|=60 > 6
# pre-check fails, so correctBowingTickFromXoffset runs Phase 2 and snaps the
# up-bow to the closest note xoffset <= 69 (note 3, xoffset 57, enc-tick=480),
# moving the up-bow off note 1.
# With fix: a note exists on the ORN's own staff at its raw enc-tick=0, so the
# raw tick is trusted and the up-bow stays on note 1.
# Expected: note 1 (enc-tick=0) carries the up-bow; note 3 (enc-tick=480)
# carries the down-bow; note 1 keeps its up-bow.
# ===========================================================================
def gen_v0c4_bowing_tick0_xoffset_mismatch():
    def orn_bow_xoff(tick, tipo, xoff, staffIdx=0, voice=0):
        """16-byte bowing ORN (tipo 0xC4 up-bow / 0xC5 down-bow) with xoffset."""
        d = bytearray(13)
        d[0] = 16
        d[1] = staffIdx & 0x3F
        d[2] = tipo
        d[7] = xoff & 0xFF
        return struct.pack('<H', tick) + bytes([(5 << 4) | (voice & 0xF)]) + bytes(d)

    e  = orn_bow_xoff(  0, 0xC4, 69)   # up-bow on note 1 (raw tick=0)
    e += note_v0c4_xoff(  0, 0, 0, fv=3, pitch=60, xoff=9)
    e += note_v0c4_xoff(240, 0, 0, fv=3, pitch=62, xoff=33)
    e += orn_bow_xoff(480, 0xC5, 120)  # down-bow on note 3 (raw tick=480)
    e += note_v0c4_xoff(480, 0, 0, fv=3, pitch=64, xoff=57)
    e += note_v0c4_xoff(720, 0, 0, fv=3, pitch=65, xoff=81)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# ornaments_fingering_grandstaff.enc
# 2-staff grand-staff file (piano-style, 2 staves under one instrument) with
# fingering ORN routing scenarios.
#
# WINI: top=0 left=0 bEdge=842 rEdge=595 (zero margins, triggers clamping test).
#
# Pattern A (m2->m3): 4 FINGER ORNs (tipos 0xB9..0xBC = "1134") placed at
#   the end of m2 (last voice=0 tick) with staffIdx pointing to staff 1
#   (instr_staff_idx=0x01). The resolver must route them to m3 staff 2
#   (not attach them to the last chord of m2 staff 1).
#   m3 staff 1 gets its own fingerings [1,2,4].
#
# Pattern B (m11): 4 FINGER ORNs (tipos 0xB9,0xBA,0xBC,0xBC = "1244") at
#   enc-tick=0 of m11 with staffIdx=0x01. Only 1 note on staff 1 (voice=0)
#   at tick=0, so there are more ORNs than staff-1 notes. The overflow
#   ORNs must land on staff 2, not on staff 1.
# ===========================================================================
# ornaments_fingering_multivoice.enc
# 2-staff file exercising FINGER ORN routing on a MULTI-VOICE top staff (all fingerings are stored on
# voice 0, as Encore does):
#   m1 tick0:  voice0 REST + voice2 C4, one FINGER_1 ORN on voice0. The bass staff (voice4) has a note
#              at tick0 too. The finger must attach to the SAME-staff voice-2 C4, NOT to the bass.
#   m1 tick240 (last voice-0 tick, no bass note there): a voice0 3-note chord + three FINGER ORNs
#              (5,3,2). Fingering count == chord note count, so they must stay on THIS chord and not
#              float cross-measure to m2.
def gen_v0c4_fingering_multivoice():
    hdr = bytearray(194)
    hdr[0:4] = b'SCOW'
    hdr[4] = 0xC4
    struct.pack_into('<H', hdr, 0x28, 0x0420)
    struct.pack_into('<h', hdr, 0x2E, 1)
    struct.pack_into('<h', hdr, 0x30, 1)
    hdr[0x32] = 1
    hdr[0x33] = 2
    struct.pack_into('<h', hdr, 0x34, 3)   # measureCount=3 (m0..m2)

    def staff_entry(clef, isidx):
        e = bytearray(30)
        e[14] = clef
        e[19] = 1
        e[21] = isidx
        return bytes(e)
    line_data = (b'\x00' * 10 + struct.pack('<H', 0) + bytes([3])
                 + staff_entry(0, 0x00) + staff_entry(1, 0x40))
    line_block = b'LINE' + struct.pack('<I', len(line_data)) + line_data

    def finger(tick, tipo):
        return ornament_v0c4(tick, 0, 0, tipo=tipo, xoffset=0)

    # m0: plain 4/4 filler both staves (avoids pickup shrink)
    m0 = (note_v0c4(0, 0, 0, 3, 60) + note_v0c4(0, 4, 0, 3, 48)
          + note_v0c4(240, 0, 0, 3, 62) + note_v0c4(240, 4, 0, 3, 50)
          + note_v0c4(480, 0, 0, 3, 64) + note_v0c4(480, 4, 0, 3, 52)
          + note_v0c4(720, 0, 0, 3, 65) + note_v0c4(720, 4, 0, 3, 53)
          + end_marker())
    # m1: multi-voice top staff
    m1 = (rest_v0c4(0, 0, 0, 3)                 # voice0 rest at beat 1
          + note_v0c4(0, 2, 0, 3, 60)           # voice2 C4 at beat 1 (target of FINGER_1)
          + note_v0c4(0, 4, 0, 3, 36)           # bass note at beat 1 (decoy for the old sibling route)
          + finger(0, 0xB9)                     # FINGER_1 -> must land on voice2 C4, not the bass
          + note_v0c4(240, 0, 0, 3, 60)         # voice0 chord (3 notes) at beat 2
          + note_v0c4(240, 0, 0, 3, 64)
          + note_v0c4(240, 0, 0, 3, 67)
          + finger(240, 0xBD) + finger(240, 0xBB) + finger(240, 0xBA)  # 5,3,2 -> stay on this chord
          + rest_v0c4(480, 0, 0, 2)             # fill beats 3-4 (voice0)
          + end_marker())
    m2 = (note_v0c4(0, 0, 0, 3, 72) + note_v0c4(0, 4, 0, 3, 48) + end_marker())

    body = (meas_block(meas_hdr(4, 4), m0)
            + meas_block(meas_hdr(4, 4), m1)
            + meas_block(meas_hdr(4, 4), m2))
    return bytes(hdr) + line_block + body


def gen_v0c4_fingering_grandstaff():
    # 2-staff 194-byte header
    hdr = bytearray(194)
    hdr[0:4] = b'SCOW'
    hdr[4] = 0xC4
    struct.pack_into('<H', hdr, 0x28, 0x0420)
    struct.pack_into('<h', hdr, 0x2E, 1)   # lineCount=1
    struct.pack_into('<h', hdr, 0x30, 1)   # pageCount=1
    hdr[0x32] = 1                           # instrumentCount=1
    hdr[0x33] = 2                           # staffPerSystem=2
    struct.pack_into('<h', hdr, 0x34, 11)  # measureCount=11 (m0..m10)

    # LINE block covering all 12 measures, 2 staff entries
    def staff_entry(clef, isidx):
        e = bytearray(30)
        e[14] = clef
        e[19] = 1
        e[21] = isidx
        return bytes(e)

    line_data = (b'\x00' * 10
                 + struct.pack('<H', 0)    # start measure = 0
                 + bytes([11])             # measCount = 11
                 + staff_entry(0, 0x00)   # staff 0: treble
                 + staff_entry(1, 0x40))  # staff 1: bass (instrIdx=0, staffWithin=1)
    line_block = b'LINE' + struct.pack('<I', len(line_data)) + line_data

    def finger_orn(tick, tipo, voice=0, staffIdx=0):
        """16-byte FINGER ORN."""
        d = bytearray(13)
        d[0] = 16
        d[1] = staffIdx & 0x3F
        d[2] = tipo
        return struct.pack('<H', tick) + bytes([(5 << 4) | (voice & 0xF)]) + bytes(d)

    def n(tick, st, pitch=60, fv=3):
        v = 4 if st else 0          # bass uses voice=4 (case A routes to staff 1)
        return note_v0c4(tick, v, 0, fv, pitch)

    # Build 12 measures:
    # m0: full 4/4 filler on both staves (4 quarter notes each) so adjustPickupMeasure
    # does not fire; score->tick2measure() relies on the static m_tickIndex which is
    # not updated when setTick() shifts subsequent measures after a pickup shrink.
    m0 = (n(  0, 0, 60) + n(  0, 1, 48)
        + n(240, 0, 62) + n(240, 1, 50)
        + n(480, 0, 64) + n(480, 1, 52)
        + n(720, 0, 65) + n(720, 1, 53)
        + end_marker())
    # m1: plain notes (Pattern A happens at m1's last tick, routes to m2)
    # Include FINGER ORNs at tick=720 (last note tick of a 4/4 measure) on staffIdx=1.
    m1 = (n(  0, 0, 60) + n(  0, 1, 48)
        + n(240, 0, 62) + n(240, 1, 50)
        + n(480, 0, 64) + n(480, 1, 52)
        + n(720, 0, 65)
        + finger_orn(720, 0xB9)   # "1" crossMeasure → m3 staff 2
        + finger_orn(720, 0xB9)   # "1"
        + finger_orn(720, 0xBB)   # "3"
        + finger_orn(720, 0xBC)   # "4"
        + end_marker())
    # m2: receives Pattern A fingerings on staff 2; staff 1 gets its own [1,2,4]
    m2 = (n(  0, 0, 60, fv=3) + n(  0, 1, 48, fv=3)
        + finger_orn(  0, 0xB9, staffIdx=0)   # "1" on staff 1
        + finger_orn(240, 0xBA, staffIdx=0)   # "2" on staff 1
        + n(240, 0, 62, fv=3) + n(240, 1, 50, fv=3)
        + finger_orn(480, 0xBC, staffIdx=0)   # "4" on staff 1
        + n(480, 0, 64, fv=3) + n(480, 1, 52, fv=3)
        + n(720, 0, 65, fv=3) + n(720, 1, 53, fv=3)
        + end_marker())
    # m3..m9: plain filler measures on both staves
    mfill = n(0, 0, 60) + n(0, 1, 48) + end_marker()
    # m10: Plain measure (index 10, Pattern B happens here at tick=0)
    m10 = (n(0, 0, 60)       # one note on staff 1 (voice=0)
         + n(0, 1, 48)        # one note on staff 2 (voice=4, case A → staff 2)
         + finger_orn(0, 0xB9)   # "1" preferSibling → staff 2
         + finger_orn(0, 0xBA)   # "2" preferSibling → staff 2
         + finger_orn(0, 0xBC)   # "4" preferSibling → staff 2
         + finger_orn(0, 0xBC)   # "4" preferSibling → staff 2
         + end_marker())

    body = bytes()
    body += meas_block(meas_hdr(4, 4), m0)
    body += meas_block(meas_hdr(4, 4), m1)
    body += meas_block(meas_hdr(4, 4), m2)
    for _ in range(7):
        body += meas_block(meas_hdr(4, 4), mfill)
    body += meas_block(meas_hdr(4, 4), m10)

    # Build WINI block: top=0 left=0 bEdge=842 rEdge=595 (zero margins).
    # Layout: [0..23]=screen data, [24..27]=top, [28..31]=left,
    #         [32..35]=bEdge, [36..39]=rEdge, [40..41]=extra
    wini_content = bytearray(42)
    wini_content[0] = 2      # version
    struct.pack_into('<i', wini_content, 24, 0)    # top = 0
    struct.pack_into('<i', wini_content, 28, 0)    # left = 0
    struct.pack_into('<i', wini_content, 32, 842)  # bEdge (A4 height in pts)
    struct.pack_into('<i', wini_content, 36, 595)  # rEdge (A4 width in pts)
    wini_block = b'WINI' + struct.pack('<I', len(wini_content)) + bytes(wini_content)

    return bytes(hdr) + line_block + body + wini_block




def gen_v0c4_chord_symbol_large_drift():
    """CHD at tick=87 (large MIDI drift from note at tick=0) must snap to beat-1 segment."""
    # note at tick=0 (quarter), chord symbol at tick=87 (large drift), another note at tick=240
    e  = note_v0c4(0,   0, 0, fv=3, pitch=60)
    # chord symbol: type=7, radiko=0 (C root), toniko=0 (major), text "C"
    raw_text = b'C\x00' + b'\x00' * 34
    e += chordsym_v0c4(87, 0, 0, raw_text)
    e += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e += note_v0c4(480, 0, 0, fv=3, pitch=64)
    e += note_v0c4(720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)])

def gen_v0c4_chord_symbol_nearbeat_subdivision():
    """CHD at tick=62 with note at tick=60 must snap to beat-1 (tick=0) via beat-floor."""
    # 16th note at tick=0 (fv=5), another 16th at tick=60, chord symbol at tick=62 (just past tick=60)
    # beat floor: floor(62/240)*240 = 0, so CHD snaps to tick=0 (beat 1), not tick=60
    e  = note_v0c4(0,   0, 0, fv=3, pitch=60)   # quarter at tick=0 (beat 1)
    e += note_v0c4(60,  0, 0, fv=5, pitch=62)   # 16th at tick=60 (subdivision trap)
    raw_text = b'C\x00' + b'\x00' * 34
    e += chordsym_v0c4(62, 0, 0, raw_text)       # CHD at tick=62: beat-floor=0, NOT 60
    e += note_v0c4(240, 0, 0, fv=3, pitch=64)
    e += note_v0c4(480, 0, 0, fv=3, pitch=65)
    e += note_v0c4(720, 0, 0, fv=3, pitch=67)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)])

def gen_v0c4_chord_symbol_snap_to_beat1():
    """CHD at tick=6 (small MIDI drift) must snap to beat-1 segment, not beat-2."""
    e  = note_v0c4(0,   0, 0, fv=3, pitch=60)
    raw_text = b'C\x00' + b'\x00' * 34
    e += chordsym_v0c4(6, 0, 0, raw_text)        # tick=6: small drift from note at tick=0
    e += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e += note_v0c4(480, 0, 0, fv=3, pitch=64)
    e += note_v0c4(720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)])

def gen_v0c4_chord_symbol_fretboard():
    """Three chord symbols, all named "Am" or "Zzz", differing only in the tipo fret-frame
    bit (0x04). A guitar frame is drawn only when that bit is set, regardless of whether the
    built-in database knows the chord. Measure 0: "Am" with the frame bit -> FretDiagram
    wrapping the Harmony. Measure 1: "Am" WITHOUT the frame bit -> plain Harmony (Encore shows
    only the chord text). Measure 2: "Zzz" with the frame bit but unknown to the database ->
    plain Harmony, since no diagram can be drawn."""
    def mk(text_bytes, fretboard):
        e  = note_v0c4(0, 0, 0, fv=3, pitch=60)
        e += chordsym_v0c4(0, 0, 0, bytes(text_bytes), fretboard=fretboard)
        e += note_v0c4(240, 0, 0, fv=3, pitch=62)
        e += note_v0c4(480, 0, 0, fv=3, pitch=64)
        e += note_v0c4(720, 0, 0, fv=3, pitch=65)
        e += end_marker()
        return e
    am = bytearray(36)
    am[0:2] = b'Am'
    zz = bytearray(36)
    zz[0:3] = b'Zzz'
    e0 = mk(am, True)    # frame bit set, known chord -> FretDiagram
    e1 = mk(am, False)   # no frame bit, known chord -> plain Harmony
    e2 = mk(zz, True)    # frame bit set, unknown chord -> plain Harmony
    return assemble(0xC4, [(meas_hdr(4, 4), e0), (meas_hdr(4, 4), e1), (meas_hdr(4, 4), e2)])

def gen_v0c4_chord_inflated_rdur_keeps_eighth():
    """fv=4 (eighth) chord at tick=240 with rdur=240 (inflated gap) must stay V_EIGHTH."""
    # F4(quarter) at tick=0, G4+A4(eighth) at tick=240, B4(quarter) at tick=480, C5(quarter) at tick=720
    # G4 and A4 are both at tick=240 fv=4; B4 is at tick=480 (not shifted), so rdur[G4]=240
    # The inflatedDottedPromotion guard must prevent rdur=240 from promoting fv=4 to quarter
    e  = note_v0c4(0,   0, 0, fv=3, pitch=65)   # F4 quarter
    e += note_v0c4(240, 0, 0, fv=4, pitch=67)   # G4 eighth - same tick as A4 -> chord
    e += note_v0c4(240, 0, 0, fv=4, pitch=69)   # A4 eighth - merges into chord with G4
    e += note_v0c4(480, 0, 0, fv=3, pitch=71)   # B4 quarter (NOT at 360, stays at 480)
    e += note_v0c4(720, 0, 0, fv=3, pitch=72)   # C5 quarter
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)])

def gen_v0c4_scale_no_anchor_no_circles():
    """Notes with options bit 0 and position 0-7 but no 0x39..0x40 anchor must not show circles."""
    # 4 notes, all with options=0x01 (bit 0 set) and position in 0..3
    # No note has articulationUp in 0x39..0x40, so hasScaleStringAnchors stays false
    # => no STRING_NUMBER fingerings should be emitted
    e  = note_v0c4_opts(0,   0, 0, fv=3, pitch=60, options=0x01, position=0)
    e += note_v0c4_opts(240, 0, 0, fv=3, pitch=62, options=0x01, position=1)
    e += note_v0c4_opts(480, 0, 0, fv=3, pitch=64, options=0x01, position=2)
    e += note_v0c4_opts(720, 0, 0, fv=3, pitch=65, options=0x01, position=3)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)])

def gen_v0c4_scale_string_numbers_anchor():
    """au=0x39 on note 1 sets anchor; all 4 notes get string numbers 1-4 via options+position."""
    # note 1: au=0x39 (string 1, sets mc.hasScaleStringAnchors=True), options=0x01, position=0
    # note 2-4: au=0x00, options=0x01, position=1/2/3 -> get string 2/3/4 via fallback path
    # articulationUp=0x39 triggers encArticByteToScaleStringNumber -> string 1 directly
    # For notes 2-4: hasScaleStringAnchors=True + options bit 0 + position -> position+1
    e  = note_v0c4_artic(0,   0, 0, fv=3, pitch=60, articUp=0x39)  # anchor: string 1
    # notes 2-4 use options+position path; they need options=0x01 and position set
    e += note_v0c4_opts(240, 0, 0, fv=3, pitch=62, options=0x01, position=1)  # -> string 2
    e += note_v0c4_opts(480, 0, 0, fv=3, pitch=64, options=0x01, position=2)  # -> string 3
    e += note_v0c4_opts(720, 0, 0, fv=3, pitch=65, options=0x01, position=3)  # -> string 4
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)])

def gen_v0c4_no_spurious_string_numbers():
    """Plain notes with options bit 0 + position but no guitar instrument must not get circles."""
    # Simulates piano/vocal notes that happen to have options=0x01 and position=1..3
    # but no 0x39..0x40 anchor => hasScaleStringAnchors stays false => no circles
    e  = note_v0c4_opts(0,   0, 0, fv=3, pitch=60, options=0x01, position=0)
    e += note_v0c4_opts(240, 0, 0, fv=3, pitch=64, options=0x01, position=1)
    e += note_v0c4_opts(480, 0, 0, fv=3, pitch=67, options=0x01, position=2)
    e += note_v0c4_opts(720, 0, 0, fv=3, pitch=65, options=0x01, position=0)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)])

def gen_v0c4_rest_dotted_before_notes():
    """7/8 measure: dotted-quarter rest (fv=3, dotControl set) must import as V_QUARTER+1dot."""
    # 7/8: beatTicks=120, durTicks=120*7=840
    # Dotted-quarter rest = 360 ticks (3/8). fv=3 (quarter base), dotControl set.
    # Then two quarter notes fill the remaining 4/8.
    # dotControl=1 (bit 0 = dotted flag)
    e  = rest_v0c4_dotted(0,   0, 0, fv=3, dotControl=1)   # dotted quarter rest, 3/8
    e += note_v0c4(360, 0, 0, fv=3, pitch=60)               # quarter at tick=360 (3/8)
    e += note_v0c4(600, 0, 0, fv=4, pitch=62)               # eighth at tick=600 (5/8)
    e += note_v0c4(720, 0, 0, fv=4, pitch=64)               # eighth at tick=720 (6/8)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(7, 8, beatTicks=120), e)], fill_ts=(7, 8))

def gen_v0c4_accent_tick0_xoffset_ornaments():
    """3/4 measure with two ACCENT ORNs at same xoffset; tick-0 accent must stay on note 1."""
    # 3 quarter notes at tick=0/240/480 (xoff=8 each).
    # Two ACCENT ORNs: one at tick=0, one at tick=480. Both have ornXoffset=11.
    # Bug: Phase 1 xoffset-cluster match moved tick-0 ORN to tick=480 -> both on note 3.
    # Fix: pre-check finds note at enc-tick=0 with xoffset=8, |11-8|=3<=6 -> return early.
    e  = note_v0c4_xoff(0,   0, 0, fv=3, pitch=60, xoff=8)   # C4, xoffset=8
    e += note_v0c4_xoff(240, 0, 0, fv=3, pitch=62, xoff=8)   # D4, xoffset=8
    e += note_v0c4_xoff(480, 0, 0, fv=3, pitch=64, xoff=8)   # E4, xoffset=8
    # ORN ACCENT (0xBE) at tick=0, ornXoffset=11
    e += ornament_v0c4(0,   0, 0, tipo=0xBE, xoffset=11)
    # ORN ACCENT at tick=480, ornXoffset=11 (same value)
    e += ornament_v0c4(480, 0, 0, tipo=0xBE, xoffset=11)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(3, 4), e)], fill_ts=(3, 4))

def gen_v0c2_grace_slur_to_main_coloc():
    """v0xC2 3/4: ACCIACCATURA+main co-located at tick=480 with bait at tick=600; grace-to-main slur via co-location detection."""
    def _acc_v0c2(tick, staffIdx, fv, pitch, xoff=0):
        d = bytearray(19)
        d[0]=22; d[1]=staffIdx&0x3F; d[2]=fv
        d[3]=0x20; d[4]=0x04   # ACCIACCATURA: grace1&0x30==0x20, grace2&0x05==0x04
        d[7]=xoff&0xFF; d[10]=pitch
        return struct.pack('<H',tick)+bytes([(9<<4)])+bytes(d)
    e  = note_v0c2(0,0,0,fv=2,pitch=60)                                            # half note fills 0..480
    e += ornament_v0c4(480,0,0,tipo=0x21,xoffset=4,alMezuro=0,xoffset2=3)          # SLURSTART
    e += _acc_v0c2(480,0,fv=4,pitch=64,xoff=6)                                     # ACCIACCATURA xoff=6
    e += note_v0c2_xoff(480,0,0,fv=4,pitch=64,xoffset=5)                           # main note xoff=5
    e += note_v0c2_xoff(600,0,0,fv=4,pitch=67,xoffset=4)                           # bait xoff=4
    e += end_marker()
    return assemble(0xC2,[(meas_hdr(3,4),e)],fill_ts=(3,4))

    def _acc_v0c2(tick, staffIdx, fv, pitch, xoff=0):
        d = bytearray(19)
        d[0]=22; d[1]=staffIdx&0x3F; d[2]=fv
        d[3]=0x20; d[4]=0x04   # ACCIACCATURA: grace1&0x30==0x20, grace2&0x05==0x04
        d[7]=xoff&0xFF; d[10]=pitch
        return struct.pack('<H',tick)+bytes([(9<<4)])+bytes(d)

def gen_v0c4_grace_after_main_grace_to_later():
    """v0xC4: main note BEFORE grace in binary at tick=240; SLURSTART→Note@480 (grace-to-later slur)."""
    e  = note_v0c4(0,0,0,fv=3,pitch=60)                                            # filler quarter@0
    e += ornament_v0c4(240,0,0,tipo=0x21,xoffset=5,alMezuro=0,xoffset2=9)          # SLURSTART
    e += note_v0c4_xoff(240,0,0,fv=3,pitch=62,xoff=5)                              # main FIRST (xoff=5)
    e += note_v0c4_grace_xoff(240,0,0,fv=4,pitch=64,grace1=0x20,grace2=0x04,xoff=3) # ACCIACCATURA SECOND
    e += note_v0c4_xoff(480,0,0,fv=3,pitch=64,xoff=9)                              # later note (target)
    e += note_v0c4(720,0,0,fv=3,pitch=65)                                           # filler
    e += end_marker()
    return assemble(0xC4,[(meas_hdr(4,4),e)],fill_ts=(4,4))

def gen_v0c4_grace_after_main_in_binary():
    """v0xC4: main note FIRST, ACCIACCATURA SECOND at tick=0; SLURSTART@0 → grace must be startElement."""
    e  = ornament_v0c4(0,0,0,tipo=0x21,xoffset=5,alMezuro=0,xoffset2=10)           # SLURSTART
    e += note_v0c4(0,0,0,fv=3,pitch=60)                                             # main FIRST (tick=0)
    e += note_v0c4_grace(0,0,0,fv=4,pitch=64,grace1=0x20,grace2=0x04)              # ACCIACCATURA SECOND
    e += note_v0c4(240,0,0,fv=3,pitch=62)                                           # slur target
    e += note_v0c4(480,0,0,fv=3,pitch=64)
    e += note_v0c4(720,0,0,fv=3,pitch=65)
    e += end_marker()
    return assemble(0xC4,[(meas_hdr(4,4),e)],fill_ts=(4,4))

def gen_v0c4_grace_after_main_preceding_notes():
    """v0xC4: Quarter@0, then main FIRST + ACCIACCATURA SECOND at tick=240; grace-to-later slur."""
    e  = note_v0c4(0,0,0,fv=3,pitch=60)                                             # preceding note
    e += ornament_v0c4(240,0,0,tipo=0x21,xoffset=5,alMezuro=0,xoffset2=9)           # SLURSTART
    e += note_v0c4_xoff(240,0,0,fv=3,pitch=62,xoff=5)                               # main FIRST
    e += note_v0c4_grace_xoff(240,0,0,fv=4,pitch=64,grace1=0x20,grace2=0x04,xoff=3)  # ACCIACCATURA SECOND
    e += note_v0c4_xoff(480,0,0,fv=3,pitch=64,xoff=9)                               # later note (target)
    e += note_v0c4(720,0,0,fv=3,pitch=65)                                            # filler
    e += end_marker()
    return assemble(0xC4,[(meas_hdr(4,4),e)],fill_ts=(4,4))

def gen_v0c4_grace_after_main_slur_to_main():
    """v0xC4: main FIRST(xoff=20) + ACCIACCATURA SECOND(xoff=10) at tick=0; grace-to-main slur (zero-span)."""
    # SLURSTART(xoffset=10, xoffset2=22): arc starts at grace(xoff=10), ends near main(xoff≈22).
    # With fix: firstNoteXoff=grace xoff=10 → targetEnd=22, main dist=2 wins → zero-span → grace-to-main.
    e  = ornament_v0c4(0,0,0,tipo=0x21,xoffset=10,alMezuro=0,xoffset2=22)           # SLURSTART
    e += note_v0c4_xoff(0,0,0,fv=3,pitch=60,xoff=20)                                # main FIRST (xoff=20)
    e += note_v0c4_grace(0,0,0,fv=4,pitch=64,grace1=0x20,grace2=0x04)               # ACCIACCATURA SECOND (xoff=10 implied)
    e += note_v0c4_xoff(240,0,0,fv=3,pitch=62,xoff=40)                              # later note (xoff=40, far away)
    e += note_v0c4(480,0,0,fv=3,pitch=64)
    e += note_v0c4(720,0,0,fv=3,pitch=65)
    e += end_marker()
    return assemble(0xC4,[(meas_hdr(4,4),e)],fill_ts=(4,4))

def gen_v0c4_grace_slur_to_main_coloc():
    """v0xC4 3/4: ACCIACCATURA + main note co-located at tick=480; grace-to-main slur (Fix B: tick2=startTick)."""
    e  = note_v0c4(0,0,0,fv=3,pitch=60)                                             # quarter@0
    e += note_v0c4(240,0,0,fv=3,pitch=62)                                           # quarter@240
    e += ornament_v0c4(480,0,0,tipo=0x21,xoffset=5,alMezuro=0,xoffset2=7)           # SLURSTART
    e += note_v0c4_grace(480,0,0,fv=4,pitch=64,grace1=0x20,grace2=0x04)             # ACCIACCATURA
    e += note_v0c4(480,0,0,fv=3,pitch=64)                                            # main (co-located)
    e += end_marker()
    return assemble(0xC4,[(meas_hdr(3,4),e)],fill_ts=(3,4))

def gen_v0c4_4to3_quadruplet():
    """v0xC4 3/8: four eighth notes with tuplet=0x43 (4:3 quadruplet) filling the whole measure."""
    e  = note_v0c4(  0,0,0,fv=4,pitch=60,tuplet=0x43)
    e += note_v0c4( 90,0,0,fv=4,pitch=62,tuplet=0x43)
    e += note_v0c4(180,0,0,fv=4,pitch=64,tuplet=0x43)
    e += note_v0c4(270,0,0,fv=4,pitch=65,tuplet=0x43)
    e += end_marker()
    return assemble(0xC4,[(meas_hdr(3,8,beatTicks=120),e)],fill_ts=(3,8))

def _grandstaff_header_and_line(measureCount=1):
    """Build SCOW header + LINE block for 1-instrument 2-staff (Piano grand staff)."""
    hdr = bytearray(194)
    hdr[0:4] = b'SCOW'
    hdr[4] = 0xC4
    struct.pack_into('<H', hdr, 0x28, 0x0420)
    struct.pack_into('<H', hdr, 0x2C, 0xF000)
    struct.pack_into('<h', hdr, 0x2E, 1)   # lineCount=1
    struct.pack_into('<h', hdr, 0x30, 1)   # pageCount=1
    hdr[0x32] = 1                           # instrumentCount=1
    hdr[0x33] = 2                           # staffPerSystem=2
    struct.pack_into('<h', hdr, 0x34, measureCount)

    def se(clef, key, isidx):
        e = bytearray(30); e[14]=clef; e[15]=key; e[19]=1; e[21]=isidx; return bytes(e)

    line_data = (b'\x00'*10 + struct.pack('<H', 0) + bytes([measureCount])
                 + se(0, 0, 0x00)   # treble staff, staffWithin=0, instrIdx=0
                 + se(1, 0, 0x40))  # bass staff, staffWithin=1, instrIdx=0
    line_block = b'LINE' + struct.pack('<I', len(line_data)) + line_data
    return bytes(hdr), line_block

    def se(clef, key, isidx):
        e = bytearray(30); e[14]=clef; e[15]=key; e[19]=1; e[21]=isidx; return bytes(e)

def tie_18byte_with_srcpos(tick, voice, staffIdx, direction=0x04, startFlag=0x80, arcX1=12, arcX2=50, srcPos=0):
    d = bytearray(18)
    struct.pack_into('<H', d, 0, tick)
    d[2] = (3<<4) | (voice & 0xF)
    d[3] = 18
    d[4] = staffIdx & 0x3F
    d[5] = direction
    d[6] = startFlag
    d[10] = arcX1 & 0xFF
    d[12] = arcX2 & 0xFF
    # d[13] = 0 (skipped)
    d[14] = srcPos & 0xFF  # sourcePosition byte
    return bytes(d)

def _gs_hdr_line(n_meas=1):
    hdr = bytearray(194)
    hdr[0:4] = b'SCOW'; hdr[4] = 0xC4
    struct.pack_into('<H', hdr, 0x28, 0x0420)
    struct.pack_into('<H', hdr, 0x2C, 0xF000)
    struct.pack_into('<h', hdr, 0x2E, 1)
    struct.pack_into('<h', hdr, 0x30, 1)
    hdr[0x32] = 1; hdr[0x33] = 2
    struct.pack_into('<h', hdr, 0x34, n_meas)
    def se(clef, isidx):
        e = bytearray(30); e[14]=clef; e[19]=1; e[21]=isidx; return bytes(e)
    ld = b'\x00'*10 + struct.pack('<H',0) + bytes([n_meas]) + se(0,0x00) + se(1,0x40)
    lb = b'LINE' + struct.pack('<I', len(ld)) + ld
    return bytes(hdr), lb

    def se(clef, isidx):
        e = bytearray(30); e[14]=clef; e[19]=1; e[21]=isidx; return bytes(e)

def _note_gs(tick, voice, raw_staff, fv, pitch):
    d = bytearray(25)
    d[0]=28; d[1]=raw_staff&0xFF; d[2]=fv; d[12]=pitch
    return struct.pack('<H',tick)+bytes([(9<<4)|(voice&0xF)])+bytes(d)

def _note_gs_raw(tick, voice, raw_staff, fv, pitch):
    """28-byte v0xC4 note with full (unmasked) raw_staff byte."""
    d = bytearray(25)
    d[0] = 28; d[1] = raw_staff & 0xFF; d[2] = fv; d[12] = pitch
    return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)

def _tie_gs_raw(tick, voice, raw_staff, direction=0xfc):
    """16-byte TIE element with full (unmasked) raw_staff byte."""
    d = bytearray(13)
    d[0] = 16; d[1] = raw_staff & 0xFF; d[2] = direction
    return struct.pack('<H', tick) + bytes([(3 << 4) | (voice & 0xF)]) + bytes(d)

def _orn16_gs_raw(tick, voice, raw_staff, tipo):
    """16-byte ORN element with full (unmasked) raw_staff byte."""
    d = bytearray(16)
    struct.pack_into('<H', d, 0, tick)
    d[2] = (5 << 4) | (voice & 0xF)
    d[3] = 16
    d[4] = raw_staff & 0xFF
    d[5] = tipo
    return bytes(d)

def _note_raw(tick, voice, raw_staff, fv, pitch):
    """28-byte v0xC4 note with unmasked raw_staff byte (preserves bit6 for grand-staff routing)."""
    d = bytearray(25)
    d[0] = 28; d[1] = raw_staff & 0xFF; d[2] = fv; d[12] = pitch
    return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)

def _note_raw_pos(tick, voice, raw_staff, fv, pitch, position=0):
    """28-byte v0xC4 note with unmasked raw_staff + position byte (elemStart+12 = d[9])."""
    d = bytearray(25)
    d[0] = 28; d[1] = raw_staff & 0xFF; d[2] = fv; d[9] = position & 0xFF; d[12] = pitch
    return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)

def _tie18_srcpos(tick, voice, staffIdx, direction, startFlag, arcX1, arcX2, srcPos):
    """18-byte TIE element with sourcePosition byte at offset +14."""
    d = bytearray(18)
    struct.pack_into('<H', d, 0, tick)
    d[2] = (3 << 4) | (voice & 0xF); d[3] = 18
    d[4] = staffIdx & 0x3F; d[5] = direction; d[6] = startFlag
    d[10] = arcX1 & 0xFF; d[12] = arcX2 & 0xFF; d[14] = srcPos & 0xFF
    return bytes(d)

def _gs_header_line(n_meas=1):
    """SCOW header + LINE block: single Piano (1 instr, 2 staves, grand staff)."""
    hdr = bytearray(194)
    hdr[0:4] = b'SCOW'; hdr[4] = 0xC4
    struct.pack_into('<H', hdr, 0x28, 0x0420)
    struct.pack_into('<H', hdr, 0x2C, 0xF000)
    struct.pack_into('<h', hdr, 0x2E, 1)
    struct.pack_into('<h', hdr, 0x30, 1)
    hdr[0x32] = 1; hdr[0x33] = 2
    struct.pack_into('<h', hdr, 0x34, n_meas)
    def se(clef, isidx):
        e = bytearray(30); e[14] = clef; e[19] = 1; e[21] = isidx; return bytes(e)
    ld = b'\x00' * 10 + struct.pack('<H', 0) + bytes([n_meas]) + se(0, 0x00) + se(1, 0x40)
    return bytes(hdr), b'LINE' + struct.pack('<I', len(ld)) + ld

    def se(clef, isidx):
        e = bytearray(30); e[14] = clef; e[19] = 1; e[21] = isidx; return bytes(e)

def _nraw(tick, voice, raw_staff, fv, pitch):
    """28-byte v0xC4 note with unmasked raw_staff byte (bit6 preserved for grand-staff routing)."""
    d = bytearray(25)
    d[0] = 28; d[1] = raw_staff & 0xFF; d[2] = fv; d[12] = pitch
    return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)

def _nraw_pos(tick, voice, raw_staff, fv, pitch, position):
    """28-byte v0xC4 note with unmasked raw_staff + note position byte (elemStart+12 = d[9])."""
    d = bytearray(25)
    d[0] = 28; d[1] = raw_staff & 0xFF; d[2] = fv; d[9] = position & 0xFF; d[12] = pitch
    return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)

def _tie_raw(tick, voice, raw_staff, direction=0xfc):
    """16-byte TIE element with unmasked raw_staff byte (bit6 for grand-staff TIE routing)."""
    d = bytearray(13)
    d[0] = 16; d[1] = raw_staff & 0xFF; d[2] = direction
    return struct.pack('<H', tick) + bytes([(3 << 4) | (voice & 0xF)]) + bytes(d)

def _orn16_raw(tick, voice, raw_staff, tipo):
    """16-byte ORN element with unmasked raw_staff byte (bit6 for grand-staff ORN routing)."""
    d = bytearray(16)
    struct.pack_into('<H', d, 0, tick)
    d[2] = (5 << 4) | (voice & 0xF); d[3] = 16
    d[4] = raw_staff & 0xFF; d[5] = tipo
    return bytes(d)

def _tie18_with_srcpos(tick, voice, staffIdx, direction, startFlag, arcX1, arcX2, srcPos):
    """18-byte TIE element with sourcePosition byte at element offset +14."""
    d = bytearray(18)
    struct.pack_into('<H', d, 0, tick)
    d[2] = (3 << 4) | (voice & 0xF); d[3] = 18
    d[4] = staffIdx & 0x3F; d[5] = direction; d[6] = startFlag
    d[10] = arcX1 & 0xFF; d[12] = arcX2 & 0xFF; d[14] = srcPos & 0xFF
    return bytes(d)

def gen_v0c4_grandstaff_bit6_second_staff():
    """Grand-staff: C5/E5 (staff0) and C3/E3 (staff1) via staffWithin bit-6 routing."""
    hdr, lb = _gs_header_line(1)
    e  = _nraw(0, 0, 0x00, fv=3, pitch=72)   # C5 treble voice=0
    e += _nraw(0, 1, 0x00, fv=3, pitch=76)   # E5 treble voice=1
    e += _nraw(0, 2, 0x40, fv=3, pitch=48)   # C3 bass   staffWithin=1 voice=2
    e += _nraw(0, 3, 0x40, fv=3, pitch=52)   # E3 bass   staffWithin=1 voice=3
    e += end_marker()
    return bytes(hdr) + lb + meas_block(meas_hdr(4, 4), e) + SKELETON_POST

def gen_v0c4_grandstaff_high_voice_own_staff():
    """Grand-staff: a voice-7 melody note (raw_staff 0x00 = top staff, staffWithin 0) must stay on the
    TOP staff, not be pushed to the bass staff. Only voice==4 is the staff-2 marker; voices 5..7 are
    genuine extra voices on the same staff. With the bug the voice>=VOICES rule routed it to staff+1."""
    hdr, lb = _gs_header_line(1)
    e  = _nraw(0, 7, 0x00, fv=3, pitch=67)   # G4 quarter on the TOP staff, voice 7, staffWithin 0
    e += end_marker()
    return bytes(hdr) + lb + meas_block(meas_hdr(4, 4), e) + SKELETON_POST

def gen_v0c4_grandstaff_staffwithin_fermata():
    """Grand-staff: fermataAbove (0xCC) on staff0, fermataBelow (0xCD) on staff1 via bit-6 routing."""
    hdr, lb = _gs_header_line(1)
    e  = _nraw(0, 0, 0x00, fv=1, pitch=72)       # C5 whole treble
    e += _nraw(0, 0, 0x40, fv=1, pitch=52)       # E3 whole bass
    e += _orn16_raw(0, 0, 0x00, tipo=0xCC)        # FERMATA_ABOVE on treble staff
    e += _orn16_raw(0, 0, 0x40, tipo=0xCD)        # FERMATA_BELOW on bass staff (bit6)
    e += end_marker()
    return bytes(hdr) + lb + meas_block(meas_hdr(4, 4), e) + SKELETON_POST

def gen_v0c4_grandstaff_staffwithin_four_voices():
    """Grand-staff: voices 0,1 on treble (C5,E5) and voices 2,3 on bass (G3,B3)."""
    hdr, lb = _gs_header_line(1)
    e  = _nraw(0, 0, 0x00, fv=3, pitch=72)   # C5 treble voice=0
    e += _nraw(0, 1, 0x00, fv=3, pitch=76)   # E5 treble voice=1
    e += _nraw(0, 2, 0x40, fv=3, pitch=55)   # G3 bass   voice=2 staffWithin=1
    e += _nraw(0, 3, 0x40, fv=3, pitch=59)   # B3 bass   voice=3 staffWithin=1
    e += end_marker()
    return bytes(hdr) + lb + meas_block(meas_hdr(4, 4), e) + SKELETON_POST

def gen_v0c4_grandstaff_staffwithin_rest_on_second_staff():
    """Grand-staff: treble C5 note on staff0; REST element with staffWithin=1 on staff1."""
    hdr, lb = _gs_header_line(1)
    def rest_raw_gs(tick, voice, raw_staff, fv):
        d = bytearray(15)
        d[0] = 18; d[1] = raw_staff & 0xFF; d[2] = fv
        return struct.pack('<H', tick) + bytes([(8 << 4) | (voice & 0xF)]) + bytes(d)
    e  = _nraw(0, 0, 0x00, fv=3, pitch=72)
    e += rest_raw_gs(0, 0, 0x40, fv=3)
    e += end_marker()
    return bytes(hdr) + lb + meas_block(meas_hdr(4, 4), e) + SKELETON_POST

    def rest_raw_gs(tick, voice, raw_staff, fv):
        d = bytearray(15)
        d[0] = 18; d[1] = raw_staff & 0xFF; d[2] = fv
        return struct.pack('<H', tick) + bytes([(8 << 4) | (voice & 0xF)]) + bytes(d)

def gen_v0c4_grandstaff_staffwithin_sequential():
    """Grand-staff: sequential quarter notes on each staff (treble C5/E5, bass C3/E3 at separate ticks)."""
    hdr, lb = _gs_header_line(1)
    e  = _nraw(  0, 0, 0x00, fv=3, pitch=72)   # C5 treble beat 1
    e += _nraw(240, 0, 0x00, fv=3, pitch=76)   # E5 treble beat 2
    e += _nraw(  0, 0, 0x40, fv=3, pitch=48)   # C3 bass beat 1 (staffWithin=1)
    e += _nraw(240, 0, 0x40, fv=3, pitch=52)   # E3 bass beat 2 (staffWithin=1)
    e += end_marker()
    return bytes(hdr) + lb + meas_block(meas_hdr(4, 4), e) + SKELETON_POST

def gen_v0c4_grandstaff_staffwithin_tie_on_second_staff():
    """Grand-staff: bass E3 half tied to E3 half on staff1 via TIE element with staffWithin=1."""
    hdr, lb = _gs_header_line(1)
    e  = _nraw(  0, 0, 0x00, fv=2, pitch=72)      # C5 half treble beat 1
    e += _nraw(480, 0, 0x00, fv=2, pitch=72)      # C5 half treble beat 3
    e += _tie_raw(0, 2, 0x40, direction=0xfc)      # TIE for bass E3 (staffWithin=1, voice=2)
    e += _nraw(  0, 2, 0x40, fv=2, pitch=52)      # E3 half bass beat 1 (tie source)
    e += _nraw(480, 2, 0x40, fv=2, pitch=52)      # E3 half bass beat 3 (tie destination)
    e += end_marker()
    return bytes(hdr) + lb + meas_block(meas_hdr(4, 4), e) + SKELETON_POST

def gen_v0c4_grandstaff_wedge_out_of_range_voice():
    """Grand-staff WEDGESTART on the bass sub-staff (staffWithin=1) plus a NOTE on that
    sub-staff whose voice nibble (15) is far above VOICES. The wedge resolver derives the
    hairpin track from the raw note voice (staffIdx*VOICES + (voice - staffWithin*VOICES/2)),
    so without a validTrack guard it created a Hairpin at a track well beyond ntracks and
    crashed at layout. The importer must drop that hairpin and lay out cleanly."""
    hdr, lb = _gs_header_line(1)
    e  = _orn16_raw(0, 0, 0x40, tipo=0x1D)   # WEDGESTART on bass sub-staff (staffWithin=1)
    e += _nraw(0, 15, 0x40, fv=1, pitch=48)  # C3 whole note, out-of-range voice nibble 15
    e += end_marker()
    return bytes(hdr) + lb + meas_block(meas_hdr(4, 4), e) + SKELETON_POST

def gen_v0c4_hostile_zero_tuplet_nibble():
    """Notes whose tuplet byte has a zero nibble (0x30 = actualNotes 3, normalNotes 0), which would
    make a tuplet ratio with a zero term. The importer must not divide by zero nor emit a bad tuplet,
    and the measure must still pass sanityCheck."""
    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60, tuplet=0x30)
    e += note_v0c4(160, 0, 0, fv=3, pitch=62, tuplet=0x30)
    e += note_v0c4(320, 0, 0, fv=3, pitch=64, tuplet=0x30)
    e += note_v0c4(480, 0, 0, fv=3, pitch=65)
    e += note_v0c4(960, 0, 0, fv=1, pitch=67)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))

def gen_v0c4_hostile_out_of_range_staff():
    """A note carrying staffIdx=63 on a single-staff score. Routing must drop it rather than index
    the staff vector out of bounds; the rest of the measure imports cleanly."""
    e  = note_v0c4(  0, 0, 63, fv=3, pitch=60)   # staffIdx 63, far past the only staff
    e += note_v0c4(  0, 0,  0, fv=1, pitch=62)   # a valid whole note on staff 0
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))

def gen_v0c4_hostile_out_of_range_voice():
    """A note whose voice nibble (15) is far above VOICES on a single-staff score. Routing must drop
    it rather than build an out-of-range track."""
    e  = note_v0c4(  0, 15, 0, fv=3, pitch=60)   # voice 15, out of range
    e += note_v0c4(  0,  0, 0, fv=1, pitch=62)   # a valid whole note on voice 0
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))

def gen_v0c4_hostile_zero_size_element():
    """A note element whose size byte (element +3) is zero. The parser must advance by a single byte
    to avoid an infinite loop and still terminate on the measure boundary."""
    n = bytearray(note_v0c4(0, 0, 0, fv=3, pitch=60))
    n[3] = 0   # zero the element size byte
    e  = bytes(n) + note_v0c4(480, 0, 0, fv=3, pitch=62) + end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))

def gen_v0c4_tie_dir_02():
    """TIE direction byte 0x02 (bit 1 set) must create a forward tie (isTieStart = true)."""
    e  = tie_v0c4(0, 0, 0, direction=0x02)
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += note_v0c4( 480, 0, 0, fv=3, pitch=60)
    e += note_v0c4( 960, 0, 0, fv=3, pitch=62)
    e += note_v0c4(1440, 0, 0, fv=3, pitch=64)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))

def gen_v0c4_tie_dir_03():
    """TIE direction byte 0x03 (bits 0+1 set) must create a forward tie (isTieStart = true)."""
    e  = tie_v0c4(0, 0, 0, direction=0x03)
    e += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    e += note_v0c4( 480, 0, 0, fv=3, pitch=60)
    e += note_v0c4( 960, 0, 0, fv=3, pitch=62)
    e += note_v0c4(1440, 0, 0, fv=3, pitch=64)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))

def gen_v0c4_tie_partial_chord_source_position():
    """18-byte TIE sourcePosition=5 ties A4 (pos=5) only; C#4 (pos=0) must not be tied."""
    e  = _tie18_with_srcpos(0, 0, 0, direction=0x04, startFlag=0x80,
                             arcX1=12, arcX2=50, srcPos=5)
    e += _nraw_pos(  0, 0, 0, fv=3, pitch=61, position=0)   # C#4 pos=0 (no tie)
    e += _nraw_pos(  0, 0, 0, fv=3, pitch=69, position=5)   # A4  pos=5 (tie source)
    e += _nraw_pos(480, 0, 0, fv=3, pitch=61, position=0)   # C#4 pos=0 (not tied)
    e += _nraw_pos(480, 0, 0, fv=3, pitch=69, position=5)   # A4  pos=5 (tie destination)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))

# ===========================================================================
# notes_nonuplet_missing_marker.enc
# Nine sixteenths in the time of eight (tuplet byte 0x98), four notes then five
# rests, filling half of a 4/4 bar, with two quarter rests after.  Encore leaves
# the tuplet byte off one interior member, the first rest, exactly as it does in
# the wild: nine members, eight marks.  The unmarked one is enclosed by marked
# members, so it belongs to the bracket.  Its stored tick is also the rounding of
# 4 x 53.33, which lands a tick before the exact position the members so far add
# up to, so a reader comparing raw ticks against the running total sees it as
# redundant and drops it.  Either mistake leaves the group a member short and the
# bar over a third too long.
# ===========================================================================
# ===========================================================================
# notes_tie_across_trimmed_overflow.enc
# The shape a real score turned out to have, twice over: two 6/8 bars, each
# holding a single whole note, tied to each other.  A whole note is 960 Encore
# ticks and the bar has room for 720, so each is trailing overflow and the pass
# that makes the bar fit removes the second one.  The tie was built while the
# music was emitted, so it is left pointing at a note that no longer exists,
# and the pitch spelling pass walks tied notes afterwards and dies on it.
# ===========================================================================
def gen_v0c4_tie_across_trimmed_overflow():
    m1  = tie_v0c4(  0, 0, 0, startFlag=0x80)
    m1 += note_v0c4( 0, 0, 0, fv=1, pitch=69)     # whole note in a 6/8 bar
    m1 += end_marker()
    m2  = note_v0c4( 0, 0, 0, fv=1, pitch=69)     # the note the tie ends on, removed as overflow
    m2 += end_marker()
    data = assemble(0xC4, [(meas_hdr(6, 8, beatTicks=360), m1),
                           (meas_hdr(6, 8, beatTicks=360), m2)], fill_ts=(6, 8))
    # The staff transposes, as it did in the score this came from. That is what puts the pitch
    # spelling pass over the tied notes, which is where a tie outliving its notes is followed.
    return _patch_key_transpose(data, 0, -2)


# ===========================================================================
# notes_tuplet_group_opens_unmarked.enc
# The shape a real score turned out to have: six sixteenths in the time of four,
# written as two triplet groups, of which Encore marks five members and leaves
# the byte off the one that opens the second group.  That member is also given a
# plain sixteenth's room, sixty ticks instead of forty, so every member after it
# sits twenty ticks late and the bar holds twenty more than its signature allows.
# The same figure appears again later in the bar that this came from, fully
# marked, which is what says the unmarked reading is the file's slip and not a
# different rhythm.  Read literally the six members span a quarter and a
# sixteenth, the bar overflows, and the staves that were exactly full get
# stretched into corruption along with it.
# ===========================================================================
def gen_v0c4_tuplet_group_opens_unmarked():
    TUP = 0x32                       # three in the time of two
    FV_16TH, FV_Q, FV_H = 5, 3, 2
    e  = rest_v0c4_tup(0, 0, 0, fv=FV_16TH, tuplet=TUP)
    e += note_v0c4(40, 0, 0, fv=FV_16TH, pitch=81, tuplet=TUP)
    e += note_v0c4(80, 0, 0, fv=FV_16TH, pitch=78, tuplet=TUP)
    e += note_v0c4(120, 0, 0, fv=FV_16TH, pitch=75, tuplet=0)     # opens the second group, unmarked
    e += note_v0c4(180, 0, 0, fv=FV_16TH, pitch=78, tuplet=TUP)   # and every member after it is late
    e += note_v0c4(220, 0, 0, fv=FV_16TH, pitch=75, tuplet=TUP)
    e += note_v0c4(260, 0, 0, fv=FV_Q, pitch=71)
    e += rest_v0c4(500, 0, 0, fv=FV_H)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_tie_start_recut_at_barline.enc
# A 2/4 bar holding a quarter, then a two-note chord of half notes carrying a tie
# start, so the chord begins on the second beat and states more length than the
# bar has room for.  The pass that makes the bar fit rewrites a figure crossing
# the barline as a tied chain of the same pitches: the chord is removed and built
# again at the same beat.  The tie start is therefore registered against notes
# that no longer exist by the time the note it ties to arrives in the next bar,
# while the notes it means still stand exactly where they were written.  Only the
# upper pitch continues into the next bar, so a reader that recognises the start
# by anything other than where it is and what pitch it holds ties the wrong one,
# and the tie comes out joining two different pitches.
# ===========================================================================
def gen_v0c4_tie_start_recut_at_barline():
    e1  = note_v0c4(0, 0, 0, fv=3, pitch=69)          # quarter, first beat
    e1 += tie_v0c4(240, 0, 0, startFlag=0x80)
    e1 += note_v0c4(240, 0, 0, fv=2, pitch=69)        # chord of halves from the second beat:
    e1 += note_v0c4(241, 0, 0, fv=2, pitch=76)        # crosses the barline and is rebuilt
    e1 += end_marker()
    e2  = note_v0c4(0, 0, 0, fv=3, pitch=76)          # only the upper pitch continues
    e2 += rest_v0c4(240, 0, 0, fv=3)
    e2 += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e1), (meas_hdr(2, 4), e2)], fill_ts=(2, 4))


# ===========================================================================
# notes_dotted_note_between_tuplet_members.enc
# The shape a played-in bar turned out to have: a triplet eighth, then a dotted
# eighth carrying no tuplet byte, then a run of marked triplet eighths.  Encore
# draws the dot, so the dot is part of what the note is worth, and a dotted
# eighth is not the eighth the bracket is built from: it takes half a slot more.
# A reader that compares bare face values lets it into the bracket, scales it by
# the ratio anyway, and then lays the next member on top of its tail, which
# leaves the bar a triplet eighth over its signature with the overrun sitting in
# the middle rather than past the barline, where no later pass looks for it.
# ===========================================================================
def gen_v0c4_dotted_note_between_tuplet_members():
    TUP = 0x32                       # three in the time of two
    FV_8TH, FV_Q, FV_16TH = 4, 3, 5
    e  = note_v0c4(0, 0, 0, fv=FV_8TH, pitch=62, tuplet=TUP)
    e += note_v0c4_dotctrl(80, 0, 0, fv=FV_8TH, pitch=57, dotControl=1)   # dotted, unmarked
    e += note_v0c4(260, 0, 0, fv=FV_8TH, pitch=65, tuplet=TUP)
    e += note_v0c4(340, 0, 0, fv=FV_8TH, pitch=65, tuplet=TUP)
    e += note_v0c4(420, 0, 0, fv=FV_8TH, pitch=61, tuplet=TUP)
    e += rest_v0c4(540, 0, 0, fv=FV_Q)
    e += rest_v0c4(780, 0, 0, fv=FV_8TH)
    e += rest_v0c4(900, 0, 0, fv=FV_16TH)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_dotted_tuplet_member.enc
# A 3/4 bar whose middle beat holds a triplet of the mixed kind Encore writes
# freely: a dotted eighth and three sixteenths, every one of them marked, in the
# room of a quarter.  Written they come to three eighths, which is exactly what a
# bracket of three in the time of two is built to hold, and sounding they come to
# the quarter they occupy.  Two readings lose it.  Dropping the dot because the
# note carries a ratio makes the first member an eighth, and everything after it
# slides.  Taking the dotted value as the value the bracket is built from makes
# the bracket three dotted eighths long, and it never closes.  The bar around it
# is plain, an eighth rest and an eighth before, two eighths after, so nothing
# but the triplet can be blamed for the bar coming out wrong.
# ===========================================================================
def gen_v0c4_dotted_tuplet_member():
    TUP = 0x32                       # three in the time of two
    FV_8TH, FV_16TH = 4, 5
    e  = rest_v0c4(0, 0, 0, fv=FV_8TH)
    e += note_v0c4(120, 0, 0, fv=FV_8TH, pitch=67)
    e += note_v0c4(240, 0, 0, fv=FV_8TH, pitch=67, tuplet=TUP, layout=1)   # dotted, and marked
    e += note_v0c4(360, 0, 0, fv=FV_16TH, pitch=67, tuplet=TUP)
    e += note_v0c4(400, 0, 0, fv=FV_16TH, pitch=67, tuplet=TUP)
    e += note_v0c4(440, 0, 0, fv=FV_16TH, pitch=67, tuplet=TUP)
    e += note_v0c4(480, 0, 0, fv=FV_8TH, pitch=67)
    e += note_v0c4(600, 0, 0, fv=FV_8TH, pitch=67)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(3, 4), e)], fill_ts=(3, 4))


# ===========================================================================
# notes_columns_apart_not_one_chord.enc
# The shape a live-recorded drum bar turned out to have: a triplet whose three
# members stand in three columns five pixels apart, the last two recorded five
# ticks apart because the strokes were played by hand.  A reader that groups by
# tick proximity swallows the third column into the second, leaving the bracket a
# member short.  The columns are what Encore draws, and they say plainly that
# these are three events: a chord's members share a column to the pixel, while on
# a staff this dense two columns come as close as a few pixels.  Losing the member
# leaves the bar short of its signature by a triplet sixteenth, and the hole then
# gets papered over with rests that overlap the notes around them.
# ===========================================================================
def gen_v0c4_columns_apart_not_one_chord():
    TUP = 0x32                       # three in the time of two
    FV_16TH, FV_Q, FV_8TH = 5, 3, 4
    e  = note_v0c4_xoff_tup(0, 0, 0, fv=FV_16TH, pitch=60, xoff=20, tuplet=TUP)
    e += note_v0c4_xoff_tup(40, 0, 0, fv=FV_16TH, pitch=64, xoff=25, tuplet=TUP)
    e += note_v0c4_xoff_tup(45, 0, 0, fv=FV_16TH, pitch=67, xoff=30, tuplet=TUP)
    e += rest_v0c4(120, 0, 0, fv=FV_Q)
    e += rest_v0c4(360, 0, 0, fv=FV_Q)
    e += rest_v0c4(600, 0, 0, fv=FV_Q)
    e += rest_v0c4(840, 0, 0, fv=FV_8TH)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_bracket_opening_rest_behind_fill.enc
# The shape three scores turned out to share: a half note, then a triplet of
# eighths opening with a rest, then four sixteenths, which comes to the bar
# exactly.  The half was played at half its written value, so every stored tick
# after it stands a beat early: the triplet's opening rest carries tick 240 while
# the notes written before it already account for 480.  A reader that drops a rest
# sitting behind the position its voice has reached, which is the right thing for
# the redundant rests Encore writes on a filled beat, loses the member that opens
# the bracket.  The bar is then a triplet eighth short of its signature, and since
# that is not a value anything can be written as, the residue comes out as a chain
# of ever smaller rests while another rest papers over the hole in the middle.
# ===========================================================================
def gen_v0c4_bracket_opening_rest_behind_fill():
    TUP = 0x32                       # three in the time of two
    FV_H, FV_8TH, FV_16TH = 2, 4, 5
    e  = note_v0c4(0, 0, 0, fv=FV_H, pitch=70)
    e += rest_v0c4_tup(240, 0, 0, fv=FV_8TH, tuplet=TUP)   # opens the bracket, tick left behind
    e += note_v0c4(336, 0, 0, fv=FV_8TH, pitch=67, tuplet=TUP)
    e += note_v0c4(409, 0, 0, fv=FV_8TH, pitch=69, tuplet=TUP)
    e += note_v0c4(476, 0, 0, fv=FV_16TH, pitch=70)
    e += note_v0c4(558, 0, 0, fv=FV_16TH, pitch=69)
    e += note_v0c4(618, 0, 0, fv=FV_16TH, pitch=67)
    e += note_v0c4(678, 0, 0, fv=FV_16TH, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_sixtyfourth_bracket_not_artifact.enc
# An ornamental flourish written the way three scores wrote it: a triplet of
# sixty-fourths and a thirty-second, together a sixteenth, tucked between a
# sixteenth rest and four plain sixteenths.  A sixty-fourth is fifteen ticks
# written and ten inside a triplet, so its recorded length falls under the
# fifteen that marks a note as a MIDI tie-continuation artifact, and all three
# members get thrown away.  The bar is then short of its signature by a value
# that cannot be written, so the residue comes out as a run of ever smaller
# rests, and the flourish is gone from the score entirely.
# ===========================================================================
def gen_v0c4_sixtyfourth_bracket_not_artifact():
    TUP = 0x32                       # three in the time of two
    FV_H, FV_8TH, FV_16TH, FV_32ND, FV_64TH = 2, 4, 5, 6, 7
    e  = note_v0c4(0, 0, 0, fv=FV_H, pitch=73)
    e += note_v0c4(480, 0, 0, fv=FV_8TH, pitch=73)
    e += rest_v0c4(600, 0, 0, fv=FV_16TH)
    e += note_v0c4(660, 0, 0, fv=FV_64TH, pitch=74, tuplet=TUP)
    e += note_v0c4(670, 0, 0, fv=FV_64TH, pitch=76, tuplet=TUP)
    e += note_v0c4(680, 0, 0, fv=FV_64TH, pitch=74, tuplet=TUP)
    e += note_v0c4(690, 0, 0, fv=FV_32ND, pitch=74)
    e += note_v0c4(720, 0, 0, fv=FV_16TH, pitch=73)
    e += note_v0c4(780, 0, 0, fv=FV_16TH, pitch=71)
    e += note_v0c4(840, 0, 0, fv=FV_16TH, pitch=69)
    e += note_v0c4(900, 0, 0, fv=FV_16TH, pitch=73)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_tick_wrapped_before_barline.enc
# A 3/4 bar whose opening note was played six ticks before the barline, so its
# position is stored as 0xFFFA, six below zero read as a signed sixteen-bit
# number.  Read as unsigned it is the largest position in the measure, and the
# note that opens the bar sorts to the end of it: everything after slides one
# place forward, a rest takes the downbeat, and the last note is cut to fit what
# is left.  The rest of the bar is stored plainly and comes to the signature
# exactly, so nothing but that one position can be blamed.
# ===========================================================================
def gen_v0c4_tick_wrapped_before_barline():
    FV_Q, FV_8TH, FV_16TH = 3, 4, 5
    e  = note_v0c4(0xFFFA, 0, 0, fv=FV_Q, pitch=70, layout=1)   # dotted quarter, six ticks early
    e += note_v0c4(354, 0, 0, fv=FV_8TH, pitch=69)
    e += note_v0c4(474, 0, 0, fv=FV_16TH, pitch=70)
    e += note_v0c4(534, 0, 0, fv=FV_16TH, pitch=68)
    e += note_v0c4(594, 0, 0, fv=FV_8TH, pitch=66)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(3, 4), e)], fill_ts=(3, 4))


# ===========================================================================
# ornaments_measure_repeat_after_pickup.enc
# Four 4/4 bars whose first holds a single quarter, so it becomes a pickup and
# every bar behind it starts a quarter earlier than a full first bar would put
# it.  The last bar carries a repeat-measure sign.  Anything resolved by tick
# after the bars are built reads them through the score's tick map, and while
# that map still describes the lengths the bars had before the pickup shortened
# the first one, it answers with the bar before the one meant: the sign empties
# the wrong bar and lands in the one before that, which is left holding its own
# music plus a whole bar's worth of repeat sign pinned at its barline.
# ===========================================================================
def gen_v0c4_measure_repeat_after_pickup():
    def repeat_meas_orn(tick, staffIdx=0, voice=0):
        d = bytearray(13)
        d[0] = 16
        d[1] = staffIdx & 0x3F
        d[2] = 0xA3       # REPEAT_MEASURE
        return struct.pack('<H', tick) + bytes([(5 << 4) | (voice & 0xF)]) + bytes(d)

    def four_quarters(pitches):
        e = b''
        for i, p in enumerate(pitches):
            e += note_v0c4(i * 240, 0, 0, fv=3, pitch=p)
        return e + end_marker()

    m1 = note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()      # a quarter alone: the pickup
    m2 = four_quarters([62, 64, 65, 67])
    m3 = four_quarters([69, 71, 72, 74])
    m4 = repeat_meas_orn(0) + four_quarters([69, 71, 72, 74])
    return assemble(0xC4, [(meas_hdr(4, 4), m1), (meas_hdr(4, 4), m2),
                           (meas_hdr(4, 4), m3), (meas_hdr(4, 4), m4)], fill_ts=(4, 4))


# ===========================================================================
# notes_bar_that_cannot_be_written.enc
# A 2/4 bar of the kind a hand-played drum track produces: five thirty-seconds
# marked three in the time of two, then two eighth rests marked the same, twice
# over.  It comes to 520 Encore ticks where the bar holds 480, and no notation
# fits it: the run of five occupies 100 ticks, which is 5/48 of a whole, and a
# bracket always spans a plain value times its member count, so the 3 in that
# denominator cannot be written however the numeral is chosen.  What the import
# has to guarantee is not the notation but the bar: a voice that does not add up
# to its signature is a corrupt score whatever the file meant.  Left alone the
# room at the end comes to a third of a beat, which no plain figure measures, so
# the residue used to come out as a run of ever smaller rests, 1/64 then 1/256
# then 1/1024, with the bar still wrong at the end of it.
# ===========================================================================
def gen_v0c4_bar_that_cannot_be_written():
    TUP = 0x32                       # three in the time of two
    FV_32ND, FV_8TH, FV_Q = 6, 4, 3
    pre = bytearray(set_chumagio(0xC4))
    pre[0x32] = 2   # two instruments: the second holds the bar exactly
    pre = bytes(pre)
    e = b''
    for half in (0, 260):
        for k in range(5):
            e += note_v0c4(half + k * 20, 0, 0, fv=FV_32ND, pitch=38, tuplet=TUP)
        e += rest_v0c4_tup(half + 100, 0, 0, fv=FV_8TH, tuplet=TUP)
        e += rest_v0c4_tup(half + 180, 0, 0, fv=FV_8TH, tuplet=TUP)
    # The second staff holds the bar exactly, as the other eleven do in the score this came from.
    # Without one the bar itself is stretched to the single voice's content and the question the
    # fixture asks disappears.
    e += note_v0c4(0, 0, 1, fv=FV_Q, pitch=60)
    e += note_v0c4(240, 0, 1, fv=FV_Q, pitch=62)
    e += end_marker()
    body  = meas_block(meas_hdr(2, 4), e)
    body += b''.join(empty_meas(2, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_nonuplet_missing_marker():
    TUP = 0x98               # nine in the time of eight
    FV_16TH = 5
    ticks = [0, 53, 106, 159, 212, 272, 325, 378, 431]
    elems = b''
    for i, t in enumerate(ticks):
        if i < 4:
            elems += note_v0c4(tick=t, voice=0, staffIdx=0, fv=FV_16TH,
                               pitch=60 + 3 * i, tuplet=TUP)
        else:
            # the fifth member is the one Encore leaves unmarked
            elems += rest_v0c4_tup(tick=t, voice=0, staffIdx=0, fv=FV_16TH,
                                   tuplet=0 if i == 4 else TUP)
    elems += rest_v0c4(tick=484, voice=0, staffIdx=0, fv=3)
    elems += rest_v0c4(tick=724, voice=0, staffIdx=0, fv=3)
    elems += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), elems)], fill_ts=(4, 4))


def gen_v0c4_tuplet_9_4_nontuplet():
    """9 quarter notes in a [9:4] nontuplet bracket filling one 4/4 measure."""
    ticks   = [i * 107 for i in range(9)]
    pitches = [60, 62, 64, 65, 67, 69, 71, 72, 74]
    e = b''.join(note_v0c4(ticks[i], 0, 0, fv=3, pitch=pitches[i], tuplet=0x94)
                 for i in range(9))
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))

def gen_v0c4_tuplet_dosillo_2_1():
    """2 half notes in a [2:1] dosillo bracket filling one 2/4 measure."""
    e  = note_v0c4(  0, 0, 0, fv=2, pitch=60, tuplet=0x21)
    e += note_v0c4(240, 0, 0, fv=2, pitch=64, tuplet=0x21)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))

def gen_v0c4_tuplet_last_note_short_rdur():
    """10 quarter notes in [10:4]; last note has rdur=6 (<15) and must not be dropped."""
    ticks   = [i * 96 for i in range(9)] + [954]
    pitches = [60, 62, 64, 65, 67, 69, 71, 72, 74, 76]
    e = b''.join(note_v0c4(ticks[i], 0, 0, fv=3, pitch=pitches[i], tuplet=0xA4)
                 for i in range(10))
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))

def gen_v0c4_tuplet_no_gapsnap_spurious_rest():
    """4/4: 3 explicit 3:2 triplet quarters + 1 half; gap-snap must not insert spurious rests inside tuplet."""
    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60, tuplet=0x32)
    e += note_v0c4(160, 0, 0, fv=3, pitch=62, tuplet=0x32)
    e += note_v0c4(320, 0, 0, fv=3, pitch=64, tuplet=0x32)
    e += note_v0c4(480, 0, 0, fv=2, pitch=67, tuplet=0)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))

def gen_v0c4_voice_overflow_dropped():
    """5 half notes in voice 0 of a 2/2 measure; only 2 fit, rest dropped not rerouted to voice 2."""
    pitches = [60, 62, 64, 65, 67]
    e = b''.join(note_v0c4(i * 480, 0, 0, fv=2, pitch=pitches[i]) for i in range(5))
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 2), e)], fill_ts=(2, 2))

def gen_v0c4_segment_no_override_clean_multiple():
    """2/4: 6 eighth notes tup=0x32 forming two clean 3:2 groups; segment-override must not fire."""
    ticks   = [i * 80 for i in range(6)]
    pitches = [60, 62, 64, 65, 67, 69]
    e = b''.join(note_v0c4(ticks[i], 0, 0, fv=4, pitch=pitches[i], tuplet=0x32)
                 for i in range(6))
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))

def gen_v0c4_segment_override_12plus2():
    """4/4: 12 eighth notes in [12:6] bracket followed by 2 plain eighth notes."""
    e = b''.join(note_v0c4(i * 60, 0, 0, fv=4, pitch=60 + i, tuplet=0xC6)
                 for i in range(12))
    e += note_v0c4(720, 0, 0, fv=4, pitch=72, tuplet=0)
    e += note_v0c4(840, 0, 0, fv=4, pitch=74, tuplet=0)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))

def gen_v0c4_segment_override_15notes():
    """4/4: 15 eighth notes all in a single [15:8] bracket via segment-override path."""
    ticks   = [i * 64 for i in range(15)]
    pitches = [60 + (i % 12) for i in range(15)]
    e = b''.join(note_v0c4(ticks[i], 0, 0, fv=4, pitch=pitches[i], tuplet=0xF8)
                 for i in range(15))
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))

def gen_v0c4_cross_staff_false_nesting():
    """Two-staff 4/4: staff 0 has QE-3:2 brackets (innerGroupStartIdx=1 trigger), staff 1 has 12 eighth triplets."""
    def make_staff_entry(clef, key, isidx):
        e = bytearray(30)
        e[14] = clef; e[15] = key; e[19] = 1; e[21] = isidx
        return bytes(e)

    def note_2s(tick, voice, raw_staff, fv, pitch, tup=0):
        d = bytearray(25)
        d[0] = 28; d[1] = raw_staff & 0xFF; d[2] = fv; d[10] = tup; d[12] = pitch
        return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)

    hdr = bytearray(194)
    hdr[0:4] = b'SCOW'; hdr[4] = 0xC4
    struct.pack_into('<H', hdr, 0x28, 0x0420)
    struct.pack_into('<H', hdr, 0x2C, 0xF000)
    struct.pack_into('<h', hdr, 0x2E, 1)    # lineCount
    struct.pack_into('<h', hdr, 0x30, 1)    # pageCount
    hdr[0x32] = 1                           # instrumentCount = 1
    hdr[0x33] = 2                           # staffPerSystem = 2
    struct.pack_into('<h', hdr, 0x34, 1)    # measureCount = 1

    line_data = (b'\x00' * 10
                 + struct.pack('<H', 0)
                 + bytes([1])
                 + make_staff_entry(0, 0, 0x00)
                 + make_staff_entry(1, 0, 0x40))
    line_block = b'LINE' + struct.pack('<I', len(line_data)) + line_data

    # Staff 0 (voice=0, raw=0x00): 1 plain Q + {Q,E}*2 3:2 groups + 1 plain Q = 4/4
    # innerGroupStartIdx=1 because there is 1 non-tuplet note before the first group.
    e  = note_2s(  0, 0, 0x00, fv=3, pitch=60, tup=0)     # plain Q: triggers innerGroupStartIdx=1
    e += note_2s(240, 0, 0x00, fv=3, pitch=62, tup=0x32)  # Q  group 1
    e += note_2s(400, 0, 0x00, fv=4, pitch=64, tup=0x32)  # E  group 1
    e += note_2s(480, 0, 0x00, fv=3, pitch=65, tup=0x32)  # Q  group 2
    e += note_2s(640, 0, 0x00, fv=4, pitch=67, tup=0x32)  # E  group 2
    e += note_2s(720, 0, 0x00, fv=3, pitch=69, tup=0)     # plain Q

    # Staff 1 (voice=0, raw=0x40): 12 triplet eighths in 4 groups of 3
    for t in [i * 80 for i in range(12)]:
        e += note_2s(t, 0, 0x40, fv=4, pitch=60, tup=0x32)

    e += end_marker()

    meas = meas_block(meas_hdr(4, 4), e)
    return bytes(hdr) + line_block + meas + SKELETON_POST

    def note_2s(tick, voice, raw_staff, fv, pitch, tup=0):
        d = bytearray(25)
        d[0] = 28; d[1] = raw_staff & 0xFF; d[2] = fv; d[10] = tup; d[12] = pitch
        return struct.pack('<H', tick) + bytes([(9 << 4) | (voice & 0xF)]) + bytes(d)

def gen_v0c4_dotted_ctrl_bit0_drift():
    """2/4: dotted-8th with rdur=163 (17-tick drift) and dotControl bit 0=1, then 16th + 8th + 8th."""
    # Note 0 tick=0:   fv=8th (4), dotControl=0x1D (bit0=1), rdur=163 (drift from clean 180)
    # Note 1 tick=163: fv=16th (5), rdur=60
    # Note 2 tick=223: fv=8th  (4), rdur=120
    # Note 3 tick=343: fv=8th  (4), rdur=137 (480-343; snaps to V_EIGHTH by face value)
    e  = note_v0c4_dotctrl(  0, 0, 0, fv=4, pitch=60, dotControl=0x1D)
    e += note_v0c4(         163, 0, 0, fv=5, pitch=62)
    e += note_v0c4(         223, 0, 0, fv=4, pitch=64)
    e += note_v0c4(         343, 0, 0, fv=4, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))

    def se(clef, isidx):
        e = bytearray(30); e[14] = clef; e[19] = 1; e[21] = isidx; return bytes(e)

    def n0(tick, pitch, fv=3):
        d = bytearray(25); d[0]=28; d[1]=0x00; d[2]=fv; d[12]=pitch
        return struct.pack('<H', tick) + bytes([0x90]) + bytes(d)

    def nbass(tick, pitch, fv=3):
        d = bytearray(25); d[0]=28; d[1]=0x00; d[2]=fv; d[12]=pitch
        return struct.pack('<H', tick) + bytes([0x94]) + bytes(d)

    def n0artic(tick, pitch, au=0, fv=3):
        d = bytearray(25); d[0]=28; d[1]=0x00; d[2]=fv; d[12]=pitch; d[21]=au
        return struct.pack('<H', tick) + bytes([0x90]) + bytes(d)

    def forn(tick, finger):
        return orn16_v0c4(tick, 0, 0, 0xB8 + finger)

    def simple_e():
        return n0(0,60)+n0(240,62)+n0(480,64)+n0(720,65)+end_marker()

def gen_v0c4_tremolo_orn_crossvoice():
    """TREMOLO_32 ORN in voice=0 attaches to the note in voice=1 on the same staff."""
    e  = note_v0c4(0, 1, 0, fv=3, pitch=60)
    e += ornament_v0c4(0, 0, 0, tipo=0xAF)
    e += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e += note_v0c4(480, 0, 0, fv=3, pitch=64)
    e += note_v0c4(720, 0, 0, fv=3, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)])

def gen_v0c4_tuplet_mixed_baseLen():
    """4/4: plain Q + {Q,E} 3:2 bracket + {Q,Q,Q} 3:2 bracket produce exactly 2 distinct tuplet groups."""
    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60, tuplet=0x00)
    e += note_v0c4(240, 0, 0, fv=3, pitch=62, tuplet=0x32)
    e += note_v0c4(400, 0, 0, fv=4, pitch=64, tuplet=0x32)
    e += note_v0c4(480, 0, 0, fv=3, pitch=65, tuplet=0x32)
    e += note_v0c4(640, 0, 0, fv=3, pitch=67, tuplet=0x32)
    e += note_v0c4(800, 0, 0, fv=3, pitch=69, tuplet=0x32)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)])


def gen_v0c4_instr_abbreviated_name_bandurr():
    name = 'Bandurr. I'.encode('utf-16-le') + b'\x00\x00'
    pre = _patch_tk00(name)
    e = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_instr_compact_no_tk_midi_oboe():
    # TK00 varsize=0 → compact layout; MIDI read from CMP_BASE=390
    pre = bytearray(SKELETON_PRE)
    struct.pack_into('<I', pre, 194 + 4, 0)   # TK00 varsize = 0
    pre[390] = 69                              # MIDI = 69 (oboe, 1-indexed)
    e = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return bytes(pre) + body + SKELETON_POST


def gen_v0c4_instr_empty_name_midi_cello():
    # Skeleton has a 3-char default name → nameTooShort; MIDI=43 resolves to violoncello
    pre = _patch_midi_program(SKELETON_PRE, 0, 43)   # 1-indexed cello
    e = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_instr_no_tk_large_tk_two_names():
    # No TK blocks, instrumentCount=2, "Oboe" at 202, "Cello" at 2360 (=202+2158)
    pre = bytearray(SKELETON_PRE)
    pre[194:198] = b'\x00\x00\x00\x00'   # zero TK magic
    pre[0x32] = 2                          # instrumentCount=2
    name0 = 'Oboe'.encode('utf-16-le') + b'\x00\x00'
    pre[202:202 + len(name0)] = name0
    name1 = 'Cello'.encode('utf-16-le') + b'\x00\x00'
    while len(pre) < 2360 + len(name1):
        pre.extend(b'\x00' * 64)
    pre[2360:2360 + len(name1)] = name1
    e = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return bytes(pre) + body + SKELETON_POST


def gen_v0c4_instr_rhythm_staff_snare():
    # LINE staff entry byte[20] = 2 (EncStaffType::RHYTHM) → routes to snare-drum template
    pre = bytearray(SKELETON_PRE)
    line_pos = bytes(pre).find(b'LINE')
    pre[line_pos + 8 + 13 + 20] = 2   # entry[20] = staffType = RHYTHM
    e = end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return bytes(pre) + body + SKELETON_POST


def gen_v0c4_instr_perc_bateria():
    # 36 measures, percussion clef (EncClefType::PERC=7)
    data = assemble(0xC4, [(meas_hdr(4, 4), end_marker())] * 36, fill_ts=(4, 4))
    return set_staff_clef(data, staff_idx=0, clef=7)


def gen_v0c4_instr_transp_oboe_jota():
    # Oboe MIDI=69, keyTransposeSemitones=5 (chromatic=5, diatonic=3)
    name = 'Oboe'.encode('utf-16-le') + b'\x00\x00'
    pre  = _patch_tk00(name)
    pre  = _patch_midi_program(pre, 0, 69)
    pre  = _patch_key_transpose(pre, 0, 5)
    e    = note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_overfill_irregular_facevalue():
    # 4/4: Q at 0, H at 240, H at 720 (third note crosses barline at tick 960)
    e  = note_v0c4(  0, 0, 0, fv=3, pitch=60)   # quarter
    e += note_v0c4(240, 0, 0, fv=2, pitch=62)   # half
    e += note_v0c4(720, 0, 0, fv=2, pitch=64)   # half (crosses barline)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_overfill_irregular_emitdrop():
    # 4/4: Q+DH+Q+Q (Q+DH fills 4/4 exactly; last two notes are past the barline)
    e  = note_v0c4(    0, 0, 0, fv=3, pitch=60)
    e += note_v0c4_dotctrl(240, 0, 0, fv=2, pitch=62, dotControl=1)   # dotted half
    e += note_v0c4(  960, 0, 0, fv=3, pitch=64)
    e += note_v0c4( 1200, 0, 0, fv=3, pitch=65)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_overfill_irregular_twostaves():
    # 2 instruments: staff0 Q+DH+Q+Q (same overfill), staff1 Q+Q+Q (3/4 fill)
    pre = bytearray(SKELETON_PRE)
    pre[0x32] = 2
    pre = _patch_instrument_name(bytes(pre), 1, 'Staff2')
    e  = note_v0c4(    0, 0, 0, fv=3, pitch=60)
    e += note_v0c4_dotctrl(240, 0, 0, fv=2, pitch=62, dotControl=1)
    e += note_v0c4(  960, 0, 0, fv=3, pitch=64)
    e += note_v0c4( 1200, 0, 0, fv=3, pitch=65)
    e += note_v0c4(    0, 0, 1, fv=3, pitch=60)
    e += note_v0c4(  240, 0, 1, fv=3, pitch=62)
    e += note_v0c4(  480, 0, 1, fv=3, pitch=64)
    e += end_marker()
    body  = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_underfill_irregular_sparse_with_empty_staff():
    # 2 instruments. Bar 1 (mid-score, 4/4 nominal) has a single quarter note on staff0 (1/4 of
    # content) while staff1 is EMPTY (a silent instrument = a whole-bar rest = the full 4/4). The
    # longest staff is the full bar, so IrregularMeasure underfill must NOT shrink bar 1: it stays
    # 4/4 with staff0 padded by a 3/4 rest and staff1 a whole-bar rest. The bug measured only the
    # note-bearing staff (1/4), shrank the bar, shifted every following measure and corrupted them.
    # Bar 0 leads (so the sparse bar is not the pickup); bars 2-3 are full and must stay intact.
    pre = bytearray(SKELETON_PRE)
    pre[0x32] = 2
    pre = _patch_instrument_name(bytes(pre), 1, 'Staff2')

    def full():
        e = b''
        for t in (0, 240, 480, 720):
            e += note_v0c4(t, 0, 0, fv=3, pitch=60)
            e += note_v0c4(t, 0, 1, fv=3, pitch=55)
        return e + end_marker()

    sparse  = note_v0c4(0, 0, 0, fv=3, pitch=60)   # staff0: one quarter; staff1: nothing
    sparse += end_marker()

    body  = meas_block(meas_hdr(4, 4), full())     # bar 0 (full, not pickup)
    body += meas_block(meas_hdr(4, 4), sparse)     # bar 1 (sparse staff0 + silent staff1)
    body += meas_block(meas_hdr(4, 4), full())     # bar 2
    body += meas_block(meas_hdr(4, 4), full())     # bar 3
    return pre + body + SKELETON_POST


def _dotted_hint_measures(note, orn):
    """Two 3/4 bars that differ only in the dot count stated by their first note.

    Both hold an eighth at 0, a sixteenth at 120 and a half at 180, which is what
    Encore writes for a DOTTED eighth: the note-on positions carry no dot, so the
    face values sum to 660 against a bar of 720. Bar 0 is the control, its layout
    byte 0x1c states zero dots and the music really is an undotted eighth. Bar 1
    carries 0x1d, which states one, the value real files show on a dotted eighth,
    and must import as one, which fills the bar exactly.
    """
    # The accent rides on the last note, anchored by the tick that note is stored at. Restoring the
    # dot moves that note later, so the anchor has to move with it or the accent lands on the
    # sixteenth. Real files show exactly this pair.
    control  = note(0,   0, 0, 4, 60, layout=0x1c)
    control += note(120, 0, 0, 5, 62)
    control += note(180, 0, 0, 2, 64)
    control += orn(180, 0, 0, tipo=0xBE, xoffset=45)
    control += end_marker()

    dotted  = note(0,   0, 0, 4, 60, layout=0x1d)
    dotted += note(120, 0, 0, 5, 62)
    dotted += note(180, 0, 0, 2, 64)
    dotted += orn(180, 0, 0, tipo=0xBE, xoffset=45)
    dotted += end_marker()
    return [(meas_hdr(3, 4), control), (meas_hdr(3, 4), dotted)]


def gen_v0c4_last_note_drawn_longer_than_its_space():
    """A 3/4 bar whose last note is drawn longer than the space left, which Encore allows.

    Seven notes fill 600 of the bar's 720 ticks, then a quarter at 600 states one dot in its layout
    byte, so Encore draws 360 where 120 remain and clips the playback to the 120. What the importer
    does with it is the user's choice: the expand-measure strategy keeps the figure and grows the
    bar, the others fit it to the space.
    """
    lead  = note_v0c4(0,   0, 0, fv=3, pitch=60)
    lead += note_v0c4(240, 0, 0, fv=3, pitch=62)
    lead += note_v0c4(480, 0, 0, fv=3, pitch=64)
    lead += end_marker()

    e  = note_v0c4(0,   0, 0, fv=4, pitch=60)
    e += note_v0c4(120, 0, 0, fv=5, pitch=62)
    e += note_v0c4(180, 0, 0, fv=5, pitch=64)
    e += note_v0c4(240, 0, 0, fv=4, pitch=65)
    e += note_v0c4(360, 0, 0, fv=5, pitch=67)
    e += note_v0c4(420, 0, 0, fv=5, pitch=69)
    e += note_v0c4(480, 0, 0, fv=4, pitch=71)
    e += note_v0c4(600, 0, 0, fv=3, pitch=72, layout=0x1d)   # quarter stating one dot
    e += end_marker()
    # A full bar leads, so the one under test is never read as a pickup.
    return assemble(0xC4, [(meas_hdr(3, 4), lead), (meas_hdr(3, 4), e)], fill_ts=(3, 4))


def gen_v0c4_accent_at_note_end_tick():
    """An accent stored at the tick where its note ENDS, which is where Encore puts it when the
    note is the last of the bar.

    Bar 0 is 3/4 with five eighths, so the last one runs from 480 to 600 and nothing starts at 600.
    The accent sits at 600 with an xoffset just past that note's, which is the only thing that says
    which note it belongs to. Bar 1 holds one quarter and must stay clean: the bug sent the accent
    forward to it, taking it out of bar 0 and into the next measure.
    """
    NOTE_XOFFS = [11, 31, 51, 71, 91]
    first = b''
    for i, xo in enumerate(NOTE_XOFFS):
        first += note_v0c4_xoff(tick=i * 120, voice=0, staffIdx=0, fv=4, pitch=60 + i, xoff=xo)
    first += ornament_v0c4(600, 0, 0, tipo=0xBE, xoffset=99)
    first += end_marker()

    second = note_v0c4_xoff(tick=0, voice=0, staffIdx=0, fv=3, pitch=72, xoff=12)
    second += end_marker()
    return assemble(0xC4, [(meas_hdr(3, 4), first), (meas_hdr(3, 4), second)], fill_ts=(3, 4))


def gen_v0c4_dotted_hint_fills_bar():
    return assemble(0xC4, _dotted_hint_measures(note_v0c4, ornament_v0c4), fill_ts=(3, 4))


def gen_v0c2_dotted_hint_fills_bar():
    # Written with layout=False: apply_encore_layout assumes the 4.20 body, where +12 is the staff
    # position and +13 the tuplet slot. On a 3.05 note those two bytes are the dot count and the
    # pitch, so the pass would overwrite both and the fixture would lose what it is testing.
    return set_version(assemble(0xC2, _dotted_hint_measures(note_v0c2, ornament_v0c4), fill_ts=(3, 4)),
                       ENC_FORMAT_3_05)


def _clef_elem(tick, voice, staffIdx, clef_type):
    # CLEF change element, size=16. Encore renders the clef from this 16-byte
    # form (a 6-byte one is read by the importer but never drawn). Layout:
    # tick(2) + typeVoice(1) + size(1) + rawStaff(1) + clefType(1) + 4 zero +
    # xoffset(2 @+10, filled by the layout pass) + 0x02 @+12 + trailer.
    d = bytearray(13)
    d[0] = 16
    d[1] = staffIdx & 0x3F
    d[2] = clef_type & 0xFF
    d[9] = 0x02                  # +12: constant marker seen in real clef changes
    return struct.pack('<H', tick) + bytes([(1 << 4) | (voice & 0xF)]) + bytes(d)


def gen_v0c4_structure_clef_change_mid_measure():
    # 2/4 measure of eight 16th notes. A CLEF(C4L=3) is serialized in the stream AFTER the
    # beat-1 notes and BEFORE the beat-2 note, but carries an earlier stored tick (180). The
    # importer must anchor the clef to the NOTE that physically follows it in the stream (the
    # beat-2 note at tick 240 = Fraction(1,4)), NOT to its own stored tick: Encore draws a
    # clef in front of the note it precedes regardless of the tick it carries.
    e  = note_v0c4(  0, 0, 0, fv=5, pitch=72)
    e += note_v0c4( 60, 0, 0, fv=5, pitch=71)
    e += note_v0c4(120, 0, 0, fv=5, pitch=69)
    e += note_v0c4(180, 0, 0, fv=5, pitch=67)
    e += _clef_elem(180, 0, 0, clef_type=3)       # C4L clef, stored tick 180, before the beat-2 note
    e += note_v0c4(240, 0, 0, fv=5, pitch=65)     # beat 2 (Fraction 1/4): clef anchors here
    e += note_v0c4(300, 0, 0, fv=5, pitch=64)
    e += note_v0c4(360, 0, 0, fv=5, pitch=62)
    e += note_v0c4(420, 0, 0, fv=5, pitch=60)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e)], fill_ts=(2, 4))


def gen_v0c4_structure_clef_trailing_cautionary():
    # Measure 1 (2/4) is filled by eight 16th notes, then a CLEF(F=1) is the LAST stream
    # element (no note/rest follows it). Such a trailing/cautionary clef takes effect on the
    # downbeat of measure 2, not before measure 1's final note.
    e0 = b''
    for i, t in enumerate([0, 60, 120, 180, 240, 300, 360, 420]):
        e0 += note_v0c4(t, 0, 0, fv=5, pitch=72 - i)
    e0 += _clef_elem(420, 0, 0, clef_type=1)      # trailing F clef, last element of measure 1
    e0 += end_marker()
    e1 = b''
    for i, t in enumerate([0, 60, 120, 180, 240, 300, 360, 420]):
        e1 += note_v0c4(t, 0, 0, fv=5, pitch=53 + i)
    e1 += end_marker()
    return assemble(0xC4, [(meas_hdr(2, 4), e0), (meas_hdr(2, 4), e1)], fill_ts=(2, 4))


def gen_v0c4_structure_page_break():
    # 2 LINE blocks, both pageIdx=0 → page break between system 0 and system 1
    data = assemble(0xC4, [(meas_hdr(4, 4), end_marker())] * 6, fill_ts=(4, 4))
    d = bytearray(data)
    line_count = 0
    pos = 0
    while pos < len(d):
        if d[pos:pos+4] == b'LINE':
            size = int.from_bytes(d[pos+4:pos+8], 'little')
            if line_count == 1:
                d[pos + 8 + 13 + 16] = 0   # second LINE pageIdx = 0 (was 1)
            line_count += 1
            pos += 8 + size
        else:
            pos += 1
    return bytes(d)


def gen_v0c4_structure_page_break_mcount_zero():
    # Same 6-measure / 2-system layout as structure_page_break, but every LINE
    # measureCount byte is zeroed to mimic SCO5 (big-endian Encore 5), which does
    # not surface the per-line measure count. The page break must still be placed
    # at the end of system 0: the line span is recovered from the start deltas.
    data = bytearray(gen_v0c4_structure_page_break())
    pos = 0
    while True:
        p = data.find(b'LINE', pos)
        if p < 0:
            break
        data[p + 8 + 12] = 0   # content +12 = measureCount byte
        pos = p + 4
    return bytes(data)


def gen_v0c4_page_break_spill():
    """6-staff, 3-system score whose first two systems (LINE pageIdx 0 then 1) belong on the
    first page, followed by a page break (line 2 resets pageIdx to 0). On a short custom page
    (145 mm tall) the two systems do not quite fit at the default staff space, so the second
    spills onto the next page. The importer must shrink the staff space (up to 0.022 inch) until
    the first page break's measure returns to the first page. Verified: the spill is recovered
    with a 0.006-inch reduction, comfortably inside the 0.022-inch budget."""
    NSTAVES, NSYS, MPS = 6, 3, 3
    total_meas = NSYS * MPS
    pageidx = (0, 1, 0)

    hdr = bytearray(194)
    hdr[0:4] = b'SCOW'
    hdr[4] = 0xC4
    struct.pack_into('<H', hdr, 0x28, 0x0420)
    struct.pack_into('<h', hdr, 0x2E, NSYS)          # lineCount
    struct.pack_into('<h', hdr, 0x30, 2)             # pageCount
    hdr[0x32] = NSTAVES                              # instrumentCount
    hdr[0x33] = NSTAVES                              # staffPerSystem
    struct.pack_into('<h', hdr, 0x34, total_meas)    # measureCount

    def staff_entry(idx, pidx):
        e = bytearray(30)
        e[16] = pidx    # EncLineStaffData.pageIdx (row-on-page counter)
        e[19] = 1       # visible
        e[21] = idx     # instrument-staff index
        return bytes(e)

    lines = b''
    for si in range(NSYS):
        ld = b'\x00' * 10 + struct.pack('<H', si * MPS) + bytes([MPS])
        for st in range(NSTAVES):
            ld += staff_entry(st, pageidx[si])
        lines += b'LINE' + struct.pack('<I', len(ld)) + ld

    meas = b''
    for _ in range(total_meas):
        elems = b''.join(note_v0c4(0, 0, st, 3, 60) for st in range(NSTAVES)) + end_marker()
        meas += meas_block(meas_hdr(4, 4), elems)

    # Short custom page (145 mm tall) via the PREC DEVMODE: dmPaperSize=0 (custom), with
    # dmPaperLength/Width in tenths of a millimetre.
    post = bytearray(SKELETON_POST)
    o = post.find(b'PREC')
    base = o + 8 + 64
    struct.pack_into('<h', post, base + 12, 1)       # orientation portrait
    struct.pack_into('<h', post, base + 14, 0)       # dmPaperSize = custom
    struct.pack_into('<h', post, base + 16, 1450)    # dmPaperLength = 145.0 mm
    struct.pack_into('<h', post, base + 18, 2100)    # dmPaperWidth  = 210.0 mm
    struct.pack_into('<h', post, base + 20, 100)     # scale
    return bytes(hdr) + lines + meas + bytes(post)


def gen_v0c4_structure_pickup_casea_volta():
    # Pickup measure (Case A, timeSig=2/4 filling 3/8), then measures with volta "1." and "2."
    e0  = note_v0c4(  0, 0, 0, fv=4, pitch=60)   # eighth
    e0 += note_v0c4(120, 0, 0, fv=4, pitch=62)   # eighth; cumTick=3/8 < 2/4
    e0 += end_marker()
    e1  = note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e1 += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e1 += note_v0c4(480, 0, 0, fv=3, pitch=64)
    e1 += note_v0c4(720, 0, 0, fv=3, pitch=65)
    e1 += end_marker()
    e2  = note_v0c4(  0, 0, 0, fv=3, pitch=60)
    e2 += end_marker()
    h1 = bytearray(meas_hdr(4, 4)); h1[0x0F] = 0x01   # repeatAlternative = 1 → volta "1."
    h2 = bytearray(meas_hdr(4, 4)); h2[0x0F] = 0x02   # repeatAlternative = 2 → volta "2."
    return assemble(0xC4, [
        (meas_hdr(2, 4), e0),
        (bytes(h1), e1),
        (bytes(h2), e2),
    ], fill_ts=(4, 4))


def gen_v0c4_lyrics_6_8_offset_ticks():
    # 4/4 (durTicks=960), 4 quarter notes; lyric ticks +50 from note ticks
    # matchThreshold = 240/2 = 120 → 50 < 120 → each lyric matches its note
    e = b''
    for note_tick, syl, pitch in [(0,'do',60),(240,'re',62),(480,'mi',64),(720,'fa',65)]:
        e += lyric_v0c4(note_tick + 50, 0, 0, syl)
        e += note_v0c4(note_tick, 0, 0, fv=3, pitch=pitch)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


def gen_v0c4_orn_tempo_eighth_beat_not_suppressed():
    # 5/8, MEAS header BPM=160, ORN TEMPO tipo=0x32 at tick=0 with tempo=63.
    # Not suppressed: next measure BPM≠63, so ORN is genuine.
    orn = bytearray(ornament_v0c4(0, 0, 0, tipo=0x32))
    orn[30] = 63   # quarter BPM = 63
    e   = bytes(orn)
    e  += note_v0c4(0, 0, 0, fv=3, pitch=60)
    e  += end_marker()
    pre  = set_chumagio(0xC4)
    body = meas_block(meas_hdr(5, 8, bpm=160), e)
    body += b''.join(meas_block(meas_hdr(5, 8, bpm=0), end_marker()) for _ in range(5))
    return pre + body + SKELETON_POST


def gen_v0c4_tempo_orn_compound_68():
    # 6/8 compound meter, ORN TEMPO=80 at beat 1.
    # 8 LINE blocks (measureCount=3 each) covering 24 total measures. No WINI block.
    orn = bytearray(ornament_v0c4(0, 0, 0, tipo=0x32))
    orn[30] = 80   # dotted-quarter BPM = 80 → BPS = 80*1.5/60 = 2.0
    e0   = bytes(orn) + note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker()
    empty68 = end_marker()

    # Build header (first 2386 bytes of SKELETON_PRE = header+TK, no LINE blocks)
    pre_arr = bytearray(SKELETON_PRE[:2386])
    struct.pack_into('<h', pre_arr, 0x34, 24)   # measureCount = 24

    # Build 8 LINE blocks from the SKELETON_PRE LINE template, modifying
    # start, measureCount, and pageIdx for each.
    line_template = bytearray(SKELETON_PRE[2386:2386 + 64])   # 8-byte header + 56-byte content
    line_blocks = b''
    for i in range(8):
        blk = bytearray(line_template)
        struct.pack_into('<H', blk, 8 + 10, i * 3)  # content[10..11] = start
        blk[8 + 12] = 3                              # content[12] = measureCount = 3
        blk[8 + 13 + 16] = i                         # entry[16] = pageIdx = i (no page breaks)
        line_blocks += bytes(blk)

    # 24 MEAS blocks (first in 6/8, rest empty)
    meas_blocks  = meas_block(meas_hdr(6, 8, bpm=100), e0)
    meas_blocks += b''.join(meas_block(meas_hdr(6, 8, bpm=0), empty68) for _ in range(23))

    # SKELETON_POST without the WINI block (WINI at offset 22964)
    post_no_wini = SKELETON_POST[:22964]

    return bytes(pre_arr) + line_blocks + meas_blocks + post_no_wini


def gen_v0c4_ottava_two_spanners():
    # m0: 8va ORN (tipo=0x10) at tick=0 + 4 quarter notes; fills 4/4 so
    # adjustPickupMeasure does not fire and score->tick2measure() stays accurate.
    # m1: 8vb ORN (tipo=0x12) at tick=0 + 4 quarter notes.
    # resolveOttavas endpoint: next ottava on same staff for 8va (= m1 startTick = 1/1),
    # scoreEnd for 8vb (= 6/1 for 6 measures of 4/4).
    m0  = orn16_v0c4(  0, 0, 0, tipo=0x10)  # OTTAVA_ALTA (8va)
    m0 += note_v0c4(   0, 0, 0, fv=3, pitch=72)
    m0 += note_v0c4( 240, 0, 0, fv=3, pitch=74)
    m0 += note_v0c4( 480, 0, 0, fv=3, pitch=76)
    m0 += note_v0c4( 720, 0, 0, fv=3, pitch=77)
    m0 += end_marker()
    m1  = orn16_v0c4(  0, 0, 0, tipo=0x12)  # OTTAVA_BASSA (8vb)
    m1 += note_v0c4(   0, 0, 0, fv=3, pitch=60)
    m1 += note_v0c4( 240, 0, 0, fv=3, pitch=62)
    m1 += note_v0c4( 480, 0, 0, fv=3, pitch=64)
    m1 += note_v0c4( 720, 0, 0, fv=3, pitch=65)
    m1 += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), m0), (meas_hdr(4, 4), m1)], fill_ts=(4, 4))


def gen_v0c4_titl_empty_second_block():
    # First TITL block: title='Multi TITL First', author='Real Author'.
    # Second TITL block: all strings empty. The importer must keep first block's data.
    post = bytearray(SKELETON_POST)
    idx = post.find(b'TITL')
    assert idx >= 0
    content1 = _titl_content(title='Multi TITL First', author0='Real Author')
    post[idx + 8: idx + 8 + len(content1)] = content1
    e    = end_marker()
    pre  = set_chumagio(0xC4)
    body = meas_block(meas_hdr(4, 4), e)
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    content2 = _titl_content()
    empty_titl = b'TITL' + struct.pack('<I', len(content2)) + content2
    return pre + body + bytes(post) + empty_titl




# ===========================================================================
# Encore-renderability layer (ported): display-layout pass + geometry-bearing
# element builders. The MuseScore importer ignores these fields, so generated
# fixtures import identically while opening correctly in Encore 5.
# ===========================================================================


# ---------------------------------------------------------------------------
# Encore on-disk layout pass (v0xC4)
#
# The MuseScore importer recomputes note positions from tick + face value, so
# it ignores the on-disk layout fields.  Real Encore 5 instead renders straight
# from them, so synthetic files that leave them zero collapse every note onto
# the first tick.  This pass fills the layout fields the way Encore writes them
# (validated by cutting + pasting notes in Encore and re-reading its output):
#
#   * note/rest x-position (+10): linear in tick, xpos = round(12 + tick*0.153)
#   * measure width (@0x10):      xpos(durTicks); the last note then keeps its
#                                 own duration as trailing space before the bar
#   * note diatonic staff position (+12): clef-relative diatonic step from MIDI
#   * accidental glyph (+21):     1 sharp / 2 flat (black keys, sharp spelling)
#   * dot control (+14):          0x1d (even position) / 0x19 (odd) for 1 dot
#
# Every field is filled only when it is still zero, so fixtures that set an
# explicit xoffset / position / dotControl for an importer regression keep their
# value.  The importer ignores all of these (xoffset feeds only spanner-anchor
# heuristics, dotControl falls back to tick-gap snapping), so the pass is a
# pure display addition.
# ---------------------------------------------------------------------------
ENC_XPOS_SLOPE = 0.153
ENC_XPOS_BASE  = 12
# Trailing pixels after the last element when a measure carries a legacy
# beatTicks (one that does not satisfy beatTicks*timeSigDen == 960, e.g. 240 in
# a 2/2 bar). Encore lays such a bar on a short beat grid, so it needs extra
# width or it collapses every note onto the first position. Validated against a
# 2/2 beatTicks=240 bar: last xpos 141 + 63 = 204 renders correctly.
_LEGACY_BEATTICKS_TRAIL = 63
# A grace note shares its main note's tick, so the tick-based xpos would stack
# it on top of the main note. Encore draws a grace just to the left, so offset
# it. Grace notes carry bit 0x20 in grace1 (+6); the common normal-note flags
# 0x10/0x40/0x50 do not, so the test isolates real graces.
_GRACE_XPOS_OFFSET = 10

# clef byte -> absolute-diatonic-index of staff position 0.  G=C4, F=E2,
# G8M=C3 (octave down), G8P=C5 (octave up).  PERC(7)/TAB(8)/other: no pitch
# mapping, skip.
_CLEF_ORIGIN = {0: 28, 1: 16, 4: 35, 5: 21}

def _enc_xpos(tick):
    return round(ENC_XPOS_BASE + tick * ENC_XPOS_SLOPE)

def _rd16(b, off):
    """Read a little-endian uint16 at b[off]."""
    return b[off] | (b[off + 1] << 8)

def _wr16(b, off, val):
    """Write a little-endian uint16 to b[off]."""
    b[off] = val & 0xFF
    b[off + 1] = (val >> 8) & 0xFF

def _face_ticks(fv):
    return {1: 960, 2: 480, 3: 240, 4: 120, 5: 60, 6: 30, 7: 15}.get(fv & 0x0F, 0)

def _dots_from_gap(gap, fv):
    base = _face_ticks(fv)
    if base <= 0 or gap <= 0:
        return 0
    if abs(gap - base) <= 1:
        return 0
    if abs(gap - base * 3 // 2) <= 1:
        return 1
    if (base * 7) % 4 == 0 and abs(gap - base * 7 // 4) <= 1:
        return 2
    if (base * 15) % 8 == 0 and abs(gap - base * 15 // 8) <= 1:
        return 3
    return 0

_NAT_SEMI    = [0, 2, 4, 5, 7, 9, 11]   # semitone of natural letters C D E F G A B
_SHARP_ORDER = [3, 0, 4, 1, 5, 2, 6]     # letters that gain a sharp as fifths rise
_FLAT_ORDER  = [6, 2, 5, 1, 4, 0, 3]     # letters that gain a flat as fifths fall
# Encore key byte -> fifths (mirrors the importer's encKeyToFifths table).
_KEY_FIFTHS  = [0, -1, -2, -3, -4, -5, -6, -7, 1, 2, 3, 4, 5, 6, 7]

def _key_alt(fifths):
    alt = [0] * 7
    if fifths > 0:
        for j in range(min(fifths, 7)):
            alt[_SHARP_ORDER[j]] = 1
    elif fifths < 0:
        for j in range(min(-fifths, 7)):
            alt[_FLAT_ORDER[j]] = -1
    return alt

def _spell(midi, fifths, origin):
    """Spell a MIDI pitch in the given key. Returns (staff position byte,
    letter 0-6, actualAlteration) where actualAlteration is the note's true
    semitone offset from its natural letter (-1 flat, 0 natural, +1 sharp).
    The displayed accidental is decided by the caller, which also tracks
    accidentals already in effect in the measure."""
    pc = midi % 12
    alt = _key_alt(fifths)
    letter = None
    actual = 0
    for L in range(7):                       # in-key spelling
        if (_NAT_SEMI[L] + alt[L]) % 12 == pc:
            letter, actual = L, alt[L]
            break
    if letter is None:                       # natural letter
        for L in range(7):
            if _NAT_SEMI[L] % 12 == pc:
                letter, actual = L, 0
                break
    if letter is None:                       # raised (sharp)
        for L in range(7):
            if (_NAT_SEMI[L] + 1) % 12 == pc:
                letter, actual = L, 1
                break
    if letter is None:                       # lowered (flat)
        for L in range(7):
            if (_NAT_SEMI[L] - 1) % 12 == pc:
                letter, actual = L, -1
                break
    octave = (midi - actual) // 12 - 1
    return (octave * 7 + letter - origin) & 0xFF, letter, actual

# actual alteration (-2..+2) -> accidental glyph byte (1 sharp, 2 flat, 3 natural)
_ALT_GLYPH = {1: 1, -1: 2, 0: 3, 2: 4, -2: 5}

def _staff_clef_map(data):
    """Map a note's rawStaff byte to its clef, read from the first LINE block.

    Each 30-byte LINE staff entry carries its clef at byte +14, its key
    signature at +15 and its instrStaffIdx at +21; a note's rawStaff (staffIdx
    | staffWithin) equals the instrStaffIdx of the staff it belongs to (0x00
    treble, 0x40 bass for a piano grand staff).  Returns
    (clef_by_raw, fallback_clef, key_by_raw, fallback_key); the fallbacks are
    the first entry's values for any unmatched rawStaff (the single-staff case)."""
    p = data.find(b'LINE')
    if p < 0:
        return {}, 0, {}, 0
    count = data[0x33] if 0x33 < len(data) else 1
    base = p + 8 + 13                 # 13-byte LINE header before staff entries
    clef_by_raw = {}
    key_by_raw = {}
    fallback_clef = 0
    fallback_key = 0
    for s in range(max(count, 1)):
        entry = base + s * 30
        if entry + 21 >= len(data):
            break
        instr_staff = data[entry + 21]
        clef_by_raw[instr_staff] = data[entry + 14]
        key_by_raw[instr_staff] = data[entry + 15]
        if s == 0:
            fallback_clef = data[entry + 14]
            fallback_key = data[entry + 15]
    return clef_by_raw, fallback_clef, key_by_raw, fallback_key

def _fill_volta_hooks(out, meas_hdrs):
    """Draw volta brackets from the ending-number bitmask at header +0x0F.
    Encore only draws the bracket when the hook byte at +0x0E is set; many
    fixtures set just +0x0F (what the importer reads). Coalesce consecutive
    measures that share a bitmask into one bracket: a lone ending gets both
    hooks (0xC0); a run gets a left hook (0x80) on the first measure and a
    right hook (0x40) on the last, none in between. Hooks already set are kept."""
    k = 0
    while k < len(meas_hdrs):
        ending = out[meas_hdrs[k] + 0x0F]
        if ending == 0:
            k += 1
            continue
        j = k
        while j + 1 < len(meas_hdrs) and out[meas_hdrs[j + 1] + 0x0F] == ending:
            j += 1
        for idx in range(k, j + 1):
            h = meas_hdrs[idx]
            if out[h + 0x0E] == 0:
                out[h + 0x0E] = 0xC0 if k == j else (0x80 if idx == k else
                                                     (0x40 if idx == j else 0))
        k = j + 1

def _measure_headers(out):
    """Header offsets (start of the 0x36-byte header) of every MEAS block."""
    hdrs = []
    p = 0
    while True:
        mm = out.find(b'MEAS', p)
        if mm < 0:
            break
        sz = struct.unpack('<I', out[mm + 4:mm + 8])[0]
        hdrs.append(mm + 8)
        p = mm + 8 + 0x36 + sz
    return hdrs

def _apply_dots_and_stems(out, groups, dur_ticks):
    """Per (rawStaff, voice): set dots, stem direction and playback fields.
    Encore stores stem direction in options (+20 bit 0x80); it is not derived.
    Notes sharing a tick form a chord and must share ONE stem direction (chosen
    by the notehead furthest from the middle line), else each chord note draws
    its own opposite stem. A secondary voice always stems down; a single voice
    stems down when the deciding note is on the middle line (position 6) or
    above, up below it. Notes are also made audible (see the +18 note)."""
    staff_voices = {}
    for (rs, vc) in groups:
        staff_voices.setdefault(rs, set()).add(vc)
    for (rs, vc), evts in groups.items():
        multi = len(staff_voices.get(rs, ())) > 1
        onsets = sorted(set(t for (t, _, _, _) in evts))
        next_onset = {t: (onsets[k + 1] if k + 1 < len(onsets) else dur_ticks)
                      for k, t in enumerate(onsets)}
        chords = {}
        for (tick, off, fv, etyp) in evts:
            chords.setdefault(tick, []).append((off, fv, etyp))
        for tick, chord in chords.items():
            gap = next_onset[tick] - tick
            note_pos = [out[off + 12] for (off, _, et) in chord if et == 9]
            furthest = max(note_pos, key=lambda p: abs(p - 6)) if note_pos else 6
            down = (vc >= 1) or (not multi and furthest >= 6)
            # playback length ~ 80% of the note's sounding duration, the gate
            # time Encore records (eighth 120 -> 96, half 480 -> 384).
            pbdur = max(1, round(gap * 0.8))
            for (off, fv, et) in chord:
                # dots apply to notes and rests alike (parity by position)
                if _dots_from_gap(gap, fv) > 0 and out[off + 14] == 0:
                    out[off + 14] = 0x1d if out[off + 12] % 2 == 0 else 0x19
                if et != 9:
                    continue
                if down and out[off + 3] >= 21 and not (out[off + 20] & 0x80):
                    out[off + 20] |= 0x80
                # Make the note sound in Encore. Playback uses the MIDI
                # on-velocity at +18 (NOT the editor velocity at +19); left at 0
                # the note is silent even though +19 is set. Also set duration.
                if out[off + 3] >= 19 and out[off + 18] == 0:
                    out[off + 18] = 0x50            # MIDI on-velocity (80)
                if out[off + 3] >= 20 and out[off + 19] == 0:
                    out[off + 19] = 64              # editor velocity
                if out[off + 3] >= 18 and _rd16(out, off + 16) == 0:
                    _wr16(out, off + 16, pbdur)

def _beam_elem(start_tick, end_tick, leftX, rightX, count, voice=0, staff=0):
    """Build a 30-byte BEAM element (type 4). Encore draws one beam joining the
    `count` member notes; the members carry grace1 bit 0x10 (set by _beam_pass).
    Only the structural fields are filled (tick, span x, end tick, member count);
    Encore recomputes the beam's vertical geometry on open, so the zeroed slope
    bytes render correctly. Validated against Encore-saved beam groups."""
    d = bytearray(30)
    _wr16(d, 0, start_tick)
    d[2] = (4 << 4) | (voice & 0x0F)
    d[3] = 30
    d[4] = staff & 0x3F
    d[5] = 0x01
    _wr16(d, 14, leftX)
    _wr16(d, 16, rightX)
    _wr16(d, 20, end_tick)
    d[22] = 0x01
    d[25] = (count - 1) & 0xFF
    d[27] = 0x0c
    return bytes(d)

def _beam_pass(out):
    """Insert BEAM elements so consecutive beamable notes render joined in Encore.

    Beamable = eighth or shorter (faceValue low nibble >= 4), not a grace note.
    Notes are grouped per (rawStaff, voice) and beamed in runs that stay within
    one beat (the measure's beatTicks); a run of >= 2 such notes gets one BEAM
    element at the run's start tick, and every member note gets grace1 bit 0x10
    (the on-disk beam-membership flag). The importer ignores BEAM (it maps to a
    generic, dropped element) and re-beams on its own, so this is a pure render
    addition that does not change import results. Runs through the buffer after
    the main layout loop so each note's x-position (+10) is already set."""
    result = bytearray()
    pos = 0
    while pos < len(out):
        if out[pos:pos + 4] != b'MEAS':
            result += out[pos:pos + 1]
            pos += 1
            continue
        size = struct.unpack('<I', out[pos + 4:pos + 8])[0]
        hdr = out[pos + 8:pos + 8 + 0x36]
        beat_ticks = _rd16(hdr, 4) or 240
        elem_start = pos + 8 + 0x36
        eb = bytearray(out[elem_start:elem_start + size])
        # parse the element stream (stops at the 0xffff end marker)
        elems = []
        i = 0
        while i + 4 <= len(eb):
            if eb[i:i + 2] == b'\xff\xff' or eb[i + 3] == 0:
                break
            elems.append((i, eb[i + 3], _rd16(eb, i), eb[i + 2] >> 4, eb[i + 2] & 0x0F))
            i += eb[i + 3]
        # collect beamable notes per (staff, voice)
        groups = {}
        for (off, esize, tick, typ, voice) in elems:
            if typ != 9 or esize < 16:
                continue
            if (eb[off + 5] & 0x0F) < 4 or (eb[off + 6] & 0x20):
                continue
            groups.setdefault((eb[off + 4], voice), []).append((tick, off))
        beams = {}     # rel-off of run's first note -> beam bytes to insert before it
        for (staff, voice), notes in groups.items():
            notes.sort()
            run = []
            for (tick, off) in notes + [(None, None)]:
                if run and (tick is None or tick // beat_ticks != run[0][0] // beat_ticks):
                    if len(run) >= 2:
                        fo, lo = run[0][1], run[-1][1]
                        beams[fo] = _beam_elem(run[0][0], run[-1][0],
                                               _rd16(eb, fo + 10), _rd16(eb, lo + 10),
                                               len(run), voice, staff)
                        for (_, o) in run:
                            eb[o + 6] |= 0x10
                    run = []
                if tick is not None:
                    run.append((tick, off))
        if beams:
            newe = bytearray()
            for (off, esize, tick, typ, voice) in elems:
                if off in beams:
                    newe += beams[off]
                newe += eb[off:off + esize]
            newe += b'\xff\xff'
            eb = newe
        result += b'MEAS' + struct.pack('<I', len(eb)) + hdr + eb
        pos = elem_start + size
    return bytes(result)

def _curve_slurs(out, slurs, groups):
    """Give each flat SLURSTART a subtle arc. A 33-byte slur has no curve
    control points so Encore draws a straight line; fill startY (+12), the apex
    midX/midY (+14/+16) and endY (+22). The arc sits opposite the stems: below a
    low-note staff (notes mostly under the middle line), above otherwise. Kept
    subtle (apex ~9, ends ~4), not a semicircle. The span (+10/+20) is kept."""
    if not slurs:
        return
    pos_sum = {}
    pos_cnt = {}
    for evts in groups.values():
        for (tick, off, fv, etyp) in evts:
            if etyp == 9:
                rs = out[off + 4]
                pos_sum[rs] = pos_sum.get(rs, 0) + out[off + 12]
                pos_cnt[rs] = pos_cnt.get(rs, 0) + 1
    for (off, rs) in slurs:
        avg = pos_sum.get(rs, 0) / max(1, pos_cnt.get(rs, 1))
        sign = 1 if avg >= 6 else -1     # high notes -> arc above (+), low -> below (-)
        sx = _rd16(out, off + 10)
        ex = _rd16(out, off + 20)
        _wr16(out, off + 12, sign * 4)            # startY
        _wr16(out, off + 14, (sx + ex) // 2)      # midX
        _wr16(out, off + 16, sign * 9)            # midY (apex)
        _wr16(out, off + 22, sign * 4)            # endY

def apply_encore_layout(data):
    """Fill Encore display-layout fields in every v0xC4 MEAS block in `data`.

    v0xC2 files (chuMagio 0xC2) are handled too: the synthetic ones keep the
    v0xC4 skeleton (chuVersio 1056), so Encore renders them with the v0xC4 note
    layout. Their notes carry the MIDI pitch in the tuplet slot (+13) instead of
    +15, so we move it across first (same normalization the importer applies),
    then the v0xC4 layout below positions and sounds them."""
    if len(data) < 5 or data[4] not in (0xC2, 0xC4):
        return data
    out = bytearray(data)
    is_c2 = out[4] == 0xC2
    clef_by_raw, fallback_clef, key_by_raw, fallback_key = _staff_clef_map(out)
    _fill_volta_hooks(out, _measure_headers(out))

    # Mid-piece CLEF changes (element type 1) override the LINE clef for every
    # later note on that staff. Tracked across measures in stream order, since
    # Encore writes a staff's elements in tick order with the clef change first.
    clef_now = {}
    pos = 0
    while True:
        m = out.find(b'MEAS', pos)
        if m < 0:
            break
        size = struct.unpack('<I', out[m + 4:m + 8])[0]
        hdr = m + 8
        elem_start = hdr + 0x36
        elem_end = elem_start + size
        beat_ticks = _rd16(out, hdr + 4)
        dur_ticks = _rd16(out, hdr + 6)
        ts_den = out[hdr + 9]
        # collect note/rest elements; track per (rawStaff, voice) for dots
        groups = {}
        max_xpos = 0
        _chord_col = {}          # (staff, voice) -> (tick of the column's first note, its x)
        slurs = []                  # flat SLURSTART ORNs needing a curve
        # accidental in effect this measure, per (rawStaff, staff position):
        # an accidental carries to later same-line notes until the barline.
        meas_accid = {}
        i = elem_start
        while i + 4 <= elem_end:
            if out[i:i + 2] == b'\xff\xff':
                break
            tv = out[i + 2]
            typ = tv >> 4
            voice = tv & 0x0F
            esize = out[i + 3]
            if esize == 0:
                break
            # type 1 = clef change: update the clef in effect for this staff,
            # and (16-byte form) draw it just left of the note at its tick.
            if typ == 1 and esize >= 6:
                clef_now[out[i + 4]] = out[i + 5]
                if esize >= 12 and _rd16(out, i + 10) == 0:
                    tk = _rd16(out, i)
                    _wr16(out, i + 10, max(0, _enc_xpos(tk) - _GRACE_XPOS_OFFSET - 2))
            # type 5 = ORN: a flat SLURSTART (tipo 0x21 with no curve control
            # points, midY at +16 still zero) gets a subtle arc added below.
            if typ == 5 and out[i + 5] == 0x21 and esize >= 24 \
                    and out[i + 16] == 0 and out[i + 17] == 0:
                slurs.append((i, out[i + 4]))
            # type 6 = lyric: it stores its x-position at +10 like a note, and
            # aligns under the note at the same tick.
            if typ == 6 and esize >= 12:
                tick = _rd16(out, i)
                if _rd16(out, i + 10) == 0:
                    _wr16(out, i + 10, _enc_xpos(tick))
            if typ in (8, 9) and esize >= 12:
                tick = _rd16(out, i)
                # x-position
                if _rd16(out, i + 10) == 0:
                    xp = _enc_xpos(tick)
                    if typ == 9 and (out[i + 6] & 0x20):   # grace -> left of its main note
                        xp = max(2, xp - _GRACE_XPOS_OFFSET)
                    else:
                        # Encore draws the notes of one chord in a single column, whatever their
                        # recorded ticks. A note landing within the chord window of the last one
                        # on its own staff and voice is one of its members, so it takes that
                        # column rather than a column of its own computed from its tick.
                        key = (out[i + 4] & 0x3F, out[i + 2] & 0x0F)
                        last = _chord_col.get(key)
                        if last is not None and 0 <= tick - last[0] < 8:
                            xp = last[1]
                        else:
                            _chord_col[key] = (tick, xp)
                    _wr16(out, i + 10, xp)
                max_xpos = max(max_xpos, _rd16(out, i + 10))
                # Rests carry a staff position at +12 too. Left at 0 they sit
                # three spaces too low; Encore places them around position 6
                # (eighth 7, sixteenth 5), staff-line relative for any clef.
                if typ == 8 and esize >= 13 and out[i + 12] == 0:
                    out[i + 12] = {4: 7, 5: 5}.get(out[i + 5] & 0x0F, 6)
                if typ == 8:
                    groups.setdefault((out[i + 4], voice), []).append((tick, i, out[i + 5] & 0x0F, 8))
                if typ == 9 and esize >= 16:
                    # v0xC2 sub-variant A keeps the MIDI pitch in the tuplet slot
                    # (+13) with +15 empty or a stray flag; move it to +15 (the
                    # v0xC4 pitch slot) so Encore reads it, clearing the tuplet
                    # slot. Same rule the importer applies. Sub-variant B (+15
                    # already a real pitch) is left alone.
                    if is_c2 and out[i + 15] < 12 and out[i + 13] >= 12:
                        out[i + 15] = out[i + 13]
                        out[i + 13] = 0
                    midi = out[i + 15]
                    # diatonic staff position + accidental (sharp spelling),
                    # using the clef of the staff this note belongs to.
                    raw_staff = out[i + 4]
                    clef = clef_now.get(raw_staff,
                                        clef_by_raw.get(raw_staff, fallback_clef))
                    origin = _CLEF_ORIGIN.get(clef)   # None for PERC/TAB -> skip
                    if origin is not None and out[i + 12] == 0 and midi:
                        fifths = _KEY_FIFTHS[key_by_raw.get(raw_staff, fallback_key) % 15]
                        posb, letter, actual = _spell(midi, fifths, origin)
                        out[i + 12] = posb
                        # show an accidental only when the note's alteration
                        # differs from what is already in effect on that line
                        # (the key default until an accidental overrides it).
                        akey = (raw_staff, posb)
                        in_effect = meas_accid.get(akey, _key_alt(fifths)[letter])
                        if actual != in_effect:
                            if out[i + 21] == 0:
                                out[i + 21] = _ALT_GLYPH.get(actual, 0)
                            meas_accid[akey] = actual
                    groups.setdefault((out[i + 4], voice), []).append((tick, i, out[i + 5] & 0x0F, 9))
            i += esize
        # measure width @0x10. Normally the x-position of the measure end, so
        # the last note keeps its own duration as trailing space. But when the
        # beatTicks is the legacy value (not 960/timeSigDen, e.g. 240 in a 2/2
        # measure where it should be 480), Encore lays the measure out on a
        # shorter beat grid and the content runs past it; the measure then needs
        # a wider stored width or every note collapses to the first position.
        # In that case size it from the actual content plus a full-beat trailing.
        correct_beat = (960 // ts_den) if ts_den else 0
        if correct_beat and beat_ticks and beat_ticks != correct_beat:
            width = max_xpos + _LEGACY_BEATTICKS_TRAIL
        else:
            width = _enc_xpos(dur_ticks)
        struct.pack_into('<I', out, hdr + 0x10, width)
        _apply_dots_and_stems(out, groups, dur_ticks)
        _curve_slurs(out, slurs, groups)
        pos = elem_end
    return _beam_pass(out)

def make_2staff_pre(clefs=(0, 1)):
    """Turn the single-staff bazo skeleton PRE into a 2-staff system.

    Sets the header staffPerSystem to 2 and rebuilds every LINE block to carry
    two 30-byte staff entries instead of one. The second entry is a copy of the
    first with its clef byte (+14) and instrStaffIdx (+21=1) changed, so notes
    route to it by staffIdx 1 (rawStaff 0x01). Encore re-spaces the two staves
    vertically on open.

    instrumentCount is deliberately left at 1: the bazo TK00 defines a single
    instrument, and raising the count to 2 makes Encore read past the TK00 data
    and crash. With one instrument and two staves there is no separate brace
    (bazo's TK00 carries no grouping), which is the intended 2-staff-no-brace
    layout. `clefs` is the (top, bottom) clef byte pair (default G over F)."""
    pre = bytearray(set_chumagio(0xC4))
    pre[0x33] = 2
    out = bytearray()
    pos = 0
    while pos < len(pre):
        if pre[pos:pos + 4] == b'LINE':
            sz = struct.unpack('<I', pre[pos + 4:pos + 8])[0]
            content = pre[pos + 8:pos + 8 + sz]
            hdr13 = content[:13]
            s0 = bytearray(content[13:13 + 30])
            tail = content[13 + 30:]
            s0[14] = clefs[0]
            s1 = bytearray(s0)
            s1[14] = clefs[1]
            s1[21] = 1                     # instrStaffIdx 1 -> bass staff slot
            newc = bytes(hdr13) + bytes(s0) + bytes(s1) + bytes(tail)
            out += b'LINE' + struct.pack('<I', len(newc)) + newc
            pos += 8 + sz
        else:
            out += pre[pos:pos + 1]
            pos += 1
    return bytes(out)

def assemble_2staff(custom_list, fill_ts=(4, 4), clefs=(0, 1)):
    """assemble() for a 2-staff system (top clef over bottom clef, no brace).

    Notes route to the top staff with staffIdx 0 (rawStaff 0x00) and to the
    bottom staff with staffIdx 1 (rawStaff 0x01); note_v0c4 already preserves
    those low values. The layout pass reads each note's clef from the LINE
    entries, so bottom-staff positions come out in the bottom clef."""
    pre = bytearray(make_2staff_pre(clefs))
    total_meas = max(len(custom_list), 6)
    struct.pack_into('<h', pre, 0x34, total_meas)
    body = b''.join(meas_block(h, e) for h, e in custom_list)
    if len(custom_list) < 6:
        body += b''.join(empty_meas(*fill_ts) for _ in range(6 - len(custom_list)))
    return bytes(pre) + body + SKELETON_POST

def set_staff_key(data, key, staff_idx=0):
    """Patch the key-signature byte for staff_idx in ALL LINE blocks. The key
    byte sits at staff-entry +15 (right after the clef at +14). Encore's key
    encoding: 0=C, 1..7 = 1..7 flats, 8..14 = 1..7 sharps (see encKeyToFifths).
    Apply before write()/apply_encore_layout so the layout pass spells notes in
    this key (in-key notes get no accidental, out-of-key notes get one)."""
    d = bytearray(data)
    pos = 0
    while pos + 8 < len(d):
        if d[pos:pos+4] == b'LINE':
            entry_offset = pos + 8 + 13 + staff_idx * 30 + 15  # +15 = key byte
            if entry_offset < len(d):
                d[entry_offset] = key & 0xFF
            size = int.from_bytes(d[pos+4:pos+8], 'little')
            pos += 8 + size
        else:
            pos += 1
    return bytes(d)


def mrest_v0c4(tick, count, staffIdx=0, voice=0):
    """Multi-measure rest that Encore actually draws as a thick bar with the
    count above it. The face value byte must be 0x20 (a plain rest fv draws a
    single whole rest instead), the count sits at +15, and +16/+17 carry the
    constant 0x80 0x07 marker seen in real files. The layout pass fills x (+10)
    and position (+12)."""
    d = bytearray(15)
    d[0] = 18
    d[1] = staffIdx & 0x3F
    d[2] = 0x20
    d[12] = count & 0xFF        # +15 mrestCount
    d[13] = 0x80                # +16
    d[14] = 0x07                # +17
    return struct.pack('<H', tick) + bytes([(8 << 4) | (voice & 0xF)]) + bytes(d)


def slur_v0c4(tick, staffIdx, startX, midX, endX, startY=9, midY=16, endY=9, voice=0):
    """28-byte SLURSTART (tipo 0x21), the geometry-rich form Encore renders an
    arc from. Three control points: (startX,startY), (midX,midY = apex),
    (endX,endY). A larger midY makes the slur bulge; all-zero Y draws a flat
    line. The 33-byte ornament_v0c4 SLURSTART used by importer fixtures draws
    straight in Encore -- use this builder for files meant to open in Encore."""
    d = bytearray(28)
    _wr16(d, 0, tick)
    d[2] = (5 << 4) | (voice & 0xF)
    d[3] = 28
    d[4] = staffIdx & 0xFF
    d[5] = 0x21
    _wr16(d, 10, startX); _wr16(d, 12, startY)
    _wr16(d, 14, midX);   _wr16(d, 16, midY)
    _wr16(d, 20, endX);   _wr16(d, 22, endY)
    return bytes(d)

def wedge_v0c4(tick, staffIdx, startX, endX, cresc=True, y=(-3, 2, 12), voice=0):
    """28-byte WEDGESTART (tipo 0x1D) hairpin. startX is stored at both +10 and
    +14, endX at +20. The Y triple (y12, y16, y22) sets vertical placement:
    negative (~ -37) above the staff, small positive (the default -3/2/12)
    just below it; large positive overshoots into the staff below. cresc=True
    writes speguleco (+26)=0 for a crescendo (<), False writes 1 for a
    diminuendo (>). The 33-byte ornament_v0c4 WEDGESTART draws as a closed
    angle in Encore -- use this builder for files meant to open in Encore."""
    d = bytearray(28)
    _wr16(d, 0, tick)
    d[2] = (5 << 4) | (voice & 0xF)
    d[3] = 28
    d[4] = staffIdx & 0xFF
    d[5] = 0x1D
    _wr16(d, 10, startX); _wr16(d, 12, y[0])
    _wr16(d, 14, startX); _wr16(d, 16, y[1])
    _wr16(d, 20, endX);   _wr16(d, 22, y[2])
    d[24] = 1
    d[26] = 0 if cresc else 1
    return bytes(d)

# Dynamic-mark tipo bytes (size-16 ORN), see ENCORE_FORMAT.md ornament table.
DYN = {'ppp': 0x80, 'pp': 0x81, 'p': 0x82, 'mp': 0x83, 'mf': 0x84, 'f': 0x85,
       'ff': 0x86, 'fff': 0x87, 'sfz': 0x88, 'sffz': 0x89, 'fp': 0x8A,
       'fz': 0xAA, 'sf': 0xAB}

def dyn_v0c4(tick, staffIdx, mark, xoffset, yoffset=-3, voice=0):
    """16-byte dynamic ORN (p, mf, f, ...). `mark` is a DYN key or a raw tipo
    byte. The glyph is drawn at its own xoffset (+10, normally the note's
    x-position at this tick) and yoffset (+12); a small value sits it next to
    the staff. Like slur/wedge this is the geometry-bearing form Encore renders;
    do not route it through the layout pass (it fills note/lyric x only)."""
    tipo = DYN.get(mark, mark) if isinstance(mark, str) else mark
    d = bytearray(16)
    _wr16(d, 0, tick)
    d[2] = (5 << 4) | (voice & 0xF)
    d[3] = 16
    d[4] = staffIdx & 0xFF
    d[5] = tipo & 0xFF
    _wr16(d, 10, xoffset)
    _wr16(d, 12, yoffset)
    return bytes(d)

def tie_arc_v0c4(tick, staffIdx, position, arcX1, arcX2, voice=0, direction=0xfe):
    """18-byte TIE (type 3) carrying the arc x-positions Encore draws from.
    arcX1 is the start note's x, arcX2 the end note's x. For a tie that crosses
    a barline, arcX2 is measure-relative to the START measure, so pass the start
    measure width plus the end note's x in the next measure (it exceeds the
    measure width). `position` is the tied note's staff position (+14/+15). The
    16-byte tie_v0c4 has no arc and draws flat; use this for Encore files."""
    d = bytearray(18)
    _wr16(d, 0, tick)
    d[2] = (3 << 4) | (voice & 0xF)
    d[3] = 18
    d[4] = staffIdx & 0x3F
    d[5] = direction
    d[10] = arcX1 & 0xFF
    d[12] = arcX2 & 0xFF
    d[14] = position & 0xFF
    d[15] = position & 0xFF
    d[16] = 0xF0
    return bytes(d)

# Size-16 ORN glyph tipo bytes. In v0xC4 Encore draws articulations and single
# ornaments as size-16 ORN elements (the note's +24 byte is only what the
# importer reads); see the ENCORE_FORMAT ornament table.
ARTIC = {'staccato': 0xC9, 'accent': 0xBE, 'tenuto': 0xC8, 'marcato': 0xBF,
         'marcato_below': 0xC6, 'fermata': 0xCC, 'fermata_below': 0xCD,
         'upbow': 0xC4,
         'trill': 0xB0, 'short_trill': 0xB6, 'mordent': 0xB8, 'tremolo': 0xAF,
         'finger1': 0xB9, 'finger2': 0xBA, 'finger3': 0xBB, 'finger4': 0xBC,
         'finger5': 0xBD}

def artic_v0c4(tick, staffIdx, mark, xoffset, yoffset=12, voice=0):
    """Articulation or single ornament as a size-16 ORN (staccato, accent,
    tenuto, marcato, trill, mordent, fermata, tremolo, ...). Same wire form as
    dyn_v0c4. The glyph sits at xoffset (+10, the note x) and yoffset (+12); a
    positive yoffset (~12) places it above the note, negative below. Pair with a
    stem-down high note so the mark does not hit the stem."""
    tipo = ARTIC.get(mark, mark) if isinstance(mark, str) else mark
    return dyn_v0c4(tick, staffIdx, tipo, xoffset, yoffset, voice)

def arpeggio_v0c4(tick, staffIdx, x, yTop, yBottom, voice=0):
    """28-byte ARPEGGIO (tipo 0x22): a vertical wavy line just left of a chord.
    All three x slots (+10/+14/+20) hold the same x so the line is vertical;
    yTop (+16) and yBottom (+22) span the chord's noteheads (more negative is
    higher). A size-16 form does not render."""
    d = bytearray(28)
    _wr16(d, 0, tick)
    d[2] = (5 << 4) | (voice & 0xF)
    d[3] = 28
    d[4] = staffIdx & 0xFF
    d[5] = 0x22
    _wr16(d, 10, x); _wr16(d, 12, 8)
    _wr16(d, 14, x); _wr16(d, 16, yTop)
    _wr16(d, 20, x); _wr16(d, 22, yBottom)
    d[24] = 1
    d[26] = 2
    return bytes(d)

# Tempo mark template captured from an Encore-5-saved file (quarter = 100 at a
# beat). The element is a 38-byte ORN (tipo 0x32 = TEMPO): beat-unit code at
# +28 (0x02 = quarter), BPM at +30, and a fixed text/glyph layout box at
# +10..+27. An earlier 33-byte guess made Encore misparse the following bytes
# and paint a giant black box; the correct size is 38.
_TEMPO_TMPL = bytes.fromhex(
    'f000502600320000000031000e003100cfff00004d00daff0000000002026400570009000000')

def tempo_v0c4(tick, bpm, beat_unit=0x02, voice=0, staffIdx=0):
    """38-byte TEMPO ORN (tipo 0x32). Draws "<note> = bpm" above the staff at
    `tick`. beat_unit is the metronome note code (0x02 = quarter); bpm is the
    beats-per-minute number. The x-position (+10/+14) is set from the tick like
    a note; the rest of the layout box comes from the captured template."""
    d = bytearray(_TEMPO_TMPL)
    _wr16(d, 0, tick)
    d[2] = (5 << 4) | (voice & 0x0F)
    d[4] = staffIdx & 0x3F
    xp = _enc_xpos(tick)
    _wr16(d, 10, xp)
    _wr16(d, 14, xp)
    d[28] = beat_unit & 0xFF
    d[30] = bpm & 0xFF
    return bytes(d)

def stafftext_orn(tick, tind=0, x=28, y=-61, box=True, voice=0, staffIdx=0):
    """86-byte STAFFTEXT ORN (tipo 0x1e). Anchors an expression/staff text at
    `tick`; the actual string lives in the TEXT block (build it with the
    existing text_block_v0c4), referenced by `tind` (entry index). `box`=True
    sets the "enclose with box" flag (+34/+35 = 1), which makes Encore display
    the FULL text -- without it Encore clips the text to a small box and shows
    only the first few characters.

    NOTE on rendering: a measure carrying staff text also needs Encore's
    computed 16-byte layout blob in its MEAS header at +0x26..+0x35 (the slot
    that otherwise holds the " Writer" tag); with the default tag the text
    stacks vertically one character per line. That blob is opaque per-text
    geometry and is not synthesized here, so use a captured header for faithful
    horizontal rendering. The element itself (and the TEXT block) import
    correctly regardless."""
    d = bytearray(86)
    _wr16(d, 0, tick)
    d[2] = (5 << 4) | (voice & 0x0F)
    d[3] = 86
    d[4] = staffIdx & 0x3F
    d[5] = 0x1e
    _wr16(d, 10, x); d[12] = 18; _wr16(d, 14, x)
    _wr16(d, 16, y & 0xFFFF); _wr16(d, 20, 88); _wr16(d, 22, (y + 17) & 0xFFFF)
    d[24] = 0x51; d[26] = 0x0c; d[28] = 0x03
    d[32] = tind & 0xFF
    if box:
        d[34] = 1; d[35] = 1
    return bytes(d)

def meas_hdr_volta(timeSigNum, timeSigDen, ending, hooks=0xC0, bpm=100, barTypeEnd=0):
    """meas_hdr variant that draws a volta (ending) bracket. `ending` is the
    ending-number bitmask at +0x0F (bit 0 = 1st ending, bit 1 = 2nd, ...);
    consecutive measures with the same bitmask form one bracket. `hooks` at
    +0x0E draws the bracket ends: 0xC0 both (single-measure ending), 0x80 left
    only (bracket start), 0x40 right only (bracket end). Without the hooks byte
    Encore draws no bracket. (Vertical placement is Encore's default, close to
    the staff; the exact height lives in the measure layout region.)"""
    h = bytearray(meas_hdr(timeSigNum, timeSigDen, bpm=bpm, barTypeEnd=barTypeEnd))
    h[0x0E] = hooks & 0xFF
    h[0x0F] = ending & 0xFF
    return bytes(h)


def gen_v0c4_singlestaff_voice4_second_voice():
    """A single-staff instrument stores a genuine second melodic voice as Encore voice nibble 4,
    overlapping voice 0 (both sound on every beat). Voice 4 must import as a SEPARATE MuseScore
    voice (voice 1), not be concatenated onto voice 0. With the bug, voice 4 collapsed to voice 0,
    doubling voice 0 (overfull, voice 1 left empty); on real files the concatenation also mis-groups
    the overlapping triplets into a non-dyadic bar that fails sanityCheck and refuses to open.
    Layout: 4/4, voice 0 = C4 D4 E4 F4 quarters, voice 4 = G4 A4 B4 C5 quarters at the same ticks."""
    e  = note_v0c4(0,   0, 0, fv=3, pitch=60)          # voice 0, beat 1
    e += note_v0c4_voice4(0,   0, fv=3, pitch=67)      # voice 4, beat 1 (overlaps)
    e += note_v0c4(240, 0, 0, fv=3, pitch=62)
    e += note_v0c4_voice4(240, 0, fv=3, pitch=69)
    e += note_v0c4(480, 0, 0, fv=3, pitch=64)
    e += note_v0c4_voice4(480, 0, fv=3, pitch=71)
    e += note_v0c4(720, 0, 0, fv=3, pitch=65)
    e += note_v0c4_voice4(720, 0, fv=3, pitch=72)
    e += end_marker()
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))



# ===========================================================================
# Encore 4 instrument entry table fixtures (reported bugs 202-211)
# The instrument blocks form a fixed-stride table from offset 194 whose entry
# size is NOT the varsize each TK header declares; these fixtures pin down the
# layouts that exposed it.  See ENCORE_FORMAT.md 'Instrument entry table'.
# ===========================================================================
_ENC4_LEGACY_TK_END = 194 + 2158              # the skeleton's TK00 entry is 2158 bytes in total
_ENC4_PAGE_LINE = SKELETON_PRE[_ENC4_LEGACY_TK_END:]


def _enc4_entry(magic4, name, entry_size, declared_varsize=112, midi=None, channel=None,
          midi_from_end=46, latin1=True):
    """One Encore 4 instrument entry: 8-byte block header + (entry_size - 8) content.

    magic4 = b'TK01' or b'\\x00' * 4 for the header-less entries some Encore 4
    saves produce. midi is the 1-indexed GM program, written midi_from_end bytes
    before the end of the entry (the 8-slot per-staff program table).
    """
    content = bytearray(entry_size - 8)
    nb = (name.encode('latin1') if latin1 else name.encode('utf-16-le')) + (b'\x00' if latin1 else b'\x00\x00')
    content[:len(nb)] = nb
    off = len(content) - midi_from_end
    if midi is not None:
        content[off:off + 8] = bytes([midi & 0xFF]) * 8
    if channel is not None:
        # the eight per-voice channels sit immediately before the program table, stored from zero
        content[off - 8:off] = bytes([(channel - 1) & 0xFF]) * 8
    head = magic4 + (struct.pack('<I', declared_varsize) if magic4 != b'\x00\x00\x00\x00' else b'\x00' * 4)
    return bytes(head) + bytes(content)


def _enc4_body_5_measures():
    b = meas_block(meas_hdr(4, 4), end_marker())
    b += b''.join(empty_meas(4, 4) for _ in range(5))
    return b


def _enc4_build(entries, instrument_count, version=0xC4):
    header = bytearray(SKELETON_PRE[:194])
    header[4] = version
    header[0x32] = instrument_count
    return bytes(header) + b''.join(entries) + _ENC4_PAGE_LINE + _enc4_body_5_measures() + SKELETON_POST


# ---------------------------------------------------------------------------
# 210: instruments_tk_index_gap.enc
# Encore 4 v0xC4 file with two 242-byte instrument entries. Entry 0's 8-byte
# block header is all zeros (no TK00 magic), so only TK01 is discoverable. The
# importer must place TK01 on instrument 1 and recover instrument 0's name from
# its entry position, instead of assigning TK01 to instrument 0 and leaving
# instrument 1 nameless.
# ---------------------------------------------------------------------------
def gen_tk_index_gap():
    e0 = _enc4_entry(b'\x00\x00\x00\x00', 'Dulzaina 1', 242)
    e1 = _enc4_entry(b'TK01', 'Dulzaina 2', 242)
    return _enc4_build([e0, e1], 2)


# ---------------------------------------------------------------------------
# 206: instruments_entry_table_names.enc
# Encore 4 v0xC4 file with two 242-byte instrument entries where only entry 0
# carries a TK00 magic; entry 1 has a zeroed block header. Its name sits 8 bytes
# into the entry and must be recovered from the entry stride the file itself
# implies, instead of being left as "Part 2".
# ---------------------------------------------------------------------------
def gen_entry_table_names():
    e0 = _enc4_entry(b'TK00', 'Lead', 242)
    e1 = _enc4_entry(b'\x00\x00\x00\x00', 'Backgnd', 242)
    return _enc4_build([e0, e1], 2)


# ---------------------------------------------------------------------------
# 203: instruments_large_entry_declared_small.enc
# Encore 5.0 layout (2158-byte instrument entries) whose TK headers nevertheless
# declare varsize 112. The declared size made the reader take the small-entry
# path and look for the MIDI program at content+188, which in a 2158-byte entry
# is empty, so both instruments fell back to Grand Piano. The programs sit at
# entry+2084: 69 (Oboe) and 71 (Bassoon), 1-indexed.
# ---------------------------------------------------------------------------
def gen_large_entry_declared_small():
    e0 = _enc4_entry(b'TK00', '', 2158, midi=69, midi_from_end=74)
    e1 = _enc4_entry(b'TK01', '', 2158, midi=71, midi_from_end=74)
    return _enc4_build([e0, e1], 2)


# ---------------------------------------------------------------------------
# 211: instruments_entry_shorter_than_declared.enc
# Encore 4 v0xC2 file with a single 112-byte instrument entry whose TK header
# declares varsize 112 as the TOTAL block size, so the content is only 104
# bytes. Reading the MIDI program at content+varsize+76 lands past the entry, in
# the following blocks, and returned a bogus program. The real program (69,
# Oboe, 1-indexed) sits at content+60.
# ---------------------------------------------------------------------------
def gen_entry_shorter_than_declared():
    e0 = _enc4_entry(b'TK00', '', 112, midi=69, midi_from_end=44)
    return _enc4_build([e0], 1, version=0xC2)


# ---------------------------------------------------------------------------
# 209: notes_tuplet_flat_group_not_nested.enc
# One 3:2 bracket over a half note holding a quarter then four eighths. From the
# face values alone this also reads as a quarter plus an inner triplet of eighths
# filling the second slot, and the importer took that reading, which makes the
# eighths play a third of their real length and leaves the measure short by an
# amount no plain rest can fill. The recorded tick positions say the three
# eighths take a full quarter each way, not one outer slot.
# ---------------------------------------------------------------------------
def gen_tuplet_flat_group_not_nested():
    e = (
        rest_v0c4(0, 0, 0, fv=3)
        + note_v0c4(240, 0, 0, fv=3, pitch=60)
        + note_v0c4(480, 0, 0, fv=3, pitch=62, tuplet=0x32)
        + note_v0c4(640, 0, 0, fv=4, pitch=64, tuplet=0x32)
        + rest_v0c4_tup(720, 0, 0, fv=4, tuplet=0x32)
        + note_v0c4(800, 0, 0, fv=4, pitch=65, tuplet=0x32)
        + note_v0c4(880, 0, 0, fv=4, pitch=67, tuplet=0x32)
        + end_marker()
    )
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ---------------------------------------------------------------------------
# 205: structure_wide_score_first_page.enc
# 20-staff score whose first system is followed by a page break, mirroring a big
# band layout: Encore puts one system per page starting on page 1. At Encore's
# nominal staff size the system is taller than the printable area, so MuseScore
# pushed it past the title frame onto page 2 and the score appeared to start with
# a blank page. The staff-size fit must reduce far enough to bring it back.
# ---------------------------------------------------------------------------
def gen_wide_score_first_page():
    STAVES = 22
    hdr = bytearray(194)
    hdr[0:4] = b'SCOW'
    hdr[4] = 0xC4
    struct.pack_into('<H', hdr, 0x28, 0x0420)   # chuVersio
    struct.pack_into('<h', hdr, 0x2E, 2)        # lineCount
    struct.pack_into('<h', hdr, 0x30, 2)        # pageCount
    hdr[0x32] = STAVES                          # instrumentCount
    hdr[0x33] = STAVES                          # staffPerSystem
    struct.pack_into('<h', hdr, 0x34, 2)        # measureCount

    def staff_entry(instr_idx):
        e = bytearray(30)
        e[13] = 3      # display size 130%
        e[14] = 0      # treble clef
        e[16] = 0      # page-row counter 0: every system starts a page
        e[19] = 1      # visible
        e[21] = instr_idx
        return bytes(e)

    def line_block(first_measure):
        data = (b'\x00' * 10
                + struct.pack('<H', first_measure)
                + bytes([1])
                + b''.join(staff_entry(i) for i in range(STAVES)))
        return b'LINE' + struct.pack('<I', len(data)) + data

    def meas(pitch):
        e = b''.join(note_v0c4(0, 0, i, 1, pitch) for i in range(STAVES)) + end_marker()
        return meas_block(meas_hdr(4, 4), e)

    # A title frame is what pushes the oversized first system off page 1, so the
    # fixture needs a real title in the TITL block the skeleton carries.
    post = bytearray(SKELETON_POST)
    idx = post.find(b'TITL')
    content = _titl_content(title='Wide Score', author0='Composer')
    post[idx + 8: idx + 8 + len(content)] = content

    return (bytes(hdr) + line_block(0) + line_block(1)
            + meas(60) + meas(62) + bytes(post))


# ---------------------------------------------------------------------------
# 207: instruments_declared_size_overshoots_entry.enc
# v0xC4 file whose single 242-byte instrument entry declares varsize 242, i.e.
# the whole entry rather than its content. The MIDI program read at
# content+varsize+76 then lands well past the entry and returned an unrelated
# byte. The program table sits 46 bytes from the end of the entry: GM 22,
# Accordion.
# ---------------------------------------------------------------------------
def gen_declared_size_overshoots_entry():
    e0 = _enc4_entry(b'TK00', '', 242, declared_varsize=242, midi=22, midi_from_end=46)
    return _enc4_build([e0], 1)


# ---------------------------------------------------------------------------
# 202: text_copyright_lines_one_byte.enc
# One-byte TITL block (varsize 2426) carrying three copyright lines. In this
# encoding the six copyright entries are 160 bytes each, not the 96 the earlier
# fields use, so reading them all at 96 found only the first line and landed
# inside its own text field for the rest.
#   2 + 14*96 + 6*160 + 120 = 2426
# ---------------------------------------------------------------------------
def _titl_content_one_byte(title='', author0='', copyrights=()):
    def item(text, width):
        prefix = bytearray(30)
        body = text.encode('latin1')[:width - 1] + b'\x00'
        body += b'\x00' * (width - len(body))
        return bytes(prefix) + bytes(body)

    c = b'\x00\x00'
    c += item(title, 66)                       # title
    c += item('', 66) * 2                      # subtitle 0-1
    c += item('', 66) * 3                      # instruction 0-2
    c += item(author0, 66)                     # author 0
    c += item('', 66) * 3                      # author 1-3
    c += item('', 66) * 2                      # header 0-1
    c += item('', 66) * 2                      # footer 0-1
    for i in range(6):                         # copyright 0-5, 130-byte text field
        c += item(copyrights[i] if i < len(copyrights) else '', 130)
    c += b'\x00' * 120
    assert len(c) == 2426, len(c)
    return c


def gen_copyright_lines_one_byte():
    post = bytearray(SKELETON_POST)
    idx = post.find(b'TITL')
    content = _titl_content_one_byte(
        title='Copyright Lines',
        author0='A Composer',
        copyrights=('(c) 1992 - 2000.', 'e-mail: someone@example.com', 'Piece V3.0'),
    )
    # The one-byte block is 2426 bytes where the skeleton carries a 21242-byte
    # two-byte one; rewrite the whole block, size field included.
    end = idx + 8 + 21242
    block = b'TITL' + struct.pack('<I', len(content)) + content
    post[idx:end] = block

    pre = set_chumagio(0xC4)
    body = meas_block(meas_hdr(4, 4), end_marker())
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + bytes(post)


# ---------------------------------------------------------------------------
# instruments_tk_magic_digits_unreliable.enc
# Four 242-byte instrument entries whose magics read TK00 TK01 TK03 TK03: Encore
# skips an index and repeats another. Trusting the digits leaves a hole at slot 2
# and pushes the last entry past the end, so every instrument from the third on
# takes the wrong staff. The entry position is what decides.
# ---------------------------------------------------------------------------
def gen_tk_magic_digits_unreliable():
    STAVES = 4
    NAMES = ('Uno', 'Dos', 'Tres', 'Cuatro')
    MAGICS = (b'TK00', b'TK01', b'TK03', b'TK03')
    MIDIS = (25, 41, 57, 74)

    hdr = bytearray(SKELETON_PRE[:194])
    hdr[4] = 0xC4
    struct.pack_into('<h', hdr, 0x2E, 1)        # lineCount
    struct.pack_into('<h', hdr, 0x30, 1)        # pageCount
    hdr[0x32] = STAVES                          # instrumentCount
    hdr[0x33] = STAVES                          # staffPerSystem
    struct.pack_into('<h', hdr, 0x34, 1)        # measureCount

    def staff_entry(instr_idx):
        e = bytearray(30)
        e[13] = 2      # display size
        e[14] = 0      # treble clef
        e[16] = 0
        e[19] = 1      # visible
        e[21] = instr_idx
        return bytes(e)

    line_data = (b'\x00' * 10 + struct.pack('<H', 0) + bytes([1])
                 + b''.join(staff_entry(i) for i in range(STAVES)))
    line = b'LINE' + struct.pack('<I', len(line_data)) + line_data

    entries = b''.join(_enc4_entry(MAGICS[i], NAMES[i], 242, midi=MIDIS[i], midi_from_end=46)
                       for i in range(STAVES))
    meas = meas_block(meas_hdr(4, 4),
                        b''.join(note_v0c4(0, 0, i, 1, 60 + i) for i in range(STAVES))
                        + end_marker())
    return bytes(hdr) + entries + line + meas + SKELETON_POST


# ===========================================================================
# structure_volta_short_last_measure.enc
# A repeat whose 2nd ending is the last measure and holds one quarter of a 4/4
# bar. With the irregular-measure strategy that measure shrinks, so the end of
# the score moves before the tick the bracket was built with and the bracket is
# left without an end element. It must follow the measure instead.
# ===========================================================================
def gen_v0c4_volta_short_last_measure():
    full = (note_v0c4(0,   0, 0, fv=3, pitch=60)
            + note_v0c4(240, 0, 0, fv=3, pitch=62)
            + note_v0c4(480, 0, 0, fv=3, pitch=64)
            + note_v0c4(720, 0, 0, fv=3, pitch=65)
            + end_marker())
    short = note_v0c4(0, 0, 0, fv=3, pitch=67) + end_marker()
    h0 = bytearray(meas_hdr(4, 4))
    h0[0x0C] = 2                                    # repeat start
    h1 = bytearray(meas_hdr(4, 4, barTypeEnd=4))
    h1[0x0F] = 0x01                                 # 1st ending, repeat end
    h2 = bytearray(meas_hdr(4, 4))
    h2[0x0F] = 0x02                                 # 2nd ending, one quarter only
    plain = bytes(meas_hdr(4, 4))
    custom = [(plain, full), (plain, full), (plain, full),
              (bytes(h0), full), (bytes(h1), full), (bytes(h2), short)]
    return assemble(0xC4, custom, fill_ts=(4, 4))


# ===========================================================================
# structure_prec_page_stub.enc
# PREC with an unlisted paper id, so the page size falls back to the custom
# width and length, which here hold a 25.4 x 25.4 mm square. Laying a system
# out on that leaves the spacing pass with no room, so it must be ignored.
# ===========================================================================
def gen_v0c4_prec_page_stub():
    pre = _patch_tk00('PrecStub'.encode('utf-16-le') + b'\x00\x00')
    body = meas_block(meas_hdr(4, 4), end_marker())
    body += b''.join(empty_meas(4, 4) for _ in range(5))
    return pre + body + _set_prec(SKELETON_POST, paper=283, length=254, width=254)


# ===========================================================================
# structure_v0xa6_staff_clefs.enc
# The 22-byte staff entry of the compact generation carries the clef one byte
# before the key. Three staves in G, F and C on the fourth line.
# ===========================================================================
def gen_v0xa6_staff_clefs():
    m = _meas_a6([(0, 0, 0, 4, 0), (0, 0, 1, 4, 0), (0, 0, 2, 4, 0)])
    return build_v0xa6([('Vz1', 74, 0), ('Vz2', 43, 0), ('Vz3', 43, 0)], [m], clefs=[0, 1, 3])


# ===========================================================================
# structure_v0xa6_per_staff_size.enc
# The same entry opens with the display size, counted from zero, so this
# generation has a size per staff like the later ones. The four staves carry
# 0, 1, 2 and 3 against a header byte of 1, and the entries must win.
# ===========================================================================
def gen_v0xa6_per_staff_size():
    m = _meas_a6([(0, 0, 0, 4, 0), (0, 0, 1, 4, 0), (0, 0, 2, 4, 0), (0, 0, 3, 4, 0)])
    return build_v0xa6([('Vz1', 74, 0), ('Vz2', 74, 0), ('Vz3', 74, 0), ('Vz4', 74, 0)], [m],
                       staff_size=1, sizes=[0, 1, 2, 3])


# ===========================================================================
# notes_notehead_without_drumset.enc
# The face-value nibbles on an ordinary pitched staff: no percussion clef, so
# no drumset is attached and the head has to stand on its own.
# ===========================================================================
def gen_v0c4_notehead_without_drumset():
    e = (note_v0c4(0,   0, 0, fv=(0 << 4) | 3, pitch=60)
         + note_v0c4(240, 0, 0, fv=(3 << 4) | 3, pitch=62)
         + note_v0c4(480, 0, 0, fv=(4 << 4) | 3, pitch=64)
         + note_v0c4(720, 0, 0, fv=(1 << 4) | 3, pitch=65)
         + end_marker())
    return assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4))


# ===========================================================================
# notes_v0xa6_notehead_cross.enc
# The compact generation draws a cross where the later ones draw a square, so
# nibble 3 must not come out as the drumset square here.
# ===========================================================================
def gen_v0xa6_notehead_cross():
    m = _meas_a6([(0, 0, 0, (0 << 4) | 3, 0), (240, 0, 0, (3 << 4) | 3, 2)], tsNum=4, tsDen=4)
    return build_v0xa6([('Melody', 74, 0)], [m])


# ===========================================================================
# instruments_drumset_name_vs_program.enc
# Two names that both score against a percussion template. The first carries a
# pitched GM program and must stay pitched; the second carries none, so the
# name is all there is and it must still become percussion.
# ===========================================================================
def gen_v0xa6_drumset_name_vs_program():
    m = _meas_a6([(0, 0, 0, 4, 0), (0, 0, 1, 4, 0)])
    return build_v0xa6([('Slap Ucillee', 37, 0), ('Congas', 0, 0)], [m])


# ===========================================================================
# instruments_v0xa6_percussion_channel.enc
# Encore keeps a MIDI channel per staff and channel 10 is the one General MIDI
# reserves for percussion. Neither instrument here carries a program: only the
# one on that channel becomes a drumset, and it keeps its own clef byte.
# ===========================================================================
def gen_v0xa6_percussion_channel():
    m = _meas_a6([(0, 0, 0, (3 << 4) | 3, 0), (240, 0, 0, (3 << 4) | 3, 2),
                  (0, 0, 1, (0 << 4) | 3, 0), (240, 0, 1, (0 << 4) | 3, 2)], tsNum=4, tsDen=4)
    return build_v0xa6([('sizzle', 0, 0), ('Melody', 0, 0)], [m], staff_size=3,
                       clefs=[1, 0], sizes=[2, 2], channels=[10, 3])


# ===========================================================================
# instruments_v0xc4_percussion_channel.enc
# The same channel table in the Encore 4 entry layout, where it precedes the
# eight-slot program table.
# ===========================================================================
def gen_v0c4_percussion_channel():
    e0 = _enc4_entry(b'TK00', 'ritmo',  242, channel=10)
    e1 = _enc4_entry(b'TK01', 'Flauta', 242, midi=74, channel=2)
    return _enc4_build([e0, e1], 2)


# ===========================================================================
# structure_scor_container.enc
# A container magic of SCOR rather than SCOW. Everything below it is the
# ordinary layout of its generation, so it must open like any other file.
# ===========================================================================
def gen_scor_container():
    e = (note_v0c4(0,   0, 0, fv=3, pitch=60)
         + note_v0c4(240, 0, 0, fv=3, pitch=62)
         + end_marker())
    data = bytearray(assemble(0xC4, [(meas_hdr(4, 4), e)], fill_ts=(4, 4)))
    data[0:4] = b'SCOR'
    return bytes(data)


# ===========================================================================
# structure_musictime_3_07.mus
# MusicTime in the middle generation rather than the compact one: version byte
# 0xC2, format 3.07 and the element geometry that goes with it. Read with the
# compact geometry its four notes come out as one or none.
# ===========================================================================
def gen_musictime_3_07():
    e = (note_v0c2_size24(0,   0, 0, fv=3, pitch=60, artic=0)
         + note_v0c2_size24(240, 0, 0, fv=3, pitch=62, artic=0)
         + note_v0c2_size24(480, 0, 0, fv=3, pitch=64, artic=0)
         + note_v0c2_size24(720, 0, 0, fv=3, pitch=65, artic=0)
         + end_marker())
    data = bytearray(set_version(assemble(0xC2, [(meas_hdr(4, 4), e)], fill_ts=(4, 4)),
                                 ENC_FORMAT_3_07))
    data[0:4] = b'MTIW'
    return bytes(data)


if __name__=='__main__':
    print("Generating synthetic Encore test files (using bazo.enc skeleton):")
    write("structure_v0c2_pitches.enc",       gen_v0c2_pitches())
    write("structure_v0c2_pre4_element_offsets.enc", gen_v0c2_pre4_element_offsets())
    write("structure_family_3x.enc", gen_family_3x())
    write("structure_family_40x_c2.enc", gen_family_40x_c2())
    write("structure_family_40x_c4.enc", gen_family_40x_c4())
    write("ornaments_v0c2_pre4_articulation_codes.enc", gen_v0c2_pre4_articulation_codes())
    write("ornaments_v0c2_post40_articulation_codes.enc", gen_v0c2_post40_articulation_codes())
    write("importer_v0c2_small_flag_chord.enc", gen_v0c2_small_flag_chord())
    write("notes_v0c2_size24_artic_pitch.enc", gen_v0c2_size24_artic_pitch())
    write("notes_v0c2_artic_grows_note.enc",  gen_v0c2_artic_grows_note())
    write("notes_v0c2_size24_semitonepitch.enc", gen_v0c2_size24_semitonepitch())
    write("notes_v0c2_common_time_glyph.enc",         gen_v0c2_common_time_glyph())
    write("notes_v0c2_common_time_glyph_uc.enc",     gen_v0c2_common_time_glyph_uppercase())
    write("structure_v0c2_triplets.enc",      gen_v0c2_triplets())
    write("structure_v0c2_triplet_pitch_in_semitone.enc", gen_v0c2_triplet_pitch_in_semitone())
    write("structure_v0c2_spurious_semitone_flag.enc", gen_v0c2_spurious_semitone_flag())
    write("importer_v0c2_snap.enc",          gen_v0c2_snap())
    write("notes_triplets.enc",      gen_v0c4_triplets())
    write("structure_v0c4_irregular_len_reduced.enc", gen_v0c4_irregular_measure_len_reduced())
    write("notes_tuplet_sort.enc",   gen_v0c4_tuplet_sort())
    write("importer_counter_bytes.enc", gen_v0c4_counter_bytes())
    write("structure_v0xa6_basic.enc",        gen_v0xa6_basic())
    write("structure_musictime_windows.mus", gen_musictime_windows())
    write("structure_musictime_mac.mus",     gen_musictime_mac())
    write("instruments_v0xa6_midi_program.enc",   gen_v0xa6_midi_program())
    write("structure_v0xa6_score_size.enc",       gen_v0xa6_score_size())
    write("structure_v0xa6_key_signature.enc",    gen_v0xa6_key_signature())
    write("importer_v0xa6_lyrics_and_stafftext.enc", gen_v0xa6_lyrics_and_stafftext())
    write("importer_v0xa6_two_verse_alignment.enc", gen_v0xa6_two_verse_alignment())
    write("importer_v0xa6_stafftext_placement.enc", gen_v0xa6_stafftext_placement())
    write("importer_v0xa6_tie_and_key_change.enc", gen_v0xa6_tie_and_key_change())
    write("parser_v0xa6_note_position.enc", gen_v0xa6_note_position_and_rest_fields())
    write("importer_v0xa6_melisma_verse_alignment.enc", gen_v0xa6_melisma_verse_alignment())
    write("notes_corrupted.enc",     gen_v0c4_corrupted())
    write("notes_swing.enc",         gen_v0c4_swing())
    write("notes_grace.enc",             gen_v0c4_grace())
    write("importer_grace1_0x30_normal_notes.enc", gen_v0c4_grace1_0x30_normal_notes())
    write("importer_cue_note.enc",               gen_v0c4_cue_note())
    write("importer_grace_beamed_group.enc",     gen_v0c4_grace_beamed_group())
    write("importer_grace_after_contiguous.enc", gen_v0c4_grace_after_contiguous())
    write("importer_grace_trailing_no_dot.enc",  gen_v0c4_grace_trailing_no_dot())
    write("importer_cue_mute_flags.enc",         gen_v0c4_cue_mute_flags())
    write("importer_grace_beam.enc",        gen_v0c4_grace_beam())
    write("importer_rest_in_tuplet.enc",    gen_v0c4_rest_in_tuplet())
    write("structure_rest_coincident_with_note.enc", gen_v0c4_rest_coincident_with_note())
    write("importer_full_voice_skipped.enc",        gen_v0c4_full_voice_skipped_via_loop())
    write("importer_merge_voices_non_overlapping.enc", gen_v0c4_merge_voices_non_overlapping())
    write("importer_merge_voices_overlapping.enc",     gen_v0c4_merge_voices_overlapping())
    write("importer_merge_voices_tremolo.enc",         gen_v0c4_merge_voices_tremolo())
    write("importer_isolated_explicit_tuplet_capped.enc", gen_v0c4_isolated_explicit_tuplet_capped())
    write("importer_rest_not_chord_anchor.enc",     gen_v0c4_rest_not_chord_anchor())
    write("importer_rest_caps_in_open_tuplet.enc",  gen_v0c4_rest_caps_in_open_tuplet())
    write("notes_swing_offgrid.enc",     gen_v0c4_swing_offgrid())
    write("notes_canonical_triplet.enc", gen_v0c4_canonical_triplet())
    write("notes_overflow_extend.enc",   gen_v0c4_overflow_extend())
    write("notes_whole_rest_2_4.enc",    gen_v0c4_whole_rest_2_4())
    write("notes_offbeat_canonical.enc",       gen_v0c4_offbeat_canonical())
    write("notes_explicit_tup_rdur_truncated.enc", gen_v0c4_explicit_tup_rdur_truncated())
    write("notes_partial_explicit_group.enc",  gen_v0c4_partial_explicit_group())
    write("notes_dotted_note_capping.enc",     gen_v0c4_dotted_note_capping())
    write("notes_mixed_value_tuplet.enc",      gen_v0c4_mixed_value_tuplet())
    write("notes_v0c2_implied_group_boundary.enc", gen_v0c2_implied_group_boundary())
    write("notes_capped_tuplet_note.enc",    gen_v0c4_capped_tuplet_note())
    write("notes_stretch_irregular_fallback.enc", gen_v0c4_stretch_irregular_fallback())
    write("notes_stretch_rob_rest.enc", gen_v0c4_stretch_rob_rest())
    write("notes_overfull_messy_precontent_tuplet.enc", gen_v0c4_overfull_messy_precontent_tuplet())
    write("notes_overfull_tuplet_with_slur.enc", gen_v0c4_overfull_tuplet_with_slur())
    write("notes_perc_clef_positions.enc",   gen_v0c4_perc_clef_positions())
    write("notes_perc_notehead_all_nibbles.enc", gen_v0c4_perc_notehead_all_nibbles())
    write("notes_perc_shared_pitch_nibbles.enc", gen_v0c4_perc_shared_pitch_nibbles())
    write("notes_perc_clef_standard_drumset_notehead.enc", gen_v0c4_perc_standard_drumset_notehead())
    write("notes_mixed_duration_tuplet_boundary_fill.enc", gen_v0c4_mixed_duration_tuplet_boundary_fill())
    write("notes_mixed_duration_triplet.enc",   gen_v0c4_mixed_duration_triplet())
    write("notes_partial_triplet_measure_end.enc", gen_v0c4_partial_triplet_measure_end())
    write("notes_triple_dotted_advance.enc",   gen_v0c4_triple_dotted_advance())
    write("notes_v0c2_near_simultaneous_chord.enc", gen_v0c2_near_simultaneous_chord())
    write("notes_tie.enc",          gen_v0c4_tie())
    write("notes_dotted_rest.enc",  gen_v0c4_dotted_rest())
    write("notes_dotted_note.enc",  gen_v0c4_dotted_note())
    write("notes_rdur_snap.enc",    gen_v0c4_rdur_snap())
    write("notes_sf_tiestart.enc",  gen_v0c4_sf_tiestart())
    write("notes_rest_before_note_midi_slop.enc", gen_v0c4_rest_before_note_midi_slop())
    write("notes_rdur_non_chord_ext_filtered.enc", gen_v0c4_rdur_non_chord_ext_filtered())
    write("notes_grace1_cascade_filter.enc",       gen_v0c4_grace1_cascade_filter())
    write("notes_chord_duplicate.enc",             gen_v0c4_chord_duplicate())
    write("notes_chord_duplicate_no_ext_bit.enc",  gen_v0c4_chord_duplicate_no_ext_bit())
    write("notes_v0c2_chord_cluster_5tick.enc",         gen_v0c2_chord_cluster_5tick())
    write("text_title_instruction_copyright.enc", gen_v0c4_title_instruction_copyright())
    write("text_titl_headers_footers.enc",        gen_v0c4_titl_headers_footers())
    write("text_header_footer_tokens.enc",        gen_v0c4_header_footer_tokens())
    write("notes_implicit_leading_rest.enc",       gen_v0c4_implicit_leading_rest())
    write("notes_inflated_rdur_quarter_chord.enc", gen_v0c4_inflated_rdur_quarter_chord())
    write("text_multi_slot_stacked_text.enc",     gen_v0c4_multi_slot_stacked_text())
    write("text_duplicate_titl_block.enc",        gen_v0c4_duplicate_titl_block())
    write("text_tempo_changes.enc",               gen_v0c4_tempo_changes())
    write("ornaments_accents_distributed.enc",     gen_v0c4_accents_distributed())
    write("structure_start_double_barline.enc",    gen_v0c4_start_double_barline())
    write("ornaments_trill_between_notes.enc",      gen_v0c4_trill_between_notes())
    write("ornaments_trill_tr_on_own_note.enc",     gen_v0c4_trill_tr_on_own_note())
    write("structure_stale_tick_by_column.enc",    gen_v0c4_stale_tick_by_column())
    write("structure_voice4_rest_with_notes.enc",  gen_v0c4_voice4_rest_with_notes())
    write("structure_merge_stray_voice_rests.enc", gen_v0c4_merge_stray_voice_rests())
    write("tempo_v0c2_eighth_beat_unit.enc",      gen_v0c2_tempo_eighth_beat_unit(), layout=False)
    write("instruments_instrument_count_padding.enc", gen_v0c4_instrument_count_padding())
    write("instruments_name_recovery.enc",             gen_v0c4_name_recovery())
    write("instruments_staff_hidden.enc",             gen_v0c4_staff_hidden())
    write("instruments_tk_utf16_name.enc",    gen_v0c4_tk_utf16_name())
    write("instruments_tk_probe_utf16.enc",   gen_v0c4_tk_probe_utf16())
    write("instruments_tk_onebyte_name.enc",  gen_v0c4_tk_onebyte_name())
    write("ornaments_beamed_triplet_capped.enc", gen_v0c4_beamed_triplet_capped())
    write("ornaments_zero_hairpin.enc",           gen_v0c4_zero_hairpin())
    write("ornaments_multi_measure_hairpin.enc",  gen_v0c4_multi_measure_hairpin(), layout=False)
    write("ornaments_grace_slur_to_main.enc",      gen_v0c4_grace_slur_to_main())
    write("ornaments_grace_slur_to_later.enc",     gen_v0c4_grace_slur_to_later())
    write("ornaments_multi_measure_slur.enc",     gen_v0c4_multi_measure_slur())
    write("ornaments_partial_quarter_triplet.enc", gen_v0c4_partial_quarter_triplet())
    write("text_lyrics.enc",                 gen_v0c4_lyrics())
    write("text_lyrics_variable.enc",        gen_v0c4_lyrics_variable())
    write("text_lyrics_two_verses.enc",      gen_v0c4_lyrics_two_verses())
    write("text_lyrics_hyphenated_words.enc", gen_v0c4_lyrics_hyphenated_words())
    write("text_lyrics_offgrid_nearest_chord.enc", gen_v0c4_lyrics_offgrid_nearest_chord())
    write("text_lyrics_hyphen_across_barline.enc", gen_v0c4_lyrics_hyphen_across_barline())
    write("text_lyrics_latin1.enc",          gen_v0c4_lyrics_latin1())
    write("text_lyrics_accent_first_letter.enc", gen_v0c4_lyrics_accent_first_letter())
    write("text_lyrics_column_says_which_note.enc", gen_v0c4_lyrics_column_says_which_note(), layout=False)
    write("instruments_instr_bass_midi_tiebreak.enc",   gen_v0c4_instr_bass_midi_tiebreak())
    write("instruments_instr_percussion_drumset.enc",   gen_v0c4_instr_percussion_drumset())
    write("instruments_small_tk_key6.enc",               gen_v0c4_small_tk_key6())
    write("instruments_small_tk_midi49.enc",            gen_v0c4_small_tk_midi49())
    write("instruments_total_size_tk_two_instrs.enc",   gen_v0c4_total_size_tk_two_instrs())
    write("instruments_total_size_tk_key_from_entry_end.enc", gen_v0c4_total_size_tk_key_from_entry_end())
    write("instruments_key_from_run_shaped_table.enc", gen_v0c4_key_from_run_shaped_table())
    write("instruments_key_from_a_sibling_measured_table.enc", gen_v0c4_key_from_a_sibling_measured_table())
    write("instruments_key_when_nothing_is_assigned.enc", gen_v0c4_key_when_nothing_is_assigned())
    write("instruments_table_the_file_measures_itself.enc", gen_v0c4_table_the_file_measures_itself())
    write("instruments_total_size_tk_key_not_from_channel_run.enc", gen_v0c4_total_size_tk_key_not_from_channel_run())
    write("instruments_no_tk_compact_table_two_instrs.enc", gen_v0c4_no_tk_compact_table_two_instrs())
    write("instruments_oversized_varsize_key_from_entry_end.enc", gen_v0c4_oversized_varsize_key_from_entry_end())
    write("instruments_small_tk_no_cross_entry_tables.enc", gen_v0c4_small_tk_no_cross_entry_tables())
    write("notes_nonuplet_missing_marker.enc",     gen_v0c4_nonuplet_missing_marker(), layout=False)
    write("notes_tuplet_group_opens_unmarked.enc", gen_v0c4_tuplet_group_opens_unmarked(), layout=False)
    write("notes_tie_start_recut_at_barline.enc", gen_v0c4_tie_start_recut_at_barline(), layout=False)
    write("notes_dotted_note_between_tuplet_members.enc", gen_v0c4_dotted_note_between_tuplet_members(), layout=False)
    write("notes_dotted_tuplet_member.enc", gen_v0c4_dotted_tuplet_member(), layout=False)
    write("notes_columns_apart_not_one_chord.enc", gen_v0c4_columns_apart_not_one_chord(), layout=False)
    write("notes_bracket_opening_rest_behind_fill.enc", gen_v0c4_bracket_opening_rest_behind_fill(), layout=False)
    write("notes_sixtyfourth_bracket_not_artifact.enc", gen_v0c4_sixtyfourth_bracket_not_artifact(), layout=False)
    write("notes_tick_wrapped_before_barline.enc", gen_v0c4_tick_wrapped_before_barline(), layout=False)
    write("ornaments_measure_repeat_after_pickup.enc", gen_v0c4_measure_repeat_after_pickup(), layout=False)
    write("notes_bar_that_cannot_be_written.enc", gen_v0c4_bar_that_cannot_be_written(), layout=False)
    write("notes_tie_across_trimmed_overflow.enc", gen_v0c4_tie_across_trimmed_overflow(), layout=False)
    write("instruments_tk_empty_name_authoritative.enc", gen_v0c4_tk_empty_name_authoritative())
    write("instruments_instr_perc_clef_drumset.enc",    gen_v0c4_instr_perc_clef_drumset())
    write("instruments_instr_drums_name_drumset.enc",   gen_v0c4_instr_drums_name_drumset())
    write("instruments_instr_laud_accent.enc",          gen_v0c4_instr_laud_accent())
    write("instruments_tab_template_forced_standard.enc", gen_v0c4_tab_template_forced_standard())
    write("instruments_tab_clef_keeps_tablature.enc",     gen_v0c4_tab_clef_keeps_tablature())
    write("instruments_tab_tuning_mandolin.enc",          gen_v0c4_tab_tuning_mandolin())
    write("instruments_tab_tuning_guitar.enc",            gen_v0c4_tab_tuning_guitar())
    write("instruments_tab_two_tunings.enc",              gen_v0c4_tab_two_tunings())
    write("instruments_tab_hidden_notation.enc",          gen_v0c4_tab_hidden_notation())
    write("instruments_tab_linked_pair.enc",              gen_v0c4_tab_linked_pair())
    write("instruments_tab_linked_overfull.enc",          gen_v0c4_tab_linked_overfull())
    write("instruments_tab_standalone_frets.enc",         gen_v0c4_tab_standalone_frets())
    write("instruments_instr_clarinet_midi72_key0.enc",         gen_v0c4_instr_clarinet_midi72_key0())
    write("instruments_instr_empty_name_midi_clarinet.enc",     gen_v0c4_instr_empty_name_midi_clarinet())
    write("instruments_instr_clarinet_midi72_key_neg2.enc",     gen_v0c4_instr_clarinet_midi72_key_neg2())
    write("instruments_instr_recorder_midi75_trackname.enc",    gen_v0c4_instr_recorder_midi75_trackname())
    write("instruments_no_tk_blocks_midi_key.enc",              gen_v0c4_no_tk_blocks_midi_key())
    write("instruments_no_tk_name_recovered.enc",               gen_v0c4_no_tk_name_recovered())
    write("instruments_unique_name_beats_midi.enc",             gen_v0c4_unique_name_beats_midi())
    write("instruments_fuzzy_name_match.enc",                   gen_v0c4_fuzzy_name_match())
    write("instruments_no_tk_name_latin1.enc",                  gen_v0c4_no_tk_name_latin1())
    write("instruments_no_tk_name_fallback.enc",                gen_v0c4_no_tk_name_fallback())
    write("notes_transposing_written_tpc.enc",                  gen_v0c4_transposing_written_tpc())
    write("notes_transposing_respell_melody.enc",               gen_v0c4_transposing_respell_melody())
    write("text_orn_tempo_3_8_dotted_quarter.enc",              gen_v0c4_orn_tempo_3_8_dotted_quarter())
    write("text_meas_bpm_suppressed_by_orn_tempo_later_tick.enc", gen_v0c4_meas_bpm_suppressed_by_orn_tempo_later_tick())
    write("text_orn_tempo_mismatch_suppressed.enc",             gen_v0c4_orn_tempo_mismatch_suppressed())
    write("text_tempo_orn_xoffset_downbeat.enc",                gen_v0c4_tempo_orn_xoffset_downbeat())
    write("text_tempo_orn_explicit_quarter_unit.enc",           gen_v0c4_tempo_orn_explicit_quarter_unit())
    write("text_tempo_orn_v0c2_bpm_offset.enc",                 gen_v0c2_tempo_orn_bpm_offset())
    write("text_tempo_orn_v0c2_v0c4_layout.enc",                gen_v0c2_tempo_orn_v0c4_layout())
    write("text_orn_tempo_misplaced_multi_measure.enc",         gen_v0c4_orn_tempo_misplaced_multi_measure())
    write("text_orn_tempo_equals_header.enc",                   gen_v0c4_orn_tempo_equals_header_at_start())
    write("notes_rdur_80_stays_16th.enc",         gen_v0c4_rdur_80_stays_16th())
    write("text_stafftext_tempo_promotion.enc",  gen_v0c4_stafftext_tempo_promotion())
    write("notes_tie_start_flag_byte6.enc",   gen_v0c4_tie_start_flag_byte6())
    write("ornaments_articulations.enc",          gen_v0c4_articulations())
    write("ornaments_articulations_combo.enc",    gen_v0c4_articulations_combo())
    write("ornaments_trill_mordent.enc",          gen_v0c4_trill_mordent())
    write("ornaments_ornament_turn.enc",           gen_v0c4_ornament_turn())
    write("ornaments_tremolos.enc",               gen_v0c4_tremolos())
    write("ornaments_fermatas.enc",               gen_v0c4_fermatas())
    write("ornaments_technical.enc",              gen_v0c4_technical())
    write("ornaments_trill_simple_on_note.enc",   gen_v0c4_trill_simple_on_note())
    write("ornaments_trill_spanner.enc",          gen_v0c4_trill_spanner())
    write("ornaments_trill_no_end_marker.enc",    gen_v0c4_trill_no_end_marker())
    write("ornaments_trill_end_far_from_start.enc", gen_v0c4_trill_end_far_from_start())
    write("ornaments_trill_cross_measure.enc",    gen_v0c4_trill_cross_measure())
    write("ornaments_staccato_orn.enc",           gen_v0c4_staccato_orn())
    write("ornaments_bowing.enc",                 gen_v0c4_bowing_orn())
    write("ornaments_fingering_orn.enc",          gen_v0c4_fingering_orn())
    write("structure_system_break.enc",           gen_v0c4_system_break())
    write("structure_system_break_mcount_zero.enc", gen_v0c4_system_break_mcount_zero())
    write("structure_section_markers.enc",        gen_v0c4_section_markers())
    write("structure_jump_marks.enc",             gen_v0c4_jump_marks())
    write("structure_jump_marks_all.enc",         gen_v0c4_jump_marks_all())
    write("notes_tie_flag_on_note.enc",       gen_v0c4_tie_flag_on_note())
    write("notes_tie_dir_fc.enc",              gen_v0c4_tie_dir_fc())
    write("notes_tie_intra_chord_arc_no_spurious.enc", gen_v0c4_tie_intra_chord_arc_no_spurious())
    write("notes_tie_18byte_real_forward.enc", gen_v0c4_tie_18byte_real_forward())
    write("notes_tie_dir_04_forward.enc", gen_v0c4_tie_dir_04_forward())
    write("notes_tie_crossmeasure_arcxx_equal.enc", gen_v0c4_tie_crossmeasure_arcxx_equal())
    write("notes_tie_spurious_far_receiver.enc", gen_v0c4_tie_spurious_far_receiver())
    write("structure_keychange_to_c.enc",          gen_v0c4_keychange_to_c())
    write("text_staff_text.enc",              gen_v0c4_staff_text())
    write("text_staff_text_multirun.enc",     gen_v0c4_staff_text_multirun())
    write("text_staff_text_two_descriptors.enc", gen_v0c4_staff_text_two_descriptors())
    write("text_staff_text_first_block_wins.enc", gen_v0c4_text_first_block_wins())
    write("text_staff_text_multiline.enc",    gen_v0c4_staff_text_multiline())
    write("ornaments_arpeggio.enc",                gen_v0c4_arpeggio())
    write("text_staff_text_placement.enc",    gen_v0c4_staff_text_placement())
    write("ornaments_dynamics.enc",                gen_v0c4_dynamics())
    write("ornaments_dynamics_stacked.enc",        gen_v0c4_dynamics_stacked())
    write("ornaments_dynamics_full.enc",           gen_v0c4_dynamics_full())
    write("ornaments_wedgestart_at_measure_end.enc", gen_v0c4_wedgestart_at_measure_end())
    write("ornaments_double_barline_multi_staff.enc", gen_v0c4_double_barline_multi_staff())
    write("importer_v0c2_multi_stream_drift.enc",            gen_v0c2_multi_stream_drift())
    write("structure_octave_lower_implicit_silences.enc", gen_v0c4_octave_lower_implicit_silences())
    write("structure_key_per_staff.enc",                  gen_v0c4_key_per_staff())
    write("text_satb_short_names_voice4_lyrics.enc", gen_v0c4_satb_short_names_voice4_lyrics())
    write("structure_octave_bassa_clef_override.enc",      gen_v0c4_octave_bassa_clef_override())
    write("instruments_bass_guitar_transposing_clef.enc",    gen_v0c4_bass_guitar_transposing_clef())
    write("structure_g_clef_8va_from_key.enc",             gen_v0c4_g_clef_8va_from_key())
    write("structure_f_clef_8vb_from_key.enc",             gen_v0c4_f_clef_8vb_from_key())
    write("structure_f_clef_8va_from_key.enc",             gen_v0c4_f_clef_8va_from_key())
    write("instruments_name_trailing_number.enc",          gen_v0c4_name_trailing_number_stripped())
    write("instruments_name_dash_separator.enc",           gen_v0c4_name_split_on_separator())
    write("instruments_weak_name_defers_to_midi.enc",      gen_v0c4_weak_name_defers_to_midi())
    write("structure_prec_page_letter.enc",                gen_v0c4_prec_page_letter())
    write("structure_prec_page_a3.enc",                    gen_v0c4_prec_page_ansi_a3())
    write("structure_prec_landscape_no_wini.enc",          gen_v0c4_prec_landscape_no_wini())
    write("structure_wini_large_margins_a3.enc",           gen_v0c4_wini_large_margins_a3())
    write("structure_non_octave_key_keeps_clef.enc",       gen_v0c4_non_octave_key_keeps_clef())
    write("structure_g_clef_key0_stays_plain.enc",         gen_v0c4_g_clef_key0_stays_plain())
    write("structure_c_clef_key_keeps_clef.enc",           gen_v0c4_c_clef_key_keeps_clef())
    write("structure_perc_clef_key_keeps_clef.enc",        gen_v0c4_perc_clef_key_keeps_clef())
    write("importer_mrest_followed_by_rest.enc",          gen_v0c4_mrest_followed_by_rest())
    write("importer_mrest_preceded_by_rest.enc",         gen_v0c4_mrest_preceded_by_rest())
    write("importer_mrest_multistaff.enc",               gen_v0c4_mrest_multistaff())
    write("importer_mrest_consecutive_groups.enc",       gen_v0c4_mrest_consecutive_groups())
    write("importer_gap_snap_eighth_meter.enc",           gen_v0c4_gap_snap_eighth_meter())
    write("importer_v0xa6_no_spurious_tremolo.enc",             gen_v0xa6_no_spurious_tremolo())
    write("importer_v0xa6_key_transposition.enc",               gen_v0xa6_key_transposition())
    write("importer_v0xa6_header_ends_at_0xa6.enc",             gen_v0xa6_header_ends_at_0xa6())
    write("importer_v0xa6_duplicate_rest_collapse.enc",         gen_v0xa6_duplicate_rest_collapse())
    write("importer_v0xa6_triplet_byte_at_offset_7.enc",        gen_v0xa6_triplet_byte_at_offset_7())
    write("structure_v0xa6_fermata.enc",                        gen_v0xa6_note_fermata_size11())
    write("importer_v0xa6_boda_like.enc",                       gen_v0xa6_boda_like())
    write("instruments_compact_tk_ignores_key_byte.enc",     gen_v0c4_compact_tk_ignores_key_byte())
    write("instruments_compact_short_header_no_midi.enc",   gen_v0c4_compact_short_header_no_midi())
    write("importer_header_measure_count_truncates_ghost_measures.enc",
          gen_v0c4_header_measure_count_truncates_ghost_measures())
    write("importer_volta_overlapping_bits.enc",          gen_v0c4_volta_overlapping_bits())
    write("importer_volta_coalesce_and_text.enc",         gen_v0c4_volta_coalesce_and_text())
    write("structure_volta_repeat_playback.enc",          gen_v0c4_volta_repeat_playback())
    write("structure_volta_repeat_playcount.enc",         gen_v0c4_volta_repeat_playcount())
    write("importer_to_coda_vs_coda_marker.enc",          gen_v0c4_to_coda_vs_coda_marker())
    write("text_text_block_latin1_decoding.enc",      gen_v0c4_text_block_latin1_decoding())
    write("importer_two_dynamics_in_one_measure.enc",     gen_v0c4_two_dynamics_in_one_measure())
    write("text_chord_sym_latin1.enc",                gen_v0c4_chord_sym_latin1())
    write("text_titl_latin1_small_varsize.enc",       gen_v0c4_titl_latin1_small_varsize())
    write("text_recovered_name_latin1.enc",           gen_v0c4_recovered_name_latin1())
    write("importer_hairpin_speguleco_bit0.enc",          gen_v0c4_hairpin_speguleco_bit0())
    write("importer_hairpin_ends_at_next_dynamic.enc",    gen_v0c4_hairpin_ends_at_next_dynamic())
    write("importer_slur_pixel_span.enc",                 gen_v0c4_slur_pixel_span())
    write("importer_slur_pixel_span_6_8.enc",             gen_v0c4_slur_pixel_span_6_8())
    write("importer_slur_xoffset_unsigned.enc",           gen_v0c4_slur_xoffset_unsigned())
    write("importer_slur_cross_measure_fallback.enc",     gen_v0c4_slur_cross_measure_fallback())
    write("importer_dyn_snap_back_by_xoffset.enc",        gen_v0c4_dyn_snap_back_by_xoffset())
    write("importer_wedge_snap_back_by_xoffset.enc",      gen_v0c4_wedge_snap_back_by_xoffset())
    write("importer_dyn_displaced_to_staff_above.enc",    gen_v0c4_dyn_displaced_to_staff_above())
    write("importer_v0xa6_grace_ongrid_snap_suppressed.enc",     gen_v0xa6_grace_ongrid_snap_suppressed())
    write("importer_v0xa6_inner_grace_group.enc",               gen_v0xa6_inner_grace_group())
    write("importer_v0xa6_grace_restores_face_value.enc",      gen_v0xa6_grace_restores_face_value())
    write("importer_hairpin_snapstart_at_barline.enc",    gen_v0c4_hairpin_snapstart_at_barline())
    write("importer_hairpin_endpoint_dynamic_wins.enc",   gen_v0c4_hairpin_endpoint_dynamic_wins())
    write("ornaments_tremolo_orn.enc",                     gen_v0c4_tremolo_orn())
    write("ornaments_fermata_not_in_tuplet.enc",           gen_v0c4_fermata_not_in_tuplet())
    write("ornaments_fermata_below_not_in_tuplet.enc",    gen_v0c4_fermata_below_not_in_tuplet())
    write("ornaments_tremolo_orn_no_tie.enc",             gen_v0c4_tremolo_orn_no_tie())
    write("ornaments_tremolo_orn_tied_from.enc",           gen_v0c4_tremolo_orn_tied_from())
    write("ornaments_tempo_sym_followtext.enc",            gen_v0c4_tempo_sym_followtext())
    write("importer_hairpin_barline_clamp.enc",           gen_v0c4_hairpin_barline_clamp())
    write("importer_dyn_dedup.enc",                       gen_v0c4_dyn_dedup())
    write("notes_partial_triplet_unreduced_cumtick.enc",  gen_v0c4_partial_triplet_unreduced_cumtick())
    write("ornaments_v0c2_orn_c4_accent.enc",      gen_v0c2_orn_c4_accent())
    write("ornaments_v0c2_cross_measure_slur.enc", gen_v0c2_cross_measure_slur())
    write("ornaments_v0c4_orn_be_accent.enc",      gen_v0c4_orn_be_accent())
    write("lyrics_v0c2_compound_meter.enc",        gen_v0c2_compound_meter_lyrics())
    write("lyrics_rest_does_not_shift_notes.enc",  gen_v0c2_lyrics_rest_does_not_shift_notes())
    write("zbot_single_note.enc",              gen_zbot_single_note())
    write("zbo6_from_sco5.enc",           gen_zbo6_from_sco5())
    write("zbot_from_bazo.enc",                 gen_zbot_from_bazo())
    write("zbot_family_40x.enc",               gen_zbot_family_40x())
    write("sintetico_all_features.enc",          gen_sintetico_all_features())
    write("notes_multiinstr_compact_routing.enc", gen_v0c4_multiinstr_compact_routing())
    write("structure_sco5_macos.enc",             gen_sco5_macos_page_setup())
    write("instruments_sco5_tk_names.enc",         gen_sco5_tk_instrument_names(), layout=False)
    write("notes_sco5_tie_arc_bigendian.enc",       gen_sco5_tie_arc_bigendian(), layout=False)
    write("ornaments_sco5_bigendian.enc",          gen_sco5_ornaments_and_rest(), layout=False)
    write("text_lyrics_grandstaff_routed_notes.enc", gen_v0c4_lyrics_grandstaff_routed_notes())
    write("importer_inner_tuplet_note_level_cap.enc", gen_v0c4_inner_tuplet_note_level_cap())
    write("importer_score_size2.enc", set_line_staff_size_hint(set_score_size(assemble(0xC4, [(meas_hdr(4, 4),
        note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker())], fill_ts=(4, 4)), sz=2), sz0indexed=1))
    write("importer_score_size3.enc", set_score_size(assemble(0xC4, [(meas_hdr(4, 4),
        note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker())], fill_ts=(4, 4)), sz=3))
    # Encore 4.x (version=775): size from LINE staff entry byte[13], NOT header 0x52.
    # 0x52 stores an unrelated field; values 1-8 map irregularly to Size 1-4.
    # byte[13]=1 (0-indexed) -> Size=2 -> 70%; byte[13]=2 -> Size=3 -> 75%.
    _enc4x_base = assemble(0xC4, [(meas_hdr(4, 4),
        note_v0c4(0, 0, 0, fv=3, pitch=60) + end_marker())], fill_ts=(4, 4))
    _enc4x_base = set_version(_enc4x_base, 775)     # Encore 4.x app version
    _enc4x_base = set_score_size(_enc4x_base, 8)    # 0x52=8: unrelated field, would give wrong scale if used
    write("importer_enc4x_line_size2_70pct.enc",
          set_line_staff_size_hint(_enc4x_base, sz0indexed=1))  # byte[13]=1 -> Size=2 -> 70%
    write("importer_enc4x_line_size3_75pct.enc",
          set_line_staff_size_hint(_enc4x_base, sz0indexed=2))  # byte[13]=2 -> Size=3 -> 75%
    write("ornaments_v0c2_same_measure_slur_no_cross.enc", gen_v0c2_same_measure_slur_no_cross())
    write("ornaments_v0c2_unreliable_slur_count.enc", gen_v0c2_unreliable_slur_count())
    write("ornaments_multiinstr_slur_routing.enc",         gen_v0c4_multiinstr_slur_routing())
    write("ornaments_v0c2_slur_firstnote_xoff_mismatch.enc", gen_v0c2_slur_firstnote_xoff_mismatch())
    write("notes_v0c2_multiinstr_compact_routing.enc",       gen_v0c2_multiinstr_compact_routing())
    write("ornaments_v0c2_multiinstr_slur_routing.enc",      gen_v0c2_multiinstr_slur_routing())
    write("structure_pickup_measure.enc",                    gen_v0c4_pickup_measure())
    write("structure_pickup_measure_same_ts.enc",            gen_v0c4_pickup_measure_same_ts())
    write("notes_implicit_trailing_gap.enc",                 gen_v0c4_implicit_trailing_gap())
    write("structure_pickup_caseb_reduces.enc",              gen_v0c4_pickup_caseb_reduces())
    write("structure_pickup_caseb_no_reduce_full.enc",       gen_v0c4_pickup_caseb_no_reduce_full())
    write("structure_pickup_casea_sparse.enc",               gen_v0c4_pickup_casea_sparse())
    write("structure_pickup_caseb_hairpin.enc",              gen_v0c4_pickup_caseb_hairpin())
    write("timesig_change_6_8_to_3_4.enc",                  gen_v0c4_timesig_change_6_8_to_3_4())
    write("notes_triplet_orphan_missing_tup.enc",           gen_v0c4_triplet_orphan_missing_tup())
    write("importer_2_2_beatticks240_gap_snap.enc",         gen_v0c4_2_2_beatticks240_gap_snap())
    write("importer_2_2_beatticks480_correct.enc",          gen_v0c4_2_2_beatticks480_correct_encoding())
    write("instruments_gm_perc_range_taiko.enc",            gen_v0c4_gm_perc_range_taiko())
    write("instruments_gm_perc_chord_notes.enc",            gen_v0c4_gm_perc_chord_notes())
    write("notes_16th_rdur112_no_triple_dot.enc",           gen_v0c4_16th_rdur112_no_triple_dot())
    write("timesig_change_2_2_to_4_4.enc",                  gen_v0c4_timesig_change_2_2_to_4_4())
    write("notes_triplet_orphan_prior_complete_group.enc",  gen_v0c4_triplet_orphan_prior_complete_group())
    write("ornaments_accent_sibling_no_spillover.enc",       gen_v0c4_accent_sibling_no_spillover())
    write("instruments_bass_enckey0_no_octave_transpos.enc", gen_v0c4_bass_enckey0_no_octave_transpos())
    write("ornaments_accent_nonzero_voice.enc",             gen_v0c4_accent_nonzero_voice())
    write("ornaments_accent_offset_tick_nonzero_voice.enc", gen_v0c4_accent_offset_tick_nonzero_voice())
    write("notes_dual_rests_same_tick_routing.enc",         gen_v0c4_dual_rests_same_tick_routing())
    write("ornaments_cross_measure_slur_precision.enc",    gen_v0c4_cross_measure_slur_precision())
    write("ornaments_new_artic_types.enc",                 gen_v0c4_new_artic_types())
    write("ornaments_staccatissimo_orns.enc",              gen_v0c4_staccatissimo_orns())
    write("ornaments_tremolo_r8_r16_r64.enc",              gen_v0c4_tremolo_r8_r16_r64())
    write("ornaments_graphic_line_skipped.enc",            gen_v0c4_graphic_line_skipped())
    write("notes_string_num_orn_no_dup.enc",              gen_v0c4_string_num_orn_no_dup())
    write("structure_wini_screen_pixel_a4.enc",           gen_wini_screen_pixel_a4())
    write("notes_v0c2_plain_sixteenth_no_spurious_dot.enc", gen_v0c2_plain_sixteenth_no_spurious_dot())
    write("notes_v0c2_full_measure_no_false_dot.enc",      gen_v0c2_full_measure_no_false_dot())
    write("instruments_c2_no_tilde_compact_names_midi.enc", gen_v0c2_no_tilde_compact_names_midi())
    write("instruments_c2_tilde_primary_block_midi.enc",    gen_v0c2_tilde_primary_block_midi())
    write("importer_mrest_single_block.enc",               gen_v0c4_mrest_single_block())
    write("bazo_left_100.enc",                             gen_bazo_left_100())
    write("bazo_top_100.enc",                              gen_bazo_top_100())
    write("importer_hairpin_xoffset2_snap.enc",            gen_v0c4_hairpin_xoffset2_snap())
    write("ornaments_breath_and_caesura.enc",              gen_v0c4_breath_and_caesura())
    write("ornaments_new_artic_bytes.enc",                 gen_v0c4_new_artic_bytes())
    write("ornaments_standalone_trill_end.enc",            gen_v0c4_standalone_trill_end())
    write("ornaments_measure_repeat.enc",                  gen_v0c4_measure_repeat())
    write("ornaments_trill_with_accidentals.enc",          gen_v0c4_trill_with_accidentals())
    write("ornaments_open_string_and_stick.enc",           gen_v0c4_open_string_and_stick())
    write("ornaments_trill_alt_standalone.enc",            gen_v0c4_trill_alt_standalone())
    write("notes_artic_dedup_trill_on_chord.enc",          gen_v0c4_artic_dedup_trill_on_chord())
    write("ornaments_accent_tick0_xoffset.enc",            gen_v0c4_accent_tick0_xoffset())
    write("notes_chord_strum_xoffset.enc",                 gen_v0c4_chord_strum_xoffset())
    write("notes_diff_column_no_merge.enc",                gen_v0c4_diff_column_no_merge())
    write("notes_tuplet_diff_column_keeps_members.enc",    gen_v0c4_tuplet_diff_column())
    write("ornaments_bowing_tick0_xoffset_mismatch.enc",    gen_v0c4_bowing_tick0_xoffset_mismatch())
    write("ornaments_fingering_grandstaff.enc",            gen_v0c4_fingering_grandstaff())
    write("ornaments_fingering_multivoice.enc",            gen_v0c4_fingering_multivoice())
    write("notes_chord_inflated_rdur_keeps_eighth.enc", gen_v0c4_chord_inflated_rdur_keeps_eighth())
    write("notes_chord_symbol_large_drift.enc", gen_v0c4_chord_symbol_large_drift())
    write("notes_chord_symbol_text_bounded.enc", gen_v0c4_chord_symbol_text_bounded_by_element())
    write("notes_chord_symbol_nearbeat_subdivision.enc", gen_v0c4_chord_symbol_nearbeat_subdivision())
    write("notes_chord_symbol_snap_to_beat1.enc", gen_v0c4_chord_symbol_snap_to_beat1())
    write("notes_chord_symbol_fretboard.enc", gen_v0c4_chord_symbol_fretboard())
    write("text_chord_quality_table.enc", gen_v0c4_chord_quality_table())
    write("notes_cross_staff_false_nesting.enc", gen_v0c4_cross_staff_false_nesting())
    write("notes_dotted_ctrl_bit0_drift.enc", gen_v0c4_dotted_ctrl_bit0_drift())
    write("notes_grandstaff_bit6_second_staff.enc", gen_v0c4_grandstaff_bit6_second_staff())
    write("notes_grandstaff_high_voice_own_staff.enc", gen_v0c4_grandstaff_high_voice_own_staff())
    write("notes_singlestaff_voice4_second_voice.enc", gen_v0c4_singlestaff_voice4_second_voice())
    write("notes_grandstaff_staffwithin_fermata.enc", gen_v0c4_grandstaff_staffwithin_fermata())
    write("notes_grandstaff_staffwithin_four_voices.enc", gen_v0c4_grandstaff_staffwithin_four_voices())
    write("notes_grandstaff_staffwithin_rest_on_second_staff.enc", gen_v0c4_grandstaff_staffwithin_rest_on_second_staff())
    write("notes_grandstaff_staffwithin_sequential.enc", gen_v0c4_grandstaff_staffwithin_sequential())
    write("notes_grandstaff_staffwithin_tie_on_second_staff.enc", gen_v0c4_grandstaff_staffwithin_tie_on_second_staff())
    write("structure_grandstaff_wedge_out_of_range_voice.enc", gen_v0c4_grandstaff_wedge_out_of_range_voice(), layout=False)
    write("structure_hostile_zero_tuplet_nibble.enc", gen_v0c4_hostile_zero_tuplet_nibble(), layout=False)
    write("structure_hostile_out_of_range_staff.enc", gen_v0c4_hostile_out_of_range_staff(), layout=False)
    write("structure_hostile_out_of_range_voice.enc", gen_v0c4_hostile_out_of_range_voice(), layout=False)
    write("structure_hostile_zero_size_element.enc", gen_v0c4_hostile_zero_size_element(), layout=False)
    write("notes_no_spurious_string_numbers.enc", gen_v0c4_no_spurious_string_numbers())
    write("notes_scale_no_anchor_no_circles.enc", gen_v0c4_scale_no_anchor_no_circles())
    write("notes_scale_string_numbers_anchor.enc", gen_v0c4_scale_string_numbers_anchor())
    write("notes_segment_no_override_clean_multiple.enc", gen_v0c4_segment_no_override_clean_multiple())
    write("notes_segment_override_12plus2.enc", gen_v0c4_segment_override_12plus2())
    write("notes_segment_override_15notes.enc", gen_v0c4_segment_override_15notes())
    write("notes_tie_dir_02.enc", gen_v0c4_tie_dir_02())
    write("notes_tie_dir_03.enc", gen_v0c4_tie_dir_03())
    write("notes_tie_partial_chord_source_position.enc", gen_v0c4_tie_partial_chord_source_position())
    write("notes_tuplet_9_4_nontuplet.enc", gen_v0c4_tuplet_9_4_nontuplet())
    write("notes_tuplet_dosillo_2_1.enc", gen_v0c4_tuplet_dosillo_2_1())
    write("notes_tuplet_last_note_short_rdur.enc", gen_v0c4_tuplet_last_note_short_rdur())
    write("notes_tuplet_no_gapsnap_spurious_rest.enc", gen_v0c4_tuplet_no_gapsnap_spurious_rest())
    write("notes_voice_overflow_dropped.enc", gen_v0c4_voice_overflow_dropped())
    write("ornaments_tremolo_orn_crossvoice.enc", gen_v0c4_tremolo_orn_crossvoice())
    write("ornaments_tuplet_mixed_baseLen.enc", gen_v0c4_tuplet_mixed_baseLen())
    write("ornaments_v0c2_grace_slur_to_main_coloc.enc", gen_v0c2_grace_slur_to_main_coloc(), layout=False)
    write("ornaments_v0c4_grace_after_main_grace_to_later.enc", gen_v0c4_grace_after_main_grace_to_later())
    write("ornaments_v0c4_grace_after_main_in_binary.enc", gen_v0c4_grace_after_main_in_binary())
    write("ornaments_v0c4_grace_after_main_preceding_notes.enc", gen_v0c4_grace_after_main_preceding_notes())
    write("ornaments_v0c4_grace_after_main_slur_to_main.enc", gen_v0c4_grace_after_main_slur_to_main())
    write("ornaments_v0c4_grace_slur_to_main_coloc.enc", gen_v0c4_grace_slur_to_main_coloc())
    write("rest_dotted_before_notes.enc", gen_v0c4_rest_dotted_before_notes())
    write("ornaments_accent_at_note_end_tick.enc",         gen_v0c4_accent_at_note_end_tick(), layout=False)
    write("notes_last_note_longer_than_space.enc",         gen_v0c4_last_note_drawn_longer_than_its_space())
    write("notes_v0c4_dotted_hint_fills_bar.enc",          gen_v0c4_dotted_hint_fills_bar())
    write("notes_v0c2_dotted_hint_fills_bar.enc",          gen_v0c2_dotted_hint_fills_bar(), layout=False)
    write("tuplet_4to3_quadruplet.enc", gen_v0c4_4to3_quadruplet())
    write("instruments_abbreviated_name_bandurr.enc",      gen_v0c4_instr_abbreviated_name_bandurr())
    write("instruments_compact_no_tk_midi_oboe.enc",       gen_v0c4_instr_compact_no_tk_midi_oboe())
    write("instruments_instr_empty_name_midi_cello.enc",   gen_v0c4_instr_empty_name_midi_cello())
    write("instruments_no_tk_large_tk_two_names.enc",      gen_v0c4_instr_no_tk_large_tk_two_names())
    write("instruments_rhythm_staff_snare.enc",            gen_v0c4_instr_rhythm_staff_snare())
    write("importer_perc_bateria.enc",                     gen_v0c4_instr_perc_bateria())
    write("importer_transp_oboe_jota.enc",                 gen_v0c4_instr_transp_oboe_jota())
    write("options_overfill_irregular_facevalue.enc",      gen_v0c4_overfill_irregular_facevalue())
    write("options_overfill_irregular_emitdrop.enc",       gen_v0c4_overfill_irregular_emitdrop())
    write("options_overfill_irregular_twostaves.enc",      gen_v0c4_overfill_irregular_twostaves())
    write("options_underfill_irregular_empty_staff.enc",   gen_v0c4_underfill_irregular_sparse_with_empty_staff())
    write("structure_clef_change_mid_measure.enc",         gen_v0c4_structure_clef_change_mid_measure())
    write("structure_clef_trailing_cautionary.enc",        gen_v0c4_structure_clef_trailing_cautionary())
    write("structure_page_break.enc",                      gen_v0c4_structure_page_break())
    write("structure_page_break_mcount_zero.enc",          gen_v0c4_structure_page_break_mcount_zero())
    write("structure_page_break_spill.enc",                gen_v0c4_page_break_spill())
    write("grace_ornament.enc",                            gen_v0c4_grace_ornament())
    write("structure_pickup_casea_volta.enc",              gen_v0c4_structure_pickup_casea_volta())
    write("text_lyrics_6_8_offset_ticks.enc",             gen_v0c4_lyrics_6_8_offset_ticks())
    write("text_orn_tempo_eighth_beat_not_suppressed.enc", gen_v0c4_orn_tempo_eighth_beat_not_suppressed())
    write("text_tempo_orn_compound_68.enc",                gen_v0c4_tempo_orn_compound_68())
    write("text_titl_empty_second_block.enc",              gen_v0c4_titl_empty_second_block())
    write("ornaments_ottava_two_spanners.enc",             gen_v0c4_ottava_two_spanners())
    write("instruments_tk_index_gap.enc",                   gen_tk_index_gap())
    write("instruments_entry_table_names.enc",              gen_entry_table_names())
    write("instruments_large_entry_declared_small.enc",     gen_large_entry_declared_small())
    write("instruments_entry_shorter_than_declared.enc",    gen_entry_shorter_than_declared())
    write("instruments_declared_size_overshoots_entry.enc", gen_declared_size_overshoots_entry())
    write("instruments_tk_magic_digits_unreliable.enc",     gen_tk_magic_digits_unreliable())
    write("notes_tuplet_flat_group_not_nested.enc",         gen_tuplet_flat_group_not_nested())
    write("structure_wide_score_first_page.enc",            gen_wide_score_first_page())
    write("text_copyright_lines_one_byte.enc",              gen_copyright_lines_one_byte())
    write("structure_volta_short_last_measure.enc",       gen_v0c4_volta_short_last_measure())
    write("structure_prec_page_stub.enc",                  gen_v0c4_prec_page_stub())
    write("structure_v0xa6_staff_clefs.enc",               gen_v0xa6_staff_clefs())
    write("structure_v0xa6_per_staff_size.enc",            gen_v0xa6_per_staff_size())
    write("notes_notehead_without_drumset.enc",            gen_v0c4_notehead_without_drumset())
    write("notes_v0xa6_notehead_cross.enc",                gen_v0xa6_notehead_cross())
    write("instruments_drumset_name_vs_program.enc",       gen_v0xa6_drumset_name_vs_program())
    write("instruments_v0xa6_percussion_channel.enc",      gen_v0xa6_percussion_channel())
    write("instruments_v0xc4_percussion_channel.enc",      gen_v0c4_percussion_channel())
    write("structure_scor_container.enc",                  gen_scor_container())
    write("structure_musictime_3_07.mus",                  gen_musictime_3_07())
    print("Done.")
