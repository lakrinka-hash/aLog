#!/bin/bash

cd "$(dirname "$0")"

echo "=== Start aLog monitoring ==="

if ! command -v docker &> /dev/null; then
    echo "Error: Docker not found"
    echo "Please install docker.io and docker-compose-v2 before running"
    read -p "Press Enter to exit..."
    exit 1
fi

if [ ! -d "data" ]; then
    echo "Creating local directories for storing data (telemetry/data) and setting permissions..."
    mkdir -p data/influxdb data/grafana
    chmod -R 777 data 2>/dev/null || true
fi

echo "Launching InfluxDB, Telegraf, and Grafana containers..."
docker compose up -d

if [ $? -ne 0 ]; then
    echo "Error: Failed to start Docker Compose"
    read -p "Press Enter to exit..."
    exit 1
fi

echo "Docker services have started successfully"
sleep 3
echo "Opening the Grafana web interface..."

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    xdg-open "http://localhost:3000" &> /dev/null
elif [[ "$OSTYPE" == "darwin"* ]]; then
    open "http://localhost:3000"
fi

echo "The telemetry monitoring stack is ready to use"
