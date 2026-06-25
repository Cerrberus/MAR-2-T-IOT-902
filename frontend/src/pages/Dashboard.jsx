import { useState, useEffect, useRef, useCallback } from 'react'
import { Link } from 'react-router-dom'
import { getHealth, getDevices, getDeviceLatest, getMeasurements } from '../api/client'
import DeviceCard from '../components/DeviceCard'
import DeviceMap from '../components/DeviceMap'
import ToastContainer from '../components/ToastContainer'
import { relativeTime } from '../utils'

const POLL_MS = 20_000

async function fetchDashboardData() {
  const [healthData, deviceList] = await Promise.all([getHealth(), getDevices()])
  const [latestList, sparklineList] = await Promise.all([
    Promise.all(deviceList.items.map((d) => getDeviceLatest(d.id).catch(() => null))),
    Promise.all(
      deviceList.items.map((d) =>
        getMeasurements({ device_id: d.id, limit: 15 }).catch(() => ({ items: [] })),
      ),
    ),
  ])
  return {
    health: healthData,
    entries: deviceList.items.map((d, i) => ({
      device: d,
      latest: latestList[i],
      sparkline: [...(sparklineList[i].items ?? [])].reverse().map((m) => ({
        temperature: m.sensors.bme280?.temperature ?? null,
        pm25: m.sensors.dust?.P2 ?? null,
      })),
    })),
  }
}

function getChangedDevices(entries, prevCounts) {
  return entries
    .filter((e) => (e.device.measurement_count ?? 0) > (prevCounts[e.device.id] ?? 0))
    .map((e) => e.device.id)
}

function syncCounts(entries, prevCounts) {
  entries.forEach((e) => { prevCounts[e.device.id] = e.device.measurement_count ?? 0 })
}

// Passed as third arg to setTimeout so setToasts receives it directly — avoids nested arrow
function withoutToast(id) {
  return (toasts) => toasts.filter((t) => t.id !== id)
}

export default function Dashboard() {
  const [health, setHealth] = useState(null)
  const [entries, setEntries] = useState([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState(null)
  const [lastUpdated, setLastUpdated] = useState(null)
  const [refreshing, setRefreshing] = useState(false)
  const [toasts, setToasts] = useState([])
  const prevCountsRef = useRef({})
  const aliveRef = useRef(true)

  const addToast = useCallback((message) => {
    const id = Date.now()
    setToasts((prev) => [...prev, { id, message }])
    setTimeout(setToasts, 3000, withoutToast(id))
  }, [])

  const applyData = useCallback((data, initial) => {
    if (!initial) {
      getChangedDevices(data.entries, prevCountsRef.current).forEach((id) =>
        addToast(`Nouvelles données de ${id}`),
      )
    }
    syncCounts(data.entries, prevCountsRef.current)
    setHealth(data.health)
    setEntries(data.entries)
    setLastUpdated(new Date())
    setError(null)
  }, [addToast])

  const pollFetch = useCallback(() => {
    setRefreshing(true)
    fetchDashboardData()
      .then((data) => { if (aliveRef.current) applyData(data, false) })
      .catch(() => {})
      .finally(() => { if (aliveRef.current) setRefreshing(false) })
  }, [applyData])

  useEffect(() => {
    aliveRef.current = true

    fetchDashboardData()
      .then((data) => { if (aliveRef.current) applyData(data, true) })
      .catch((err) => { if (aliveRef.current) setError(err.message) })
      .finally(() => { if (aliveRef.current) setLoading(false) })

    const timer = setInterval(pollFetch, POLL_MS)
    return () => { aliveRef.current = false; clearInterval(timer) }
  }, [applyData, pollFetch])

  if (loading) return <div className="state-msg"><span className="loader" />Chargement…</div>
  if (error) return <div className="state-msg error">Erreur : {error}</div>

  const withLatest = entries.filter((e) => e.latest)
  const offlineCount = entries.length - withLatest.length
  const tempEntries = withLatest.filter((e) => e.latest.sensors?.bme280?.temperature != null)
  const avgTemp =
    tempEntries.length > 0
      ? (
          tempEntries.reduce((s, e) => s + e.latest.sensors.bme280.temperature, 0) /
          tempEntries.length
        ).toFixed(1)
      : null
  const dustEntries = withLatest.filter((e) => e.latest.sensors?.dust)
  const avgPm25 =
    dustEntries.length > 0
      ? Math.min(100, Math.round(dustEntries.reduce((s, e) => s + e.latest.sensors.dust.P2, 0) / dustEntries.length / 150 * 100))
      : null
  const batteryEntries = withLatest.filter((e) => e.latest.battery && e.latest.battery.voltage_v > 0)
  const avgBattery =
    batteryEntries.length > 0
      ? Math.round(
          batteryEntries.reduce((s, e) => s + e.latest.battery.percentage, 0) /
            batteryEntries.length,
        )
      : null
  const totalMeasurements = entries.reduce((s, e) => s + (e.device.measurement_count ?? 0), 0)

  return (
    <div className="container">
      <div className="dashboard-topbar">
        {health && (
          <div className={`health-banner ${health.status === 'ok' ? 'health-ok' : 'health-degraded'}`}>
            <span className="health-dot" />
            API {health.status === 'ok' ? 'opérationnelle' : 'dégradée'} · DB {health.database} · v
            {health.version}
          </div>
        )}
        <div className="live-badge">
          {refreshing ? <span className="loader loader-sm" /> : <span className="live-dot" />}
          {lastUpdated ? `Mis à jour ${relativeTime(lastUpdated)}` : 'En direct'}
        </div>
      </div>

      <h1 className="page-title">Tableau de bord</h1>

      <div className="stat-grid">
        <div className="stat-card">
          <div className="stat-value">
            {withLatest.length}
            {offlineCount > 0 && <span className="stat-offline"> / {entries.length}</span>}
          </div>
          <div className="stat-label">Capteurs actifs</div>
          {offlineCount > 0 && <div className="stat-warning">{offlineCount} hors ligne</div>}
        </div>
        {avgTemp !== null && (
          <div className="stat-card">
            <div className="stat-value">{avgTemp}<span className="stat-unit"> °C</span></div>
            <div className="stat-label">Temp. moyenne</div>
          </div>
        )}
        {avgPm25 !== null && (
          <div className="stat-card">
            <div className="stat-value">{avgPm25}<span className="stat-unit"> %</span></div>
            <div className="stat-label">Poussière moyenne</div>
          </div>
        )}
        {avgBattery !== null && (
          <div className="stat-card">
            <div className="stat-icon">🔋</div>
            <div className="stat-value">
              {avgBattery}
              <span className="stat-unit"> %</span>
            </div>
            <div className="stat-label">Batterie moyenne</div>
          </div>
        )}
        <div className="stat-card">
          <div className="stat-value">{totalMeasurements.toLocaleString('fr-FR')}</div>
          <div className="stat-label">Mesures totales</div>
        </div>
      </div>

      {entries.length > 0 && <DeviceMap entries={entries} />}

      <h2 className="section-title">Capteurs</h2>
      {entries.length === 0 ? (
        <p className="empty-state">Aucun capteur enregistré pour l'instant.</p>
      ) : (
        <div className="device-grid">
          {entries.map(({ device, latest, sparkline }) => (
            <Link
              key={device.id}
              to={`/devices/${encodeURIComponent(device.id)}`}
              className="device-link"
            >
              <DeviceCard device={device} latest={latest} sparkline={sparkline} />
            </Link>
          ))}
        </div>
      )}

      <ToastContainer toasts={toasts} />
    </div>
  )
}
