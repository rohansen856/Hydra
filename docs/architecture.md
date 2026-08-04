# Architecture

## Overview

The platform is a single-process control plane that accepts HTTP requests, schedules invocations onto worker processes, and returns JSON responses. Functions run as separate OS processes communicating via stdin/stdout JSON.

```text
┌─────────────┐     ┌──────────────────┐     ┌─────────────┐
│  HttpServer │────▶│ InvocationManager│────▶│  Scheduler  │
└─────────────┘     └──────────────────┘     └──────┬──────┘
                                                    │
                     ┌──────────────────┐           │
                     │   Autoscaler     │◀──────────┤
                     └──────────────────┘           │
                                                    ▼
                     ┌──────────────────┐     ┌─────────────┐
                     │ FunctionRegistry │     │WorkerManager│
                     │    (SQLite)      │     └──────┬──────┘
                     └──────────────────┘            │
                                                     ▼
                                              ┌─────────────┐
                                              │FunctionRunner│
                                              │ fork/exec   │
                                              └─────────────┘
```

## Control loop

```text
HTTP request → InvocationManager → Scheduler → WorkerManager → FunctionRunner → fork/exec
                                      ↑                │
                                      │                ↓
                                   Autoscaler    worker state update
```

1. HTTP handler validates the request and enqueues an invocation
2. Scheduler picks an idle worker or spawns a new one (within limits)
3. FunctionRunner writes stdin JSON, reads stdout JSON, enforces timeout
4. Result propagates back through the scheduler to the HTTP response
5. Autoscaler periodically adjusts worker counts based on load

## Components

| Component | Responsibility |
|-----------|----------------|
| **HttpServer** | Boost.Beast HTTP front door; routes to handlers |
| **FunctionRegistry** | SQLite-backed function metadata; copies executables to managed storage |
| **Scheduler** | Strand-serialized dispatch, queueing, invocation lifecycle |
| **WorkerManager** | Worker lifecycle, idle/busy pools, state machine |
| **FunctionRunner** | Process spawn, protocol I/O, timeout enforcement |
| **Autoscaler** | Periodic scale-up/down signals based on queue depth |
| **Metrics / StructuredLogger** | Prometheus metrics and structured logs |

## Threading model

- **HTTP thread**  dedicated blocking thread accepts connections and dispatches route handlers synchronously
- **Scheduler strand**  all scheduler mutations serialized on a Boost.Asio strand
- **FunctionRunner pool**  4-thread pool for blocking `waitpid` and pipe I/O
- **Autoscaler timer**  periodic tick on the `io_context`

Route handlers must not block the HTTP thread for long operations; invocations are async through the scheduler.

## Worker state machine

```text
  spawn ──▶ idle ──▶ busy ──▶ idle
              │         │
              │         └──▶ crashed ──▶ (replaced)
              └──▶ draining ──▶ terminated
```

Workers are OS processes running the registered function executable. The platform maintains a warm pool (`min_workers`) and scales up to `max_workers`.

## Isolation model

Each invocation executes the registered executable in a separate OS process via `fork` + `execv`:

- Stdin receives the invocation JSON
- Stdout must contain exactly one response JSON
- Stderr is for diagnostics
- Non-zero exit or timeout marks failure

This MVP uses process isolation only  no cgroups, namespaces, or seccomp. Not production-grade sandboxing.

## Data persistence

| Path | Contents |
|------|----------|
| `data/serverless.db` | Function registrations, worker records |
| `data/functions/{name}/{version}/` | Copied executables and polyglot packages |

On register, the platform copies the command artifact into managed storage so the original path need not remain available.

## Multi-node (Phase 7)

Worker nodes can register via `POST /internal/v1/nodes/register` and send heartbeats. The control plane tracks node health centrally. In the default single-node deployment, workers run locally on the control plane host.

Docker Compose supports an optional multi-node profile:

```bash
docker compose --profile multi-node up
```

## Configuration

All runtime settings come from `config/config.yaml`:

- Server bind address/port
- Scheduler queue limits
- Default timeouts and memory hints
- Logging level
- SQLite database path

Invalid config fails fast at startup.

## Related docs

- [Function Protocol](function-protocol.md)  stdin/stdout contract
- [Operations](operations.md)  config, logs, metrics
- [Docker](docker.md)  container deployment
