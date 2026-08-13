import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import test from 'node:test'

import { IndexStore } from '../src/index-store.mjs'
import { ScanManager } from '../src/scan-manager.mjs'

test('scan manager accepts batches, publishes progress and commits a snapshot', async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'fullindex-scan-test-'))
  const store = new IndexStore(path.join(directory, 'index.sqlite3'), 10)
  const broadcasts = []
  let listener
  const manager = new ScanManager({
    indexStore: store,
    requestPlugin: async (_serverId, action, params) => ({
      type: 'response',
      action,
      requestId: 'test',
      ok: true,
      data: { accepted: true, jobId: params.jobId }
    }),
    broadcast: message => broadcasts.push(message),
    onPluginMessage: callback => {
      listener = callback
      return () => { listener = null }
    }
  })

  try {
    const job = await manager.start('default', 25)
    for (const kind of ['players', 'containers', 'drops', 'entities']) {
      listener({
        type: 'index.batch',
        serverId: 'default',
        jobId: job.jobId,
        kind,
        batchIndex: 0,
        batchCount: 1,
        replace: true,
        items: kind === 'players' ? [{
          source: 'storage',
          name: 'Alex',
          realName: 'Alex',
          uuid: '11111111-1111-4111-8111-111111111111',
          storageIds: ['11111111-1111-4111-8111-111111111111'],
          inventory: []
        }] : []
      })
    }
    listener({
      type: 'index.complete',
      serverId: 'default',
      jobId: job.jobId,
      counts: { players: 1, containers: 0, drops: 0, entities: 0 }
    })

    assert.equal(manager.status('default').status, 'complete')
    assert.equal(store.latest('default').player_count, 1)
    assert.equal(store.listData({ serverId: 'default', kind: 'players' }).items[0].name, 'Alex')
    assert.ok(broadcasts.some(message => message.type === 'index.job' && message.status === 'complete'))
  } finally {
    manager.close()
    store.close()
    fs.rmSync(directory, { recursive: true, force: true })
  }
})

test('scan batches are not visible before the snapshot commits', async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'fullindex-scan-isolation-test-'))
  const store = new IndexStore(path.join(directory, 'index.sqlite3'), 10)
  let listener
  const manager = new ScanManager({
    indexStore: store,
    requestPlugin: async (_serverId, action, params) => ({ ok: true, action, data: { jobId: params.jobId } }),
    broadcast: () => {},
    onPluginMessage: callback => {
      listener = callback
      return () => { listener = null }
    }
  })
  try {
    const job = await manager.start('default', 25)
    listener({
      type: 'index.batch', serverId: 'default', jobId: job.jobId, kind: 'players',
      batchIndex: 0, batchCount: 1, replace: true,
      items: [{ name: 'NotCommitted', uuid: 'id', storageIds: ['id'], inventory: [] }]
    })
    assert.equal(store.listData({ serverId: 'default', kind: 'players' }).total, 0)
  } finally {
    manager.close()
    store.close()
    fs.rmSync(directory, { recursive: true, force: true })
  }
})
