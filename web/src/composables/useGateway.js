import { onBeforeUnmount, onMounted, ref } from 'vue'

export function useGateway() {
  const connected = ref(false)
  const events = ref([])
  let socket

  const connect = () => {
    const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:'
    socket = new WebSocket(`${protocol}//${location.host}/ws/browser`)

    socket.addEventListener('open', () => { connected.value = true })
    socket.addEventListener('close', () => { connected.value = false })
    socket.addEventListener('message', event => {
      try {
        events.value.unshift(JSON.parse(event.data))
        events.value = events.value.slice(0, 100)
      } catch {}
    })
  }

  onMounted(connect)
  onBeforeUnmount(() => socket?.close())

  return { connected, events }
}
