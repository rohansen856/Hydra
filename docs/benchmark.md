# Benchmark Report Template

Use this template when recording performance results. Fill in hardware and scenario details before comparing runs.

## Environment

| Field | Value |
|-------|-------|
| Hardware | |
| OS | |
| Compiler | |
| Build type | Release |
| Workers min/max | |
| Concurrency | |
| Duration | |

## Results

| Metric | Value |
|--------|------:|
| Requests | |
| Throughput (req/s) | |
| P50 (ms) | |
| P95 (ms) | |
| P99 (ms) | |
| Errors (%) | |
| Max workers observed | |
| Cold starts | |

## How to run

### Quick load test (shell)

```bash
# Start server first
./scripts/run-dev.sh

# In another terminal
./scripts/load-test.sh http://127.0.0.1:8080 hello 1000 10
# args: url function count concurrency
```

### k6 scenarios (optional)

Install [k6](https://k6.io/), then:

```bash
k6 run benchmarks/scenarios/warm.js
```

## Baseline expectations (local dev)

These are rough guides on a typical laptop  not SLA targets:

| Function | P50 latency | Notes |
|----------|------------:|-------|
| C++ hello | ~10–20 ms | Includes fork/exec overhead |
| Python hello | ~30–50 ms | Interpreter startup |
| Node hello | ~25–40 ms | V8 startup |

Cold starts (first invocation after register) add 50–200 ms depending on binary size and OS cache state.

## Tips for consistent results

1. Use Release builds (`cmake -DCMAKE_BUILD_TYPE=Release`)
2. Warm up with 10–20 requests before measuring
3. Set `min_workers: 1` to avoid cold-start noise
4. Close other CPU-heavy processes
5. Run multiple iterations and report median

## Related docs

- [Operations](operations.md)  load test commands
- [Architecture](architecture.md)  worker pool behavior
