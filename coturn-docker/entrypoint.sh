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

# ── Traffic tracking via coturn prometheus metrics ────────────

STATS_FILE="/var/lib/coturn/traffic_stats"
PROMETHEUS_URL="http://localhost:9641/metrics"

get_total_bytes() {
    local metrics rcvb sentb
    metrics=$(curl -s --connect-timeout 2 --max-time 5 "$PROMETHEUS_URL" 2>/dev/null) || { echo 0; return; }
    rcvb=$(echo "$metrics" | awk '/^turn_total_traffic_rcvb /{printf "%.0f", $2}')
    sentb=$(echo "$metrics" | awk '/^turn_total_traffic_sentb /{printf "%.0f", $2}')
    echo $(( ${rcvb:-0} + ${sentb:-0} ))
}

load_stats() {
    LAST_TOTAL=0 DAILY_BYTES=0 MONTHLY_BYTES=0
    CURRENT_DAY=$(date +%Y-%m-%d)
    CURRENT_MONTH=$(date +%Y-%m)
    [ -f "$STATS_FILE" ] && . "$STATS_FILE" || true
}

save_stats() {
    cat > "$STATS_FILE" <<EOL
LAST_TOTAL=$LAST_TOTAL
DAILY_BYTES=$DAILY_BYTES
MONTHLY_BYTES=$MONTHLY_BYTES
CURRENT_DAY=$CURRENT_DAY
CURRENT_MONTH=$CURRENT_MONTH
EOL
}

update_traffic() {
    local total_now today this_month delta
    total_now=$(get_total_bytes)
    total_now=${total_now:-0}
    today=$(date +%Y-%m-%d)
    this_month=$(date +%Y-%m)

    if [ "$today" != "$CURRENT_DAY" ]; then DAILY_BYTES=0; CURRENT_DAY="$today"; fi
    if [ "$this_month" != "$CURRENT_MONTH" ]; then MONTHLY_BYTES=0; CURRENT_MONTH="$this_month"; fi

    if [ "$total_now" -ge "$LAST_TOTAL" ] 2>/dev/null; then
        delta=$((total_now - LAST_TOTAL))
    else
        delta=$total_now
    fi

    DAILY_BYTES=$((DAILY_BYTES + delta))
    MONTHLY_BYTES=$((MONTHLY_BYTES + delta))
    LAST_TOTAL=$total_now
    save_stats
}

# ── Publish iceServers JSON to PUBLISH_URL every PUBLISH_INTERVAL seconds ─

publish_ice() {
    local ttl="${CREDENTIAL_TTL:-300}"
    echo "ice-publisher: -> $PUBLISH_URL every ${PUBLISH_INTERVAL}s"
    load_stats
    while true; do
        update_traffic
        local ts=$(($(date +%s) + ttl))
        local cred
        cred=$(printf '%s' "$ts" | openssl dgst -sha1 -hmac "$TURN_SECRET" -binary | base64)
        local code
        code=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 10 --max-time 30 -X POST \
            -H "Content-Type: application/json" \
            -d "{\"domain\":\"${DOMAIN}\",\"turnSecret\":\"${TURN_SECRET}\",\"iceServers\":[{\"urls\":[\"stun:${DOMAIN}:3478\"]},{\"urls\":[\"turn:${DOMAIN}:3478\",\"turns:${DOMAIN}:5349\"],\"username\":\"${ts}\",\"credential\":\"${cred}\"}],\"traffic\":{\"dailyBytes\":${DAILY_BYTES},\"monthlyBytes\":${MONTHLY_BYTES}}}" \
            "$PUBLISH_URL" 2>&1) || true
        echo "ice-publisher: user=$ts daily=${DAILY_BYTES}B monthly=${MONTHLY_BYTES}B -> HTTP $code"
        sleep "${PUBLISH_INTERVAL:-60}"
    done
}

mkdir -p /var/lib/coturn
generate_config

trap 'kill 0; wait; exit 0' TERM INT

watch_certs &
[ -n "$PUBLISH_URL" ] && publish_ice &

echo "Starting coturn..."
turnserver -c "$CONF" --pidfile /var/run/turnserver.pid -v &
wait
