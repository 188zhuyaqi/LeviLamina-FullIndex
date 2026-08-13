export async function query(action, params = {}, serverId = 'default') {
  const response = await fetch('/api/query', {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ serverId, action, params })
  })

  const data = await response.json()
  if (!response.ok || data.ok === false) {
    throw new Error(data.error ?? `HTTP ${response.status}`)
  }
  return data
}

export async function listServers() {
  const response = await fetch('/api/servers')
  return response.json()
}
