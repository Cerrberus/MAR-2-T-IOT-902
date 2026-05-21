const BASE_URL = import.meta.env.VITE_API_URL ?? ''
const TOKEN = import.meta.env.VITE_API_TOKEN ?? 'changeme'

function authHeaders() {
  return {
    'Content-Type': 'application/json',
    Authorization: `Bearer ${TOKEN}`,
  }
}

async function request(path, options = {}) {
  const res = await fetch(`${BASE_URL}${path}`, options)
  if (!res.ok) {
    const body = await res.json().catch(() => ({}))
    throw new Error(body.message ?? `HTTP ${res.status}`)
  }
  return res.json()
}

export function getHealth() {
  return request('/api/v1/health')
}

export function getDevices() {
  return request('/api/v1/devices', { headers: authHeaders() })
}

export function getDevice(id) {
  return request(`/api/v1/devices/${encodeURIComponent(id)}`, { headers: authHeaders() })
}

export function getDeviceLatest(id) {
  return request(`/api/v1/devices/${encodeURIComponent(id)}/latest`, { headers: authHeaders() })
}

export function getDeviceBatteryHistory(id, limit = 10) {
  return request(
    `/api/v1/devices/${encodeURIComponent(id)}/battery/history?limit=${limit}`,
    { headers: authHeaders() },
  )
}

export function getMeasurements(params = {}) {
  const query = new URLSearchParams()
  if (params.device_id) query.set('device_id', params.device_id)
  if (params.from) query.set('from', params.from)
  if (params.to) query.set('to', params.to)
  if (params.limit) query.set('limit', String(params.limit))
  if (params.cursor) query.set('cursor', params.cursor)
  return request(`/api/v1/measurements?${query}`, { headers: authHeaders() })
}
