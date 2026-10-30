#!/usr/bin/env python3
# Dump an engine probe file as named fields. The MTXS dump (the stack an error
# screen was reached from) additionally classifies every stack word, because the
# question it answers is "which code called", and a bare hex word cannot say.
import struct, subprocess, sys

LABELS = {
    'MTXN.LOG': ['marker', 'mtx_params', 'mtx_depth()', '&mtx_params', '&mtx_engine_alloc',
                 'size', 'mainzone', 'mtx_starts', 'mtx_engine_depth()', 'ace_segment',
                 'doom_code_segment', 'doom_data_segment', '&mtx_engine_reserve', '-', '-',
                 'marker2'],
    'MTXW.LOG': ['marker', 'lumpinfo', 'numlumps', 'idx', 'li->fd', 'li->offset', 'li->size',
                 'dest', 'limit', 'ret', 'lumpcache', '&li', 'depth', 'mainzone',
                 'li off in lumpinfo', 'marker2',
                 'pool->base', 'pool->size', 'pool->used', 'pool->served', 'pool->refusals',
                 'pool->reclaimed', 'pool->leaked', 'pool->invalid'],
    'MTXL.LOG': ['marker', 'site', 'idx', 'value', 'numlumps', 'aux', 'scheduler depth',
                 'marker2'],
    'MTXA.LOG': ['marker', 'size', 'caller', 'self', 'numlumps', 'mainzone',
                 'scheduler depth', 'marker2'],
    'MTXS.LOG': ['marker', 'esp', 'ret (into I_Error caller)', 'fmt', 'value',
                 'depth (mtx_engine_depth)', 'numlumps', 'doom_code_segment',
                 'doom_data_segment', 'ace_segment', 'lumpinfo', 'mainzone'],
    'MTXT0.LOG': ['marker', 'first index', 'count (tmp_count)', 'numtextures',
                  'depth', 'textures[]', 'param block', 'marker2'],
    'MTXT1.LOG': ['marker', 'first index', 'count (tmp_count)', 'numtextures',
                  'depth', 'textures[]', 'param block', 'marker2'],
}

DOOM_CODE_SIZE = 0x40000
DOOM_DATA_SIZE = 0x80000
ACE_IMAGE_SIZE = 0x100000


def resolve_ace(addrs, elf):
    """Map an ACE runtime address to file:line using nm offsets from the ELF."""
    if not elf or not addrs:
        return {}
    syms = {}
    for line in subprocess.run(['nm', '-C', '--defined-only', elf], capture_output=True,
                               text=True).stdout.splitlines():
        f = line.split()
        if len(f) >= 3 and f[2] not in syms:
            syms[f[2]] = int(f[0], 16)
    known = subprocess.run(['nm', elf], capture_output=True, text=True).stdout
    anchors = {}
    for line in known.splitlines():
        f = line.split()
        if len(f) == 3 and f[2] in ('mtx_engine_alloc', 'mtx_error_probe'):
            anchors[f[2]] = int(f[0], 16)
    return syms, anchors


def main():
    p = sys.argv[1]
    name = p.split('/')[-1]
    d = open(p, 'rb').read()
    v = struct.unpack('<%dI' % (len(d) // 4), d[:len(d) // 4 * 4])
    names = LABELS.get(name)
    for i, x in enumerate(v):
        n = names[i] if names and i < len(names) else 'word[%d]' % i
        print('  %-28s %-12u 0x%08X' % (n, x, x))

    if name != 'MTXS.LOG':
        return
    code, data, ace = v[7], v[8], v[9]
    print('\n  stack words as classified (doom code / doom data / ace image / other):')
    for i in range(12, len(v)):
        w = v[i]
        where = 'other'
        if code <= w < code + DOOM_CODE_SIZE:
            where = 'DOOM CODE +0x%05X' % (w - code)
        elif data <= w < data + DOOM_DATA_SIZE:
            where = 'DOOM DATA +0x%05X' % (w - data)
        elif ace <= w < ace + ACE_IMAGE_SIZE:
            where = 'ACE IMAGE +0x%06X' % (w - ace)
        print('    [%2d] 0x%08X  %s' % (i - 12, w, where))


main()
