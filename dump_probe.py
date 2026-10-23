#!/usr/bin/env python3
# Dump an engine probe file (MTXN.LOG / MTXW.LOG) as named fields.
import struct, sys
LABELS = {
    'MTXN.LOG': ['marker', 'mtx_params', 'mtx_depth()', '&mtx_params', '&mtx_engine_alloc',
                 'size', 'mainzone', 'mtx_starts', 'mtx_engine_depth()', 'ace_segment',
                 'doom_code_segment', 'doom_data_segment', '&mtx_engine_reserve', '-', '-', 'marker2'],
    'MTXW.LOG': ['marker', 'lumpinfo', 'numlumps', 'idx', 'li->fd', 'li->offset', 'li->size',
                 'dest', 'limit', 'ret', 'lumpcache', '&li', 'depth', 'mainzone',
                 'li off in lumpinfo', 'marker2',
                 'pool->base', 'pool->size', 'pool->used', 'pool->served', 'pool->refusals',
                 'pool->reclaimed', 'pool->leaked', 'pool->invalid'],
}
p = sys.argv[1]
names = LABELS[p.split('/')[-1]]
d = open(p, 'rb').read()
v = struct.unpack('<%dI' % (len(d) // 4), d[:len(d) // 4 * 4])
for n, x in zip(names, v):
    extra = ''
    if n.startswith('pool->') and n not in ('pool->size', 'pool->used'):
        extra = ''
    print('  %-24s %-12u 0x%08X%s' % (n, x, x, extra))
