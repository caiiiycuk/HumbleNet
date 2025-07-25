## Installint STUN/TURN

```
sudo apt update
sudo apt install coturn
```

Configure /etc/turnserver.conf

```
listening-port=3478
tls-listening-port=5349

cert=/etc/turn/fullchain.pem
pkey=/etc/turn/privkey.pem
fingerprint
cipher-list="ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-SHA256"
no-stdout-log
no-tlsv1
no-tlsv1_1
lt-cred-mech

realm=cloud.js-dos.com
user=cloud:
external-ip=158.160.59.144
proto=udp
proto=tcp
log-file=/var/log/turnserver.log
simple-log
```

Configure /etc/letsencrypt/renewal-hooks/deploy/coturn-cert-sync.sh

```
#!/bin/bash

DOMAIN="cloud-turn-rsa"
SRC="/etc/letsencrypt/live/$DOMAIN"
DST="/etc/turn"

sudo mkdir -p "$DST"

sudo cp "$SRC/fullchain.pem" "$DST/fullchain.pem"
sudo cp "$SRC/privkey.pem" "$DST/privkey.pem"

sudo chown turnserver:turnserver "$DST/fullchain.pem" "$DST/privkey.pem"
sudo chmod 600 "$DST/fullchain.pem" "$DST/privkey.pem"

sudo systemctl restart coturn
```

## Supervisor configuration

Create file in `/etc/supervisor/conf.d/humblenet.conf`:

```
[program:humblenet]
command=/home/caiiiycuk/peer-server --email caiiiycuk@gmail.com --common-name net.js-dos.com --TURN-server auto --TURN-username cloud --TURN-password ...
autostart=true
autorestart=true
stderr_logfile=/var/log/humblenet.err.log
stdout_logfile=/var/log/humblenet.out.log
```

Update supervisor and start:

```
sudo supervisorctl reread
sudo supervisorctl update
```

Restart script:

```
#!/bin/bash


set -ex
supervisorctl stop humblenet
mv ./peer-server.new ./peer-server
supervisorctl start humblenet
supervisorctl status
tail -f /var/log/humblenet.*
```

## Way to test ws (curl)

```
curl --include --no-buffer \
    --header "Connection: Upgrade" \
    --header "Upgrade: websocket" \
    --header "Host: localhost:8080" \
    --header "Origin: http://localhost:8080" \
    --header "Sec-WebSocket-Key: SGVsbG8sIHdvcmxkIQ==" \
    --header "Sec-WebSocket-Version: 13" \
    http://localhost:8080
```