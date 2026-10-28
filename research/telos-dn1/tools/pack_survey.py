# SPDX-License-Identifier: GPL-2.0-or-later
"""TELOS research: could each Digitone mk1 mod be loaded at run time?

    python research/telos-dn1/tools/pack_survey.py DIR_OR_FILES...

For every format-2 mod but the core: its own sites, its sections, the stock
references in its .run (bytes a package built from it would carry from the
user's firmware), its relocations (a run-time loader takes abs32 to an import
and anything inside the mod; a PC-relative reference to an import it cannot
take), what it imports and from whom, the tables it adds to, its claims and
its fixed bulk regions. Give core and the companions too, so imports resolve.
"""
import collections
import glob
import json
import os
import sys


def files(args):
    for a in args:
        if os.path.isdir(a):
            yield from sorted(glob.glob(os.path.join(a, '*.elemod')))
        else:
            yield a


def main():
    mods = [json.load(open(p)) for p in files(sys.argv[1:])]
    if not mods:
        sys.exit(__doc__)
    providers = {}
    for d in mods:
        for e in d.get('exports', []):
            providers.setdefault(e, d['id'])
        for t in d.get('collections', {}):
            providers.setdefault(t, d['id'])
    print('| mod | own sites | .run | .bss | stock refs in .run | relocs | to imports | PC-rel. to imports '
          '| imports by provider | tables | claims | bulk regions |')
    print('|---|---|---|---|---|---|---|---|---|---|---|---|')
    seen = set()
    for d in mods:
        key = (d['id'], d['version'])
        if d['id'] == 'core' or key in seen:
            continue
        seen.add(key)
        secs = d.get('sections', {})
        run = secs.get('.run', {})
        stock = [p for p in run.get('parts', []) if p[0] == 'stock']
        imports = set(d.get('imports', [])) | set(d.get('weak', []))
        n = to_imp = pc_imp = 0
        for _sec, _off, kind, target, _add in d.get('relocs', []):
            n += 1
            if target.startswith('sym:') and target[4:] in imports:
                to_imp += 1
                pc_imp += kind != 'abs32'
        by = collections.Counter(providers.get(i, '?') for i in imports)
        tables = collections.Counter(c['to'] for c in d.get('contribute', []))
        res = d.get('resources', {})
        regs = res.get('regions', [])
        mb = sum(int(r['hi'], 16) - int(r['lo'], 16) for r in regs) / 2 ** 20
        print('| %s %s | %d | %d | %d | %d runs, %d B | %d | %d | %d | %s | %s | %s | %s |' % (
            d['id'], d['version'], len(d.get('sites', [])), run.get('len', 0),
            secs.get('.bss', {}).get('len', secs.get('.bss', {}).get('size', 0)),
            len(stock), sum(p[2] for p in stock), n, to_imp, pc_imp,
            ', '.join('%s %d' % kv for kv in sorted(by.items())),
            ', '.join(t if c == 1 else '%s x%d' % (t, c) for t, c in sorted(tables.items())),
            ', '.join(res.get('names', [])) or '-',
            '%d, %.1f MB' % (len(regs), mb) if regs else '-'))


if __name__ == '__main__':
    main()
