import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import test from 'node:test'

import { IndexStore } from '../src/index-store.mjs'

function datasets(diamondCount) {
  return {
    players: [{
      name: 'Alex',
      realName: 'Alex',
      uuid: '11111111-1111-4111-8111-111111111111',
      storageIds: ['11111111-1111-4111-8111-111111111111'],
      dimension: 'overworld',
      position: { x: -1, y: 64, z: 2 },
      inventory: [{
        slot: 0,
        slotName: 'hotbar:selected',
        id: 'minecraft:shulker_box',
        displayName: 'Shulker Box',
        count: 1,
        children: [{
          slot: 4,
          slotName: 'container',
          id: 'minecraft:diamond',
          displayName: 'Diamond',
          count: diamondCount,
          enchanted: true,
          damage: 2,
          children: []
        }]
      }],
      armor: [],
      offhand: null,
      enderChest: []
    }],
    containers: [],
    drops: [],
    entities: []
  }
}

test('SQLite snapshots index nested items and calculate diffs', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'fullindex-test-'))
  const store = new IndexStore(path.join(directory, 'index.sqlite3'), 10)
  try {
    const first = store.createSnapshot('default', datasets(3))
    const second = store.createSnapshot('default', datasets(8))

    const search = store.searchItems({ serverId: 'default', typeId: 'minecraft:diamond' })
    assert.equal(search.total, 1)
    assert.equal(search.items[0].count, 8)
    assert.match(search.items[0].item_path, /shulker_box/)

    const filtered = store.searchItems({
      serverId: 'default',
      sourceType: 'player',
      dimension: 'overworld',
      enchanted: true,
      nestedOnly: true,
      countMin: 5,
      countMax: 10,
      xMin: -2,
      xMax: 0,
      sortField: 'count',
      sortOrder: 'descending'
    })
    assert.equal(filtered.total, 1)
    assert.equal(filtered.items[0].item_id, 'minecraft:diamond')
    assert.equal(filtered.items[0].enchanted, 1)
    assert.equal(filtered.items[0].damage, 2)

    const diff = store.diff(first.id, second.id)
    const diamonds = diff.items.find(item => item.itemId === 'minecraft:diamond')
    assert.equal(diamonds.delta, 5)
    assert.equal(store.listSnapshots('default').length, 2)
  } finally {
    store.close()
    fs.rmSync(directory, { recursive: true, force: true })
  }
})

test('data listing filters, sorts and groups entities by chunk', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'fullindex-chunks-test-'))
  const store = new IndexStore(path.join(directory, 'index.sqlite3'), 10)
  try {
    const initial = datasets(3)
    initial.entities = [
      {
        typeName: 'minecraft:zombie', category: 'NATURAL_MOB', source: 'storage',
        dimension: 'overworld', chunkX: 4, chunkZ: -2, position: { x: 65, y: 64, z: -31 }
      },
      {
        typeName: 'minecraft:zombie', category: 'NATURAL_MOB', source: 'runtime',
        dimension: 'overworld', chunkX: 4, chunkZ: -2, position: { x: 66, y: 64, z: -30 }
      },
      {
        typeName: 'minecraft:item', category: 'SPECIAL_ENTITY', source: 'runtime',
        dimension: 'overworld', chunkX: 4, chunkZ: -2, position: { x: 67, y: 64, z: -29 }
      },
      {
        typeName: 'minecraft:skeleton', category: 'NATURAL_MOB', source: 'storage',
        dimension: 'nether', chunkX: 1, chunkZ: 1, position: { x: 20, y: 70, z: 20 }
      }
    ]
    initial.containers = [{
      kind: 'minecraft:chest', source: 'storage', dimension: 'overworld',
      chunkX: 4, chunkZ: -2, position: { x: 65, y: 64, z: -31 },
      occupiedSlots: 0, itemCount: 0, items: []
    }]
    store.createSnapshot('default', initial)

    const containerAtPosition = store.listData({
      serverId: 'default', kind: 'containers', x: 65, y: 64, z: -31
    })
    assert.equal(containerAtPosition.total, 1)
    const wrongPosition = store.listData({
      serverId: 'default', kind: 'containers', x: 65, y: 70, z: -31
    })
    assert.equal(wrongPosition.total, 0)

    const chunks = store.listData({ serverId: 'default', kind: 'entities', view: 'chunks' })
    assert.equal(chunks.total, 2)
    assert.equal(chunks.items[0].entityCount, 3)
    assert.equal(chunks.items[0].naturalCount, 2)
    assert.equal(chunks.items[0].specialCount, 1)
    assert.equal(chunks.items[0].distinctTypeCount, 2)
    assert.equal(chunks.items[0].source, 'runtime')

    const naturalChunks = store.listData({
      serverId: 'default', kind: 'entities', view: 'chunks', category: 'NATURAL_MOB',
      sortField: 'chunkX', sortOrder: 'ascending'
    })
    assert.equal(naturalChunks.total, 2)
    assert.equal(naturalChunks.items[0].chunkX, 1)
    assert.equal(naturalChunks.items[1].entityCount, 2)

    const zombies = store.listData({
      serverId: 'default', kind: 'entities', typeId: 'minecraft:zombie',
      sortField: 'position.x', sortOrder: 'descending', pageSize: 1
    })
    assert.equal(zombies.total, 2)
    assert.equal(zombies.items.length, 1)
    assert.equal(zombies.items[0].position.x, 66)
    assert.deepEqual(store.typeOptions('default', 'entities'), [
      { value: 'minecraft:item', displayName: '' },
      { value: 'minecraft:skeleton', displayName: '' },
      { value: 'minecraft:zombie', displayName: '' }
    ])
    const detail = store.detail('default', 'containers', containerAtPosition.items[0].id)
    assert.equal(detail.kind, 'minecraft:chest')
  } finally {
    store.close()
    fs.rmSync(directory, { recursive: true, force: true })
  }
})

test('data pages remain on the latest completed snapshot until a new snapshot commits', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'fullindex-snapshot-only-test-'))
  const store = new IndexStore(path.join(directory, 'index.sqlite3'), 10)
  try {
    const initial = datasets(3)
    store.createSnapshot('default', initial)
    const listed = store.listData({ serverId: 'default', kind: 'players' })
    assert.equal(listed.total, 1)
    assert.equal(listed.items[0].name, 'Alex')
  } finally {
    store.close()
    fs.rmSync(directory, { recursive: true, force: true })
  }
})
