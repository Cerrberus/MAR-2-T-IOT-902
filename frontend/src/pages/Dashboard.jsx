import { useState, useEffect } from 'react'
import { Link } from 'react-router-dom'
import { getHealth, getDevices, getDeviceLatest } from '../api/client'
import DeviceCard from '../components/DeviceCard'
import DeviceMap from '../components/DeviceMap'

export default function Dashboard() {
  const [health, setHealth] = useState(null)
  const [entries, setEntries] = useState([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState(null)

  useEffect(() => {
    async function load() {
      const [healthData, deviceList] = await Promise.all([getHealth(), getDevices()])
      const latestList = await Promise.all(
        deviceList.items.map((d) => getDeviceLatest(d.id).catch(() => null)),
      )
      setHealth(healthData)
      setEntries(deviceList.items.map((d, i) => ({ device: d, latest: latestList[i] })))
    }
    load()
      .catch((err) => setError(err.message))
      .finally(() => setLoading(false))
  }, [])

  if (loading) return <div className="state-msg"><span className="loader" />Chargement…</div>
  if (error) return <div className="state-msg error">⚠ Erreur : {error}</div>

  const withLatest = entries.filter((e) => e.latest)
  const avgTemp =
    withLatest.length > 0
      ? (
          withLatest.reduce((s, e) => s + (e.latest.sensors.bme280?.temperature ?? 0), 0) /
          withLatest.length
        ).toFixed(1)
      : null
  const dustEntries = withLatest.filter((e) => e.latest.sensors.dust)
  const avgPm25 =
    dustEntries.length > 0
      ? (dustEntries.reduce((s, e) => s + e.latest.sensors.dust.P2, 0) / dustEntries.length).toFixed(1)
      : null
  const totalMeasurements = entries.reduce((s, e) => s + (e.device.measurement_count ?? 0), 0)

  return (
    <div className="container">
      {health && (
        <div className={`health-banner ${health.status === 'ok' ? 'health-ok' : 'health-degraded'}`}>
          <span className="health-dot" />
          API {health.status === 'ok' ? 'opérationnelle' : 'dégradée'} · DB {health.database} · v
          {health.version}
        </div>
      )}

      <h1 className="page-title gradient-text">Tableau de bord</h1>

      {/* ── Stats ── */}
      <div className="stat-grid">
        <div className="stat-card">
          <div className="stat-icon">📡</div>
          <div className="stat-value">{entries.length}</div>
          <div className="stat-label">Capteurs</div>
        </div>
        {avgTemp !== null && (
          <div className="stat-card">
            <div className="stat-icon">🌡️</div>
            <div className="stat-value">
              {avgTemp}
              <span className="stat-unit"> °C</span>
            </div>
            <div className="stat-label">Temp. moyenne</div>
          </div>
        )}
        {avgPm25 !== null && (
          <div className="stat-card">
            <div className="stat-icon">💨</div>
            <div className="stat-value">
              {avgPm25}
              <span className="stat-unit"> µg/m³</span>
            </div>
            <div className="stat-label">PM2.5 moyen</div>
          </div>
        )}
        <div className="stat-card">
          <div className="stat-icon">📊</div>
          <div className="stat-value">{totalMeasurements.toLocaleString('fr-FR')}</div>
          <div className="stat-label">Mesures totales</div>
        </div>
      </div>

      {/* ── Carte ── */}
      {entries.length > 0 && <DeviceMap entries={entries} />}

      {/* ── Capteurs ── */}
      <h2 className="section-title">Capteurs</h2>
      {entries.length === 0 ? (
        <p className="empty-state">Aucun capteur enregistré pour l'instant.</p>
      ) : (
        <div className="device-grid">
          {entries.map(({ device, latest }) => (
            <Link
              key={device.id}
              to={`/devices/${encodeURIComponent(device.id)}`}
              className="device-link"
            >
              <DeviceCard device={device} latest={latest} />
            </Link>
          ))}
        </div>
      )}
    </div>
  )
}
