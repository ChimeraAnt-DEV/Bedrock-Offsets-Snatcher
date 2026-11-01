#!/usr/bin/env python3
"""Build a tiny but real ELF64 arm64 fixture that mimics the shape of
libminecraftpe.so: PT_LOAD segments plus .text / .rodata / .data.rel.ro
sections.

The scanner patterns embedded in the fixture are parsed directly out of
app/src/main/cpp/signatures.cpp, so the test binary always exercises the
exact pattern bytes the probe releases, and the fixture is regenerated
whenever signatures.cpp changes (run: python3 cpp-tests/build_fixture.py).
"""
import re
import struct
from pathlib import Path


def align(x, a):
    return (x + a - 1) & ~(a - 1)


def parse_signatures():
    """Extract (id, category, pattern-hex) tuples from signatures.cpp.

    Each signature is a push_back({...}) with the id as a string literal,
    the category as a second string literal, and the pattern as a third
    (possibly multi-line) string literal. We parse the raw tokens instead of
    the C++ so drift between the fixture and the probe set is impossible.
    """
    src = Path("app/src/main/cpp/signatures.cpp").read_text()
    out = []
    # Match each push_back({ "id", "category", "hex..." }) block.
    pat = re.compile(
        r'push_back\(\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*'
        r'((?:".*?"(?:\s+|,)?\s*)+?)\s*,?\s*0\s*\}\)',
        re.S)
    for m in pat.finditer(src):
        sig_id, cat = m.group(1), m.group(2)
        raw = m.group(3)
        tokens = re.findall(r'"((?:[^"\\]|\\.)*)"', raw)
        joined = "".join(tokens)
        joined = joined.replace("\\\"", '"')
        hexes = re.findall(r"[0-9a-fA-F]{2}|\?\?", joined)
        pattern = " ".join(hexes)
        out.append((sig_id, cat, pattern))
    return out


STRINGS = {
    # id suffix used by signatures.cpp ("string:" prefix stripped) -> bytes
    "Minecraft.WebSocket.Server": b"Minecraft.WebSocket.Server",
}


def hex_to_bytes(hexpat):
    """Expand a pattern string into bytes. '??' wildcards become 0xCC so the
    pattern keeps its full length and the scanner can match over them."""
    out = bytearray()
    for t in hexpat.split():
        if t == "??":
            out.append(0xCC)
        else:
            out.append(int(t, 16))
    return bytes(out)


# ---------------------------------------------------------------------------
sigs = parse_signatures()
assert sigs, "no signatures parsed - check signature format"
text_blocks = []          # list of (name, bytes)
text_index = {}           # name -> offset within text section
_text_cursor = 0


def add_text_block(name, data):
    global _text_cursor
    off = align(_text_cursor, 16)
    pad = off - _text_cursor
    if pad:
        text_blocks.append((None, b"\xcc" * pad))
    text_blocks.append((name, data))
    _text_cursor = off + len(data)
    text_index[name] = off
    return off


for sig_id, cat, hexpat in sigs:
    if cat == "text":
        add_text_block(sig_id, hex_to_bytes(hexpat))

text_section = b"".join(b for (_, b) in text_blocks)

# Virtual layout: load segments satisfy p_vaddr == p_offset (base address 0),
# matching production libminecraftpe.so, so each section's sh_addr equals its
# own file offset and the parser's fileOffsetToVaddr is the identity mapping.

# .rodata content: leading pad, the version string (read by no signature but
# kept realistic), the "Minecraft.WebSocket.Server" string guide.
_version_bytes = b"1.20.62\x00"
rodata = bytes(16) + _version_bytes + bytes(16)
for sig_id, cat, hexpat in sigs:
    if cat == "string":
        txt_id = sig_id[len("string:"):]
        if txt_id in STRINGS:
            rodata += STRINGS[txt_id] + b"\x00"
            rodata += bytes(align(len(rodata), 16) - len(rodata))

vtable_slots = [
    "ClientInstance::getLocalPlayer",
    "Actor::getPos",
    "Actor::getRot",
    "+Actor::getLevel",
    "Level::getHitResult",
    "Player::getDimensionId",
    "ItemStack::getItem",
    "Player::getCarriedItem",
    "+Level::getCurrentTick",
    "ClientInstance::getLevel",
    "+ClientInstance::getLocalPlayer_alias",
]

# ---- ELF64 layout ---------------------------------------------------------
EHDR_SZ = 64
PHDR_OFF = EHDR_SZ
PHDR_CNT = 3
PHDR_SZ = 56
SHDR_SZ = 64

strtab = (b"\x00.text\x00.rodata\x00.data.rel.ro\x00.dynsym\x00"
          b".dynstr\x00.shstrtab\x00")

