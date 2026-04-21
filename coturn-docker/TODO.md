# TODO

## Coturn / Firefox ICE failure

Context:
- Firefox reports: `WebRTC: ICE failed, your TURN server appears to be broken`.
- In `webrtclog.0` and `webrtclog.1`, TURN allocation first gets normal REST auth challenge `401`, then retry often fails with STUN error `486`.
- `486` means coturn rejects allocation because the allocation quota for the REST username is exhausted.
- Previous `coturn-docker/turnserver.conf.template` had `user-quota=32`.
- Publisher credentials used one shared REST username pattern, so many clients could hit the same per-user quota.

Changes prepared in working tree:
- `coturn-docker/turnserver.conf.template`: use `$TOTAL_QUOTA` and `$USER_QUOTA`.
- `coturn-docker/entrypoint.sh`: set defaults `TOTAL_QUOTA=600`, `USER_QUOTA=600`.
- `coturn-docker/entrypoint.sh`: publish REST username as `expires:publisher:issued_at`, not `expires:master`.
- `coturn-docker/.env.example`: document `TOTAL_QUOTA` and `USER_QUOTA`.
- `coturn-docker/README.md`: document the Firefox `486` failure mode and per-client REST username recommendation.

To apply on the running server:
1. Add these values to the real `coturn-docker/.env` if they are missing:

   ```bash
   TOTAL_QUOTA=600
   USER_QUOTA=600
   ```

2. Rebuild and recreate coturn:

   ```bash
   cd coturn-docker
   docker compose build coturn
   docker compose up -d --force-recreate coturn
   ```

3. Make sure the backend that serves ICE credentials does not cache one generated `username` / `credential` pair for all browsers. Prefer usernames like:

   ```text
   <expiry-timestamp>:<client-or-session-id>
   ```

   The HMAC credential must be generated from the full username string.

4. Verify after deploy:

   ```bash
   docker compose logs coturn --tail 100
   ```

   In Firefox `about:webrtc`, `401` may still appear as the normal auth challenge, but the retry should not fail with `486` under normal load.
