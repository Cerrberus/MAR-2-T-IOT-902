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

export default function DeviceCard({ device, latest }) {
  const bme = latest?.sensors?.bme280
  const dust = latest?.sensors?.dust
  const battery = latest?.battery

  return (
    <div className="device-card">
      <div className="device-card-header">
        <span className="device-id">{device.id}</span>
        {battery != null && (
          <span className={`badge ${batteryClass(battery.percentage)}`}>
            🔋 {battery.percentage}%
          </span>
        )}
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
                <span className="val-label">PM2.5</span>
                <span className="val-num">{dust.P2.toFixed(1)} µg/m³</span>
              </div>
              <div className="val">
                <span className="val-label">PM10</span>
                <span className="val-num">{dust.P1.toFixed(1)} µg/m³</span>
              </div>
            </>
          )}
        </div>
      ) : (
        <div className="no-data">Aucune mesure disponible</div>
      )}

      <div className="device-footer">
        Vu le {new Date(device.last_seen_at).toLocaleString('fr-FR')}
      </div>
    </div>
  )
}
