#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

STAGING=""
if [ "$1" = "--staging" ]; then
    STAGING="--staging"
    echo "*** STAGING MODE: using Let's Encrypt staging server ***"
    echo "*** Certificates will NOT be trusted by browsers     ***"
    echo ""
fi

# ── Pre-flight checks ────────────────────────────────────────────

if ! command -v docker &>/dev/null; then
    echo "ERROR: Docker is not installed."
    echo "Install with: curl -fsSL https://get.docker.com | sh"
    exit 1
fi

if ! docker compose version &>/dev/null; then
    echo "ERROR: Docker Compose v2 is not available."
    echo "Update Docker or install the compose plugin."
    exit 1
fi

if ! docker info &>/dev/null 2>&1; then
    echo "ERROR: Docker daemon is not running, or you need sudo."
    echo "Try: sudo ./init.sh"
    exit 1
fi

# ── Configuration ─────────────────────────────────────────────────

if [ ! -f .env ]; then
    echo "============================================"
    echo "  First-time setup: creating .env file"
    echo "============================================"
    cp .env.example .env

    read -p "Enter your domain name (e.g. turn.example.com): " domain
    read -p "Enter your email for Let's Encrypt: " email

    turn_secret=$(openssl rand -hex 32)

    sed -i "s|DOMAIN=.*|DOMAIN=$domain|" .env
    sed -i "s|EMAIL=.*|EMAIL=$email|" .env
    sed -i "s|TURN_SECRET=.*|TURN_SECRET=$turn_secret|" .env

    echo ""
    echo "Configuration saved to .env"
    echo "TURN_SECRET generated automatically."
fi

source .env

echo ""
echo "============================================"
echo "  Domain:  $DOMAIN"
echo "  Email:   $EMAIL"
echo "  Auth:    TURN REST API (ephemeral credentials)"
echo "============================================"
echo ""

# ── DNS check ─────────────────────────────────────────────────────

if command -v dig &>/dev/null; then
    RESOLVED_IP=$(dig +short "$DOMAIN" | tail -1)
    if [ -z "$RESOLVED_IP" ]; then
        echo "WARNING: $DOMAIN does not resolve to any IP address."
        echo "Make sure the DNS A record is set before continuing."
        read -p "Continue anyway? [y/N] " confirm
        [ "$confirm" = "y" ] || [ "$confirm" = "Y" ] || exit 1
    else
        echo "DNS check: $DOMAIN -> $RESOLVED_IP"
    fi
fi

# ── Port check ────────────────────────────────────────────────────

check_port() {
    if ss -tlnp 2>/dev/null | grep -q ":$1 "; then
        echo "WARNING: Port $1 is already in use:"
        ss -tlnp | grep ":$1 "
        read -p "Continue anyway? [y/N] " confirm
        [ "$confirm" = "y" ] || [ "$confirm" = "Y" ] || exit 1
    fi
}

ACME_PORT="${ACME_PORT:-80}"

check_port "$ACME_PORT"
check_port 3478
check_port 5349

# ── Build ─────────────────────────────────────────────────────────

echo ""
echo "Step 1: Building coturn image..."
docker compose build

# ── Certificate ───────────────────────────────────────────────────

echo ""
echo "Step 2: Generating Let's Encrypt certificate..."

if [ "$ACME_PORT" = "80" ]; then
    echo "  Using certbot standalone (port 80 must be reachable from the internet)"
    echo ""

    docker compose run --rm --entrypoint "" \
        -p 80:80 \
        certbot \
        certbot certonly \
            --standalone \
            --preferred-challenges http \
            --email "$EMAIL" \
            --agree-tos \
            --no-eff-email \
            $STAGING \
            -d "$DOMAIN"
else
    echo "  ACME_PORT=$ACME_PORT — using webroot mode"
    echo "  Your host nginx must proxy /.well-known/acme-challenge/ to localhost:$ACME_PORT"
    echo ""
    echo "  Example host nginx config:"
    echo "    location /.well-known/acme-challenge/ {"
    echo "        proxy_pass http://127.0.0.1:$ACME_PORT;"
    echo "    }"
    echo ""

    echo "  Starting nginx for ACME challenge..."
    docker compose up -d nginx

    sleep 3

    docker compose run --rm --entrypoint "" \
        certbot \
        certbot certonly \
            --webroot \
            -w /var/www/certbot \
            --preferred-challenges http \
            --email "$EMAIL" \
            --agree-tos \
            --no-eff-email \
            $STAGING \
            -d "$DOMAIN"

    docker compose stop nginx
fi

# ── Start ─────────────────────────────────────────────────────────

echo ""
echo "Step 3: Starting services..."
docker compose up -d

echo ""
echo "============================================"
echo "  Coturn server is running!"
echo "============================================"
echo ""
echo "  STUN:   stun:$DOMAIN:3478"
echo "  TURN:   turn:$DOMAIN:3478 (UDP/TCP)"
echo "  TURNS:  turns:$DOMAIN:5349 (TLS)"
echo ""
echo "  Auth:   TURN REST API (ephemeral credentials)"
echo "  Secret: ${TURN_SECRET:0:8}...  (full value in .env)"
echo ""
echo "  Ping test page: https://$DOMAIN:8000/"
echo "  Ping API:       https://$DOMAIN:8000/ping"
echo ""
echo "  Your backend must generate credentials using TURN_SECRET."
echo "  See README.md for Node.js / Python examples."
echo ""
echo "  Verify health:  docker compose ps"
echo "  View logs:      docker compose logs -f coturn"
echo ""
if [ -n "$STAGING" ]; then
echo "  *** STAGING certificates — not browser-trusted ***"
echo "  Re-run without --staging for production certs:"
echo "    rm -f .env && docker compose down -v && ./init.sh"
echo ""
fi
