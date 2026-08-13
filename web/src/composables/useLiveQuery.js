import { computed, onBeforeUnmount, ref, watch } from 'vue'
import { sendGateway, subscribeGateway, useGateway } from './useGateway.js'
import { errorLabel } from '../i18n/index.js'

export function useLiveQuery(kind, serverId = 'default') {
  const { pluginConnected } = useGateway()
  const enabled = ref(false)
  const jobId = ref('')
  const status = ref('idle')
  const phase = ref('idle')
  const progress = ref(0)
  const received = ref(0)
  const matched = ref(0)
  const durationMs = ref(0)
  const items = ref([])
  const error = ref('')
  const version = ref(0)
  const fallbackToken = ref(0)
  const running = computed(() => ['starting', 'running', 'cancelling'].includes(status.value))
  const complete = computed(() => status.value === 'complete')

  function clear() {
    items.value = []
    received.value = 0
    matched.value = 0
    progress.value = 0
    durationMs.value = 0
    version.value += 1
  }

  function cancel({ notify = true, clearResults = false } = {}) {
    if (notify && jobId.value) {
      sendGateway({ type: 'live.query.cancel', serverId, jobId: jobId.value })
    }
    jobId.value = ''
    status.value = 'idle'
    phase.value = 'idle'
    if (clearResults) clear()
  }

  function requestCancel() {
    if (!jobId.value || !running.value) return
    status.value = 'cancelling'
    phase.value = 'cancelling'
    sendGateway({ type: 'live.query.cancel', serverId, jobId: jobId.value })
  }

  function start(filters = {}) {
    if (!enabled.value || !pluginConnected.value) return false
    cancel({ notify: true, clearResults: true })
    error.value = ''
    jobId.value = crypto.randomUUID()
    status.value = 'starting'
    phase.value = 'starting'
    const sent = sendGateway({
      type: 'live.query.start',
      serverId,
      jobId: jobId.value,
      kind,
      filters,
      batchSize: 200
    })
    if (!sent) {
      error.value = '实时连接不可用'
      cancel({ notify: false })
      return false
    }
    return true
  }

  function setEnabled(value) {
    if (value && pluginConnected.value) {
      enabled.value = true
      return true
    }
    enabled.value = false
    cancel({ notify: true, clearResults: true })
    return false
  }

  const unsubscribe = subscribeGateway(message => {
    if (!message.type?.startsWith('live.query.') || message.jobId !== jobId.value) return
    if (message.type === 'live.query.status') {
      status.value = message.status ?? status.value
      phase.value = message.phase ?? phase.value
      return
    }
    if (message.type === 'live.query.progress') {
      status.value = 'running'
      phase.value = message.phase ?? phase.value
      progress.value = Number(message.percent ?? progress.value)
      matched.value = Number(message.matched ?? matched.value)
      return
    }
    if (message.type === 'live.query.batch') {
      status.value = 'running'
      phase.value = 'streaming'
      items.value.push(...(message.items ?? []))
      received.value = Number(message.received ?? items.value.length)
      matched.value = Number(message.matched ?? matched.value)
      progress.value = Number(message.percent ?? progress.value)
      version.value += 1
      return
    }
    if (message.type === 'live.query.complete') {
      status.value = 'complete'
      phase.value = 'complete'
      progress.value = 100
      matched.value = Number(message.count ?? items.value.length)
      received.value = items.value.length
      durationMs.value = Number(message.durationMs ?? 0)
      jobId.value = ''
      version.value += 1
      return
    }
    if (message.type === 'live.query.cancelled') {
      status.value = 'cancelled'
      phase.value = 'cancelled'
      jobId.value = ''
      return
    }
    if (message.type === 'live.query.failed') {
      status.value = 'failed'
      phase.value = 'failed'
      error.value = errorLabel(message.error ?? '实时查询失败')
      jobId.value = ''
    }
  })

  watch(pluginConnected, available => {
    if (available || !enabled.value) return
    enabled.value = false
    cancel({ notify: false, clearResults: true })
    fallbackToken.value += 1
  })

  onBeforeUnmount(() => {
    cancel({ notify: true, clearResults: true })
    unsubscribe()
  })

  return {
    available: pluginConnected,
    enabled,
    running,
    complete,
    status,
    phase,
    progress,
    received,
    matched,
    durationMs,
    items,
    error,
    version,
    fallbackToken,
    start,
    cancel,
    requestCancel,
    setEnabled
  }
}
