# SPDX-License-Identifier: GPL-2.0-or-later
"""TELOS research: is a mod's 1.43 build the same code as its 1.44 build?

    python research/telos-dn1/tools/os_compare.py MOD.elemod MOD-os1.44.elemod

Compares the bytes both files ship (a stock reference is unknown without the
firmware, and the two OS versions may cut them differently), the
relocations, the imports, the contributions and the sites. Same code means
one run-time package could serve both OS versions.
"""
import json
import sys


def expand(sec):
    out = []
    for p in sec.get('parts', []):
        out += list(bytes.fromhex(p[1])) if p[0] == 'hex' else [None] * p[2]
    return out


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    a, b = (json.load(open(p)) for p in sys.argv[1:])
    diffs = unknown = 0
    for name in a['sections']:
        x, y = expand(a['sections'][name]), expand(b['sections'].get(name, {}))
        if len(x) != len(y):
            diffs += abs(len(x) - len(y)) or 1
        for i in range(min(len(x), len(y))):
            if x[i] is None or y[i] is None:
                unknown += 1
            elif x[i] != y[i]:
                diffs += 1
    same = {k: a.get(k) == b.get(k) for k in ('relocs', 'imports', 'contribute', 'sites')}
    print('%s %s: %d bytes differ, %d unknown (stock references); %s' % (
        a['id'], a['version'], diffs, unknown,
        ', '.join('%s %s' % (k, 'same' if v else 'DIFFER') for k, v in same.items())))


if __name__ == '__main__':
    main()