cur = EHDR_SZ + PHDR_CNT * PHDR_SZ
shstr_off = cur
cur += len(strtab)
cur = align(cur, 4)

sym_tbl_len = 8 * 24
dynsym_off = cur
cur += sym_tbl_len

dynstr = b"\x00_getstr\x00_version\x00\x00"
dynstr_off = cur
cur += len(dynstr)

text_off = align(cur, 16)
cur = text_off + len(text_section)

rodata_off = align(cur, 16)
cur = rodata_off + len(rodata)

# --- .data.rel.ro content (needs text_off so vtable pointers are real) ---
relro_pad = 16
vt_bytes = b"".join(struct.pack("<Q", text_off + text_index[name])
                    for name in vtable_slots)

datarelro_off = align(cur, 8)
cur = datarelro_off + len(vt_bytes) + relro_pad

shoff = cur
SHDR_CNT = 7
total = align(shoff + SHDR_CNT * SHDR_SZ, 16)

# ---- program headers ------------------------------------------------------
ph = []
# PT_LOAD R+X covering the whole file at vaddr 0 (base == 0).
ph.append(struct.pack("<IIQQQQQQ", 1, 5, 0, 0, total, total, 0x1000, 0))
# Additional PT_LOAD entries are redundant but kept to test the base calc
# on multiple segments (all map vaddr 0 -> offset 0, so base stays 0).
ph.append(struct.pack("<IIQQQQQQ", 1, 4, 0, 0, total, total, 0x1000, 4))


def sec(name_off, typ, fl, vaddr, off, sz, align_, link=0, info=0, entsz=0):
    return struct.pack("<IIQQQQIIQQ",
                       name_off, typ, fl, vaddr, off, sz, link, info,
                       align_, entsz)


SHT_NULL = 0
SHT_PROGBITS = 1
SHT_STRTAB = 3
SHT_DYNSYM = 11


def nameof(name):
    return strtab.index(name.encode())


sections = [sec(0, SHT_NULL, 0, 0, 0, 0, 0)]
sections.append(sec(nameof(".text"), SHT_PROGBITS, 6, text_off, text_off,
                    len(text_section), 16))
sections.append(sec(nameof(".rodata"), SHT_PROGBITS, 2, rodata_off,
                    rodata_off, len(rodata), 16))
sections.append(sec(nameof(".data.rel.ro"), SHT_PROGBITS, 3, datarelro_off,
                    datarelro_off, len(vt_bytes) + relro_pad, 8, 0, 0, 8))
sections.append(sec(nameof(".dynsym"), SHT_DYNSYM, 2, 0, dynsym_off,
                    sym_tbl_len, 8, 5, 1, 24))
sections.append(sec(nameof(".dynstr"), SHT_STRTAB, 3, 0, dynstr_off,
                    len(dynstr), 1))
sections.append(sec(nameof(".shstrtab"), SHT_STRTAB, 0, 0, shstr_off,
                    len(strtab), 1))

e_shnum = len(sections)
shstr_idx = e_shnum - 1

e_ident = b"\x7fELF" + bytes([2, 1, 1, 0]) + b"\x00" * 8
ehdr = e_ident + struct.pack("<HHIQQQIHHHHHH",
                             2,        # e_type = ET_DYN
                             183,      # e_machine = EM_AARCH64
                             1,        # e_version
                             0,        # e_entry
                             EHDR_SZ,  # e_phoff
                             shoff,    # e_shoff
                             0,        # e_flags
                             EHDR_SZ,  # e_ehsize
                             PHDR_SZ,  # e_phentsize
                             PHDR_CNT, # e_phnum
                             SHDR_SZ,  # e_shentsize
                             e_shnum,  # e_shnum
                             shstr_idx)

img = bytearray(total)
img[0:64] = ehdr
for i, hdr in enumerate(ph):
    img[PHDR_OFF + i * PHDR_SZ: PHDR_OFF + (i + 1) * PHDR_SZ] = hdr
img[shstr_off:shstr_off + len(strtab)] = strtab
img[dynstr_off:dynstr_off + len(dynstr)] = dynstr
img[text_off:text_off + len(text_section)] = text_section
img[rodata_off:rodata_off + len(rodata)] = rodata
relro_n = len(vt_bytes) + relro_pad
img[datarelro_off:datarelro_off + len(vt_bytes)] = vt_bytes
st = b"".join(sections)
img[shoff:shoff + len(st)] = st

out = Path("cpp-tests/fixture_arm64.so")
out.write_bytes(img)

print(f"wrote {out} ({len(img)} bytes)")
print(f"text_off=0x{text_off:x} text_vaddr=0x{text_off:x} "
      f"text_len=0x{len(text_section):x}")
print(f"rodata_off=0x{rodata_off:x} datarelro_off=0x{datarelro_off:x} "
      f"slots={len(vtable_slots)}")