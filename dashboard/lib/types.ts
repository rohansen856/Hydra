export interface PlatformStats {
  platform: {
    ready: boolean
    sqlite_ok: boolean
    uptime_ms: number
  }
  invocations: {
    accepted: number
    completed: number
  }
  workers: {
    total: number
    idle: number
    busy: number
  }
  queue: {
    total_depth: number
  }
  rates: {
    invocations_per_second: number
    errors_per_second: number
  }
}

export interface FunctionRecord {
  id: string
  name: string
  version: string
  command: string
  min_workers: number
  max_workers: number
  timeout_ms: number
  max_concurrency: number
  memory_mb: number
  status: string
  created_at?: number
}

export interface FunctionStats {
  name: string
  function_id: string
  queue_depth: number
  workers: {
    total: number
    idle: number
    busy: number
  }
}

export interface Invocation {
  request_id: string
  function_id: string
  status: string
  duration_ms: number
  error_code: string
  started_at: number
  finished_at: number
}

export interface InvocationsResponse {
  invocations: Invocation[]
  total: number
}

export interface Worker {
  id: string
  function_id: string
  node_id: string
  state: string
  is_remote: boolean
}

export interface Node {
  id: string
  host: string
  port: number
  cpu_capacity: number
  memory_mb: number
  running_workers: number
  available_workers: number
  healthy: boolean
}

export interface MetricEntry {
  name: string
  labels: Record<string, string>
  value: number
}

export interface HistogramEntry {
  name: string
  labels: Record<string, string>
  count: number
  sum: number
  buckets: { le: number; count: number }[]
}

export interface MetricsJson {
  counters: MetricEntry[]
  gauges: MetricEntry[]
  histograms: HistogramEntry[]
}

export interface ThroughputPoint {
  timestamp: number
  invocationsPerSecond: number
  errorsPerSecond: number
}

export const API_URL_STORAGE_KEY = 'serverless_api_url'
export const POLL_INTERVAL_STORAGE_KEY = 'serverless_poll_interval_ms'
