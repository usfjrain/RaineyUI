#!/usr/bin/env python3
"""Retarget GLIBC_2.34+ and GLIBCXX_3.4.29+ version needs in an aarch64 ELF .so to GLIBC_2.17.

The TrimUI Brick runs glibc 2.33. Recent libretro buildbot cores reference a
handful of pthread/dl/sem/libm symbols at GLIBC_2.34/2.35 only because those
symbols moved into libc.so.6 in 2.34. On 2.33 the same functions exist at
GLIBC_2.17 in libpthread/libdl/libm, which RaineyUI loads into the global scope
before dlopen()ing a core. glibc's symbol matcher compares only version
name+hash (not the providing file), so rewriting the vernaux entries is enough.

GLIBCXX_3.4.29+ needs (only std::__throw_bad_array_new_length in practice) are
retargeted to GLIBCXX_3.4; RaineyUI exports a stand-in for that symbol.

usage: glibc_downgrade.py in.so out.so
"""
import struct
import sys

TARGET = b"GLIBC_2.17"


def elf_hash(name: bytes) -> int:
    h = 0
    for c in name:
        h = ((h << 4) + c) & 0xFFFFFFFF
        g = h & 0xF0000000
        if g:
            h ^= g >> 24
        h &= ~g & 0xFFFFFFFF
    return h


def find_str(dynstr, s):
    off = dynstr.find(s + b"\0")
    while off > 0 and dynstr[off - 1] != 0:  # must start a string
        off = dynstr.find(s + b"\0", off + 1)
    return off


def main(src, dst):
    data = bytearray(open(src, "rb").read())
    assert data[:4] == b"\x7fELF" and data[4] == 2 and data[5] == 1, "need ELF64 LE"
    e_shoff, = struct.unpack_from("<Q", data, 0x28)
    e_shentsize, e_shnum = struct.unpack_from("<HH", data, 0x3A)
    sections = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info = \
            struct.unpack_from("<IIQQQQII", data, off)
        sections.append((sh_type, sh_offset, sh_size, sh_link, sh_info))
    vr = [s for s in sections if s[0] == 0x6FFFFFFE]
    if not vr:
        print("no verneed section; copying unchanged")
        open(dst, "wb").write(data)
        return
    _, vr_off, _, vr_link, vr_cnt = vr[0]
    dynstr_off = sections[vr_link][1]
    dynstr_size = sections[vr_link][2]
    dynstr = bytes(data[dynstr_off:dynstr_off + dynstr_size])

    def s_at(o):
        return dynstr[o:dynstr.index(b"\0", o)]

    patched = 0
    p = vr_off
    for _ in range(vr_cnt):
        vn_version, vn_cnt, vn_file, vn_aux, vn_next = struct.unpack_from("<HHIII", data, p)
        a = p + vn_aux
        for _ in range(vn_cnt):
            vna_hash, vna_flags, vna_other, vna_name, vna_next = struct.unpack_from("<IHHII", data, a)
            name = s_at(vna_name)
            target = None
            if name.startswith(b"GLIBC_2."):
                minor = name[len(b"GLIBC_2."):]
                if minor.isdigit() and int(minor) > 33:
                    target = TARGET
            elif name.startswith(b"GLIBCXX_3.4."):
                minor = name[len(b"GLIBCXX_3.4."):]
                if minor.isdigit() and int(minor) > 28:
                    target = b"GLIBCXX_3.4"
            if target is not None:
                if True:
                    toff = find_str(dynstr, target)
                    if toff < 0:
                        # overwrite the string in place (target is never longer)
                        assert len(target) <= len(name)
                        pad = target + b"\0" * (len(name) - len(target))
                        data[dynstr_off + vna_name:dynstr_off + vna_name + len(name)] = pad
                        toff = vna_name
                    struct.pack_into("<IHHII", data, a, elf_hash(target), vna_flags & ~0x2,
                                     vna_other, toff, vna_next)
                    print(f"  {s_at(vn_file).decode()}: {name.decode()} -> {target.decode()}")
                    patched += 1
            if not vna_next:
                break
            a += vna_next
        if not vn_next:
            break
        p += vn_next
    open(dst, "wb").write(data)
    print(f"patched {patched} version need(s)")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
