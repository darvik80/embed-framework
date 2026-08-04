# Crearts IoT — local stack (RabbitMQ + Node-RED)

## Quick start

```bash
cd deploy
cp .env.example .env   # optional
docker compose up -d
# or: podman compose up -d
```

| Service | URL / port |
|---------|------------|
| RabbitMQ Management | http://localhost:15672 (`crearts` / `crearts`) |
| MQTT | `mqtt://localhost:1883` |
| MQTT WebSocket | `ws://localhost:15675/ws` |
| AMQP | `amqp://localhost:5672` |
| Node-RED editor | http://localhost:1880 |

Same Docker/Podman network: Node-RED reaches the broker as hostname **`rabbitmq`** (port 1883).

### Podman on Windows (WSL machine) — broken port publish

**Symptom:** `podman ps` shows `0.0.0.0:1883->1883`, but from Windows
`Test-NetConnection 127.0.0.1 -Port 1883` fails / connection refused.
ESP to `192.168.x.x:1883` also fails. Platform `:8080` may still work.

**Cause:** rootless Podman + WSL `gvproxy` often fails to publish container ports
onto the Windows host. Inside the VM the broker is fine (`10.89.0.x:1883`).

**Workaround (required while on Podman/WSL mirrored):** after stack is up, run in
**elevated Admin PowerShell** (non-admin cannot open Hyper-V firewall — LAN stays closed):

```powershell
cd deploy
.\scripts\fix-podman-ports.ps1
```

This opens **TCP 1883 only**:
1. Windows firewall inbound allow
2. **Hyper-V firewall** inbound allow for WSL (mirrored)
3. **`netsh portproxy`**: `0.0.0.0:1883 → 127.0.0.1:1883` so the PC LAN IP accepts connections (localhost MQTT already works; hairpin to own Wi‑Fi IP does not)
4. Classic NAT (non-mirrored): WSL relay + portproxy to the `172.x` eth IP

Then check:

```powershell
Test-NetConnection 127.0.0.1 -Port 1883
Test-NetConnection 192.168.1.22 -Port 1883
```

Both should be **OK** after portproxy. Then point ESP at `192.168.1.22:1883`.

Remove later:

```powershell
.\scripts\fix-podman-ports.ps1 -Remove
```

**Longer-term options**

1. Enable user-mode networking (recreates machine networking):
   ```powershell
   podman machine stop
   podman machine set --user-mode-networking
   podman machine start
   cd deploy; podman compose up -d
   .\scripts\fix-podman-ports.ps1   # still use if publish stays broken
   ```
2. Use Docker Desktop instead of Podman for this stack (port publish is usually reliable).
3. Keep `rabbitmq.conf` listeners on `0.0.0.0` (already set) — IPv6-only `[::]:1883` makes publish worse.

ESP firmware broker host = **Windows LAN IPv4** (e.g. `192.168.1.22`), never `localhost` / WSL `172.x`.

### Synology Container Manager — `prelaunch` / `badarg`

RabbitMQ dies early with `failed_to_start_child,prelaunch,badarg` when:

1. **`enabled_plugins` / `rabbitmq.conf` have Windows CRLF** — copy files with LF, or paste from this repo after pull
2. **Bind mount became a directory** — Synology sometimes creates a folder named `rabbitmq.conf` if the file path was missing; delete it and remount the **file**
3. **Stale data volume** after hostname/nodename change — remove container + volume and start clean
4. **Volume permissions** — data dir must be writable by container UID (usually `999`)

Clean restart on Synology:

```bash
docker compose down
docker volume rm deploy_rabbitmq_data   # name may differ; check `docker volume ls`
docker compose up -d
docker logs -f crearts-rabbitmq
```

Confirm mounts are files:

```bash
docker exec crearts-rabbitmq ls -la /etc/rabbitmq/
# enabled_plugins and rabbitmq.conf must be files, not directories
```

## Layout

```
deploy/
  docker-compose.yml
  .env.example
  rabbitmq/
    enabled_plugins
    rabbitmq.conf
  nodered/
    settings.js
```

## Device MQTT (production model)

Devices do **not** use the admin user. After dashboard registration:

```
broker:     mqtt://<host>:1883
client_id:  {product_id}.{device_id}
username:   {product_id}.{device_id}
password:   <access_token>
```

Firmware: `CreartsCredentials::createAccessToken(...)`.

### Lab: create device MQTT user on RabbitMQ

Until the platform provisions users automatically, add the device user manually
(Management UI → Admin → Users, or CLI). Username is `product.device`, password is the token:

```bash
docker exec crearts-rabbitmq rabbitmqctl add_user \
  'home.esp32-s3' \
  'HZhO8crzK29Ah1p_3XjI5c0SNuk-E3xoUzyUQDWLsOM'
docker exec crearts-rabbitmq rabbitmqctl set_permissions -p / \
  'home.esp32-s3' '.*' '.*' '.*'
```

Without this user, MQTT CONNECT is rejected (`bad username or password` / `0x4`).

Quick broker check from the PC: `mqtt://localhost:1883` with user/pass `crearts`/`crearts`.
Device firmware must use the **LAN IP** of the PC (not `localhost`).

## Node-RED / rules bus

Node-RED must not use device tokens. Wire MQTT nodes to:

- Broker: `rabbitmq` (in-compose) or `localhost` from the host
- Events (subscribe): `platform/v1/events/#`
- Commands (publish): `platform/v1/commands/...`

Create a RabbitMQ user e.g. `nodered` with ACL on those topics (platform can provision this later). Lab-only: admin user.

Example flows: temperature threshold → RPC; device offline → webhook.

See `docs/iot-platform-service-prompt.md` (Rule engine / CEP).

## Plugins (RabbitMQ)

- `rabbitmq_management`
- `rabbitmq_mqtt`
- `rabbitmq_web_mqtt`

## Production notes

- Change `RABBITMQ_*` and `NODERED_CREDENTIAL_SECRET`
- Enable Node-RED `adminAuth` in `nodered/settings.js`
- Prefer MQTT TLS; per-device token users with scoped ACL
