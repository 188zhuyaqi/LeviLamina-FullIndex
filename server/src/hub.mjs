import crypto from 'node:crypto'

const plugins = new Map()
const browsers = new Set()
const pending = new Map()
const pluginMessageListeners = new Set()
const pluginStatusListeners = new Set()

export function registerPlugin(serverId, socket) {
  const previous = plugins.get(serverId)
  if (previous && previous !== socket) {
    try { previous.close(4001, 'replaced by a new connection') } catch {}
  }

  plugins.set(serverId, socket)
  for (const listener of pluginStatusListeners) listener({ serverId, online: true })
  broadcast({
    type: 'server.status',
    serverId,
    online: true,
    at: Date.now()
  })

  socket.on('close', () => {
    if (plugins.get(serverId) === socket) {
      plugins.delete(serverId)
      for (const listener of pluginStatusListeners) listener({ serverId, online: false })
      broadcast({
        type: 'server.status',
        serverId,
        online: false,
        at: Date.now()
      })
    }
  })
}

export function registerBrowser(socket) {
  browsers.add(socket)
  socket.on('close', () => browsers.delete(socket))
}

export function broadcast(message) {
  const text = JSON.stringify(message)
  for (const socket of browsers) {
    if (socket.readyState === 1) socket.send(text)
  }
}

export function onPluginMessage(listener) {
  pluginMessageListeners.add(listener)
  return () => pluginMessageListeners.delete(listener)
}

export function onPluginStatus(listener) {
  pluginStatusListeners.add(listener)
  return () => pluginStatusListeners.delete(listener)
}

export function pluginOnline(serverId = 'default') {
  const socket = plugins.get(serverId)
  return Boolean(socket && socket.readyState === 1)
}

export function listServers() {
  return [...plugins.keys()].map(serverId => ({ serverId, online: true }))
}

export function handlePluginMessage(serverId, raw) {
  let message
  try {
    message = JSON.parse(raw.toString())
  } catch {
    return
  }

  if (message.type === 'response' && message.requestId) {
    const waiter = pending.get(message.requestId)
    if (waiter) {
      pending.delete(message.requestId)
      clearTimeout(waiter.timer)
      waiter.resolve(message)
      return
    }
  }

  const enriched = { ...message, serverId }
  for (const listener of pluginMessageListeners) listener(enriched)
  if (!message.type?.startsWith('live.query.') && !message.type?.startsWith('index.')) broadcast(enriched)
}

export function requestPlugin(serverId, action, params = {}, timeoutMs = 30000) {
  const socket = plugins.get(serverId)
  if (!socket || socket.readyState !== 1) {
    return Promise.reject(new Error(`server ${serverId} is offline`))
  }

  const requestId = crypto.randomUUID()
  const payload = JSON.stringify({
    type: 'request',
    requestId,
    action,
    params
  })

  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      pending.delete(requestId)
      reject(new Error(`plugin request timed out: ${action}`))
    }, timeoutMs)

    pending.set(requestId, { resolve, reject, timer })
    socket.send(payload)
  })
}
