import assert from 'node:assert/strict'
import test from 'node:test'

import { LiveQueryManager } from '../src/live-query-manager.mjs'

function socket() {
  const handlers = new Map()
  return {
    readyState: 1,
    sent: [],
    on(event, handler) { handlers.set(event, handler) },
    emit(event, value) { handlers.get(event)?.(value) },
    send(value) { this.sent.push(JSON.parse(value)) }
  }
}

test('live query batches route only to their browser and never touch a store', async () => {
  let pluginListener
  let statusListener
  const requests = []
  const manager = new LiveQueryManager({
    requestPlugin: async (serverId, action, params) => {
      requests.push({ serverId, action, params })
      return { ok: true, data: { accepted: true } }
    },
    onPluginMessage: listener => {
      pluginListener = listener
      return () => { pluginListener = null }
    },
    onPluginStatus: listener => {
      statusListener = listener
      return () => { statusListener = null }
    }
  })
  const first = socket()
  const second = socket()
  manager.registerBrowser(first)
  manager.registerBrowser(second)
  try {
    first.emit('message', JSON.stringify({
      type: 'live.query.start', serverId: 'default', jobId: 'job-1', kind: 'players',
      filters: { keyword: 'Alex' }
    }))
    await new Promise(resolve => setImmediate(resolve))
    pluginListener({
      type: 'live.query.batch', serverId: 'default', jobId: 'job-1', kind: 'players',
      received: 1, items: [{ name: 'Alex' }]
    })
    assert.equal(first.sent.at(-1).items[0].name, 'Alex')
    assert.equal(second.sent.length, 0)
    assert.equal(requests[0].action, 'live.query.start')

    statusListener({ serverId: 'default', online: false })
    assert.equal(first.sent.at(-1).type, 'live.query.failed')
  } finally {
    manager.close()
  }
})
