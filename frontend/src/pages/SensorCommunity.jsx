import { useState, useEffect, useMemo } from 'react'
import { Link } from 'react-router-dom'
import { getDevices, getDeviceLatest, updateDevice, pushToSensorCommunity } from '../api/client'

const PIN_DUST = 1
const PIN_BME280 = 11
const SOFTWARE_VERSION = 'sensor-sensei/1.0'

function buildPayloads(latest, pins) {
  const out = []
  if (pins.dust && latest?.sensors?.dust) {
    const d = latest.sensors.dust
    out.push({
      pin: PIN_DUST,
      label: 'Particules (P1, P2)',
      body: {
        software_version: SOFTWARE_VERSION,
        sensordatavalues: [
          { value_type: 'P1', value: String(d.P1) },
          { value_type: 'P2', value: String(d.P2) },
        ],
      },
    })
  }
  if (pins.bme280 && latest?.sensors?.bme280) {
    const b = latest.sensors.bme280
    const values = [
      { value_type: 'temperature', value: String(b.temperature) },
      { value_type: 'pressure', value: String(b.pressure) },
    ]
    if (b.humidity != null) {
      values.push({ value_type: 'humidity', value: String(b.humidity) })
    }
    out.push({
      pin: PIN_BME280,
      label: 'BME280 (T°, P, H)',
      body: { software_version: SOFTWARE_VERSION, sensordatavalues: values },
    })
  }
  return out
}

function DevicePreview({ entry, pins, selected, onToggleSelect, onSaveCommunityId }) {
  const { device, latest } = entry
  const [editing, setEditing] = useState(false)
  const [idInput, setIdInput] = useState(device.sensor_community_id ?? '')
  const [saving, setSaving] = useState(false)
  const [saveError, setSaveError] = useState(null)

  const payloads = useMemo(() => buildPayloads(latest, pins), [latest, pins])
  const hasCommunityId = device.sensor_community_id != null
  const hasData = payloads.length > 0
  const canSelect = hasCommunityId && hasData

  async function handleSave() {
    setSaving(true)
    setSaveError(null)
    try {
      const parsed = idInput === '' ? null : Number(idInput)
      if (parsed !== null && (!Number.isInteger(parsed) || parsed <= 0)) {
        throw new Error('ID doit être un entier positif')
      }
      await onSaveCommunityId(device.id, parsed)
      setEditing(false)
    } catch (err) {
      setSaveError(err.message)
    } finally {
      setSaving(false)
    }
  }

  return (
    <article className={`sc-device ${selected ? 'is-selected' : ''} ${!canSelect ? 'is-disabled' : ''}`}>
      <header className="sc-device-header">
        <label className="sc-checkbox">
          <input
            type="checkbox"
            checked={selected}
            disabled={!canSelect}
            onChange={() => onToggleSelect(device.id)}
          />
          <span />
        </label>
        <div className="sc-device-id">
          <Link to={`/devices/${encodeURIComponent(device.id)}`}>{device.id}</Link>
          <span className="sc-device-fw">fw {device.firmware_version}</span>
        </div>
        <div className="sc-device-status">
          {hasCommunityId ? (
            <span className="sc-pill ok">ID #{device.sensor_community_id}</span>
          ) : (
            <span className="sc-pill warn">ID manquant</span>
          )}
        </div>
      </header>

      <div className="sc-device-meta">
        <span>📍 {device.location.latitude.toFixed(4)}, {device.location.longitude.toFixed(4)}</span>
        <span>📊 {device.measurement_count} mesures</span>
        {latest?.timestamp && (
          <span>🕒 {new Date(latest.timestamp).toLocaleString('fr-FR')}</span>
        )}
      </div>

      <div className="sc-id-row">
        {editing ? (
          <>
            <input
              className="sc-id-input"
              type="number"
              min="1"
              value={idInput}
              onChange={(e) => setIdInput(e.target.value)}
              placeholder="ID sensor.community"
            />
            <button className="sc-btn" onClick={handleSave} disabled={saving}>
              {saving ? '…' : 'Enregistrer'}
            </button>
            <button
              className="sc-btn ghost"
              onClick={() => {
                setEditing(false)
                setIdInput(device.sensor_community_id ?? '')
                setSaveError(null)
              }}
            >
              Annuler
            </button>
            {saveError && <span className="sc-err">{saveError}</span>}
          </>
        ) : (
          <button className="sc-btn ghost" onClick={() => setEditing(true)}>
            {hasCommunityId ? 'Modifier l\'ID' : 'Configurer l\'ID sensor.community'}
          </button>
        )}
      </div>

      {payloads.length === 0 ? (
        <p className="sc-no-data">
          {pins.dust || pins.bme280
            ? 'Aucune donnée disponible pour les pins sélectionnés.'
            : 'Sélectionnez au moins un pin pour voir l\'aperçu.'}
        </p>
      ) : (
        <div className="sc-payload-list">
          {payloads.map((p) => (
            <details key={p.pin} className="sc-payload">
              <summary>
                <span className="sc-pin">Pin {p.pin}</span>
                <span className="sc-pin-label">{p.label}</span>
                <span className="sc-pin-count">
                  {p.body.sensordatavalues.length} valeur{p.body.sensordatavalues.length > 1 ? 's' : ''}
                </span>
              </summary>
              <div className="sc-payload-values">
                {p.body.sensordatavalues.map((v) => (
                  <div className="sc-payload-row" key={v.value_type}>
                    <span className="sc-payload-key">{v.value_type}</span>
                    <span className="sc-payload-val">{v.value}</span>
                  </div>
                ))}
              </div>
              <pre className="sc-json">{JSON.stringify(p.body, null, 2)}</pre>
            </details>
          ))}
        </div>
      )}
    </article>
  )
}

