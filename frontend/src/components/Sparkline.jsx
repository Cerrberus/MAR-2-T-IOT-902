import { useId } from 'react'
import PropTypes from 'prop-types'
import { ResponsiveContainer, AreaChart, Area } from 'recharts'

export default function Sparkline({ data, dataKey, color }) {
  const uid = useId()
  const gradId = `sg-${uid}-${dataKey}`

  const points = data
    .filter((d) => d[dataKey] != null)
    .map((d) => ({ v: +d[dataKey] }))

  if (points.length < 2) return null

  return (
    <ResponsiveContainer width="100%" height={44}>
      <AreaChart data={points} margin={{ top: 2, right: 0, bottom: 0, left: 0 }}>
        <defs>
          <linearGradient id={gradId} x1="0" y1="0" x2="0" y2="1">
            <stop offset="5%" stopColor={color} stopOpacity={0.18} />
            <stop offset="95%" stopColor={color} stopOpacity={0} />
          </linearGradient>
        </defs>
        <Area
          type="monotone"
          dataKey="v"
          stroke={color}
          strokeWidth={1.5}
          fill={`url(#${gradId})`}
          dot={false}
          isAnimationActive={false}
        />
      </AreaChart>
    </ResponsiveContainer>
  )
}

Sparkline.propTypes = {
  data: PropTypes.arrayOf(PropTypes.object).isRequired,
  dataKey: PropTypes.string.isRequired,
  color: PropTypes.string,
}

Sparkline.defaultProps = {
  color: '#00cfff',
}
