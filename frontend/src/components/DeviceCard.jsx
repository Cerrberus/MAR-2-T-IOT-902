import PropTypes from 'prop-types'
import { relativeTime, freshnessClass } from '../utils'
import Sparkline from './Sparkline'

function pmClass(pm25) {
  if (pm25 == null) return ''
  if (pm25 < 12) return 'pm-good'
  if (pm25 < 35) return 'pm-moderate'
  if (pm25 < 55) return 'pm-unhealthy'
  return 'pm-hazardous'
}

function batteryClass(pct) {
  if (pct >= 50) return 'battery-ok'
  if (pct >= 20) return 'battery-mid'
  return 'battery-low'
}

function Row({ label, value, valueClass }) {
  return (
    <div className="sensor-row">
      <span className="sensor-row-label">{label}</span>
      <span className={`sensor-row-value ${valueClass ?? ''}`}>{value}</span>
    </div>
  )
}
function pmBarWidth(pm25) {
  return `${Math.min(100, (pm25 / 150) * 100)}%`
}

export default function DeviceCard({ device, latest, sparkline }) {
  const bme = latest?.sensors?.bme280
  const dust = latest?.sensors?.dust
  const battery = latest?.battery
  const isOffline = latest == null

  const sparklineKey = dust ? 'pm25' : 'temperature'
  const sparklineColor = dust ? '#00cfff' : '#ff6b6b'
  const hasSparkline = sparkline?.some((d) => d[sparklineKey] != null)

  return (
    <div className={`device-card${isOffline ? ' offline' : ''}`}>
      <div className="device-card-header">
        <span className="device-id">{device.id}</span>
        <div className="device-card-badges">
          {!isOffline && <span className={`freshness-dot ${freshnessClass(device.last_seen_at)}`} />}
          {isOffline && <span className="badge offline-badge">Hors ligne</span>}
          {!isOffline && battery != null && (
            <span className={`badge ${batteryClass(battery.percentage)}`}>
              {battery.percentage}%
            </span>
          )}
        </div>
      </div>

export default function SensorTile({ deviceId, kind, latest }) {
  if (!latest) return null

  const battery = latest.battery
  const transmission = latest.transmission
  const bme = latest.sensors?.bme280
  const dust = latest.sensors?.dust
  const microphone = latest.sensors?.microphone
  const location = latest.device?.location

  let icon, title, status, statusClass, body

  switch (kind) {
    case 'gps': {
      if (!location) return null
      const hasFix = location.latitude !== 0 || location.longitude !== 0
      icon = '📍'
      title = 'GPS'
      status = hasFix ? 'fix' : 'pas de fix'
      statusClass = hasFix ? 'ok' : 'off'
      body = hasFix ? (
        <>
          <Row label="Latitude" value={`${location.latitude.toFixed(4)}°`} />
          <Row label="Longitude" value={`${location.longitude.toFixed(4)}°`} />
        </>
      ) : (
        <Row label="Position" value="indisponible" />
      )
      break
    }
    case 'bme280':
      if (!bme) return null
      icon = '🌡️'
      title = 'BMP280'
      status = 'connecté'
      statusClass = 'ok'
      body = (
        <>
          <Row label="Température" value={`${bme.temperature.toFixed(2)} °C`} />
          <Row label="Pression" value={`${bme.pressure.toFixed(2)} hPa`} />
          {bme.humidity != null && (
            <Row label="Humidité" value={`${bme.humidity.toFixed(0)} %`} />
          )}
        </>
      )
      break
    case 'dust':
      if (!dust) return null
      icon = '💨'
      title = 'Poussière'
      status = 'prêt'
      statusClass = 'ok'
      body = (
        <>
          <Row label="Capteur" value={dust.type} />
          <Row
            label="PM2.5"
            value={`${dust.P2.toFixed(1)} µg/m³`}
            valueClass={pmClass(dust.P2)}
          />
          {dust.P1 != null && (
            <Row label="PM10" value={`${dust.P1.toFixed(1)} µg/m³`} />
          )}
        </>
      )
      break
    case 'battery':
      if (!battery || battery.voltage_v <= 0) return null
      icon = '🔋'
      title = 'Batterie'
      status = battery.charging ? 'en charge' : 'sur batterie'
      statusClass = 'ok'
      body = (
        <>
          <Row label="Niveau" value={`${battery.percentage} %`} />
          <Row label="Tension" value={`${battery.voltage_v.toFixed(2)} V`} />
          <div className="battery-bar-wrap">
            <div
              className={`battery-bar ${batteryClass(battery.percentage)}`}
              style={{ width: `${battery.percentage}%` }}
            />
          </div>
        </>
      )
      break
    case 'lora':
      if (!transmission) return null
      icon = '📡'
      title = 'LoRa'
      status = transmission.protocol
      statusClass = 'ok'
      body = (
        <>
          <Row label="RSSI" value={`${transmission.rssi} dBm`} />
          <Row label="SNR" value={`${transmission.snr.toFixed(1)} dB`} />
        </>
      )
      break
    case 'microphone':
      if (!microphone) return null
      icon = '🎤'
      title = 'Microphone'
      status = 'connecté'
      statusClass = 'ok'
      body = (
        <>
          <Row label="Type" value={microphone.type} />
          <Row label="Niveau" value={`${microphone.level} ADC`} />
        </>
      )
      break
    default:
      return null
  }

  return (
    <div className="sensor-tile">
      <div className="sensor-tile-header">
        <span className="sensor-tile-icon">{icon}</span>
        <span className="sensor-tile-title">{title}</span>
        <span className={`sensor-tile-status ${statusClass}`}>{status}</span>
      </div>
      <div className="sensor-tile-body">{body}</div>
      <div className="sensor-tile-footer">
        <span className="sensor-tile-device">{deviceId}</span>
        <span className="sensor-tile-time">
          {new Date(latest.timestamp).toLocaleString('fr-FR')}
        </span>
        </div>
      ) : (
        <div className="no-data">Aucune mesure enregistrée</div>
      )}

      {hasSparkline && (
        <div className="card-sparkline">
          <Sparkline data={sparkline} dataKey={sparklineKey} color={sparklineColor} />
        </div>
      )}

      {dust && (
        <div className="pm-bar-wrap">
          <div
            className={`pm-bar ${pmClass(dust.P2)}`}
            style={{ width: pmBarWidth(dust.P2) }}
          />
        </div>
      )}

      <div className="device-footer">
        {isOffline
          ? <span className="stale-time">Dernier contact : {relativeTime(device.last_seen_at)}</span>
          : `Mis à jour ${relativeTime(device.last_seen_at)}`}
      </div>
    </div>
  )
}

const locationShape = PropTypes.shape({
  latitude: PropTypes.number.isRequired,
  longitude: PropTypes.number.isRequired,
})

const sensorsShape = PropTypes.shape({
  bme280: PropTypes.shape({
    temperature: PropTypes.number,
    pressure: PropTypes.number,
    humidity: PropTypes.number,
  }),
  dust: PropTypes.shape({ P1: PropTypes.number, P2: PropTypes.number, type: PropTypes.string }),
})

DeviceCard.propTypes = {
  device: PropTypes.shape({
    id: PropTypes.string.isRequired,
    firmware_version: PropTypes.string,
    location: locationShape.isRequired,
    last_seen_at: PropTypes.string.isRequired,
    measurement_count: PropTypes.number,
  }).isRequired,
  latest: PropTypes.shape({
    sensors: sensorsShape,
    battery: PropTypes.shape({
      percentage: PropTypes.number,
      voltage_v: PropTypes.number,
      charging: PropTypes.bool,
    }),
  }),
  sparkline: PropTypes.arrayOf(
    PropTypes.shape({ temperature: PropTypes.number, pm25: PropTypes.number }),
  ),
}

DeviceCard.defaultProps = {
  latest: null,
  sparkline: [],
}
