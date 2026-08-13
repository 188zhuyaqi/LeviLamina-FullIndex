import Fastify from 'fastify'
import cors from '@fastify/cors'
import websocket from '@fastify/websocket'
import fs from 'node:fs/promises'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

import { config } from './config.mjs'
import { IndexStore } from './index-store.mjs'
import { LiveQueryManager } from './live-query-manager.mjs'
import { ScanManager } from './scan-manager.mjs'
import {
  broadcast,
  handlePluginMessage,
  listServers,
  onPluginMessage,
  onPluginStatus,
  pluginOnline,
  registerBrowser,
  registerPlugin,
  requestPlugin
} from './hub.mjs'

const app = Fastify({ logger: true })
const indexStore = new IndexStore(config.indexDatabase, config.keepSnapshots)
const scanManager = new ScanManager({ indexStore, requestPlugin, broadcast, onPluginMessage })
const liveQueryManager = new LiveQueryManager({ requestPlugin, onPluginMessage, onPluginStatus })

await app.register(cors, { origin: true })
await app.register(websocket)

app.get('/health', async () => ({ ok: true, now: Date.now() }))

app.get('/api/servers', async () => ({ items: listServers() }))

app.post('/api/query', async (request, reply) => {
  const { serverId = 'default', action, params = {} } = request.body ?? {}
  if (!action) {
    return reply.code(400).send({ ok: false, error: 'action is required' })
  }

  try {
    return await requestPlugin(serverId, action, params)
  } catch (error) {
    return reply.code(503).send({ ok: false, error: error.message })
  }
})

app.get('/api/index/status', async request => {
  const serverId = String(request.query?.serverId ?? 'default')
  return {
    ok: true,
    refreshing: Boolean(scanManager.active(serverId)),
    job: scanManager.status(serverId),
    snapshot: indexStore.latest(serverId)
  }
})

app.post('/api/index/refresh', async (request, reply) => {
  const serverId = String(request.body?.serverId ?? 'default')
  try {
    const job = await scanManager.start(serverId, request.body?.batchSize)
    return reply.code(202).send({ ok: true, job })
  } catch (error) {
    return reply.code(409).send({ ok: false, error: error.message, job: scanManager.status(serverId) })
  }
})

app.get('/api/index/job', async request => {
  const serverId = String(request.query?.serverId ?? 'default')
  return { ok: true, job: scanManager.status(serverId) }
})

app.post('/api/index/cancel', async (request, reply) => {
  const serverId = String(request.body?.serverId ?? 'default')
  try {
    const job = await scanManager.cancel(serverId, request.body?.jobId)
    return { ok: true, job }
  } catch (error) {
    return reply.code(409).send({ ok: false, error: error.message, job: scanManager.status(serverId) })
  }
})

app.get('/api/data/:kind', async (request, reply) => {
  try {
    return {
      ok: true,
      ...indexStore.listData({
        ...request.query,
        serverId: String(request.query?.serverId ?? 'default'),
        kind: String(request.params.kind),
        page: request.query?.page,
        pageSize: request.query?.pageSize
      })
    }
  } catch (error) {
    return reply.code(400).send({ ok: false, error: error.message })
  }
})

app.get('/api/types/:kind', async (request, reply) => {
  try {
    const serverId = String(request.query?.serverId ?? 'default')
    return { ok: true, items: indexStore.typeOptions(serverId, String(request.params.kind)) }
  } catch (error) {
    return reply.code(400).send({ ok: false, error: error.message })
  }
})

app.get('/api/details/entities/chunk', async (request, reply) => {
  try {
    return {
      ok: true,
      item: indexStore.entityChunkDetail({
        ...request.query,
        serverId: String(request.query?.serverId ?? 'default')
      })
    }
  } catch (error) {
    return reply.code(400).send({ ok: false, error: error.message })
  }
})

