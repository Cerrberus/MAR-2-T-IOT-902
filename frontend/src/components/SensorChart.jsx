import {
  ResponsiveContainer,
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Area,
  AreaChart,
} from 'recharts'

const TOOLTIP_STYLE = {
  background: '#0b0f1e',
  border: '1px solid rgba(255,255,255,0.08)',
  borderRadius: '10px',
  color: '#eaf0fb',
  fontSize: '13px',
  boxShadow: '0 8px 32px rgba(0,0,0,0.5)',
}

export default function SensorChart({ data, dataKey, label, color = '#00cfff', unit = '' }) {
  if (!data || data.length === 0) {
    return (
      <div className="chart-card">
        <h3 className="chart-title">{label}</h3>
        <div className="chart-empty">Pas de données</div>
      </div>
    )
  }

  const formatted = data
    .filter((d) => d[dataKey] != null)
    .map((d) => ({
      time: new Date(d.timestamp).toLocaleTimeString('fr-FR', {
        hour: '2-digit',
        minute: '2-digit',
      }),
      value: +Number(d[dataKey]).toFixed(2),
    }))

  if (formatted.length === 0) return null

  const vals = formatted.map((d) => d.value)
  const min = Math.min(...vals)
  const max = Math.max(...vals)
  const avg = vals.reduce((s, v) => s + v, 0) / vals.length

  const gradId = `grad-${dataKey}`

  return (
    <div className="chart-card">
      <div className="chart-header">
        <h3 className="chart-title">{label}</h3>
        <div className="chart-stats">
          <span>min <strong style={{ color }}>{min.toFixed(1)}</strong></span>
          <span>moy <strong style={{ color }}>{avg.toFixed(1)}</strong></span>
          <span>max <strong style={{ color }}>{max.toFixed(1)}</strong></span>
        </div>
      </div>
      <ResponsiveContainer width="100%" height={200}>
        <AreaChart data={formatted} margin={{ top: 8, right: 8, bottom: 0, left: -10 }}>
          <defs>
            <linearGradient id={gradId} x1="0" y1="0" x2="0" y2="1">
              <stop offset="5%" stopColor={color} stopOpacity={0.2} />
              <stop offset="95%" stopColor={color} stopOpacity={0} />
            </linearGradient>
          </defs>
          <CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,0.04)" vertical={false} />
          <XAxis
            dataKey="time"
            tick={{ fill: '#5a6880', fontSize: 10 }}
            tickLine={false}
            axisLine={false}
            interval={Math.max(1, Math.floor(formatted.length / 6))}
          />
          <YAxis
            tick={{ fill: '#5a6880', fontSize: 10 }}
            tickLine={false}
            axisLine={false}
            width={36}
            tickFormatter={(v) => v.toFixed(0)}
          />
          <Tooltip
            contentStyle={TOOLTIP_STYLE}
            formatter={(val) => [`${Number(val).toFixed(2)} ${unit}`, label]}
            labelStyle={{ color: '#5a6880', fontSize: 11 }}
            cursor={{ stroke: 'rgba(255,255,255,0.08)', strokeWidth: 1 }}
          />
          <Area
            type="monotone"
            dataKey="value"
            stroke={color}
            strokeWidth={2}
            fill={`url(#${gradId})`}
            dot={false}
            activeDot={{ r: 5, fill: color, stroke: '#0b0f1e', strokeWidth: 2 }}
          />
        </AreaChart>
      </ResponsiveContainer>
    </div>
  )
}
