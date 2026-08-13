import { computed, onMounted, ref } from 'vue'
import { cancelIndex, indexJob, refreshIndex } from '../api/client.js'
import { subscribeGateway } from './useGateway.js'
import { errorLabel } from '../i18n/index.js'

const job = ref(null)
const error = ref('')
const submitting = ref(false)
let initialized = false

subscribeGateway(message => {
  if (message.type === 'index.job' && message.serverId === 'default') {
    job.value = message
    error.value = errorLabel(message.error ?? '')
  }
})

async function load() {
  try {
    const response = await indexJob()
    job.value = response.job
  } catch (e) {
    error.value = e.message
  }
}

export function useIndexJob() {
  const active = computed(() => ['starting', 'running', 'cancelling', 'committing'].includes(job.value?.status))

  async function start() {
    submitting.value = true
    error.value = ''
    try {
      const response = await refreshIndex()
      job.value = response.job
      return response.job
    } catch (e) {
      error.value = e.message
      throw e
    } finally {
      submitting.value = false
    }
  }

  async function cancel() {
    if (!job.value?.jobId) return
    submitting.value = true
    error.value = ''
    try {
      const response = await cancelIndex(job.value.jobId)
      job.value = response.job
    } catch (e) {
      error.value = e.message
    } finally {
      submitting.value = false
    }
  }

  onMounted(() => {
    if (!initialized) {
      initialized = true
      load()
    }
  })

  return { job, active, error, submitting, start, cancel, load }
}