app.get('/api/details/:kind/:id', async (request, reply) => {
  try {
    const item = indexStore.detail(
      String(request.query?.serverId ?? 'default'),
      String(request.params.kind),
      Number(request.params.id)
    )
    if (!item) return reply.code(404).send({ ok: false, error: 'snapshot detail not found' })
    return { ok: true, item }
  } catch (error) {
    return reply.code(400).send({ ok: false, error: error.message })
  }
})

app.get('/api/snapshots', async request => {
  const serverId = String(request.query?.serverId ?? 'default')
  return { ok: true, items: indexStore.listSnapshots(serverId, request.query?.limit) }
})

app.get('/api/snapshots/diff', async (request, reply) => {
  try {
    const fromId = Number(request.query?.from)
    const toId = Number(request.query?.to)
    if (!Number.isInteger(fromId) || !Number.isInteger(toId)) {
      return reply.code(400).send({ ok: false, error: 'from and to snapshot ids are required' })
    }
    return { ok: true, ...indexStore.diff(fromId, toId) }
  } catch (error) {
    return reply.code(400).send({ ok: false, error: error.message })
  }
})

app.post('/api/search/items', async request => {
  const body = request.body ?? {}
  return {
    ok: true,
    ...indexStore.searchItems({
      ...body,
      serverId: String(body.serverId ?? 'default')
    })
  }
})

app.get('/ws/plugin', { websocket: true }, (socket, request) => {
  const auth = request.headers.authorization ?? ''
  const token = auth.startsWith('Bearer ') ? auth.slice(7) : ''
  const serverId = String(request.headers['x-fullindex-server-id'] ?? 'default')

  if (token !== config.pluginToken) {
    socket.close(4003, 'unauthorized')
    return
  }

  registerPlugin(serverId, socket)
  socket.on('message', raw => handlePluginMessage(serverId, raw))
})

app.get('/ws/browser', { websocket: true }, socket => {
  registerBrowser(socket)
  liveQueryManager.registerBrowser(socket)
  socket.send(JSON.stringify({
    type: 'server.snapshot',
    items: listServers()
  }))
  socket.send(JSON.stringify({
    type: 'server.status',
    serverId: 'default',
    online: pluginOnline('default'),
    at: Date.now()
  }))
})

app.addHook('onClose', async () => {
  liveQueryManager.close()
  scanManager.close()
  indexStore.close()
})

try {
  const webRoot = config.webDist instanceof URL ? fileURLToPath(config.webDist) : config.webDist
  const assetRoot = path.resolve(webRoot, 'assets')
  const indexPath = path.join(webRoot, 'index.html')
  await fs.access(indexPath)

  app.get('/assets/*', async (request, reply) => {
    const assetPath = path.resolve(assetRoot, String(request.params['*'] ?? ''))
    if (!assetPath.startsWith(`${assetRoot}${path.sep}`)) {
      return reply.code(400).send({ ok: false, error: 'invalid asset path' })
    }
    try {
      const content = await fs.readFile(assetPath)
      const extension = path.extname(assetPath).toLowerCase()
      const contentType = extension === '.js' ? 'text/javascript; charset=utf-8'
        : extension === '.css' ? 'text/css; charset=utf-8'
          : extension === '.svg' ? 'image/svg+xml'
            : extension === '.png' ? 'image/png'
              : 'application/octet-stream'
      return reply.header('Cache-Control', 'public, max-age=31536000, immutable').type(contentType).send(content)
    } catch (error) {
      if (error.code === 'ENOENT') return reply.code(404).send({ ok: false, error: 'asset not found' })
      throw error
    }
  })
  // Vite 每次构建都会生成新的哈希资源名。入口 HTML 必须按请求读取，不能在 Node 启动时缓存，
  // 否则运行中重新构建后会出现“旧 index.html 引用已删除 assets”的白屏。
  app.get('/*', async (_, reply) => {
    const indexHtml = await fs.readFile(indexPath)
    return reply
      .header('Cache-Control', 'no-store, max-age=0')
      .type('text/html; charset=utf-8')
      .send(indexHtml)
  })
} catch (error) {
  app.log.warn({ err: error }, 'web assets could not be registered; API/WS gateway will still run')
}

await app.listen({ host: config.host, port: config.port })
