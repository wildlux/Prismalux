#!/bin/bash
# setup_tls.sh — Configura nginx + TLS davanti al server LAN di Prismalux
#
# Uso:
#   ./setup_tls.sh lan                        # self-signed per LAN/VPN (default)
#   ./setup_tls.sh internet mio.dominio.com   # Let's Encrypt (richiede DNS pubblico)
#
# Il server Prismalux gira su localhost:PRISMALUX_PORT (default 11500).
# Nginx ascolta su 443 (HTTPS) e reindirizza 80 → 443.

set -euo pipefail

MODE="${1:-lan}"
DOMAIN="${2:-prismalux.local}"
PRISMALUX_PORT="${PRISMALUX_PORT:-11500}"
NGINX_CONF="/etc/nginx/sites-available/prismalux"
SSL_DIR="/etc/nginx/ssl/prismalux"

# ── Controlli preliminari ────────────────────────────────────────────────────
if [[ "$EUID" -ne 0 ]]; then
    echo "[ERRORE] Esegui come root: sudo $0 $*"
    exit 1
fi

if ! command -v nginx &>/dev/null; then
    echo "[INFO] nginx non trovato — installazione in corso..."
    apt-get update -qq && apt-get install -y nginx
fi

# ── Certificato ─────────────────────────────────────────────────────────────
mkdir -p "$SSL_DIR"

if [[ "$MODE" == "internet" ]]; then
    # Let's Encrypt via certbot
    if ! command -v certbot &>/dev/null; then
        echo "[INFO] certbot non trovato — installazione in corso..."
        apt-get install -y certbot python3-certbot-nginx
    fi
    echo "[INFO] Richiesta certificato Let's Encrypt per $DOMAIN..."
    certbot certonly --nginx -d "$DOMAIN" --non-interactive --agree-tos \
        --email "admin@${DOMAIN}" --redirect
    CERT_PATH="/etc/letsencrypt/live/$DOMAIN/fullchain.pem"
    KEY_PATH="/etc/letsencrypt/live/$DOMAIN/privkey.pem"
else
    # Self-signed per LAN (valido 10 anni)
    echo "[INFO] Generazione certificato self-signed per $DOMAIN..."
    openssl req -x509 -nodes -days 3650 -newkey rsa:2048 \
        -keyout "$SSL_DIR/privkey.pem" \
        -out    "$SSL_DIR/fullchain.pem" \
        -subj   "/CN=$DOMAIN/O=Prismalux/C=IT" \
        -addext "subjectAltName=DNS:$DOMAIN,DNS:localhost,IP:127.0.0.1" 2>/dev/null
    chmod 600 "$SSL_DIR/privkey.pem"
    CERT_PATH="$SSL_DIR/fullchain.pem"
    KEY_PATH="$SSL_DIR/privkey.pem"
    echo "[INFO] Per far accettare il certificato al browser:"
    echo "       Importa $CERT_PATH nelle CA attendibili del sistema o del browser."
fi

# ── Configurazione nginx ─────────────────────────────────────────────────────
cat > "$NGINX_CONF" << NGINXEOF
# Prismalux — reverse proxy TLS
# Generato da setup_tls.sh il $(date '+%Y-%m-%d %H:%M')

server {
    listen 80;
    server_name $DOMAIN;
    # Redirect HTTP → HTTPS
    return 301 https://\$host\$request_uri;
}

server {
    listen 443 ssl;
    http2 on;
    server_name $DOMAIN;

    ssl_certificate     $CERT_PATH;
    ssl_certificate_key $KEY_PATH;

    # TLS 1.2 minimo; preferisce TLS 1.3
    ssl_protocols       TLSv1.2 TLSv1.3;
    ssl_ciphers         ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305;
    ssl_prefer_server_ciphers off;

    # HSTS (6 mesi — attiva solo dopo aver verificato che tutto funziona)
    # add_header Strict-Transport-Security "max-age=15552000" always;

    # Header di sicurezza aggiuntivi
    add_header X-Frame-Options          SAMEORIGIN always;
    add_header X-Content-Type-Options   nosniff    always;
    add_header Referrer-Policy          same-origin always;

    # Proxy verso Prismalux
    location / {
        proxy_pass         http://127.0.0.1:$PRISMALUX_PORT;
        proxy_http_version 1.1;
        proxy_set_header   Upgrade     \$http_upgrade;
        proxy_set_header   Connection  "upgrade";
        proxy_set_header   Host        \$host;
        proxy_set_header   X-Real-IP   \$remote_addr;
        proxy_set_header   X-Forwarded-For   \$proxy_add_x_forwarded_for;
        proxy_set_header   X-Forwarded-Proto \$scheme;
        # Timeout esteso per richieste LLM (streaming long-poll)
        proxy_read_timeout  300s;
        proxy_send_timeout  300s;
        # Buffer disabilitato per lo streaming token-by-token
        proxy_buffering    off;
    }
}
NGINXEOF

# ── Abilita sito e riavvia nginx ─────────────────────────────────────────────
ln -sf "$NGINX_CONF" /etc/nginx/sites-enabled/prismalux

# Rimuove il sito default se attivo (occupa la porta 80)
rm -f /etc/nginx/sites-enabled/default

echo "[INFO] Verifica configurazione nginx..."
nginx -t

echo "[INFO] Riavvio nginx..."
systemctl reload nginx || systemctl restart nginx

# ── Riepilogo ────────────────────────────────────────────────────────────────
echo ""
echo "╔══════════════════════════════════════════════════════════╗"
echo "║  Prismalux TLS configurato correttamente                ║"
echo "╠══════════════════════════════════════════════════════════╣"
echo "║  Modalità  : $MODE"
echo "║  Dominio   : $DOMAIN"
echo "║  Porta LAN : $PRISMALUX_PORT (localhost)"
echo "║  HTTPS     : https://$DOMAIN"
if [[ "$MODE" == "internet" ]]; then
echo "║  Rinnovo   : certbot renew (automatico via systemd timer)"
else
echo "║  Cert      : $CERT_PATH"
echo "║  Key       : $KEY_PATH"
echo "║  Scadenza  : 10 anni (self-signed)"
fi
echo "╚══════════════════════════════════════════════════════════╝"

if [[ "$MODE" == "lan" ]]; then
    echo ""
    echo "Per /etc/hosts (su ogni client che usa prismalux.local):"
    echo "  echo '$(hostname -I | awk '{print $1}')  $DOMAIN' | sudo tee -a /etc/hosts"
fi
