#pragma once

#include "embed/embed.hpp"
#include <cstdint>

namespace embed {

/// Last applied LED-strip state (one strip, keyed by GPIO).
struct LedStripChanged {
    int      gpio;
    uint16_t count;
    uint8_t  brightness;
    uint8_t  r;
    uint8_t  g;
    uint8_t  b;
    bool     on;
};
static_assert(embed::Message<LedStripChanged>);

/// Snapshot for `list()` — trivially copyable, no heap.
struct LedStripInfo {
    int      gpio;
    uint16_t count;
    uint8_t  brightness;
};
static_assert(embed::Message<LedStripInfo>);

/// Multiple WS2812/SK6812 strips via RMT. Bind each strip to a GPIO at runtime.
///
/// Usage:
///   auto* leds = registry.createService<LedStripService>();
///   leds->attach(48, 16);
///   leds->attach(21, 8, 64);
///   leds->setRange(48, 0, 4, 255, 0, 0);
///   leds->detach(21);
///
/// Max concurrent strips: `CONFIG_EMBED_LED_STRIP_MAX` (RMT TX channels).
class LedStripService : public Service {
public:
    LedStripService() = default;
    ~LedStripService() override;

    const char* serviceName() const override { return "LedStripService"; }

    void start() override;
    void stop() override;

    /// Create or replace a strip on `gpio`. `count` ≥ 1. brightness 0–255 (0 = default).
    bool attach(int gpio, uint16_t count, uint8_t brightness = 0);
    bool detach(int gpio);
    void detachAll();

    [[nodiscard]] bool attached(int gpio) const;
    [[nodiscard]] uint16_t ledCount(int gpio) const;
    [[nodiscard]] uint8_t brightness(int gpio) const;
    [[nodiscard]] uint8_t stripCount() const;

    /// Write up to `max` attached strips into `out`. Returns number written.
    uint8_t list(LedStripInfo* out, uint8_t max) const;

    bool setBrightness(int gpio, uint8_t brightness);
    bool setPixel(int gpio, uint16_t index, uint8_t r, uint8_t g, uint8_t b);
    bool setRange(int gpio, uint16_t offset, uint16_t length, uint8_t r, uint8_t g, uint8_t b);
    bool fill(int gpio, uint8_t r, uint8_t g, uint8_t b);
    bool clear(int gpio);

    Signal<LedStripChanged> onChanged;

private:
    struct Hw;
    struct Strip;
    Hw* hw_ = nullptr;

    Strip* find(int gpio);
    const Strip* find(int gpio) const;
    Strip* allocSlot();
    void destroyStrip(Strip& s);
    bool refresh(Strip& s);
    void emitChanged(const Strip& s, uint8_t r, uint8_t g, uint8_t b, bool on);
};

} // namespace embed
