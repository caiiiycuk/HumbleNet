#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "Triggering certificate renewal..."
docker compose exec certbot certbot renew --webroot -w /var/www/certbot
docker compose exec nginx nginx -s reload

echo "Restarting coturn to load new certificates..."
docker compose restart coturn

echo "Done. Verify with: docker compose ps"
