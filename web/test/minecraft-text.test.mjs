import assert from 'node:assert/strict'
import test from 'node:test'

import { parseMinecraftText } from '../src/utils/minecraftText.js'

test('Minecraft formatting codes become styled segments without leaking markers', () => {
  const segments = parseMinecraftText('普通 §r§6金色 §l粗体', '#aaaaaa')

  assert.equal(segments.map(segment => segment.text).join(''), '普通 金色 粗体')
  assert.equal(segments.some(segment => segment.text.includes('§')), false)
  assert.deepEqual(segments[0], {
    text: '普通 ', color: '#aaaaaa', bold: false, italic: false, obfuscated: false
  })
  assert.equal(segments[1].color, '#ffaa00')
  assert.equal(segments[2].color, '#ffaa00')
  assert.equal(segments[2].bold, true)
})

test('Bedrock material colors and reset preserve the configured default color', () => {
  const segments = parseMinecraftText('§u紫晶§r 默认', '#ffffff')

  assert.equal(segments[0].color, '#9a5cc6')
  assert.equal(segments[1].color, '#ffffff')
  assert.equal(segments.map(segment => segment.text).join(''), '紫晶 默认')
})

test('Bedrock resin material color is rendered', () => {
  const segments = parseMinecraftText('§v树脂§r 默认', '#ffffff')

  assert.equal(segments[0].color, '#fc7812')
  assert.equal(segments[1].color, '#ffffff')
  assert.equal(segments.map(segment => segment.text).join(''), '树脂 默认')
})
