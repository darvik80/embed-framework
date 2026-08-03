# Crearts IoT — local stack (RabbitMQ + Node-RED)

## Quick start

```bash
cd deploy
cp .env.example .env   # optional
docker compose up -d
```

| Service | URL / port |
|---------|------------|
| RabbitMQ Management | http://localhost:15672 (`crearts` / `crearts`) |
| MQTT | `mqtt://localhost:1883` |
| MQTT WebSocket | `ws://localhost:15675/ws` |
| AMQP | `amqp://localhost:5672` |
| Node-RED editor | http://localhost:1880 |

Same Docker network: Node-RED reaches the broker as hostname **`rabbitmq`** (port 1883).

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
username:   <access_token>
password:   <access_token>
```

Firmware: `CreartsCredentials::createAccessToken(...)`.

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
