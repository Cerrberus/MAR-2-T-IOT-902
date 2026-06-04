import { MapContainer, TileLayer, Marker, Popup } from 'react-leaflet'
import L from 'leaflet'
import { Link } from 'react-router-dom'
import 'leaflet/dist/leaflet.css'

function pmColor(pm25) {
  if (pm25 == null) return '#5a6880'
  if (pm25 < 12) return '#00e676'
  if (pm25 < 35) return '#ffc107'
  if (pm25 < 55) return '#ff6d00'
  return '#ff1744'
}

function createPulseIcon(color) {
  return L.divIcon({
    className: '',
    html: `<div class="pulse-dot" style="background:${color};box-shadow:0 0 12px ${color},0 0 32px ${color}66;"></div>`,
    iconSize: [22, 22],
    iconAnchor: [11, 11],
    popupAnchor: [0, -14],
  })
}

export default function DeviceMap({ entries }) {
  const located = (entries ?? []).filter((e) => {
    const loc = e.latest?.device?.location ?? e.device.location
    return loc && (loc.latitude !== 0 || loc.longitude !== 0)
  })
  if (located.length === 0) return null

  const lats = located.map((e) => (e.latest?.device?.location ?? e.device.location).latitude)
  const lngs = located.map((e) => (e.latest?.device?.location ?? e.device.location).longitude)
  const center = [
    lats.reduce((a, b) => a + b, 0) / lats.length,
    lngs.reduce((a, b) => a + b, 0) / lngs.length,
  ]

  return (
    <div className="map-wrapper">
      <MapContainer center={center} zoom={13} className="leaflet-map" zoomControl>
        <TileLayer
          attribution='&copy; <a href="https://carto.com/">CARTO</a>'
          url="https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png"
        />
        {located.map(({ device, latest }) => {
          const loc = latest?.device?.location ?? device.location
          const pm25 = latest?.sensors?.dust?.P2
          const temp = latest?.sensors?.bme280?.temperature
          const bat = latest?.battery?.percentage
          return (
            <Marker
              key={device.id}
              position={[loc.latitude, loc.longitude]}
              icon={createPulseIcon(pmColor(pm25))}
            >
              <Popup>
                <div className="map-popup">
                  <div className="map-popup-id">{latest?.device?.id ?? device.id}</div>
                  {temp != null && <div>🌡️ {temp.toFixed(1)} °C</div>}
                  {pm25 != null && <div>💨 PM2.5 : {pm25.toFixed(1)} µg/m³</div>}
                  {bat != null && <div>🔋 {bat} %</div>}
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
    </div>
  )
}