export default function SensorCommunity() {
  const [entries, setEntries] = useState([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState(null)
  const [pins, setPins] = useState({ dust: true, bme280: true })
  const [selectedIds, setSelectedIds] = useState(new Set())
  const [pushState, setPushState] = useState({ status: 'idle', message: null })

  async function loadEntries() {
    const deviceList = await getDevices()
    const latestList = await Promise.all(
      deviceList.items.map((d) => getDeviceLatest(d.id).catch(() => null)),
    )
    setEntries(deviceList.items.map((d, i) => ({ device: d, latest: latestList[i] })))
  }

  useEffect(() => {
    loadEntries()
      .catch((err) => setError(err.message))
      .finally(() => setLoading(false))
  }, [])

  function toggleSelect(id) {
    setSelectedIds((prev) => {
      const next = new Set(prev)
      if (next.has(id)) next.delete(id)
      else next.add(id)
      return next
    })
  }

  async function handleSaveCommunityId(id, value) {
    await updateDevice(id, { sensor_community_id: value })
    await loadEntries()
  }

  const eligible = entries.filter(
    (e) => e.device.sensor_community_id != null && e.latest != null,
  )

  function selectAllEligible() {
    setSelectedIds(new Set(eligible.map((e) => e.device.id)))
  }

  function clearSelection() {
    setSelectedIds(new Set())
  }

  async function handlePush() {
    setPushState({ status: 'pending', message: null })
    try {
      const enabledPins = []
      if (pins.dust) enabledPins.push(PIN_DUST)
      if (pins.bme280) enabledPins.push(PIN_BME280)
      await pushToSensorCommunity({
        device_ids: Array.from(selectedIds),
        pins: enabledPins,
      })
      setPushState({ status: 'ok', message: 'Envoyé.' })
    } catch (err) {
      setPushState({ status: 'error', message: err.message })
    }
  }

  if (loading) return <div className="state-msg"><span className="loader" />Chargement…</div>
  if (error) return <div className="state-msg error">⚠ Erreur : {error}</div>

  const totalSelected = selectedIds.size
  const pinsCount = (pins.dust ? 1 : 0) + (pins.bme280 ? 1 : 0)
  const callsCount = totalSelected * pinsCount

  return (
    <div className="container">
      <h1 className="page-title gradient-text">Sensor.Community</h1>
      <p className="sc-intro">
        Prévisualisez les données qui seraient transmises à sensor.community et
        choisissez ce qui doit être envoyé. Le forward automatique est piloté
        côté backend par la variable <code>SENSOR_COMMUNITY_ENABLED</code>.
      </p>

      <div className="sc-info-banner">
        <span className="sc-info-icon">ℹ️</span>
        <div>
          <strong>Aujourd'hui</strong> — chaque mesure ingérée est automatiquement
          forwardée (Pin 1 + Pin 11) si <code>SENSOR_COMMUNITY_ENABLED=true</code> et
          si le device a un <code>sensor_community_id</code>. Cette page sert à
          inspecter / configurer / pousser manuellement (endpoint dédié à venir).
        </div>
      </div>

      <h2 className="section-title">Options d'envoi</h2>
      <div className="sc-options">
        <label className="sc-option">
          <input
            type="checkbox"
            checked={pins.dust}
            onChange={() => setPins((p) => ({ ...p, dust: !p.dust }))}
          />
          <span className="sc-option-label">
            <strong>Pin 1</strong> — Particules (P1 / PM10, P2 / PM2.5)
          </span>
        </label>
        <label className="sc-option">
          <input
            type="checkbox"
            checked={pins.bme280}
            onChange={() => setPins((p) => ({ ...p, bme280: !p.bme280 }))}
          />
          <span className="sc-option-label">
            <strong>Pin 11</strong> — BME280 (température, pression, humidité)
          </span>
        </label>
      </div>

      <div className="sc-action-bar">
        <div className="sc-action-stats">
          <span><strong>{totalSelected}</strong> device{totalSelected > 1 ? 's' : ''} sélectionné{totalSelected > 1 ? 's' : ''}</span>
          <span className="sc-sep">·</span>
          <span><strong>{callsCount}</strong> requête{callsCount > 1 ? 's' : ''} POST</span>
          {eligible.length > 0 && (
            <>
              <span className="sc-sep">·</span>
              <button className="sc-link" onClick={selectAllEligible}>
                Tout sélectionner ({eligible.length})
              </button>
              {totalSelected > 0 && (
                <>
                  <span className="sc-sep">·</span>
                  <button className="sc-link" onClick={clearSelection}>
                    Vider
                  </button>
                </>
              )}
            </>
          )}
        </div>
        <button
          className="sc-btn primary"
          onClick={handlePush}
          disabled={totalSelected === 0 || pinsCount === 0 || pushState.status === 'pending'}
          title="L'endpoint backend POST /api/v1/sensor-community/push n'est pas encore implémenté."
        >
          {pushState.status === 'pending' ? 'Envoi…' : 'Envoyer la sélection'}
        </button>
      </div>

      {pushState.status === 'error' && (
        <div className="sc-result error">
          ⚠ {pushState.message}
          <div className="sc-result-hint">
            Note : l'endpoint <code>POST /api/v1/sensor-community/push</code> est
            un stub côté front. Il faut l'implémenter côté FastAPI pour que ce
            bouton fonctionne (voir <code>app/workers/sensor_community.py</code>).
          </div>
        </div>
      )}
      {pushState.status === 'ok' && (
        <div className="sc-result ok">✓ {pushState.message}</div>
      )}

      <h2 className="section-title">Devices ({entries.length})</h2>
      {entries.length === 0 ? (
        <p className="empty-state">Aucun device enregistré.</p>
      ) : (
        <div className="sc-device-list">
          {entries.map((entry) => (
            <DevicePreview
              key={entry.device.id}
              entry={entry}
              pins={pins}
              selected={selectedIds.has(entry.device.id)}
              onToggleSelect={toggleSelect}
              onSaveCommunityId={handleSaveCommunityId}
            />
          ))}
        </div>
      )}
    </div>
  )
}
