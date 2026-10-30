#!/usr/bin/env python3
"""
pack_sars.py - pack the SARS analog sounding folders (sars/hail/*, sars/supercell/*)
into ONE archive file, so a repository holds a single file instead of thousands.

    python pack_sars.py pack   <sars_dir> <out.sarspack>   # sars_dir contains hail/ and supercell/
    python pack_sars.py unpack <in.sarspack> <out_dir>     # recreate hail/ and supercell/
    python pack_sars.py list   <in.sarspack>               # show contents
    python pack_sars.py verify <sars_dir> <in.sarspack>    # confirm the archive matches the folders byte-for-byte

Format ("SARSPK1", all integers big-endian) - easy to read from C++ with QFile + qUncompress:

    8 bytes   magic  "SARSPK1\\0"
    u32       entry count N
    N x entry:
        u8   type        0 = hail, 1 = supercell
        u16  name length
        ...  name        UTF-8 file name (e.g. "95052300.DDC")
        u32  offset      from the start of the file to the blob
        u32  csize       stored size
        u32  usize       original size
    blobs (back to back). Each blob is  u32(usize, big-endian) + zlib stream  -  exactly what Qt's
    qCompress()/qUncompress() use, so `qUncompress(blob)` returns the original file.

The archive is deterministic (sorted entries, no timestamps, fixed compression level), so
re-packing unchanged data produces an identical file and git doesn't see a diff.
Only the standard library is needed.
"""
import os
import struct
import sys
import zlib

MAGIC = b"SARSPK1\0"
TYPES = {"hail": 0, "supercell": 1}
NAMES = {v: k for k, v in TYPES.items()}


def collect(sars_dir):
    entries = []
    for tname, tid in TYPES.items():
        d = os.path.join(sars_dir, tname)
        if not os.path.isdir(d):
            print("warning: %s not found, skipping" % d, file=sys.stderr)
            continue
        for fn in sorted(os.listdir(d)):
            p = os.path.join(d, fn)
            if os.path.isfile(p):
                entries.append((tid, fn, p))
    entries.sort(key=lambda e: (e[0], e[1]))
    return entries


def pack(sars_dir, out):
    entries = collect(sars_dir)
    if not entries:
        sys.exit("no files found under %s/{hail,supercell}" % sars_dir)
    blobs, meta = [], []
    for tid, fn, p in entries:
        raw = open(p, "rb").read()
        blob = struct.pack(">I", len(raw)) + zlib.compress(raw, 9)
        blobs.append(blob)
        meta.append((tid, fn.encode("utf-8"), len(blob), len(raw)))
    index_size = 12 + sum(1 + 2 + len(m[1]) + 12 for m in meta)
    off = index_size
    head = bytearray(MAGIC + struct.pack(">I", len(meta)))
    for (tid, nb, cs, us) in meta:
        head += struct.pack(">BH", tid, len(nb)) + nb + struct.pack(">III", off, cs, us)
        off += cs
    assert len(head) == index_size
    with open(out, "wb") as f:
        f.write(head)
        for b in blobs:
            f.write(b)
    raw_total = sum(m[3] for m in meta)
    print("packed %d files (%d hail, %d supercell): %.1f MB -> %.1f MB  (%s)" % (
        len(meta), sum(1 for m in meta if m[0] == 0), sum(1 for m in meta if m[0] == 1),
        raw_total / 1e6, os.path.getsize(out) / 1e6, out))


def read_index(path):
    data = open(path, "rb").read()
    if data[:8] != MAGIC:
        sys.exit("%s is not a SARSPK1 archive" % path)
    n = struct.unpack_from(">I", data, 8)[0]
    pos, items = 12, []
    for _ in range(n):
        tid, nl = struct.unpack_from(">BH", data, pos); pos += 3
        name = data[pos:pos + nl].decode("utf-8"); pos += nl
        off, cs, us = struct.unpack_from(">III", data, pos); pos += 12
        items.append((tid, name, off, cs, us))
    return data, items


def blob_data(data, off, cs, us):
    blob = data[off:off + cs]
    if struct.unpack(">I", blob[:4])[0] != us:
        sys.exit("corrupt entry (size header mismatch)")
    raw = zlib.decompress(blob[4:])
    if len(raw) != us:
        sys.exit("corrupt entry (length mismatch)")
    return raw


def unpack(path, out_dir):
    data, items = read_index(path)
    for tid, name, off, cs, us in items:
        d = os.path.join(out_dir, NAMES[tid])
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, name), "wb") as f:
            f.write(blob_data(data, off, cs, us))
    print("unpacked %d files into %s" % (len(items), out_dir))


def list_(path):
    data, items = read_index(path)
    for tid, name, off, cs, us in items:
        print("%-10s %-24s %8d -> %8d" % (NAMES[tid], name, us, cs))
    print("%d entries" % len(items))


def verify(sars_dir, path):
    data, items = read_index(path)
    on_disk = {(TYPES[t], fn) for t in TYPES for fn in
               (os.listdir(os.path.join(sars_dir, t)) if os.path.isdir(os.path.join(sars_dir, t)) else [])
               if os.path.isfile(os.path.join(sars_dir, t, fn))}
    packed = {(t, n) for t, n, *_ in items}
    bad = 0
    if on_disk != packed:
        print("file lists differ: %d only on disk, %d only in archive" % (len(on_disk - packed), len(packed - on_disk)))
        bad = 1
    for tid, name, off, cs, us in items:
        p = os.path.join(sars_dir, NAMES[tid], name)
        if os.path.isfile(p) and open(p, "rb").read() != blob_data(data, off, cs, us):
            print("MISMATCH:", p); bad = 1
    print("OK - archive matches the folders" if not bad else "verification FAILED")
    sys.exit(bad)


if __name__ == "__main__":
    a = sys.argv[1:]
    if len(a) == 3 and a[0] == "pack": pack(a[1], a[2])
    elif len(a) == 3 and a[0] == "unpack": unpack(a[1], a[2])
    elif len(a) == 2 and a[0] == "list": list_(a[1])
    elif len(a) == 3 and a[0] == "verify": verify(a[1], a[2])
    else:
        print(__doc__); sys.exit(2)
