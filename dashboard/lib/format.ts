export function formatUptime(ms: number): string {
  const s = Math.floor(ms / 1000)
  const d = Math.floor(s / 86400)
  const h = Math.floor((s % 86400) / 3600)
  const m = Math.floor((s % 3600) / 60)
  if (d > 0) return `${d}d ${h}h`
  if (h > 0) return `${h}h ${m}m`
  return `${m}m ${s % 60}s`
}

export function formatDuration(ms: number): string {
  if (!Number.isFinite(ms)) return ''
  if (ms < 1) return `${(ms * 1000).toFixed(0)}µs`
  if (ms < 1000) return `${ms.toFixed(1)}ms`
  return `${(ms / 1000).toFixed(2)}s`
}

export function relativeTime(ts: number): string {
  if (!ts) return ''
  const diff = Date.now() - ts
  if (diff < 5_000) return 'just now'
  if (diff < 60_000) return `${Math.floor(diff / 1000)} sec ago`
  if (diff < 3_600_000) return `${Math.floor(diff / 60_000)} min ago`
  return `${Math.floor(diff / 3_600_000)}h ago`
}

export function fnColor(name: string): string {
  const colors = ['lime', 'cyan', 'amber', 'purple']
  let h = 0
  for (let i = 0; i < name.length; i++) h = (h + name.charCodeAt(i) * (i + 1)) % colors.length
  return colors[h]
}

export function throughputPath(
  points: { invocationsPerSecond: number; errorsPerSecond: number }[],
  width = 900,
  height = 190,
): { inv: string; err: string; maxY: number } {
  if (points.length === 0) {
    return { inv: '', err: '', maxY: 1 }
  }
  const maxY = Math.max(
    1,
    ...points.map((p) => Math.max(p.invocationsPerSecond, p.errorsPerSecond)),
  )
  const step = points.length > 1 ? width / (points.length - 1) : width
  const toY = (v: number) => height - (v / maxY) * (height - 10) - 5
  const line = (key: 'invocationsPerSecond' | 'errorsPerSecond') =>
    points
      .map((p, i) => `${i === 0 ? 'M' : 'L'}${i * step} ${toY(p[key])}`)
      .join(' ')
  const invLine = line('invocationsPerSecond')
  const area = `${invLine} L${(points.length - 1) * step} ${height} L0 ${height} Z`
  return { inv: area + '|' + invLine, err: line('errorsPerSecond'), maxY }
}
