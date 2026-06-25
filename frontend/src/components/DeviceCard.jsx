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

function dustPct(value, max) {
  return Math.min(100, Math.round((value / max) * 100))
}

export default function DeviceCard({ device, latest = null, sparkline = [] }) {
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

      <div className="device-meta">
        <span>FW {device.firmware_version}</span>
        <span>
          {device.location.latitude.toFixed(4)}°, {device.location.longitude.toFixed(4)}°
        </span>
      </div>

      {latest ? (
        <div className="device-values">
          {bme && (
            <>
              <div className="val">
                <span className="val-label">Temp.</span>
                <span className="val-num">{bme.temperature.toFixed(1)} °C</span>
              </div>
              <div className="val">
                <span className="val-label">Pression</span>
                <span className="val-num">{bme.pressure.toFixed(0)} hPa</span>
              </div>
              {bme.humidity != null && (
                <div className="val">
                  <span className="val-label">Humidité</span>
                  <span className="val-num">{bme.humidity.toFixed(0)} %</span>
                </div>
              )}
            </>
          )}
          {dust && (
            <>
              <div className={`val ${pmClass(dust.P2)}`}>
                <span className="val-label">Poussière fine</span>
                <span className="val-num">{dustPct(dust.P2, 150)} %</span>
              </div>
              {dust.P1 != null && (
                <div className="val">
                  <span className="val-label">Poussière grossière</span>
                  <span className="val-num">{dustPct(dust.P1, 300)} %</span>
                </div>
              )}
            </>
          )}
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
            style={{ width: `${dustPct(dust.P2, 150)}%` }}
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

