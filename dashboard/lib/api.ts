import type {
  FunctionRecord,
  FunctionStats,
  InvocationsResponse,
  MetricsJson,
  Node,
  PlatformStats,
  Worker,
} from './types'
import { API_URL_STORAGE_KEY } from './types'

export function getBaseUrl(): string {
  if (typeof window !== 'undefined') {
    const stored = localStorage.getItem(API_URL_STORAGE_KEY)
    if (stored) return stored.replace(/\/$/, '')
  }
  return (process.env.NEXT_PUBLIC_API_URL ?? 'http://localhost:8080').replace(/\/$/, '')
}

export function setBaseUrl(url: string) {
  if (typeof window !== 'undefined') {
    localStorage.setItem(API_URL_STORAGE_KEY, url.replace(/\/$/, ''))
  }
}

async function fetchJson<T>(path: string, init?: RequestInit): Promise<T> {
  const res = await fetch(`${getBaseUrl()}${path}`, {
    ...init,
    headers: {
      ...(init?.body ? { 'Content-Type': 'application/json' } : {}),
      ...init?.headers,
    },
  })
  const text = await res.text()
  let json: unknown = {}
  try {
    json = text ? JSON.parse(text) : {}
  } catch {
    json = { raw: text }
  }
  if (!res.ok) {
    const err = json as { error?: { message?: string } }
    throw new Error(err.error?.message ?? `API ${path} returned ${res.status}`)
  }
  return json as T
}

export const api = {
  healthz: () => fetchJson<{ status: string }>('/healthz'),
  readyz: () => fetchJson<{ status: string }>('/readyz'),
  stats: () => fetchJson<PlatformStats>('/api/v1/stats'),
  functionStats: (name: string) =>
    fetchJson<FunctionStats>(`/api/v1/stats/functions/${encodeURIComponent(name)}`),
  functions: () => fetchJson<{ functions: FunctionRecord[] }>('/api/v1/functions'),
  getFunction: (name: string) =>
    fetchJson<FunctionRecord>(`/api/v1/functions/${encodeURIComponent(name)}`),
  registerFunction: (body: Record<string, unknown>) =>
    fetchJson<{ id: string; name: string; version: string; status: string }>(
      '/api/v1/functions',
      { method: 'POST', body: JSON.stringify(body) },
    ),
  deleteFunction: (name: string) =>
    fetchJson<{ deleted: boolean; name: string }>(
      `/api/v1/functions/${encodeURIComponent(name)}`,
      { method: 'DELETE' },
    ),
  invocations: (params?: Record<string, string>) => {
    const qs = params && Object.keys(params).length
      ? '?' + new URLSearchParams(params).toString()
      : ''
    return fetchJson<InvocationsResponse>(`/api/v1/invocations${qs}`)
  },
  workers: () => fetchJson<{ workers: Worker[] }>('/api/v1/workers'),
  nodes: () => fetchJson<{ nodes: Node[] }>('/api/v1/nodes'),
  metrics: () => fetchJson<MetricsJson>('/api/v1/metrics/json'),
  invoke: (name: string, payload: object) =>
    fetchJson<Record<string, unknown>>(
      `/api/v1/functions/${encodeURIComponent(name)}/invoke`,
      { method: 'POST', body: JSON.stringify(payload) },
    ),
}
