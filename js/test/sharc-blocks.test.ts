// SPDX-License-Identifier: GPL-3.0-or-later
// The Digitone II SHARC blocks payload's checks (tests/test_sharc_blocks.py has the linking, with your stock file).
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { DIGITONE_MK2 } from '../src/devices.ts'
import { blocksPayload, limits, modulePayload } from '../src/sharc.ts'
import { ModError } from '../src/elemod.ts'

const profile = DIGITONE_MK2.releases[0].sharc!
const doc = (loads: object[] = [], fills: object[] = [], patches: object[] = []) =>
  ({ abi: 'dn2-blocks-v1', loads, fills, patches })

test('a blocks payload: loads, fills and patches in the profile ranges', () => {
  const p = modulePayload(doc([{ target: 0x200a0000, hex: '00010203' }], [{ target: 0x84000000, size_bytes: 64, word: 7 }],
    [{ target: 0x20000100, stock: '0000', hex: '0100' }]), profile)
  assert.deepEqual(p.blocks!.spans, [[0x200a0000, 0x200a0004], [0x84000000, 0x84000040]])
  assert.equal(p.blocks!.patches.length, 1)
})

test('a blocks payload: each refusal', () => {
  for (const [d, message] of [
    [doc([{ target: 0x20000000, hex: '00000000' }]), 'outside the profile blocks ranges'],
    [doc([{ target: 0x200c0000, hex: '00000000' }]), 'outside the profile blocks ranges'],
    [doc([{ target: 0x200a0002, hex: '00000000' }]), 'unaligned'],
    [doc([{ target: 0x200a0000, hex: '0000000000000000' }, { target: 0x200a0004, hex: '00000000' }]), 'overlap'],
    [doc([], [{ target: 0x84000000, size_bytes: 0, word: 0 }]), 'unaligned or empty'],
    [doc(), 'empty'],
    [doc([], [], [{ target: 0x20000101, stock: '0000', hex: '0000' }]), 'equal, even'],
    [{ ...doc(), extra: 1 }, 'needs exactly abi, loads, fills, patches'],
  ] as [object, string][]) assert.throws(() => blocksPayload(d, profile), (e: unknown) => e instanceof ModError && e.message.includes(message))
})

test('the upload limits follow the main image', () => {
  const base = 0x40000400
  assert.deepEqual(limits(profile, new Uint8Array(0xd0000), base), [profile.storedMax, profile.decodedMax])
  const raised = new Uint8Array(0xd0000)
  for (const [a, , hex] of profile.uploadRaise![3]) raised.set(hex.match(/../g)!.map(x => parseInt(x, 16)), a - base)
  assert.deepEqual(limits(profile, raised, base), [0x180000, 0x17ffff])
})
