# VCpp Live Editor — Multi-stage Docker build
#
# Stage 1 (prebuilder): Build VCpp library + dependencies with Emscripten
# Stage 2 (gobuilder):  Build the Go HTTP server
# Stage 3 (runtime):    Combine everything into the final image

# ===========================================================================
# Stage 1: Pre-build VCpp library and dependencies
# ===========================================================================
FROM emscripten/emsdk:4.0.8 AS prebuilder

# Install CMake 4.x and Ninja (required for C++23 module support)
RUN apt-get update && apt-get install -y --no-install-recommends \
    ninja-build \
    && rm -rf /var/lib/apt/lists/*

# Install modern CMake (4.0+) — required for CXX_IMPORT_STD
RUN pip3 install cmake>=4.0.0

# Copy the full lam source tree
COPY . /src/lam

# Create the clang-scan-deps symlink needed for module scanning
RUN if [ ! -f "${EMSDK}/upstream/bin/clang-scan-deps" ]; then \
      ln -sf "${EMSDK}/upstream/bin/clang-scan-deps-19" \
             "${EMSDK}/upstream/bin/clang-scan-deps" 2>/dev/null || true; \
    fi

# Run the prebuild script
RUN chmod +x /src/lam/vcpp/editor/prebuilt/prebuild.sh \
             /src/lam/vcpp/editor/prebuilt/collect_artifacts.sh \
    && /src/lam/vcpp/editor/prebuilt/prebuild.sh

# Copy template files into cache
RUN cp /src/lam/vcpp/editor/templates/runner.html /prebuilt/cache/templates/ \
    && cp /src/lam/vcpp/editor/templates/shell.html /prebuilt/cache/templates/ \
    && cp /src/lam/vcpp/examples/web/graph_instance.js /prebuilt/cache/templates/ \
    && cp /src/lam/vcpp/examples/web/graph_manager.js /prebuilt/cache/templates/

# ===========================================================================
# Stage 2: Build the Go server
# ===========================================================================
FROM golang:1.22-bookworm AS gobuilder

WORKDIR /app

# Copy go module files first for caching
COPY vcpp/editor/server/go.mod vcpp/editor/server/go.sum* ./
RUN go mod download 2>/dev/null || true

# Copy server source
COPY vcpp/editor/server/ ./
RUN go mod tidy && CGO_ENABLED=0 GOOS=linux go build -o /vcpp-editor-server .

# ===========================================================================
# Stage 3: Runtime image
# ===========================================================================
FROM emscripten/emsdk:4.0.8 AS runtime

# Install minimal dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy prebuilt cache from Stage 1
COPY --from=prebuilder /prebuilt/cache /prebuilt/cache

# Copy Go server binary from Stage 2
COPY --from=gobuilder /vcpp-editor-server /app/vcpp-editor-server

# Copy frontend
COPY vcpp/editor/frontend/ /app/frontend/

# Copy templates
COPY vcpp/editor/templates/ /app/templates/

# Create session directory
RUN mkdir -p /tmp/sessions

# Environment
ENV PORT=8080 \
    WORKERS=4 \
    SESSION_TTL=1800 \
    MAX_SESSIONS=100 \
    COMPILE_TIMEOUT=60 \
    PREBUILT_DIR=/prebuilt/cache \
    SESSION_DIR=/tmp/sessions \
    FRONTEND_DIR=/app/frontend \
    TEMPLATE_DIR=/app/templates

EXPOSE 8080

WORKDIR /app

CMD ["/app/vcpp-editor-server"]
