#!/usr/bin/env bash
set -euo pipefail

TLS_DIR=/opt/netdisk/tls
CERT_FILE="$TLS_DIR/server.crt"
KEY_FILE="$TLS_DIR/server.key"

mkdir -p "$TLS_DIR" /opt/netdisk/Version4.0/workspace

if [[ ! -s "$CERT_FILE" || ! -s "$KEY_FILE" ]]; then
    echo "[netdisk] generating development TLS certificate"
    openssl req -x509 -newkey rsa:2048 -nodes \
        -keyout "$KEY_FILE" \
        -out "$CERT_FILE" \
        -days 3650 \
        -subj "/CN=localhost" \
        >/dev/null 2>&1
    chmod 600 "$KEY_FILE"
fi

cd /opt/netdisk/Version4.0/server
echo "[netdisk] starting server on 0.0.0.0:2000"
exec ./server ../conf/server.docker.conf
