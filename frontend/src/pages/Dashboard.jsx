import { useState, useEffect } from 'react'
import { Link } from 'react-router-dom'
import { getHealth, getDevices, getDeviceLatest } from '../api/client'
import SensorTile from '../components/DeviceCard'
import DeviceMap from '../components/DeviceMap'

const SENSOR_KINDS = ['gps', 'bme280', 'dust', 'battery', 'lora', 'microphone']

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
      const merged = deviceList.items
        .map((d, i) => ({ device: d, latest: latestList[i] }))
        .filter((e) => e.latest)
      setHealth(healthData)
      setEntries(merged)
    }
    load()
      .catch((err) => setError(err.message))
      .finally(() => setLoading(false))
  }, [])

  if (loading) return <div className="state-msg"><span className="loader" />Chargement…</div>
  if (error) return <div className="state-msg error">⚠ Erreur : {error}</div>

  const tempEntries = entries.filter((e) => e.latest.sensors?.bme280)
  const avgTemp =
    tempEntries.length > 0
      ? (
          tempEntries.reduce((s, e) => s + e.latest.sensors.bme280.temperature, 0) /
          tempEntries.length
        ).toFixed(1)
      : null
  const dustEntries = entries.filter((e) => e.latest.sensors?.dust)
  const avgPm25 =
    dustEntries.length > 0
      ? (dustEntries.reduce((s, e) => s + e.latest.sensors.dust.P2, 0) / dustEntries.length).toFixed(1)
      : null
  const batteryEntries = entries.filter((e) => e.latest.battery && e.latest.battery.voltage_v > 0)
  const avgBattery =
    batteryEntries.length > 0
      ? Math.round(
          batteryEntries.reduce((s, e) => s + e.latest.battery.percentage, 0) /
            batteryEntries.length,
        )
      : null
  const totalMeasurements = entries.reduce((s, e) => s + (e.device.measurement_count ?? 0), 0)

  const tiles = entries.flatMap(({ device, latest }) => {
    const deviceId = latest?.device?.id ?? device.id
    return SENSOR_KINDS.map((kind) => ({ key: `${deviceId}:${kind}`, deviceId, kind, latest }))
  })

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

      <div className="stat-grid">
        <div className="stat-card">
          <div className="stat-icon">📡</div>
          <div className="stat-value">{entries.length}</div>
          <div className="stat-label">Capteurs actifs</div>
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
          <div className="stat-icon">📊</div>
          <div className="stat-value">{totalMeasurements.toLocaleString('fr-FR')}</div>
          <div className="stat-label">Mesures totales</div>
        </div>
      </div>

      {entries.length > 0 && <DeviceMap entries={entries} />}

      <h2 className="section-title">Capteurs</h2>
      {entries.length === 0 ? (
        <p className="empty-state">Aucun capteur enregistré pour l'instant.</p>
      ) : (
        <div className="tile-grid">
          {tiles.map(({ key, deviceId, kind, latest }) => (
            <Link
              key={key}
              to={`/devices/${encodeURIComponent(deviceId)}`}
              className="tile-link"
            >
              <SensorTile deviceId={deviceId} kind={kind} latest={latest} />
            </Link>
          ))}
        </div>
      )}
    </div>
  )
}
