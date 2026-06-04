import { useState, useEffect } from 'react'
import { useParams, Link } from 'react-router-dom'
import { getDevice, getDeviceLatest, getDeviceBatteryHistory, getMeasurements } from '../api/client'
import SensorChart from '../components/SensorChart'

function Field({ label, value }) {
  if (value == null) return null
  return (
    <div className="field">
      <span className="field-label">{label}</span>
      <span className="field-value">{value}</span>
    </div>
  )
}

function AirQualityBadge({ pm25 }) {
  let label, cls
  if (pm25 < 12) { label = 'Bonne'; cls = 'aq-good' }
  else if (pm25 < 35) { label = 'Modérée'; cls = 'aq-moderate' }
  else if (pm25 < 55) { label = 'Mauvaise'; cls = 'aq-unhealthy' }
  else { label = 'Très mauvaise'; cls = 'aq-hazardous' }
  return (
    <div className={`aq-badge ${cls}`}>
      {label} - PM2.5 = {pm25.toFixed(1)} µg/m³
    </div>
  )
}

export default function DeviceDetail() {
  const { id } = useParams()
  const [device, setDevice] = useState(null)
  const [latest, setLatest] = useState(null)
  const [batteryHistory, setBatteryHistory] = useState([])
  const [chartData, setChartData] = useState([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState(null)

  useEffect(() => {
    async function load() {
      const [dev, lat, bat, meas] = await Promise.all([
        getDevice(id),
        getDeviceLatest(id).catch(() => null),
        getDeviceBatteryHistory(id, 20).catch(() => ({ items: [] })),
        getMeasurements({ device_id: id, limit: 50 }).catch(() => ({ items: [] })),
      ])
      setDevice(dev)
      setLatest(lat)
      setBatteryHistory(bat.items ?? [])
      // oldest-first for charts
      setChartData(
        [...(meas.items ?? [])].reverse().map((m) => ({
          timestamp: m.timestamp,
          temperature: m.sensors.bme280?.temperature ?? null,
          humidity: m.sensors.bme280?.humidity ?? null,
          pressure: m.sensors.bme280?.pressure ?? null,
          pm25: m.sensors.dust?.P2 ?? null,
          pm10: m.sensors.dust?.P1 ?? null,
          battery: m.battery?.percentage ?? null,
        })),
      )
    }
    load()
      .catch((err) => setError(err.message))
      .finally(() => setLoading(false))
  }, [id])

  if (loading) return <div className="state-msg"><span className="loader" />Chargement…</div>
  if (error) return <div className="state-msg error">Erreur : {error}</div>
  if (!device) return <div className="state-msg">Capteur introuvable.</div>

  const bme = latest?.sensors?.bme280
  const dust = latest?.sensors?.dust
  const microphone = latest?.sensors?.microphone
  const battery = latest?.battery
  const transmission = latest?.transmission

  return (
    <div className="container">
      <Link to="/" className="back-link">← Retour</Link>
      <h1 className="page-title gradient-text">{latest?.device?.id ?? device.id}</h1>

      {/* ── Métriques clés ── */}
      <div className="metric-row">
        {latest?.sensors?.bme280?.temperature != null && (
          <div className="metric-chip" style={{ '--mc': '#ff6b6b' }}>
            <span className="metric-icon">🌡️</span>
            <span className="metric-val">{latest.sensors.bme280.temperature.toFixed(1)} °C</span>
            <span className="metric-lbl">Température</span>
          </div>
        )}
        {latest?.sensors?.dust?.P2 != null && (
          <div className="metric-chip" style={{ '--mc': '#00cfff' }}>
            <span className="metric-icon">💨</span>
            <span className="metric-val">{latest.sensors.dust.P2.toFixed(1)} µg/m³</span>
            <span className="metric-lbl">PM2.5</span>
          </div>
        )}
        {latest?.battery?.percentage != null && (
          <div className="metric-chip" style={{ '--mc': '#00e676' }}>
            <span className="metric-icon">🔋</span>
            <span className="metric-val">{latest.battery.percentage} %</span>
            <span className="metric-lbl">Batterie</span>
          </div>
        )}
        {latest?.transmission?.rssi != null && (
          <div className="metric-chip" style={{ '--mc': '#a855f7' }}>
            <span className="metric-icon">📶</span>
            <span className="metric-val">{latest.transmission.rssi} dBm</span>
            <span className="metric-lbl">RSSI</span>
          </div>
        )}
      </div>

      {/* ── Graphiques ── */}
      {chartData.length > 0 && (
        <>
          <h2 className="section-title">Historique</h2>
          <div className="charts-row">
            <SensorChart
              data={chartData}
              dataKey="temperature"
              label="Température"
              color="#ff6b6b"
              unit="°C"
            />
            {chartData.some((d) => d.pm25 != null) && (
              <SensorChart
                data={chartData}
                dataKey="pm25"
                label="PM2.5"
                color="#00cfff"
                unit="µg/m³"
              />
            )}
            {chartData.some((d) => d.humidity != null) && (
              <SensorChart
                data={chartData}
                dataKey="humidity"
                label="Humidité"
                color="#a855f7"
                unit="%"
              />
            )}
            <SensorChart
              data={chartData}
              dataKey="battery"
              label="Batterie"
              color="#00e676"
              unit="%"
            />
          </div>
        </>
      )}

      <h2 className="section-title">Informations détaillées</h2>
      <div className="section-grid">
        {/* Informations générales */}
        <section className="card">
          <h2>Informations</h2>
          <Field label="Firmware" value={device.firmware_version} />
          <Field label="Latitude" value={`${device.location.latitude.toFixed(6)}°`} />
          <Field label="Longitude" value={`${device.location.longitude.toFixed(6)}°`} />
          <Field label="sensor.community ID" value={device.sensor_community_id} />
          <Field label="Premier vu" value={new Date(device.first_seen_at).toLocaleString('fr-FR')} />
          <Field label="Dernier vu" value={new Date(device.last_seen_at).toLocaleString('fr-FR')} />
          <Field label="Mesures totales" value={device.measurement_count} />
        </section>

        {/* Environnement (BME280) */}
        {bme && (
          <section className="card">
            <h2>Environnement</h2>
            <Field label="Température" value={`${bme.temperature.toFixed(1)} °C`} />
            <Field label="Pression" value={`${bme.pressure.toFixed(1)} hPa`} />
            {bme.humidity != null && (
              <Field label="Humidité" value={`${bme.humidity.toFixed(0)} %`} />
            )}
            <Field
              label="Horodatage"
              value={latest?.timestamp ? new Date(latest.timestamp).toLocaleString('fr-FR') : null}
            />
          </section>
        )}

        {/* Qualité de l'air (capteur à poussière) */}
        {dust && (
          <section className="card">
            <h2>Qualité de l'air</h2>
            <Field label="PM10 (P1)" value={`${dust.P1.toFixed(1)} µg/m³`} />
            <Field label="PM2.5 (P2)" value={`${dust.P2.toFixed(1)} µg/m³`} />
            <Field label="Capteur" value={dust.type} />
            <AirQualityBadge pm25={dust.P2} />
          </section>
        )}

        {/* Batterie */}
        {battery && (
          <section className="card">
            <h2>Batterie</h2>
            <Field label="Niveau" value={`${battery.percentage} %`} />
            <Field label="Tension" value={`${battery.voltage_v.toFixed(2)} V`} />
            <Field label="En charge" value={battery.charging ? 'Oui ⚡' : 'Non'} />
            <div className="battery-bar-wrap">
              <div
                className={`battery-bar ${battery.percentage >= 50 ? 'battery-ok' : battery.percentage >= 20 ? 'battery-mid' : 'battery-low'}`}
                style={{ width: `${battery.percentage}%` }}
              />
            </div>
          </section>
        )}

        {/* Transmission LoRa */}
        {transmission && (
          <section className="card">
            <h2>Transmission</h2>
            <Field label="Protocole" value={transmission.protocol} />
            <Field label="RSSI" value={`${transmission.rssi} dBm`} />
            <Field label="SNR" value={`${transmission.snr.toFixed(1)} dB`} />
          </section>
        )}

        {/* Microphone */}
        {microphone && (
          <section className="card">
            <h2>Microphone</h2>
            <Field label="Type" value={microphone.type} />
            <Field label="Niveau" value={`${microphone.level} ADC`} />
          </section>
        )}
      </div>

      {/* Historique batterie */}
      {batteryHistory.length > 0 && (
        <section className="card mt-4">
          <h2>Historique batterie (10 dernières entrées)</h2>
          <table className="data-table">
            <thead>
              <tr>
                <th>Horodatage</th>
                <th>Niveau (%)</th>
                <th>Tension (V)</th>
                <th>En charge</th>
              </tr>
            </thead>
            <tbody>
              {batteryHistory.map((entry, i) => (
                <tr key={i}>
                  <td>{new Date(entry.timestamp).toLocaleString('fr-FR')}</td>
                  <td>{entry.percentage} %</td>
                  <td>{entry.voltage_v.toFixed(2)}</td>
                  <td>{entry.charging ? '⚡' : '-'}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </section>
      )}
    </div>
  )
}
