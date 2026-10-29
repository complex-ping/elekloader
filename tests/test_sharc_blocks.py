# SPDX-License-Identifier: GPL-2.0-or-later
"""DN2 SHARC blocks mods (docs/FORMAT.md, "Digitone II SHARC blocks"); requires owner stock and a ColdFire cross
compiler. Every refusal here has a twin that links."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from elekloader import elemod, formats, link, sharc  # noqa: E402
from elekloader.sdk import build as sdk  # noqa: E402
from elekloader.sdk.sharc import filter_artifact, voice_artifact  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
STOCK = os.environ.get('ELEKLOADER_DN2_SYX')


@unittest.skipUnless(STOCK and os.environ.get('ELEKLOADER_CROSS'),
                     'needs ELEKLOADER_DN2_SYX and ELEKLOADER_CROSS')
class BlocksTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.work = tempfile.TemporaryDirectory()
        w = cls.work.name
        cls.core = elemod.load_any(sdk.build(str(ROOT / 'mods/core-dn2'), STOCK, w)[0])
        cls.sine = elemod.load_any(sdk.build(str(ROOT / 'examples/sharc-sine'), STOCK, w)[0])
        cls.upload = elemod.load_any(sdk.build(str(ROOT / 'mods/dsp-upload-dn2'), STOCK, w)[0])
        cls.stock, cls.dev, cls.rel = formats.load(STOCK)
        cls.image = formats.main_image(cls.stock, cls.dev)
        cls.stream = cls.stock.section(7)
        cls.profile = cls.rel.sharc
        code = next(r for r in sharc.parse(cls.stream, cls.profile)
                    if r.flags == sharc.LOAD and r.target == 0x20000000)
        cls.code = code

    @classmethod
    def tearDownClass(cls):
        cls.work.cleanup()

    def stock_at(self, target, n):
        r = next(r for r in sharc.parse(self.stream, self.profile)
                 if r.flags == sharc.LOAD and r.target <= target and target + n <= r.target + r.count)
        at = r.offset + 16 + target - r.target
        return self.stream[at:at + n]

    def mod(self, ident, loads=(), fills=(), patches=(), names=()):
        d = Path(self.work.name) / ident
        d.mkdir(exist_ok=True)
        artifact = {'abi': self.profile['blocks_abi'],
                    'loads': [{'target': t, 'hex': data.hex()} for t, data in loads],
                    'fills': [{'target': t, 'size_bytes': n, 'word': w} for t, n, w in fills],
                    'patches': [{'target': p[0], 'stock': (p[1] if len(p) == 3 else self.stock_at(p[0], len(p[-1]))).hex(),
                                 'hex': p[-1].hex()} for p in patches]}
        (d / 'blocks.sharc.json').write_text(json.dumps(artifact))
        doc = {'id': ident, 'version': '0.1', 'title': ident, 'description': 'test', 'license': 'GPL-2.0-or-later',
               'device': 'digitone-mk2', 'os': '1.12', 'requires': ['core'], 'resources': {'core': '0.2'},
               'sharc': {'artifact': 'blocks.sharc.json'}}
        if names:
            doc['resources']['names'] = list(names)
        (d / 'mod.json').write_text(json.dumps(doc))
        return elemod.load_any(sdk.build(str(d), STOCK, str(d / 'out'))[0])

    def link(self, *mods):
        return link.link([self.core, *mods], self.image, sharc_stream=self.stream)

    def refused(self, message, *mods):
        self.assertTrue(any(message in p for p in link.check([self.core, *mods], self.image)),
                        link.check([self.core, *mods], self.image))
        with self.assertRaises(elemod.ModError):
            self.link(*mods)

    def test_loads_fills_and_patches_land_and_the_rest_stays_stock(self):
        new = bytes(6)
        m = self.mod('blocks-a', loads=[(0x200a0000, bytes(range(32)))], fills=[(0x84000000, 256, 0)],
                     patches=[(0x20000100, new)])
        linked = self.link(m)
        records = sharc.parse(linked.sharc_stream, self.profile)
        self.assertEqual(len(records), self.profile['record_count'] + 2)
        load = next(r for r in records if r.target == 0x200a0000)
        self.assertEqual(load.raw[16:], bytes(range(32)))
        self.assertTrue(any(r.flags == sharc.FILL and r.target == 0x84000000 and r.count == 256 for r in records))
        at = self.code.offset + 16 + 0x100
        self.assertEqual(linked.sharc_stream[at:at + 6], new)
        self.assertEqual(linked.sharc_stream[at - 64:at] + linked.sharc_stream[at + 6:at + 64],
                         self.stream[at - 64:at] + self.stream[at + 6:at + 64])
        self.assertEqual(linked.layout['sharc']['blocks'],
                         [{'id': 'blocks-a', 'spans': [[0x200a0000, 0x200a0020], [0x84000000, 0x84000100]],
                           'patches': 1}])

    def test_the_payload_refusals(self):
        for loads, fills, patches, message in (
                ([(0x20000000, bytes(4))], [], [], 'outside the profile blocks ranges'),
                ([(0x200c0000, bytes(4))], [], [], 'outside the profile blocks ranges'),
                ([(0x200a0002, bytes(4))], [], [], 'unaligned'),
                ([(0x200a0000, bytes(8)), (0x200a0004, bytes(4))], [], [], 'overlap'),
                ([], [(0x84000000, 0, 0)], [], 'unaligned or empty'),
                ([], [], [], 'empty')):
            with self.subTest(message=message), self.assertRaisesRegex((elemod.ModError, sdk.BuildError), message):
                self.mod('bad', loads, fills, patches)
        self.mod('good', [(0x200a0000, bytes(8)), (0x200a0008, bytes(4))])

    def test_two_mods_may_not_share_dsp_memory(self):
        a = self.mod('blocks-a', loads=[(0x200a0000, bytes(64))])
        self.refused('both use DSP memory', a, self.mod('blocks-b', loads=[(0x200a0020, bytes(64))]))
        self.link(a, self.mod('blocks-b', loads=[(0x200a0040, bytes(64))]))
        self.refused('both use DSP memory', self.sine, self.mod('blocks-c', loads=[(0x20094000, bytes(64))]))
        self.link(self.sine, self.mod('blocks-c', loads=[(0x200a0000, bytes(64))]))

    def test_two_mods_may_not_patch_the_same_code(self):
        a = self.mod('blocks-a', patches=[(0x20000100, bytes(6))])
        self.refused('both patch DSP code', a, self.mod('blocks-b', patches=[(0x20000104, bytes(6))]))
        self.link(a, self.mod('blocks-b', patches=[(0x20000106, bytes(6))]))

    def test_a_patch_may_not_touch_a_qualified_hook_or_new_records(self):
        hook = self.profile['hook'][0]
        self.refused('a site the profile qualifies', self.mod('blocks-a', patches=[(hook, bytes(6))]))
        self.link(self.mod('blocks-a', patches=[(hook + 6, bytes(6))]))
        a = self.mod('blocks-b', loads=[(0x200a0000, bytes(64))])
        self.refused('inside new DSP records', a,
                     self.mod('blocks-c', patches=[(0x200a0010, bytes(4), b'\x01\x00\x01\x00')]))
        self.link(a, self.mod('blocks-c', patches=[(0x20000200, bytes(4))]))

    def test_a_patch_may_not_touch_a_qualified_data_site(self):
        # a profile's exact data sites (the kind a companion such as a DSP diagnostics mod qualifies) are as off-limits as
        # its patch ranges: here one made up over stock code, so the check runs without such a profile
        stock6 = self.stock_at(0x20000300, 6)
        with patch.dict(self.profile, data_sites={0x20000300: (stock6.hex(), '000000000000')}):
            self.refused('a site the profile qualifies', self.mod('blocks-a', patches=[(0x20000302, bytes(2))]))
            with self.assertRaisesRegex(elemod.ModError, 'overlaps a qualified hook'):
                sharc.insert(self.stream, self.profile, stock_patches=[(0x20000300, stock6, bytes(6))])
            self.link(self.mod('blocks-b', patches=[(0x20000306, bytes(2))]))

    def test_a_section_over_1_mib_needs_the_raised_upload(self):
        data = bytes((i * 2654435761 >> 13) & 0xff for i in range(300000 // 4 * 4))
        m = self.mod('blocks-big', loads=[(0x84000000, data)])
        with self.assertRaisesRegex(elemod.ModError, 'decoded stream exceeds the profile limit'):
            self.link(m)
        linked = self.link(self.upload, m)
        self.assertGreater(len(linked.sharc_stream), 0xfffff)
        self.assertLessEqual(len(linked.sharc_stream), self.profile['upload_raise'][2])
        for site in self.upload.doc['sites']:
            at = int(site['addr'], 16) - self.dev.main_load
            self.assertEqual(linked.image[at:at + site['len']].hex(), site['new'])


@unittest.skipUnless(STOCK and os.environ.get('ELEKLOADER_CROSS'),
                     'needs ELEKLOADER_DN2_SYX and ELEKLOADER_CROSS')
class BlocksWithMachinesTests(unittest.TestCase):
    """A blocks mod beside machines-dn2's synth and filter: they share the build when their memory and sites do not meet."""

    @classmethod
    def setUpClass(cls):
        cls.work = tempfile.TemporaryDirectory()
        root = Path(cls.work.name)
        cls.stock, cls.dev, cls.rel = formats.load(STOCK)
        cls.image = formats.main_image(cls.stock, cls.dev)
        cls.stream = cls.stock.section(7)
        cls.profile = cls.rel.sharc
        cls.base = [elemod.load_any(sdk.build(str(ROOT / 'mods' / src), STOCK, str(root / src))[0])
                    for src in ('core-dn2', 'machines-dn2')]
        cls.machines = []
        for kind, producer, native in (('voice', voice_artifact, 'synth'), ('filter', filter_artifact, 'filter')):
            p = root / kind
            p.mkdir()
            (p / 'machine.c').write_text('#include "mk2/machines.h"\n'
                                         'const struct mk2_%s %s_6 = {6,"PAIR","PAIR"};\n' % (native, kind))
            (p / 'kernel.json').write_text(json.dumps(producer(bytes(4), bytes(4), 0, 6, 128 if kind == 'voice' else 12)))
            (p / 'mod.json').write_text(json.dumps({
                'id': kind, 'version': '0.1', 'device': 'digitone-mk2', 'os': '1.12',
                'sources': ['machine.c'], 'sharc': {'artifact': 'kernel.json'},
                'resources': {'core': '0.1'}, 'requires': ['core', 'machines-dn2'],
                'contribute': [{'to': cls.profile[kind + '_table'], 'data': '00000000',
                                'relocs': [[0, 'abs32', 'sym:%s_6' % kind, 0]]}]}))
            cls.machines.append(elemod.load_any(sdk.build(str(p), STOCK, str(p / 'build'))[0]))

    @classmethod
    def tearDownClass(cls):
        cls.work.cleanup()

    mod = BlocksTests.mod
    stock_at = BlocksTests.stock_at

    def check(self, blocks):
        return link.check(self.base + self.machines + [blocks], self.image)

    def test_blocks_link_beside_a_synth_and_a_filter(self):
        blocks = self.mod('blocks-m', loads=[(0x200a0000, bytes(range(32)))], patches=[(0x20000100, bytes(6))])
        self.assertEqual(self.check(blocks), [])
        out = link.link(self.base + self.machines + [blocks], self.image, sharc_stream=self.stream)
        records = sharc.parse(out.sharc_stream, self.profile)
        self.assertEqual(next(r.raw[16:] for r in records if r.target == 0x200a0000), bytes(range(32)))
        self.assertEqual(out.layout['sharc']['blocks'][0]['id'], 'blocks-m')
        self.assertEqual(sorted(m['id'] for m in out.layout['sharc']['mods']), ['filter', 'voice'])
        for kind in ('voice', 'filter'):
            site, _old, new = self.profile[kind + '_hook']
            r = next(r for r in records if r.flags == sharc.LOAD and r.target <= site < r.target + r.count)
            at = r.offset + 16 + site - r.target
            self.assertEqual(out.sharc_stream[at:at + 6], bytes.fromhex(new))

    def test_blocks_may_not_take_the_machines_memory_or_sites(self):
        for target in (self.profile['voice_range'][0], self.profile['filter_range'][1] - 64):
            problems = self.check(self.mod('blocks-x%x' % target, loads=[(target, bytes(64))]))
            self.assertTrue(any('both use DSP memory' in p for p in problems), problems)
        for site, _old, new in (self.profile['voice_hook'], self.profile['filter_hook'], self.profile['filter_capture']):
            problems = self.check(self.mod('blocks-p%x' % site, patches=[(site, bytes(len(new) // 2))]))
            self.assertTrue(any('a site the profile qualifies' in p for p in problems), problems)


if __name__ == '__main__':
    unittest.main()
