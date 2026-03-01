#!/bin/bash
set -e

: "${DOMAIN:?ERROR: DOMAIN is required}"
: "${TURN_SECRET:?ERROR: TURN_SECRET is required (generate with: openssl rand -hex 32)}"

if [ -z "$PUBLIC_IP" ]; then
    PUBLIC_IP=$(curl -s -4 --connect-timeout 5 https://api.ipify.org \
             || curl -s -4 --connect-timeout 5 https://ifconfig.me \
             || curl -s -4 --connect-timeout 5 https://checkip.amazonaws.com \
             || true)
    PUBLIC_IP=$(echo "$PUBLIC_IP" | tr -d '[:space:]')
    : "${PUBLIC_IP:?ERROR: Could not detect public IP. Set PUBLIC_IP in .env}"
    echo "Detected public IP: $PUBLIC_IP"
fi

export PUBLIC_IP TURN_SECRET
export RELAY_PORT_MIN="${RELAY_PORT_MIN:-49152}"
export RELAY_PORT_MAX="${RELAY_PORT_MAX:-49252}"

CERT_DIR="/etc/letsencrypt/live/$DOMAIN"
CONF="/etc/turnserver.conf"

generate_config() {
    envsubst < /etc/turnserver.conf.template > "$CONF"
    if [ -f "$CERT_DIR/fullchain.pem" ] && [ -f "$CERT_DIR/privkey.pem" ]; then
        printf '\ncert=%s/fullchain.pem\npkey=%s/privkey.pem\n' "$CERT_DIR" "$CERT_DIR" >> "$CONF"
        echo "TLS: certificates loaded from $CERT_DIR"
    else
        echo "WARNING: no TLS certs at $CERT_DIR — TURNS unavailable. Run ./init.sh"
    fi
}

# Watch for cert renewal (certbot deploy-hook touches this file)
watch_certs() {
    while true; do
        sleep 60
        [ -f /etc/letsencrypt/renewed ] || continue
        rm -f /etc/letsencrypt/renewed
        echo "Certificate renewed — restarting turnserver..."
        generate_config
        kill -TERM "$(cat /var/run/turnserver.pid 2>/dev/null)" 2>/dev/null || true
        wait 2>/dev/null || true
        turnserver -c "$CONF" --pidfile /var/run/turnserver.pid -v &
    done
}

# Publish iceServers JSON to PUBLISH_URL every PUBLISH_INTERVAL seconds
publish_ice() {
    local ttl="${CREDENTIAL_TTL:-86400}"
    echo "ice-publisher: -> $PUBLISH_URL every ${PUBLISH_INTERVAL}s"
    while true; do
        local ts=$(($(date +%s) + ttl))
        local cred
        cred=$(printf '%s' "$ts" | openssl dgst -sha1 -hmac "$TURN_SECRET" -binary | base64)
        local code
        code=$(curl -s -o /dev/null -w "%{http_code}" -X POST \
            -H "Content-Type: application/json" \
            -d "{\"iceServers\":[{\"urls\":[\"stun:${DOMAIN}:3478\"]},{\"urls\":[\"turn:${DOMAIN}:3478\",\"turns:${DOMAIN}:5349\"],\"username\":\"${ts}\",\"credential\":\"${cred}\"}]}" \
            "$PUBLISH_URL" 2>&1) || true
        echo "ice-publisher: user=$ts -> HTTP $code"
        sleep "${PUBLISH_INTERVAL:-60}"
    done
}

generate_config

trap 'kill 0; wait; exit 0' TERM INT

watch_certs &
[ -n "$PUBLISH_URL" ] && publish_ice &

echo "Starting coturn..."
turnserver -c "$CONF" --pidfile /var/run/turnserver.pid -v &
wait
