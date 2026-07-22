#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

echo "=== aLog telemetry startup ==="

if ! command -v docker >/dev/null 2>&1; then
    echo "Error: Docker not found"
    echo "Please install Docker and Docker Compose v2 before running"
    exit 1
fi

if [ ! -f ".env" ]; then
    echo "Local .env not found. Creating from .env.example."
    cp .env.example .env
    echo "Please edit .env before starting the stack."
    echo "  nano .env"
fi

case "${1:-}" in
  reset-grafana)
    echo "Stopping containers and resetting Grafana runtime state..."
    docker compose down -v
    rm -rf data/grafana
    echo "Grafana runtime state removed. Start again with ./run.sh"
    exit 0
    ;;
  reset-all)
    echo "Stopping containers and resetting all telemetry data..."
    docker compose down -v
    rm -rf data
    echo "All local telemetry data removed. Start again with ./run.sh"
    exit 0
    ;;
  help|-h|--help)
    echo "Usage: $0 [reset-grafana|reset-all]"
    echo "  reset-grafana  - remove Grafana runtime state and start fresh"
    echo "  reset-all      - remove all telemetry data and start from scratch"
    exit 0
    ;;
  "")
    ;;
  *)
    echo "Unknown command: $1"
    echo "Usage: $0 [reset-grafana|reset-all]"
    exit 1
    ;;
esac

# Ensure local directories exist and have open read/write permissions for container users (e.g., Grafana UID 472)
mkdir -p data/influxdb data/grafana
chmod -R 777 data || true

echo "Launching InfluxDB, Telegraf, and Grafana containers..."
docker compose up -d

echo "Docker services have started successfully"
sleep 3

echo "Opening the Grafana web interface..."
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    xdg-open "http://localhost:3000" &> /dev/null || true
elif [[ "$OSTYPE" == "darwin"* ]]; then
    open "http://localhost:3000" || true
fi

echo "The telemetry monitoring stack is ready to use"
echo "If provisioning configuration changed, run './run.sh reset-grafana' to clear Grafana state."

