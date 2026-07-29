# embed

Core framework component: services, registry, event loop, Signal/Slot, fixed-size types, CRTP state machine.

## Public headers

Include `embed/embed.hpp` or specific headers under `include/embed/`.

| Header | Role |
|--------|------|
| `service.hpp` | `Service` base lifecycle |
| `registry.hpp` | `ServiceRegistry` pool |
| `event_loop.hpp` | Dedicated `esp_event` loop |
| `signal.hpp` / `slot.hpp` | Typed pub/sub |
| `message.hpp` | `Message` concept |
| `string.hpp` / `types.hpp` | Trivially-copyable containers |
| `state_machine.hpp` | CRTP SM helpers |
| `config.hpp` | Compile-time limits |

## Rules of thumb

- Messages must be trivially copyable and fit `EMBED_MAX_EVENT_DATA_SIZE`.
- `EventLoop::post` waits at most `EMBED_EVENT_POST_TIMEOUT_MS` then returns an error; `Signal::emit` logs and drops.
- Connection pool is thread-safe when `EMBED_THREAD_SAFE=1`.

## Tests

See [docs/testing.md](../../docs/testing.md) and `test/` / `test_apps/embed_unity`.
