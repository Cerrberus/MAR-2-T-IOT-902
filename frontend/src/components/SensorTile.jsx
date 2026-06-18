import PropTypes from 'prop-types'

function batteryClass(pct) {
  if (pct >= 50) return 'battery-ok'
  if (pct >= 20) return 'battery-mid'
  return 'battery-low'
}

function pmClass(pm25) {
  if (pm25 == null) return ''
  if (pm25 < 12) return 'pm-good'
  if (pm25 < 35) return 'pm-moderate'
  if (pm25 < 55) return 'pm-unhealthy'
  return 'pm-hazardous'
}

function Row({ label, value, valueClass = '' }) {
  return (
    <div className="sensor-row">
      <span className="sensor-row-label">{label}</span>
      <span className={`sensor-row-value ${valueClass}`}>{value}</span>
    </div>
  )
}

Row.propTypes = {
  label: PropTypes.string.isRequired,
  value: PropTypes.oneOfType([PropTypes.string, PropTypes.number]).isRequired,
  valueClass: PropTypes.string,
}

// ── Per-kind config builders ─────────────────────────────────────────────────

function buildGps({ device }) {
  const location = device?.location
  if (!location) return null
  const hasFix = location.latitude !== 0 || location.longitude !== 0
  return {
    icon: '📍',
    title: 'GPS',
    status: hasFix ? 'fix' : 'pas de fix',
    statusClass: hasFix ? 'ok' : 'off',
    body: hasFix ? (
      <>
        <Row label="Latitude" value={`${location.latitude.toFixed(4)}°`} />
        <Row label="Longitude" value={`${location.longitude.toFixed(4)}°`} />
      </>
    ) : (
      <Row label="Position" value="indisponible" />
    ),
  }
}

function buildBme280({ sensors }) {
  const bme = sensors?.bme280
  if (!bme) return null
  return {
    icon: '🌡️',
    title: 'BMP280',
    status: 'connecté',
    statusClass: 'ok',
    body: (
      <>
        <Row label="Température" value={`${bme.temperature.toFixed(2)} °C`} />
        <Row label="Pression" value={`${bme.pressure.toFixed(2)} hPa`} />
        {bme.humidity != null && (
          <Row label="Humidité" value={`${bme.humidity.toFixed(0)} %`} />
        )}
      </>
    ),
  }
}

function buildDust({ sensors }) {
  const dust = sensors?.dust
  if (!dust) return null
  return {
    icon: '💨',
    title: 'Poussière',
    status: 'prêt',
    statusClass: 'ok',
    body: (
      <>
        <Row label="Capteur" value={dust.type} />
        <Row label="PM2.5" value={`${dust.P2.toFixed(1)} µg/m³`} valueClass={pmClass(dust.P2)} />
        {dust.P1 != null && <Row label="PM10" value={`${dust.P1.toFixed(1)} µg/m³`} />}
      </>
    ),
  }
}

function buildBattery({ battery }) {
  if (!battery || battery.voltage_v <= 0) return null
  return {
    icon: '🔋',
    title: 'Batterie',
    status: battery.charging ? 'en charge' : 'sur batterie',
    statusClass: 'ok',
    body: (
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
    ),
  }
}

function buildLora({ transmission }) {
  if (!transmission) return null
  return {
    icon: '📡',
    title: 'LoRa',
    status: transmission.protocol,
    statusClass: 'ok',
    body: (
      <>
        <Row label="RSSI" value={`${transmission.rssi} dBm`} />
        <Row label="SNR" value={`${transmission.snr.toFixed(1)} dB`} />
      </>
    ),
  }
}

function buildMicrophone({ sensors }) {
  const mic = sensors?.microphone
  if (!mic) return null
  return {
    icon: '🎤',
    title: 'Microphone',
    status: 'connecté',
    statusClass: 'ok',
    body: (
      <>
        <Row label="Type" value={mic.type} />
        <Row label="Niveau" value={`${mic.level} ADC`} />
      </>
    ),
  }
}

const KIND_BUILDERS = {
  gps: buildGps,
  bme280: buildBme280,
  dust: buildDust,
  battery: buildBattery,
  lora: buildLora,
  microphone: buildMicrophone,
}

// ────────────────────────────────────────────────────────────────────────────

export default function SensorTile({ deviceId, kind, latest = null }) {
  if (!latest) return null

  const builder = KIND_BUILDERS[kind]
  if (!builder) return null

  const config = builder(latest)
  if (!config) return null

  const { icon, title, status, statusClass, body } = config

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
    </div>
  )
}

SensorTile.propTypes = {
  deviceId: PropTypes.string.isRequired,
  kind: PropTypes.oneOf(['gps', 'bme280', 'dust', 'battery', 'lora', 'microphone']).isRequired,
  latest: PropTypes.object,
}
