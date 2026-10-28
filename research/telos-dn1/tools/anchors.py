# SPDX-License-Identifier: GPL-2.0-or-later
"""TELOS research: find the Digitone mk1 anchors a run-time loader needs.

    python research/telos-dn1/tools/anchors.py Digitone_and_Digitone_Keys_OS1.43.syx [..._OS1.44.syx]
    python research/telos-dn1/tools/anchors.py --selftest

Reads the user's own stock file (never stored here), checks the sites
elekloader and digihealth already rely on, then scans the main OS for the
constants that lead to the routines research/telos-dn1/README.md asks for
(the card's block read and write, the restart, the USB controller). For each
hit it prints the instruction that holds it, a guess at its routine's start
and the calls around it. A lead, not a finding: read the code it names.
"""
import argparse
import hashlib
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))

from elekloader import formats                      # noqa: E402
from elekloader.isa import coldfire as cf           # noqa: E402

# Sites other mods hold today, with the stock bytes they replace (their mod.json).
KNOWN = {
    '1.43': [
        (0x400ea880, '47f9401cde1a', 'SysEx router entry, every F0 message but F0 7E (digihealth: lea 0x401cde1a,a3)'),
        (0x40000438, '203cffffdfff', 'RTOS context switch (digihealth: idle time)'),
    ],
    '1.44': [
        (0x400eaaf4, '47f9401ce1ea', 'SysEx router entry (digihealth: lea 0x401ce1ea,a3)'),
        (0x40000438, '203cffffdfff', 'RTOS context switch (digihealth)'),
    ],
}

# (name, lo, hi): a 32-bit word in [lo, hi) anywhere in the image is a hit.
SCANS = [
    ('RCM.RCR, the soft reset (0x80 written there)', 0xEC090000, 0xEC090001),
    ('RCM.RSR, the reset status', 0xEC090001, 0xEC090002),
    ('eSDHC registers (the +Drive controller)', 0xFC0CC000, 0xFC0CC100),
    ('USB OTG registers', 0xFC0B0000, 0xFC0B0200),
    ('the working copy, sector 0x7a000', 0x0007A000, 0x0007A001),
    ('the working copy, sector 0x7c000', 0x0007C000, 0x0007C001),
    ('the project block magic 0xBEEFBACE', 0xBEEFBACE, 0xBEEFBACF),
    ('DDR word 0 through the uncached alias', 0x48000000, 0x48000001),
    ('the console magic 0xB0B0DADA', 0xB0B0DADA, 0xB0B0DADB),
]

WINDOW = 0x1000          # how far back a routine's start is looked for
AROUND = 24              # instructions listed after the hit


def word(img, o):
    return int.from_bytes(img[o:o + 2], 'big')


def routine_start(img, o):
    """The nearest link.w a6 before o, or the byte after the nearest rts: a guess."""
    for p in range(o - 2, max(0, o - WINDOW), -2):
        w = word(img, p)
        if w == 0x4E56:
            return p
        if w == 0x4E75:
            return p + 2
    return max(0, o - WINDOW)


def sweep(img, base, start, until, extra):
    """Decode from start; -> [(offset, Insn)] up to `extra` instructions past `until`."""
    out, o, past = [], start, 0
    while o < len(img) - 1 and past <= extra:
        ins = cf.decode(lambda d, o=o: word(img, o + d), base + o)
        out.append((o, ins))
        if o + ins.length > until:
            past += 1
        o += ins.length
    return out


def scan(img, base, lo, hi):
    for o in range(0, len(img) - 3, 2):
        v = int.from_bytes(img[o:o + 4], 'big')
        if lo <= v < hi:
            yield o, v


def report_hit(img, base, o, v):
    start = routine_start(img, o)
    insns = sweep(img, base, start, o, AROUND)
    holder = next(((p, i) for p, i in insns if p <= o < p + i.length), None)
    if holder is None or holder[1].flags & cf.ILLEGAL:
        print('    0x%08x  0x%08x  (no instruction holds it from 0x%08x: data?)' % (base + o, v, base + start))
        return
    p, ins = holder
    calls = ['0x%08x' % i.target for q, i in insns
             if i.flow in (cf.F_JSR, cf.F_BSR, cf.F_JMP) and i.target is not None and abs(q - o) <= 0x80]
    print('    0x%08x  0x%08x  in %s at 0x%08x; routine from ~0x%08x; calls nearby: %s'
          % (base + o, v, ins.op, base + p, base + start, ', '.join(calls) or '-'))


def run(path):
    try:
        parsed, dev, rel = formats.load(path)
    except (OSError, ValueError) as e:
        sys.exit('%s: %s' % (path, e))
    if dev.key != 'digitone-mk1':
        sys.exit('%s is the %s, not the Digitone mk1' % (path, dev.name))
    img = formats.main_image(parsed, dev)
    base = dev.main_load
    ok = len(img) == rel.main_len and hashlib.sha256(img).hexdigest() == rel.main_sha256
    print('## Digitone mk1 %s: main OS %d bytes at 0x%08x, %s' % (rel.version, len(img), base,
                                                                  'as the profile says' if ok else 'NOT the profile\'s image'))
    print('\nKnown sites:')
    for addr, stock, what in KNOWN.get(rel.version, []):
        o = addr - base
        got = img[o:o + len(stock) // 2].hex()
        print('    0x%08x  %s  %s' % (addr, 'ok' if got == stock else 'DIFFERS (%s)' % got, what))
    for name, lo, hi in SCANS:
        hits = list(scan(img, base, lo, hi))
        print('\n%s: %d hit%s' % (name, len(hits), '' if len(hits) == 1 else 's'))
        for o, v in hits[:40]:
            report_hit(img, base, o, v)
        if len(hits) > 40:
            print('    ... %d more' % (len(hits) - 40))
    print()


def selftest():
    """A made-up routine: link; move.b d0,RCR; jsr; rts, and a stray data word."""
    base = 0x40000400
    code = bytes.fromhex('4e560000' '13c0ec090000' '4eb940001234' '4e5e' '4e75' 'fc0cc024')
    img = bytes(64) + code
    hits = list(scan(img, base, 0xEC090000, 0xEC090001))
    assert [o for o, _ in hits] == [64 + 6], hits
    assert routine_start(img, 64 + 6) == 64
    insns = sweep(img, base, 64, 64 + 6, 4)
    assert [i.op for _, i in insns][:4] == ['link', 'move', 'jsr', 'unlk'], [i.op for _, i in insns]
    assert any(i.target == 0x40001234 for _, i in insns)
    assert [o for o, _ in scan(img, base, 0xFC0CC000, 0xFC0CC100)] == [64 + len(code) - 4]
    print('selftest ok')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('stock', nargs='*', help='a Digitone mk1 stock OS file (.syx or Elektron\'s .zip)')
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    if a.selftest:
        selftest()
    for p in a.stock:
        run(p)
    if not a.selftest and not a.stock:
        ap.error('give a stock file, or --selftest')


if __name__ == '__main__':
    main()
