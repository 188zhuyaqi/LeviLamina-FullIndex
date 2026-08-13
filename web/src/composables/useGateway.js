import { computed, onMounted, ref } from 'vue'

const connected = ref(false)
const onlineServers = ref(new Set())
const events = ref([])
const listeners = new Set()
let socket
let reconnectTimer

function connect() {
  if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) return
  const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:'
  socket = new WebSocket(`${protocol}//${location.host}/ws/browser`)

  socket.addEventListener('open', () => {
    connected.value = true
    if (reconnectTimer) clearTimeout(reconnectTimer)
  })
  socket.addEventListener('close', () => {
    connected.value = false
    onlineServers.value = new Set()
    reconnectTimer = setTimeout(connect, 2000)
  })
  socket.addEventListener('message', event => {
    try {
      const message = JSON.parse(event.data)
      if (message.type === 'server.snapshot') {
        onlineServers.value = new Set((message.items ?? []).filter(item => item.online).map(item => item.serverId))
      } else if (message.type === 'server.status') {
        const next = new Set(onlineServers.value)
        if (message.online) next.add(message.serverId)
        else next.delete(message.serverId)
        onlineServers.value = next
      }
      events.value.unshift(message)
      events.value = events.value.slice(0, 100)
      for (const listener of listeners) listener(message)
    } catch {}
  })
}

export function sendGateway(message) {
  if (!socket || socket.readyState !== WebSocket.OPEN) return false
  socket.send(JSON.stringify(message))
  return true
}

export function subscribeGateway(listener) {
  listeners.add(listener)
  connect()
  return () => listeners.delete(listener)
}

export function useGateway() {
  onMounted(connect)
  const pluginConnected = computed(() => connected.value && onlineServers.value.has('default'))
  return { connected, pluginConnected, onlineServers, events }
}
