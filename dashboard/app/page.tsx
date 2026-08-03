'use client'

import { useQuery, useQueryClient } from '@tanstack/react-query'
import {
  Activity,
  AlertTriangle,
  BarChart3,
  Box,
  Check,
  CircleHelp,
  Clock3,
  Copy,
  Cpu,
  Database,
  Gauge,
  Layers3,
  LayoutDashboard,
  Menu,
  Network,
  Play,
  Plus,
  RefreshCw,
  Search,
  Server,
  Settings,
  Terminal,
  Users,
  Wifi,
  WifiOff,
  X,
} from 'lucide-react'
import { useEffect, useMemo, useState } from 'react'
import { Area, CartesianGrid, Line, LineChart, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts'

import { api, getBaseUrl, setBaseUrl } from '@/lib/api'
import { fnColor, formatDuration, formatUptime, relativeTime } from '@/lib/format'
import type { FunctionRecord, ThroughputPoint } from '@/lib/types'
import { POLL_INTERVAL_STORAGE_KEY } from '@/lib/types'

const nav = [
  { label: 'Overview', icon: LayoutDashboard },
  { label: 'Functions', icon: Box },
  { label: 'Invocations', icon: Activity },
  { label: 'Workers', icon: Cpu },
  { label: 'Metrics', icon: BarChart3 },
  { label: 'Benchmarks', icon: Gauge },
]

function Badge({ children, tone = 'green' }: { children: React.ReactNode; tone?: string }) {
  return (
    <span className={`badge ${tone}`}>
      <i />
      {children}
    </span>
  )
}

function Kpi({
  label,
  value,
  hint,
  icon: Icon,
  tone = 'lime',
  loading,
}: {
  label: string
  value: string
  hint?: string
  icon: React.ComponentType<{ size?: number }>
  tone?: string
  loading?: boolean
}) {
  return (
    <div className="kpi card" aria-live="polite">
      <div className="kpi-top">
        <span>{label}</span>
        <Icon size={16} />
      </div>
      <div className="kpi-value">{loading ? '' : value}</div>
      <div className="kpi-bottom">
        <span className={`delta ${tone}`}>{hint ?? 'live'}</span>
        <span className="period">polled</span>
      </div>
    </div>
  )
}

function Empty({ title, description }: { title: string; description: string }) {
  return (
    <div className="empty-state">
      <strong>{title}</strong>
      <span>{description}</span>
    </div>
  )
}

function ErrorBanner({ message, onRetry }: { message: string; onRetry?: () => void }) {
  return (
    <div className="error-banner">
      <AlertTriangle size={14} />
      <span>{message}</span>
      {onRetry && (
        <button type="button" className="button secondary" onClick={onRetry}>
          Retry
        </button>
      )}
    </div>
  )
}

function usePollMs() {
  const [ms, setMs] = useState(2000)
  useEffect(() => {
    const stored = localStorage.getItem(POLL_INTERVAL_STORAGE_KEY)
    if (stored) setMs(Number(stored) || 2000)
  }, [])
  return ms
}

export default function Page() {
  const [active, setActive] = useState('Overview')
  const [collapsed, setCollapsed] = useState(false)
  const [live, setLive] = useState(true)
  const [query, setQuery] = useState('')
  const [throughput, setThroughput] = useState<ThroughputPoint[]>([])
  const [apiUrl, setApiUrl] = useState('http://localhost:8080')
  const [pollMs, setPollMs] = useState(2000)
  const [monitor, setMonitor] = useState(false)
  const [invokeName, setInvokeName] = useState('')
  const [invokeBody, setInvokeBody] = useState('{"name":"Test"}')
  const [invokeResult, setInvokeResult] = useState('')
  const [regForm, setRegForm] = useState({
    name: '',
    command: '',
    version: '1',
    min_workers: '0',
    max_workers: '4',
    timeout_ms: '5000',
  })
  const [showRegister, setShowRegister] = useState(false)
  const [invStatus, setInvStatus] = useState('All')
  const [invFunction, setInvFunction] = useState('All')
  const queryClient = useQueryClient()
  const defaultPoll = usePollMs()

  useEffect(() => {
    setApiUrl(getBaseUrl())
    const stored = localStorage.getItem(POLL_INTERVAL_STORAGE_KEY)
    if (stored) setPollMs(Number(stored) || 2000)
  }, [])

  const interval = live ? (monitor && active === 'Benchmarks' ? 1000 : pollMs || defaultPoll) : false

  const healthQ = useQuery({
    queryKey: ['health'],
    queryFn: async () => {
      const [h, r] = await Promise.all([
        api.healthz().catch(() => null),
        api.readyz().catch(() => null),
      ])
      return { health: h?.status === 'ok', ready: r?.status === 'ready' }
    },
    refetchInterval: interval === false ? false : 5000,
  })

  const statsQ = useQuery({
    queryKey: ['stats'],
    queryFn: () => api.stats(),
    refetchInterval: interval,
  })

  const functionsQ = useQuery({
    queryKey: ['functions'],
    queryFn: () => api.functions(),
    refetchInterval: interval === false ? false : 10000,
  })

  const invocationsQ = useQuery({
    queryKey: ['invocations', 'recent'],
    queryFn: () => api.invocations({ limit: '10' }),
    refetchInterval: interval === false ? false : 5000,
  })

  const allInvocationsQ = useQuery({
    queryKey: ['invocations', invFunction, invStatus],
    queryFn: () => {
      const params: Record<string, string> = { limit: '50' }
      if (invFunction !== 'All') params.function = invFunction
      if (invStatus !== 'All') params.status = invStatus
      return api.invocations(params)
    },
    refetchInterval: interval === false ? false : 5000,
    enabled: active === 'Invocations' || active === 'Overview',
  })

  const workersQ = useQuery({
    queryKey: ['workers'],
    queryFn: () => api.workers(),
    refetchInterval: interval,
    enabled: active === 'Workers' || active === 'Overview',
  })

  const metricsQ = useQuery({
    queryKey: ['metrics'],
    queryFn: () => api.metrics(),
    refetchInterval: interval === false ? false : 5000,
    enabled: active === 'Metrics' || active === 'Overview',
  })

  const functionNames = functionsQ.data?.functions.map((f) => f.name) ?? []

  const functionStatsQ = useQuery({
    queryKey: ['functionStats', functionNames.join(',')],
    queryFn: async () => {
      const rows = await Promise.all(
        functionNames.map(async (name) => {
          try {
            return await api.functionStats(name)
          } catch {
            return null
          }
        }),
      )
      return rows.filter(Boolean)
    },
    enabled: functionNames.length > 0,
    refetchInterval: interval === false ? false : 5000,
  })

  useEffect(() => {
    if (!statsQ.data || !live) return
    const point: ThroughputPoint = {
      timestamp: Date.now(),
      invocationsPerSecond: statsQ.data.rates.invocations_per_second,
      errorsPerSecond: statsQ.data.rates.errors_per_second,
    }
    setThroughput((prev) => [...prev.slice(-59), point])
  }, [statsQ.dataUpdatedAt, live]) // eslint-disable-line react-hooks/exhaustive-deps

  const filteredFunctions = useMemo(() => {
    const list = functionsQ.data?.functions ?? []
    if (!query) return list
    return list.filter((f) => f.name.toLowerCase().includes(query.toLowerCase()))
  }, [functionsQ.data, query])

  const healthLabel = !healthQ.data?.health
    ? 'Offline'
    : healthQ.data.ready
      ? 'All systems operational'
      : 'Degraded'
  const healthTone = !healthQ.data?.health ? 'fail' : healthQ.data.ready ? '' : 'warn'

  const stats = statsQ.data
  const chartData = throughput.map((p, i) => ({
    i,
    inv: Number(p.invocationsPerSecond.toFixed(3)),
    err: Number(p.errorsPerSecond.toFixed(3)),
    t: relativeTime(p.timestamp),
  }))

  async function refreshAll() {
    await queryClient.invalidateQueries()
  }

  async function handleInvoke() {
    if (!invokeName) return
    try {
      const payload = JSON.parse(invokeBody)
      const result = await api.invoke(invokeName, payload)
      setInvokeResult(JSON.stringify(result, null, 2))
      await refreshAll()
    } catch (e) {
      setInvokeResult(String(e))
    }
  }

  async function handleRegister() {
    try {
      await api.registerFunction({
        name: regForm.name,
        command: regForm.command,
        version: regForm.version,
        min_workers: Number(regForm.min_workers),
        max_workers: Number(regForm.max_workers),
        timeout_ms: Number(regForm.timeout_ms),
      })
      setShowRegister(false)
      setRegForm({ name: '', command: '', version: '1', min_workers: '0', max_workers: '4', timeout_ms: '5000' })
      await refreshAll()
    } catch (e) {
      alert(String(e))
    }
  }

  function saveSettings() {
    setBaseUrl(apiUrl)
    localStorage.setItem(POLL_INTERVAL_STORAGE_KEY, String(pollMs))
    refreshAll()
  }

  return (
    <div className="app-shell">
      <aside className={`sidebar ${collapsed ? 'collapsed' : ''}`}>
        <div className="brand">
          <img src="/icon.png" alt="Hydra" className="brand-logo" width={28} height={28} />
          <span>Hydra</span>
        </div>
        <div className="workspace">
          <div className="workspace-icon">S</div>
          <div className="workspace-copy">
            <strong>serverless-cpp</strong>
            <span>Local control plane</span>
          </div>
        </div>
        <nav>
          <div className="nav-label">MONITORING</div>
          {nav.map(({ label, icon: Icon }) => (
            <button
              key={label}
              type="button"
              onClick={() => setActive(label)}
              className={active === label ? 'active' : ''}
            >
              <Icon size={17} />
              <span>{label}</span>
              {label === 'Invocations' && invocationsQ.data && (
                <b>{invocationsQ.data.total}</b>
              )}
            </button>
          ))}
          <div className="nav-label lower">CONFIGURATION</div>
          <button
            type="button"
            onClick={() => setActive('Settings')}
            className={active === 'Settings' ? 'active' : ''}
          >
            <Settings size={17} />
            <span>Settings</span>
          </button>
        </nav>
        <div className="sidebar-bottom">
          <a className="help" href={apiUrl.replace(/\/$/, '') + '/'} target="_blank" rel="noreferrer">
            <CircleHelp size={16} />
            <span>Documentation</span>
            <span className="external">↗</span>
          </a>
          <div className="user">
            <div className="avatar">{healthQ.data?.ready ? <Wifi size={14} /> : <WifiOff size={14} />}</div>
            <div>
              <strong>API</strong>
              <span className="mono" title={apiUrl}>
                {apiUrl.replace(/^https?:\/\//, '')}
              </span>
            </div>
          </div>
        </div>
      </aside>

      <main className="main">
        <header className="topbar">
          <button type="button" className="icon-button mobile-menu" onClick={() => setCollapsed(!collapsed)}>
            <Menu size={20} />
          </button>
          <div className="crumb">
            <span>Control plane</span>
            <span className="slash">/</span>
            <strong>{active}</strong>
          </div>
          <div className="top-actions">
            <div className={`connection ${healthTone}`}>
              <span className={`pulse ${healthTone}`} />
              {healthLabel}
            </div>
            <button type="button" className="icon-button" onClick={() => refreshAll()} title="Refresh">
              <RefreshCw size={16} className={statsQ.isFetching ? 'spin' : ''} />
            </button>
          </div>
        </header>

        <div className="content">
          {(statsQ.isError || functionsQ.isError) && (
            <ErrorBanner
              message={String(statsQ.error ?? functionsQ.error ?? 'API unreachable')}
              onRetry={refreshAll}
            />
          )}

          <div className="page-head">
            <div>
              <div className="eyebrow">
                <span className="live-dot" /> LIVE {active.toUpperCase()}
              </div>
              <h1>{active}</h1>
              <p>Hydra  real-time observability for your C++ serverless platform.</p>
            </div>
            <div className="head-actions">
              <button type="button" className="button secondary" onClick={() => setLive(!live)}>
                <Clock3 size={15} />
                {live ? 'Live' : 'Paused'}
              </button>
              {active === 'Functions' && (
                <button type="button" className="button primary" onClick={() => setShowRegister(true)}>
                  <Plus size={16} /> Register function
                </button>
              )}
            </div>
          </div>

          {active === 'Overview' && (
            <>
              <section className="kpi-grid">
                <Kpi
                  label="Invocations / sec"
                  value={(stats?.rates.invocations_per_second ?? 0).toFixed(2)}
                  hint={`${stats?.invocations.completed ?? 0} completed`}
                  icon={Activity}
                  loading={statsQ.isLoading}
                />
                <Kpi
                  label="Errors / sec"
                  value={(stats?.rates.errors_per_second ?? 0).toFixed(2)}
                  hint="error rate"
                  icon={AlertTriangle}
                  tone="cyan"
                  loading={statsQ.isLoading}
                />
                <Kpi
                  label="Active workers"
                  value={String(stats?.workers.total ?? 0)}
                  hint={`${stats?.workers.busy ?? 0} busy / ${stats?.workers.idle ?? 0} idle`}
                  icon={Users}
                  loading={statsQ.isLoading}
                />
                <Kpi
                  label="Queue depth"
                  value={String(stats?.queue.total_depth ?? 0)}
                  hint={stats?.queue.total_depth ? 'backpressure' : 'clear'}
                  icon={Layers3}
                  tone={stats?.queue.total_depth ? 'amber' : 'cyan'}
                  loading={statsQ.isLoading}
                />
              </section>

              <section className="chart card">
                <div className="section-head">
                  <div>
                    <h2>Throughput</h2>
                    <p>Last 60 samples from /api/v1/stats</p>
                  </div>
                  <div className="legend">
                    <span>
                      <i className="legend-lime" /> Invocations
                    </span>
                    <span>
                      <i className="legend-muted" /> Errors
                    </span>
                    <button type="button" className="live-toggle" onClick={() => setLive(!live)}>
                      <span className={live ? 'on' : ''} />
                      {live ? 'Live' : 'Paused'}
                    </button>
                  </div>
                </div>
                <div className="chart-recharts">
                  {chartData.length === 0 ? (
                    <Empty title="Waiting for samples" description="Rates appear after invocations complete." />
                  ) : (
                    <ResponsiveContainer width="100%" height={220}>
                      <LineChart data={chartData}>
                        <CartesianGrid stroke="#202b32" strokeDasharray="3 3" />
                        <XAxis dataKey="i" hide />
                        <YAxis stroke="#52606a" fontSize={10} width={36} />
                        <Tooltip
                          contentStyle={{ background: '#151d24', border: '1px solid #243044', fontSize: 11 }}
                        />
                        <Line type="monotone" dataKey="inv" stroke="var(--lime)" strokeWidth={2} dot={false} name="inv/s" />
                        <Line type="monotone" dataKey="err" stroke="#52616a" strokeWidth={2} dot={false} name="err/s" />
                      </LineChart>
                    </ResponsiveContainer>
                  )}
                </div>
              </section>

              <div className="two-col">
                <section className="card table-card">
                  <div className="section-head">
                    <div>
                      <h2>Function health</h2>
                      <p>Workers and queue by function</p>
                    </div>
                    <button type="button" className="text-button" onClick={() => setActive('Functions')}>
                      View all <span>→</span>
                    </button>
                  </div>
                  <div className="table-wrap">
                    {(functionsQ.data?.functions.length ?? 0) === 0 ? (
                      <Empty
                        title="No functions registered"
                        description="Run docker seed or register via Functions tab."
                      />
                    ) : (
                      <table>
                        <thead>
                          <tr>
                            <th>FUNCTION</th>
                            <th>STATUS</th>
                            <th>IDLE/BUSY</th>
                            <th>QUEUE</th>
                            <th>MEMORY</th>
                          </tr>
                        </thead>
                        <tbody>
                          {filteredFunctions.map((f) => {
                            const st = functionStatsQ.data?.find((s) => s?.name === f.name)
                            return (
                              <tr key={f.id} onClick={() => { setActive('Functions'); setInvokeName(f.name) }}>
                                <td>
                                  <div className="function-name">
                                    <span className={`fn-icon ${fnColor(f.name)}`}>
                                      <Terminal size={13} />
                                    </span>
                                    <strong>{f.name}</strong>
                                    <small>v{f.version}</small>
                                  </div>
                                </td>
                                <td>
                                  <Badge tone={f.status === 'ACTIVE' ? 'green' : 'amber'}>{f.status}</Badge>
                                </td>
                                <td className="mono">
                                  {st ? `${st.workers.idle}/${st.workers.busy}` : ''}
                                </td>
                                <td className="mono">{st?.queue_depth ?? 0}</td>
                                <td className="mono muted-text">{f.memory_mb} MB</td>
                              </tr>
                            )
                          })}
                        </tbody>
                      </table>
                    )}
                  </div>
                </section>

                <section className="card recent">
                  <div className="section-head">
                    <div>
                      <h2>Recent invocations</h2>
                      <p>Latest from SQLite</p>
                    </div>
                  </div>
                  <div className="invocation-list">
                    {(invocationsQ.data?.invocations.length ?? 0) === 0 ? (
                      <Empty title="No invocations yet" description="Invoke a function to populate this feed." />
                    ) : (
                      invocationsQ.data!.invocations.map((inv) => (
                        <div className="invocation" key={inv.request_id}>
                          <div className="invocation-status">
                            <span className={`status-icon ${inv.status !== 'COMPLETED' ? 'fail' : ''}`}>
                              {inv.status !== 'COMPLETED' ? <X size={12} /> : <Check size={12} />}
                            </span>
                            <div>
                              <strong>{inv.function_id}</strong>
                              <span className="mono id">{inv.request_id}</span>
                            </div>
                          </div>
                          <div className="invocation-meta">
                            <strong>{formatDuration(inv.duration_ms)}</strong>
                            <span>{relativeTime(inv.finished_at)}</span>
                          </div>
                        </div>
                      ))
                    )}
                  </div>
                  <button type="button" className="full-link" onClick={() => setActive('Invocations')}>
                    View all invocations <span>→</span>
                  </button>
                </section>
              </div>

              <div className="status-strip card">
                <div className="status-main">
                  <span className="status-check">
                    <Check size={14} />
                  </span>
                  <div>
                    <strong>
                      Control plane is {stats?.platform.ready ? 'ready' : 'not ready'}
                    </strong>
                    <span>{getBaseUrl()}</span>
                  </div>
                </div>
                <div className="status-item">
                  <Database size={15} />
                  <span>SQLite</span>
                  <Badge tone={stats?.platform.sqlite_ok ? 'green' : 'amber'}>
                    {stats?.platform.sqlite_ok ? 'Connected' : 'Down'}
                  </Badge>
                </div>
                <div className="status-item">
                  <Network size={15} />
                  <span>Functions</span>
                  <strong className="mono">{functionsQ.data?.functions.length ?? 0}</strong>
                </div>
                <div className="status-item">
                  <Server size={15} />
                  <span>Uptime</span>
                  <strong className="mono">{formatUptime(stats?.platform.uptime_ms ?? 0)}</strong>
                </div>
              </div>
            </>
          )}

          {active === 'Functions' && (
            <FunctionsPanel
              functions={filteredFunctions}
              query={query}
              setQuery={setQuery}
              invokeName={invokeName}
              setInvokeName={setInvokeName}
              invokeBody={invokeBody}
              setInvokeBody={setInvokeBody}
              invokeResult={invokeResult}
              onInvoke={handleInvoke}
              showRegister={showRegister}
              setShowRegister={setShowRegister}
              regForm={regForm}
              setRegForm={setRegForm}
              onRegister={handleRegister}
              stats={functionStatsQ.data}
              loading={functionsQ.isLoading}
            />
          )}

          {active === 'Invocations' && (
            <InvocationsPanel
              data={allInvocationsQ.data}
              loading={allInvocationsQ.isLoading}
              functions={functionsQ.data?.functions ?? []}
              invFunction={invFunction}
              setInvFunction={setInvFunction}
              invStatus={invStatus}
              setInvStatus={setInvStatus}
            />
          )}

          {active === 'Workers' && (
            <WorkersPanel
              workers={workersQ.data?.workers ?? []}
              stats={stats}
              loading={workersQ.isLoading}
            />
          )}

          {active === 'Metrics' && (
            <MetricsPanel data={metricsQ.data} loading={metricsQ.isLoading} />
          )}

          {active === 'Benchmarks' && (
            <BenchmarksPanel
              monitor={monitor}
              setMonitor={setMonitor}
              chartData={chartData}
              stats={stats}
            />
          )}

          {active === 'Settings' && (
            <section className="placeholder card settings-card">
              <div className="placeholder-head">
                <div>
                  <div className="eyebrow">CONFIGURATION</div>
                  <h2>Settings</h2>
                  <p>API endpoint and polling for this local dashboard.</p>
                </div>
              </div>
              <div className="settings-form">
                <label>
                  API base URL
                  <input
                    value={apiUrl}
                    onChange={(e) => setApiUrl(e.target.value)}
                    placeholder="http://localhost:8080"
                  />
                </label>
                <label>
                  Poll interval (ms)
                  <select value={pollMs} onChange={(e) => setPollMs(Number(e.target.value))}>
                    <option value={1000}>1000</option>
                    <option value={2000}>2000</option>
                    <option value={5000}>5000</option>
                  </select>
                </label>
                <div className="head-actions">
                  <button type="button" className="button primary" onClick={saveSettings}>
                    Save
                  </button>
                  <button
                    type="button"
                    className="button secondary"
                    onClick={async () => {
                      try {
                        await api.healthz()
                        alert('Health check OK')
                      } catch (e) {
                        alert(String(e))
                      }
                    }}
                  >
                    Test connection
                  </button>
                </div>
              </div>
            </section>
          )}
        </div>
      </main>
    </div>
  )
}

function FunctionsPanel({
  functions,
  query,
  setQuery,
  invokeName,
  setInvokeName,
  invokeBody,
  setInvokeBody,
  invokeResult,
  onInvoke,
  showRegister,
  setShowRegister,
  regForm,
  setRegForm,
  onRegister,
  stats,
  loading,
}: {
  functions: FunctionRecord[]
  query: string
  setQuery: (v: string) => void
  invokeName: string
  setInvokeName: (v: string) => void
  invokeBody: string
  setInvokeBody: (v: string) => void
  invokeResult: string
  onInvoke: () => void
  showRegister: boolean
  setShowRegister: (v: boolean) => void
  regForm: Record<string, string>
  setRegForm: (v: Record<string, string>) => void
  onRegister: () => void
  stats?: ({ name: string; queue_depth: number; workers: { idle: number; busy: number; total: number } } | null)[]
  loading: boolean
}) {
  return (
    <section className="placeholder card">
      <div className="toolbar">
        <div className="search">
          <Search size={16} />
          <input
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            placeholder="Search functions..."
          />
        </div>
        <button type="button" className="button primary" onClick={() => setShowRegister(true)}>
          <Plus size={16} /> Register
        </button>
      </div>
      {loading && <Empty title="Loading…" description="Fetching /api/v1/functions" />}
      {!loading && functions.length === 0 && (
        <Empty title="No functions" description="Register a function or run the Docker seed service." />
      )}
      <div className="placeholder-list">
        {functions.map((f) => {
          const st = stats?.find((s) => s?.name === f.name)
          return (
            <div
              className={`resource-row ${invokeName === f.name ? 'selected' : ''}`}
              key={f.id}
              onClick={() => setInvokeName(f.name)}
            >
              <div className="resource-primary">
                <span className={`resource-glyph fn-icon ${fnColor(f.name)}`}>
                  <Terminal size={14} />
                </span>
                <div>
                  <strong>{f.name}</strong>
                  <span className="mono muted-text">
                    v{f.version} · {f.timeout_ms}ms · {f.memory_mb}MB
                  </span>
                </div>
              </div>
              <Badge>{f.status}</Badge>
              <span className="mono">
                {st ? `${st.workers.idle}i/${st.workers.busy}b` : ''}
              </span>
            </div>
          )
        })}
      </div>

      {invokeName && (
        <div className="invoke-panel">
          <h3>
            Invoke <span className="mono">{invokeName}</span>
          </h3>
          <textarea value={invokeBody} onChange={(e) => setInvokeBody(e.target.value)} rows={4} />
          <button type="button" className="button primary" onClick={onInvoke}>
            <Play size={14} /> Invoke
          </button>
          {invokeResult && <pre className="mono result-box">{invokeResult}</pre>}
        </div>
      )}

      {showRegister && (
        <div className="modal-backdrop" onClick={() => setShowRegister(false)}>
          <div className="modal card" onClick={(e) => e.stopPropagation()}>
            <h3>Register function</h3>
            {(['name', 'command', 'version', 'min_workers', 'max_workers', 'timeout_ms'] as const).map(
              (key) => (
                <label key={key}>
                  {key}
                  <input
                    value={regForm[key]}
                    onChange={(e) => setRegForm({ ...regForm, [key]: e.target.value })}
                  />
                </label>
              ),
            )}
            <div className="head-actions">
              <button type="button" className="button secondary" onClick={() => setShowRegister(false)}>
                Cancel
              </button>
              <button type="button" className="button primary" onClick={onRegister}>
                Register
              </button>
            </div>
          </div>
        </div>
      )}
    </section>
  )
}

function InvocationsPanel({
  data,
  loading,
  functions,
  invFunction,
  setInvFunction,
  invStatus,
  setInvStatus,
}: {
  data?: { invocations: { request_id: string; function_id: string; status: string; duration_ms: number; error_code: string; finished_at: number }[]; total: number }
  loading: boolean
  functions: FunctionRecord[]
  invFunction: string
  setInvFunction: (v: string) => void
  invStatus: string
  setInvStatus: (v: string) => void
}) {
  const invs = data?.invocations ?? []
  const buckets = useMemo(() => {
    const edges = [10, 25, 50, 100, 500, Infinity]
    const labels = ['0-10', '10-25', '25-50', '50-100', '100-500', '500+']
    const counts = labels.map((label) => ({ label, count: 0 }))
    for (const inv of invs) {
      const i = edges.findIndex((e) => inv.duration_ms < e)
      counts[i === -1 ? counts.length - 1 : i].count++
    }
    return counts
  }, [invs])

  return (
    <section className="placeholder card">
      <div className="toolbar">
        <div className="toolbar-filters">
          <select value={invFunction} onChange={(e) => setInvFunction(e.target.value)} aria-label="Function">
            <option>All</option>
            {functions.map((f) => (
              <option key={f.id}>{f.name}</option>
            ))}
          </select>
          <select value={invStatus} onChange={(e) => setInvStatus(e.target.value)} aria-label="Status">
            {['All', 'COMPLETED', 'FAILED', 'TIMEOUT', 'CANCELLED'].map((s) => (
              <option key={s}>{s}</option>
            ))}
          </select>
        </div>
        <span className="muted-text mono total-count">{data?.total ?? 0} total</span>
      </div>
      <div className="chart-recharts" style={{ height: 160, marginBottom: 16 }}>
        <ResponsiveContainer width="100%" height="100%">
          <LineChart data={buckets}>
            <CartesianGrid stroke="#202b32" />
            <XAxis dataKey="label" stroke="#52606a" fontSize={10} />
            <YAxis stroke="#52606a" fontSize={10} allowDecimals={false} />
            <Tooltip contentStyle={{ background: '#151d24', border: '1px solid #243044', fontSize: 11 }} />
            <Area dataKey="count" stroke="var(--lime)" fill="var(--lime)" />
            <Line type="monotone" dataKey="count" stroke="var(--lime)" strokeWidth={2} dot />
          </LineChart>
        </ResponsiveContainer>
      </div>
      {loading && <Empty title="Loading…" description="" />}
      {!loading && invs.length === 0 && <Empty title="No invocations" description="Try invoking a function first." />}
      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>REQUEST</th>
              <th>FUNCTION</th>
              <th>STATUS</th>
              <th>DURATION</th>
              <th>TIME</th>
              <th>ERROR</th>
            </tr>
          </thead>
          <tbody>
            {invs.map((inv) => (
              <tr key={inv.request_id}>
                <td className="mono">
                  {inv.request_id.slice(0, 18)}…
                  <button
                    type="button"
                    className="icon-button"
                    onClick={() => navigator.clipboard.writeText(inv.request_id)}
                    title="Copy"
                  >
                    <Copy size={12} />
                  </button>
                </td>
                <td>{inv.function_id}</td>
                <td>
                  <Badge tone={inv.status === 'COMPLETED' ? 'green' : 'amber'}>{inv.status}</Badge>
                </td>
                <td className="mono">{formatDuration(inv.duration_ms)}</td>
                <td>{relativeTime(inv.finished_at)}</td>
                <td className="mono muted-text">{inv.error_code || ''}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  )
}

function WorkersPanel({
  workers,
  stats,
  loading,
}: {
  workers: { id: string; function_id: string; node_id: string; state: string; is_remote: boolean }[]
  stats?: { workers: { total: number; idle: number; busy: number } }
  loading: boolean
}) {
  return (
    <>
      <section className="kpi-grid">
        <Kpi label="Total" value={String(stats?.workers.total ?? workers.length)} icon={Cpu} />
        <Kpi label="Idle" value={String(stats?.workers.idle ?? 0)} icon={Users} tone="cyan" />
        <Kpi label="Busy" value={String(stats?.workers.busy ?? 0)} icon={Activity} />
      </section>
      <section className="worker-grid">
        {loading && <Empty title="Loading workers…" description="" />}
        {!loading && workers.length === 0 && (
          <Empty title="No workers" description="Register a function with min_workers ≥ 1." />
        )}
        {workers.map((w) => (
          <div className="card worker-card" key={w.id}>
            <div className="worker-top">
              <Badge tone={w.state === 'IDLE' ? 'green' : w.state === 'RUNNING' ? 'cyan' : 'amber'}>
                {w.state}
              </Badge>
              {w.is_remote && <Badge tone="amber">remote</Badge>}
            </div>
            <strong className="mono">{w.id}</strong>
            <span>{w.function_id}</span>
            <span className="muted-text">node: {w.node_id}</span>
          </div>
        ))}
      </section>
    </>
  )
}

function MetricsPanel({
  data,
  loading,
}: {
  data?: {
    counters: { name: string; labels: Record<string, string>; value: number }[]
    gauges: { name: string; labels: Record<string, string>; value: number }[]
    histograms: { name: string; count: number; sum: number; buckets: { le: number; count: number }[] }[]
  }
  loading: boolean
}) {
  const highlight = [
    'invocations_total',
    'cold_start_total',
    'warm_invocation_count',
    'worker_crash_total',
    'worker_timeout_total',
    'queue_depth',
    'worker_count',
  ]
  const tiles = [
    ...(data?.counters ?? []),
    ...(data?.gauges ?? []),
  ].filter((m) => highlight.includes(m.name))

  const hist = data?.histograms.find((h) => h.name.includes('invocation_duration'))

  return (
    <section className="placeholder card">
      {loading && <Empty title="Loading metrics…" description="" />}
      <div className="kpi-grid">
        {tiles.map((m, i) => (
          <Kpi
            key={`${m.name}-${i}`}
            label={m.name}
            value={String(m.value)}
            hint={Object.entries(m.labels)
              .map(([k, v]) => `${k}=${v}`)
              .join(' ') || 'gauge/counter'}
            icon={BarChart3}
          />
        ))}
      </div>
      {hist && (
        <div className="chart-recharts" style={{ marginTop: 20, height: 220 }}>
          <h3>invocation_duration_seconds buckets</h3>
          <ResponsiveContainer width="100%" height={180}>
            <LineChart data={hist.buckets.map((b) => ({ le: String(b.le), count: b.count }))}>
              <CartesianGrid stroke="#202b32" />
              <XAxis dataKey="le" stroke="#52606a" fontSize={10} />
              <YAxis stroke="#52606a" fontSize={10} />
              <Tooltip contentStyle={{ background: '#151d24', border: '1px solid #243044', fontSize: 11 }} />
              <Line type="monotone" dataKey="count" stroke="var(--cyan)" strokeWidth={2} />
            </LineChart>
          </ResponsiveContainer>
          <p className="muted-text mono">
            count={hist.count} sum={hist.sum.toFixed(4)} avg=
            {hist.count ? (hist.sum / hist.count).toFixed(4) : 0}s
          </p>
        </div>
      )}
    </section>
  )
}

function BenchmarksPanel({
  monitor,
  setMonitor,
  chartData,
  stats,
}: {
  monitor: boolean
  setMonitor: (v: boolean) => void
  chartData: { i: number; inv: number; err: number }[]
  stats?: { rates: { invocations_per_second: number }; queue: { total_depth: number }; workers: { busy: number; total: number } }
}) {
  return (
    <section className="placeholder card">
      <div className="placeholder-head">
        <div>
          <div className="eyebrow">LOAD MONITOR</div>
          <h2>Load & Benchmarks</h2>
          <p>Poll the control plane while you run external load tests.</p>
        </div>
        <button
          type="button"
          className={`button ${monitor ? 'primary' : 'secondary'}`}
          onClick={() => setMonitor(!monitor)}
        >
          {monitor ? '● REC Monitoring' : 'Start monitor'}
        </button>
      </div>
      <section className="kpi-grid" style={{ marginTop: 20 }}>
        <Kpi label="Throughput" value={(stats?.rates.invocations_per_second ?? 0).toFixed(2)} icon={Activity} />
        <Kpi label="Queue" value={String(stats?.queue.total_depth ?? 0)} icon={Layers3} />
        <Kpi label="Busy workers" value={String(stats?.workers.busy ?? 0)} icon={Cpu} />
      </section>
      <div className="chart-recharts" style={{ height: 220, marginTop: 16 }}>
        <ResponsiveContainer width="100%" height="100%">
          <LineChart data={chartData}>
            <CartesianGrid stroke="#202b32" />
            <YAxis stroke="#52606a" fontSize={10} />
            <Tooltip contentStyle={{ background: '#151d24', border: '1px solid #243044', fontSize: 11 }} />
            <Line type="monotone" dataKey="inv" stroke="var(--lime)" strokeWidth={2} dot={false} />
          </LineChart>
        </ResponsiveContainer>
      </div>
      <div className="commands">
        <h3>Commands</h3>
        <pre className="mono result-box">{`./scripts/load-test.sh warm\nk6 run benchmarks/scenarios/warm.js`}</pre>
      </div>
    </section>
  )
}
