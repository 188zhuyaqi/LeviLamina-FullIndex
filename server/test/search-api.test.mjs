import assert from 'node:assert/strict'
import { spawn } from 'node:child_process'
import fs from 'node:fs'
import net from 'node:net'
import os from 'node:os'
import path from 'node:path'
import test from 'node:test'

import { IndexStore } from '../src/index-store.mjs'

function freePort() {
  return new Promise((resolve, reject) => {
    const server = net.createServer()
    server.once('error', reject)
    server.listen(0, '127.0.0.1', () => {
      const { port } = server.address()
      server.close(error => error ? reject(error) : resolve(port))
    })
  })
}

async function waitForServer(url) {
  for (let attempt = 0; attempt < 60; attempt += 1) {
    try {
      const response = await fetch(url)
      if (response.ok) return
    } catch {
      // 服务仍在启动。
    }
    await new Promise(resolve => setTimeout(resolve, 50))
  }
  throw new Error('test server did not start')
}

test('item search API forwards every combined filter', async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'fullindex-search-api-test-'))
  const database = path.join(directory, 'index.sqlite3')
  const store = new IndexStore(database, 2)
  store.createSnapshot('default', {
    players: [{
      name: 'Steve', realName: 'Steve', uuid: '11111111-1111-4111-8111-111111111111',
      storageIds: ['11111111-1111-4111-8111-111111111111'], source: 'storage', online: false,
      dimension: 'overworld', position: { x: 100, y: 64, z: -100 },
      inventory: [{
        slot: 0, slotName: 'hotbar', id: 'minecraft:diamond_sword', displayName: 'Diamond Sword',
        customName: 'Search Sentinel', count: 1, damage: 7, enchanted: true, children: []
      }],
      armor: [], offhand: null, enderChest: []
    }],
    containers: [{
      kind: 'minecraft:chest', source: 'storage', dimension: 'nether', chunkX: 2, chunkZ: 3,
      position: { x: 32, y: 70, z: 48 }, occupiedSlots: 1, itemCount: 8,
      items: [{
        slot: 0, slotName: 'container', id: 'minecraft:diamond', displayName: 'Diamond',
        customName: '', count: 8, damage: 0, enchanted: false, children: []
      }]
    }],
    drops: [],
    entities: []
  })
  store.close()

  const port = await freePort()
  const child = spawn(process.execPath, ['src/index.mjs'], {
    cwd: path.resolve(import.meta.dirname, '..'),
    env: {
      ...process.env,
      FULLINDEX_HOST: '127.0.0.1',
      FULLINDEX_PORT: String(port),
      FULLINDEX_INDEX_DB: database
    },
    stdio: 'ignore'
  })

  try {
    await waitForServer(`http://127.0.0.1:${port}/api/index/status`)
    const request = async body => {
      const response = await fetch(`http://127.0.0.1:${port}/api/search/items`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({ serverId: 'default', ...body })
      })
      assert.equal(response.status, 200)
      return response.json()
    }

    const exact = await request({
      typeId: 'minecraft:diamond_sword', name: 'sentinel', sourceType: 'player', dimension: 'overworld', enchanted: 'true',
      countMin: 1, countMax: 1, xMin: 90, xMax: 110
    })
    assert.equal(exact.total, 1)
    assert.equal(exact.items[0].item_id, 'minecraft:diamond_sword')

    const wrongDimension = await request({
      typeId: 'minecraft:diamond_sword', name: 'sentinel', sourceType: 'player', dimension: 'nether'
    })
    assert.equal(wrongDimension.total, 0)

    const wrongEnchantment = await request({
      typeId: 'minecraft:diamond', name: 'Diamond', sourceType: 'container', enchanted: 'true'
    })
    assert.equal(wrongEnchantment.total, 0)
  } finally {
    child.kill()
    await new Promise(resolve => child.once('exit', resolve))
    fs.rmSync(directory, { recursive: true, force: true })
  }
})
