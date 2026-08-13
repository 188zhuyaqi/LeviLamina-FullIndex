const KINDS = new Set(['players', 'containers', 'drops', 'entities', 'items'])

function send(socket, message) {
  if (socket?.readyState === 1) socket.send(JSON.stringify(message))
}

export class LiveQueryManager {
  constructor({ requestPlugin, onPluginMessage, onPluginStatus }) {
    this.requestPlugin = requestPlugin
    this.jobs = new Map()
    this.socketJobs = new Map()
    this.finishWaiters = new Map()
    this.unsubscribeMessages = onPluginMessage(message => this.handlePluginMessage(message))
    this.unsubscribeStatus = onPluginStatus(status => this.handlePluginStatus(status))
  }

  registerBrowser(socket) {
    this.socketJobs.set(socket, new Set())
    socket.on('message', raw => this.handleBrowserMessage(socket, raw))
    socket.on('close', () => this.closeBrowser(socket))
  }

  async handleBrowserMessage(socket, raw) {
    let message
    try {
      message = JSON.parse(raw.toString())
    } catch {
      send(socket, { type: 'live.query.failed', error: '实时查询消息不是有效 JSON' })
      return
    }

    if (message.type === 'live.query.start') {
      await this.start(socket, message)
    } else if (message.type === 'live.query.cancel') {
      await this.cancel(socket, message.jobId)
    }
  }

  async start(socket, message) {
    const serverId = String(message.serverId ?? 'default')
    const jobId = String(message.jobId ?? '')
    const kind = String(message.kind ?? '')
    if (!jobId || !KINDS.has(kind)) {
      send(socket, { type: 'live.query.failed', jobId, serverId, kind, error: '实时查询参数无效' })
      return
    }
    if (this.jobs.has(jobId)) {
      send(socket, { type: 'live.query.failed', jobId, serverId, kind, error: '实时查询任务编号重复' })
      return
    }

    for (const activeId of [...(this.socketJobs.get(socket) ?? [])]) {
      await this.cancel(socket, activeId)
      await this.waitForFinish(activeId)
    }

    const job = { jobId, serverId, kind, socket, status: 'starting', startedAt: Date.now() }
    this.jobs.set(jobId, job)
    if (!this.socketJobs.has(socket)) this.socketJobs.set(socket, new Set())
    this.socketJobs.get(socket).add(jobId)
    send(socket, { type: 'live.query.status', jobId, serverId, kind, status: 'starting', phase: 'starting' })

    try {
      const response = await this.requestPlugin(serverId, 'live.query.start', {
        jobId,
        kind,
        filters: message.filters ?? {},
        batchSize: message.batchSize ?? 200
      }, 10000)
      if (response.ok === false) throw new Error(response.error ?? '插件拒绝了实时查询')
      if (!this.jobs.has(jobId)) return
      job.status = 'running'
      send(socket, { type: 'live.query.status', jobId, serverId, kind, status: 'running', phase: 'accepted' })
    } catch (error) {
      this.finish(jobId)
      send(socket, { type: 'live.query.failed', jobId, serverId, kind, error: error.message })
    }
  }

  async cancel(socket, jobId) {
    const job = this.jobs.get(String(jobId ?? ''))
    if (!job || job.socket !== socket) return
    job.status = 'cancelling'
    send(socket, { type: 'live.query.status', jobId: job.jobId, serverId: job.serverId, kind: job.kind, status: 'cancelling', phase: 'cancelling' })
    try {
      await this.requestPlugin(job.serverId, 'live.query.cancel', { jobId: job.jobId }, 10000)
    } catch {
      this.finish(job.jobId)
    }
  }

  handlePluginMessage(message) {
    if (!message.type?.startsWith('live.query.')) return
    const job = this.jobs.get(message.jobId)
    if (!job || job.serverId !== message.serverId) return
    send(job.socket, message)
    if (['live.query.complete', 'live.query.cancelled', 'live.query.failed'].includes(message.type)) {
      this.finish(job.jobId)
    }
  }

  handlePluginStatus({ serverId, online }) {
    if (online) return
    for (const job of [...this.jobs.values()]) {
      if (job.serverId !== serverId) continue
      send(job.socket, {
        type: 'live.query.failed', jobId: job.jobId, serverId, kind: job.kind,
        error: '插件实时连接已断开'
      })
      this.finish(job.jobId)
    }
  }

  closeBrowser(socket) {
    const ids = [...(this.socketJobs.get(socket) ?? [])]
    this.socketJobs.delete(socket)
    for (const jobId of ids) {
      const job = this.jobs.get(jobId)
      if (!job) continue
      this.jobs.delete(jobId)
      this.requestPlugin(job.serverId, 'live.query.cancel', { jobId }, 3000).catch(() => {})
    }
  }

  finish(jobId) {
    const job = this.jobs.get(jobId)
    if (!job) return
    this.jobs.delete(jobId)
    this.socketJobs.get(job.socket)?.delete(jobId)
    for (const resolve of this.finishWaiters.get(jobId) ?? []) resolve()
    this.finishWaiters.delete(jobId)
  }

  waitForFinish(jobId, timeoutMs = 15000) {
    if (!this.jobs.has(jobId)) return Promise.resolve()
    return new Promise(resolve => {
      const timer = setTimeout(() => {
        const waiters = this.finishWaiters.get(jobId) ?? []
        this.finishWaiters.set(jobId, waiters.filter(waiter => waiter !== done))
        resolve()
      }, timeoutMs)
      const done = () => {
        clearTimeout(timer)
        resolve()
      }
      if (!this.finishWaiters.has(jobId)) this.finishWaiters.set(jobId, [])
      this.finishWaiters.get(jobId).push(done)
    })
  }

  close() {
    this.unsubscribeMessages?.()
    this.unsubscribeStatus?.()
    for (const job of this.jobs.values()) {
      this.requestPlugin(job.serverId, 'live.query.cancel', { jobId: job.jobId }, 3000).catch(() => {})
    }
    this.jobs.clear()
    this.socketJobs.clear()
    for (const waiters of this.finishWaiters.values()) for (const resolve of waiters) resolve()
    this.finishWaiters.clear()
  }
}
