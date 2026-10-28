# SPDX-License-Identifier: GPL-2.0-or-later
"""TELOS research: download the Digitone mk1 mods web/catalog.json pins.

    python research/telos-dn1/tools/fetch_dn1_mods.py OUTDIR

Each file comes from its author's release and must match the catalog's
sha256. A .elemod holds no firmware bytes, but keep OUTDIR out of git all
the same: these are other people's releases.
"""
import hashlib
import json
import os
import sys
import urllib.request

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    items = json.load(open(os.path.join(ROOT, 'web', 'catalog.json')))['items']
    bad = 0
    for m in items:
        if m.get('device') != 'digitone-mk1':
            continue
        url = 'https://github.com/%s/releases/download/%s/%s' % (m['repo'], m['tag'], m['file'])
        data = urllib.request.urlopen(url, timeout=60).read()
        if hashlib.sha256(data).hexdigest() != m['sha256']:
            print('MISMATCH', m['file'])
            bad += 1
            continue
        with open(os.path.join(out, m['file']), 'wb') as fh:
            fh.write(data)
        print('ok', m['file'], len(data))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
