import PropTypes from 'prop-types'
import { MapContainer, TileLayer, Marker, Popup } from 'react-leaflet'
import L from 'leaflet'
import { Link } from 'react-router-dom'
import 'leaflet/dist/leaflet.css'
import { relativeTime } from '../utils'

function pmColor(pm25) {
  if (pm25 < 12) return '#00e676'
  if (pm25 < 35) return '#ffc107'
  if (pm25 < 55) return '#ff6d00'
  return '#ff1744'
}

function createOnlineIcon(color) {
  return L.divIcon({
    className: '',
    html: `<div class="pulse-dot" style="background:${color};box-shadow:0 0 12px ${color},0 0 32px ${color}66;"></div>`,
    iconSize: [22, 22],
    iconAnchor: [11, 11],
    popupAnchor: [0, -14],
  })
}

const OFFLINE_ICON = L.divIcon({
  className: '',
  html: '<div class="offline-dot"></div>',
  iconSize: [18, 18],
  iconAnchor: [9, 9],
  popupAnchor: [0, -12],
})

export default function DeviceMap({ entries }) {
  if (!entries || entries.length === 0) return null

  const lats = entries.map((e) => e.device.location.latitude)
  const lngs = entries.map((e) => e.device.location.longitude)
  const center = [
    lats.reduce((a, b) => a + b, 0) / lats.length,
    lngs.reduce((a, b) => a + b, 0) / lngs.length,
  ]

  const onlineCount = entries.filter((e) => e.latest).length
  const offlineCount = entries.length - onlineCount

  return (
    <div className="map-wrapper">
      <MapContainer center={center} zoom={13} className="leaflet-map" zoomControl>
        <TileLayer
          attribution='&copy; <a href="https://carto.com/">CARTO</a>'
          url="https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png"
        />
        {entries.map(({ device, latest }) => {
          const pm25 = latest?.sensors?.dust?.P2
          const temp = latest?.sensors?.bme280?.temperature
          const bat = latest?.battery?.percentage
          const isOnline = latest != null
          const markerColor = pm25 == null ? '#00cfff' : pmColor(pm25)
          const icon = isOnline ? createOnlineIcon(markerColor) : OFFLINE_ICON

          return (
            <Marker
              key={device.id}
              position={[device.location.latitude, device.location.longitude]}
              icon={icon}
            >
              <Popup>
                <div className="map-popup">
                  <div className="map-popup-id">
                    {device.id}
                    {!isOnline && <span className="map-popup-offline-tag">hors ligne</span>}
                  </div>
                  {isOnline && (
                    <>
                      {temp != null && <div>🌡 {temp.toFixed(1)} °C</div>}
                      {pm25 != null && <div>💨 PM2.5 : {pm25.toFixed(1)} µg/m³</div>}
                      {bat != null && <div>🔋 {bat} %</div>}
                    </>
                  )}
                  {!isOnline && (
                    <div className="map-popup-nodata">
                      Aucune mesure disponible
                      <br />
                      <span className="map-popup-lastseen">
                        Vu {relativeTime(device.last_seen_at)}
                      </span>
                    </div>
                  )}
                  <Link
                    to={`/devices/${encodeURIComponent(device.id)}`}
                    className="map-popup-link"
                  >
                    Voir le détail →
                  </Link>
                </div>
              </Popup>
            </Marker>
          )
        })}
      </MapContainer>
      <div className="map-legend">
        {onlineCount > 0 && (
          <span className="legend-item legend-online">
            <span className="legend-dot" style={{ background: '#00e676' }} />
            {onlineCount} actif{onlineCount > 1 ? 's' : ''}
          </span>
        )}
        {offlineCount > 0 && (
          <span className="legend-item legend-offline">
            <span className="legend-dot legend-dot-offline" />
            {offlineCount} hors ligne
          </span>
        )}
      </div>
    </div>
  )
}

const entryShape = PropTypes.shape({
  device: PropTypes.shape({
    id: PropTypes.string.isRequired,
    last_seen_at: PropTypes.string.isRequired,
    location: PropTypes.shape({
      latitude: PropTypes.number.isRequired,
      longitude: PropTypes.number.isRequired,
    }).isRequired,
  }).isRequired,
  latest: PropTypes.shape({
    sensors: PropTypes.shape({
      dust: PropTypes.shape({ P2: PropTypes.number }),
      bme280: PropTypes.shape({ temperature: PropTypes.number }),
    }),
    battery: PropTypes.shape({ percentage: PropTypes.number }),
  }),
})

DeviceMap.propTypes = {
  entries: PropTypes.arrayOf(entryShape).isRequired,
}
