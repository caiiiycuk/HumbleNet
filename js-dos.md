## Supervisor configuration

Create file in `/etc/supervisor/conf.d/humblenet.conf`:

```
[program:humblenet]
command=/home/caiiiycuk/peer-server --email caiiiycuk@gmail.com --common-name net.js-dos.com
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


