import crypto from 'node:crypto'

const KINDS = ['players', 'containers', 'drops', 'entities']

export class ScanManager {
  constructor({ indexStore, requestPlugin, broadcast, onPluginMessage }) {
    this.indexStore = indexStore
    this.requestPlugin = requestPlugin
    this.broadcast = broadcast
    this.jobs = new Map()
    this.unsubscribe = onPluginMessage(message => this.handlePluginMessage(message))
  }

  active(serverId) {
    const job = this.jobs.get(serverId)
    return job && ['starting', 'running', 'cancelling', 'committing'].includes(job.status)
      ? this.publicJob(job)
      : null
  }

  status(serverId) {
    const job = this.jobs.get(serverId)
    return job ? this.publicJob(job) : null
  }

  async start(serverId = 'default', batchSize = 100) {
    const active = this.active(serverId)
    if (active) throw new Error(`index job ${active.jobId} is already running`)

    const job = {
      jobId: crypto.randomUUID(),
      serverId,
      status: 'starting',
      phase: 'starting',
      kind: null,
      percent: 0,
      counts: {},
      datasets: Object.fromEntries(KINDS.map(kind => [kind, []])),
      startedAt: Date.now(),
      updatedAt: Date.now(),
      error: null,
      snapshot: null
    }
    this.jobs.set(serverId, job)
    this.publish(job)

    try {
      const response = await this.requestPlugin(serverId, 'index.start', {
        jobId: job.jobId,
        batchSize
      }, 10000)
      if (response.ok === false) throw new Error(response.error ?? 'plugin rejected index job')
      job.status = 'running'
      job.phase = 'accepted'
      job.updatedAt = Date.now()
      this.publish(job)
      return this.publicJob(job)
    } catch (error) {
      job.status = 'failed'
      job.phase = 'failed'
      job.error = error.message
      job.updatedAt = Date.now()
      this.publish(job)
      throw error
    }
  }

  async cancel(serverId = 'default', jobId) {
    const job = this.jobs.get(serverId)
    if (!job || !this.active(serverId)) throw new Error('index job is not running')
    if (jobId && job.jobId !== jobId) throw new Error('index job id does not match')
    job.status = 'cancelling'
    job.phase = 'cancelling'
    job.updatedAt = Date.now()
    this.publish(job)
    const response = await this.requestPlugin(serverId, 'index.cancel', { jobId: job.jobId }, 10000)
    if (response.ok === false) throw new Error(response.error ?? 'plugin rejected cancellation')
    return this.publicJob(job)
  }

  handlePluginMessage(message) {
    if (!message.type?.startsWith('index.')) return
    const job = this.jobs.get(message.serverId)
    if (!job || message.jobId !== job.jobId) return

    job.updatedAt = Date.now()
    if (message.type === 'index.progress') {
      job.status = 'running'
      job.phase = message.phase
      job.kind = message.kind
      job.percent = Number(message.percent ?? job.percent)
      if (message.count != null) job.counts[message.kind] = Number(message.count)
      this.publish(job, message)
      return
    }

    if (message.type === 'index.batch') {
      if (!KINDS.includes(message.kind)) return
      if (message.replace) job.datasets[message.kind] = []
      job.datasets[message.kind].push(...(message.items ?? []))
      job.counts[message.kind] = job.datasets[message.kind].length
      this.publish(job, {
        ...message,
        received: job.datasets[message.kind].length,
        items: undefined
      })
      return
    }

    if (message.type === 'index.complete') {
      job.status = 'committing'
      job.phase = 'committing'
      job.percent = 100
      job.counts = message.counts ?? job.counts
      this.publish(job)
      try {
        job.snapshot = this.indexStore.createSnapshot(job.serverId, job.datasets)
        job.status = 'complete'
        job.phase = 'complete'
        job.error = null
      } catch (error) {
        job.status = 'failed'
        job.phase = 'failed'
        job.error = error.message
      }
      job.datasets = null
      job.updatedAt = Date.now()
      this.publish(job)
      return
    }

    if (message.type === 'index.cancelled' || message.type === 'index.failed') {
      job.status = message.type === 'index.cancelled' ? 'cancelled' : 'failed'
      job.phase = job.status
      job.error = message.error ?? null
      job.datasets = null
      this.publish(job)
    }
  }

  publish(job, source = {}) {
    this.broadcast({
      type: 'index.job',
      ...this.publicJob(job),
      batch: source.type === 'index.batch' ? {
        kind: source.kind,
        batchIndex: source.batchIndex,
        batchCount: source.batchCount,
        received: source.received
      } : undefined
    })
  }

  publicJob(job) {
    return {
      jobId: job.jobId,
      serverId: job.serverId,
      status: job.status,
      phase: job.phase,
      kind: job.kind,
      percent: job.percent,
      counts: { ...job.counts },
      startedAt: job.startedAt,
      updatedAt: job.updatedAt,
      error: job.error,
      snapshot: job.snapshot
    }
  }

  close() {
    this.unsubscribe?.()
  }
}
