# syntax=docker/dockerfile:1

FROM ubuntu:22.04 AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF \
    && cmake --build build -j"$(nproc)"

FROM ubuntu:22.04 AS runtime
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    libboost-system1.74.0 ca-certificates curl python3 nodejs \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
RUN mkdir -p /app/data /app/build/functions
COPY --from=builder /app/build/serverless /app/build/serverless
COPY --from=builder /app/build/worker_node /app/build/worker_node
COPY --from=builder /app/build/functions/ /app/build/functions/
COPY config/config.yaml /app/config/config.yaml
COPY functions/hello_python /app/functions/hello_python
COPY functions/hello_node /app/functions/hello_node
RUN chmod +x /app/functions/hello_python/run.sh /app/functions/hello_node/run.sh
EXPOSE 8080
HEALTHCHECK --interval=10s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -sf http://127.0.0.1:8080/healthz || exit 1
CMD ["/app/build/serverless", "/app/config/config.yaml"]
