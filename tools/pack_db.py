#!/usr/bin/env python3
"""
pack_db.py - pack the whole data/ tree (datasources, SARS analog soundings + tables, shapefiles, sharptab climatology)
into ONE archive, so a repository holds a single file instead of thousands.

    python pack_db.py pack   <data_dir> <out.dbpack>   # every file under data_dir (except *.dbpack / *.sarspack)
    python pack_db.py unpack <in.dbpack|in.sarspack> <out_dir>
    python pack_db.py list   <in.dbpack|in.sarspack>
    python pack_db.py verify <data_dir> <in.dbpack>

Typical layout of <data_dir>:
    datasources/*.xml *.csv     sars/hail/*  sars/supercell/*  sars/sars_hail.txt  sars/sars_supercell.txt
    shapefiles/*.dat            sharptab/*.txt

Format "DBPACK1" (big-endian), readable from C++ with QFile + qUncompress:
    8 bytes  magic "DBPACK1\\0"
    u32      entry count N
    N x entry: u16 path length, path (UTF-8, '/' separated, relative to data/), u32 offset, u32 csize, u32 usize
    blobs back to back; each blob = u32(usize, big-endian) + zlib stream (exactly qCompress()'s layout).
Deterministic (sorted, no timestamps, fixed level), so repacking unchanged data gives an identical file.
`unpack`/`list` also understand the older SARSPK1 archives (hail/ -> sars/hail/, supercell/ -> sars/supercell/,
datasources -> datasources/). Standard library only.
"""
import os, struct, sys, zlib

MAGIC, OLD = b"DBPACK1\0", b"SARSPK1\0"
SKIP = (".dbpack", ".sarspack")


def collect(root):
    out = []
    for d, dirs, files in os.walk(root):
        dirs.sort()
        for fn in sorted(files):
            if fn.endswith(SKIP) or fn.startswith("."):
                continue
            p = os.path.join(d, fn)
            out.append((os.path.relpath(p, root).replace(os.sep, "/"), p))
    out.sort()
    return out


def pack(root, out):
    entries = collect(root)
    if not entries:
        sys.exit("no files found under %s" % root)
    blobs, meta = [], []
    for rel, p in entries:
        raw = open(p, "rb").read()
        blobs.append(struct.pack(">I", len(raw)) + zlib.compress(raw, 9))
        meta.append((rel.encode("utf-8"), len(blobs[-1]), len(raw)))
    index = 12 + sum(2 + len(m[0]) + 12 for m in meta)
    off, head = index, bytearray(MAGIC + struct.pack(">I", len(meta)))
    for nb, cs, us in meta:
        head += struct.pack(">H", len(nb)) + nb + struct.pack(">III", off, cs, us)
        off += cs
    assert len(head) == index
    with open(out, "wb") as f:
        f.write(head)
        for b in blobs:
            f.write(b)
    print("packed %d files: %.1f MB -> %.1f MB  (%s)" % (len(meta), sum(m[2] for m in meta) / 1e6, os.path.getsize(out) / 1e6, out))


def read_index(path):
    data = open(path, "rb").read()
    magic = data[:8]
    if magic not in (MAGIC, OLD):
        sys.exit("%s is not a DBPACK1/SARSPK1 archive" % path)
    n, pos, items = struct.unpack_from(">I", data, 8)[0], 12, []
    for _ in range(n):
        t = None
        if magic == OLD:
            t = data[pos]; pos += 1
        nl = struct.unpack_from(">H", data, pos)[0]; pos += 2
        name = data[pos:pos + nl].decode("utf-8"); pos += nl
        off, cs, us = struct.unpack_from(">III", data, pos); pos += 12
        if t is not None:
            name = ("sars/hail/", "sars/supercell/", "datasources/")[t] + name
        items.append((name, off, cs, us))
    return data, items


def blob(data, off, cs, us):
    b = data[off:off + cs]
    if struct.unpack(">I", b[:4])[0] != us:
        sys.exit("corrupt entry (size header mismatch)")
    raw = zlib.decompress(b[4:])
    if len(raw) != us:
        sys.exit("corrupt entry (length mismatch)")
    return raw


def unpack(path, out_dir):
    data, items = read_index(path)
    for name, off, cs, us in items:
        dst = os.path.join(out_dir, *name.split("/"))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        open(dst, "wb").write(blob(data, off, cs, us))
    print("unpacked %d files into %s" % (len(items), out_dir))


def list_(path):
    data, items = read_index(path)
    for name, off, cs, us in items:
        print("%-48s %10d -> %9d" % (name, us, cs))
    print("%d entries" % len(items))


def verify(root, path):
    data, items = read_index(path)
    on_disk = {rel for rel, _ in collect(root)}
    packed = {n for n, *_ in items}
    bad = 0
    if on_disk != packed:
        print("file lists differ: %d only on disk, %d only in archive" % (len(on_disk - packed), len(packed - on_disk))); bad = 1
    for name, off, cs, us in items:
        p = os.path.join(root, *name.split("/"))
        if os.path.isfile(p) and open(p, "rb").read() != blob(data, off, cs, us):
            print("MISMATCH:", name); bad = 1
    print("OK - archive matches the folder" if not bad else "verification FAILED")
    sys.exit(bad)


if __name__ == "__main__":
    a = sys.argv[1:]
    if len(a) == 3 and a[0] == "pack": pack(a[1], a[2])
    elif len(a) == 3 and a[0] == "unpack": unpack(a[1], a[2])
    elif len(a) == 2 and a[0] == "list": list_(a[1])
    elif len(a) == 3 and a[0] == "verify": verify(a[1], a[2])
    else:
        print(__doc__); sys.exit(2)
