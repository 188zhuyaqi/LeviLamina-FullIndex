import { errorLabel } from '../i18n/index.js'

export async function query(action, params = {}, serverId = 'default') {
  const response = await fetch('/api/query', {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ serverId, action, params })
  })

  const data = await response.json()
  if (!response.ok || data.ok === false) {
    throw new Error(errorLabel(data.error || 'HTTP ' + response.status))
  }
  return data
}

export async function listServers() {
  const response = await fetch('/api/servers')
  return response.json()
}

async function jsonRequest(url, options) {
  const response = await fetch(url, options)
  const data = await response.json()
  if (!response.ok || data.ok === false) {
    throw new Error(errorLabel(data.error || 'HTTP ' + response.status))
  }
  return data
}

export function indexStatus(serverId = 'default') {
  return jsonRequest(`/api/index/status?serverId=${encodeURIComponent(serverId)}`)
}

export function refreshIndex(serverId = 'default') {
  return jsonRequest('/api/index/refresh', {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ serverId })
  })
}

export function indexJob(serverId = 'default') {
  return jsonRequest(`/api/index/job?serverId=${encodeURIComponent(serverId)}`)
}

export function cancelIndex(jobId, serverId = 'default') {
  return jsonRequest('/api/index/cancel', {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ serverId, jobId })
  })
}

export function indexedData(kind, { serverId = 'default', page = 1, pageSize = 50, ...params } = {}) {
  const query = new URLSearchParams({ serverId, page: String(page), pageSize: String(pageSize) })
  for (const [key, value] of Object.entries(params)) {
    if (value !== '' && value != null) query.set(key, String(value))
  }
  return jsonRequest(`/api/data/${encodeURIComponent(kind)}?${query}`)
}

export function typeOptions(kind, serverId = 'default') {
  return jsonRequest(`/api/types/${encodeURIComponent(kind)}?serverId=${encodeURIComponent(serverId)}`)
}

export function indexedDetail(kind, id, serverId = 'default') {
  return jsonRequest(`/api/details/${encodeURIComponent(kind)}/${encodeURIComponent(id)}?serverId=${encodeURIComponent(serverId)}`)
}

export function entityChunkDetail(params = {}) {
  const query = new URLSearchParams({ serverId: 'default' })
  for (const [key, value] of Object.entries(params)) {
    if (value !== '' && value != null) query.set(key, String(value))
  }
  return jsonRequest(`/api/details/entities/chunk?${query}`)
}

export function searchItems(params = {}) {
  return jsonRequest('/api/search/items', {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ serverId: 'default', ...params })
  })
}

export function listSnapshots(serverId = 'default') {
  return jsonRequest(`/api/snapshots?serverId=${encodeURIComponent(serverId)}`)
}

export function diffSnapshots(from, to) {
  return jsonRequest(`/api/snapshots/diff?from=${encodeURIComponent(from)}&to=${encodeURIComponent(to)}`)
}
