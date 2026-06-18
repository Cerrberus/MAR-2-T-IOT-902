export function relativeTime(dateStr) {
  const diff = Date.now() - new Date(dateStr).getTime()
  const s = Math.floor(diff / 1000)
  if (s < 60) return 'à l\'instant'
  const m = Math.floor(s / 60)
  if (m < 60) return `il y a ${m} min`
  const h = Math.floor(m / 60)
  if (h < 24) return `il y a ${h} h`
  return `il y a ${Math.floor(h / 24)} j`
}

export function freshnessClass(dateStr) {
  const mins = (Date.now() - new Date(dateStr).getTime()) / 60000
  if (mins < 2) return 'fresh-live'
  if (mins < 10) return 'fresh-recent'
  if (mins < 60) return 'fresh-stale'
  return 'fresh-old'
}
