import Fastify from 'fastify'
import cors from '@fastify/cors'
import websocket from '@fastify/websocket'
import fastifyStatic from '@fastify/static'
import { fileURLToPath } from 'node:url'

import { config } from './config.mjs'
import {
  handlePluginMessage,
  listServers,
  registerBrowser,
  registerPlugin,
  requestPlugin
} from './hub.mjs'

const app = Fastify({ logger: true })

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
  socket.send(JSON.stringify({
    type: 'server.snapshot',
    items: listServers()
  }))
})

try {
  await app.register(fastifyStatic, {
    root: fileURLToPath(config.webDist),
    wildcard: false
  })

  app.get('/*', async (_, reply) => reply.sendFile('index.html'))
} catch {
  app.log.warn('web/dist not found; API/WS gateway will still run')
}

await app.listen({ host: config.host, port: config.port })
