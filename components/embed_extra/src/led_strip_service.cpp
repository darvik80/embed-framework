#include "embed_extra/led_strip_service.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include <cstdlib>
#include <cstring>

static const char* TAG = "LedStrip";

#ifndef CONFIG_EMBED_LED_STRIP_MAX
#define CONFIG_EMBED_LED_STRIP_MAX 4
#endif

#ifndef CONFIG_EMBED_LED_STRIP_DEFAULT_BRIGHTNESS
#define CONFIG_EMBED_LED_STRIP_DEFAULT_BRIGHTNESS 32
#endif

namespace embed {

struct LedStripService::Strip {
    led_strip_handle_t handle     = nullptr;
    uint8_t*           pixels     = nullptr;
    uint16_t           count      = 0;
    uint8_t            brightness = 255;
    int                gpio       = -1;
};

struct LedStripService::Hw {
    SemaphoreHandle_t lock = nullptr;
    uint8_t           cap  = 0;
    Strip*            strips = nullptr;
};

LedStripService::~LedStripService() {
    stop();
}

void LedStripService::start() {
    if (hw_) {
        return;
    }
    auto* hw = static_cast<Hw*>(calloc(1, sizeof(Hw)));
    if (!hw) {
        ESP_LOGE(TAG, "Hw alloc failed");
        return;
    }
    hw->cap = static_cast<uint8_t>(CONFIG_EMBED_LED_STRIP_MAX);
    if (hw->cap < 1) {
        hw->cap = 1;
    }
    hw->strips = static_cast<Strip*>(calloc(hw->cap, sizeof(Strip)));
    hw->lock = xSemaphoreCreateMutex();
    if (!hw->strips || !hw->lock) {
        ESP_LOGE(TAG, "strip table alloc failed");
        if (hw->lock) {
            vSemaphoreDelete(hw->lock);
        }
        free(hw->strips);
        free(hw);
        return;
    }
    for (uint8_t i = 0; i < hw->cap; ++i) {
        hw->strips[i].gpio = -1;
    }
    hw_ = hw;
    ESP_LOGI(TAG, "ready (max %u strips, bind via attach(gpio, count))", hw->cap);
}

void LedStripService::stop() {
    if (!hw_) {
        return;
    }
    detachAll();
    if (hw_->lock) {
        vSemaphoreDelete(hw_->lock);
        hw_->lock = nullptr;
    }
    free(hw_->strips);
    hw_->strips = nullptr;
    free(hw_);
    hw_ = nullptr;
    ESP_LOGI(TAG, "stopped");
}

LedStripService::Strip* LedStripService::find(int gpio) {
    return const_cast<Strip*>(static_cast<const LedStripService*>(this)->find(gpio));
}

const LedStripService::Strip* LedStripService::find(int gpio) const {
    if (!hw_ || gpio < 0) {
        return nullptr;
    }
    for (uint8_t i = 0; i < hw_->cap; ++i) {
        if (hw_->strips[i].gpio == gpio && hw_->strips[i].handle) {
            return &hw_->strips[i];
        }
    }
    return nullptr;
}

LedStripService::Strip* LedStripService::allocSlot() {
    if (!hw_) {
        return nullptr;
    }
    for (uint8_t i = 0; i < hw_->cap; ++i) {
        if (hw_->strips[i].gpio < 0) {
            return &hw_->strips[i];
        }
    }
    return nullptr;
}

void LedStripService::destroyStrip(Strip& s) {
    if (s.handle) {
        led_strip_clear(s.handle);
        led_strip_refresh(s.handle);
        led_strip_del(s.handle);
        s.handle = nullptr;
    }
    free(s.pixels);
    s.pixels = nullptr;
    s.count = 0;
    s.brightness = 0;
    s.gpio = -1;
}

bool LedStripService::attach(int gpio, uint16_t count, uint8_t brightness) {
    if (!hw_ || gpio < 0 || count < 1) {
        return false;
    }
    if (brightness == 0) {
        brightness = static_cast<uint8_t>(CONFIG_EMBED_LED_STRIP_DEFAULT_BRIGHTNESS);
    }

    xSemaphoreTake(hw_->lock, portMAX_DELAY);

    if (Strip* existing = find(gpio)) {
        ESP_LOGI(TAG, "rebind gpio=%d (was count=%u)", gpio, existing->count);
        destroyStrip(*existing);
    }

    Strip* slot = allocSlot();
    if (!slot) {
        xSemaphoreGive(hw_->lock);
        ESP_LOGE(TAG, "no free slot (max %u) for gpio=%d", hw_->cap, gpio);
        return false;
    }

    slot->pixels = static_cast<uint8_t*>(calloc(static_cast<size_t>(count) * 3, 1));
    if (!slot->pixels) {
        xSemaphoreGive(hw_->lock);
        ESP_LOGE(TAG, "pixel alloc failed gpio=%d count=%u", gpio, count);
        return false;
    }

    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = gpio;
    strip_config.max_leds = count;
    strip_config.led_model = LED_MODEL_WS2812;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    strip_config.flags.invert_out = false;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt_config.resolution_hz = 10 * 1000 * 1000;
    rmt_config.mem_block_symbols = 64;
    rmt_config.flags.with_dma = false;

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &slot->handle);
    if (err != ESP_OK) {
        free(slot->pixels);
        slot->pixels = nullptr;
        xSemaphoreGive(hw_->lock);
        ESP_LOGE(TAG, "RMT init failed gpio=%d: %s", gpio, esp_err_to_name(err));
        return false;
    }

    slot->gpio = gpio;
    slot->count = count;
    slot->brightness = brightness;
    led_strip_clear(slot->handle);
    led_strip_refresh(slot->handle);
    const Strip snap = *slot;
    xSemaphoreGive(hw_->lock);

    emitChanged(snap, 0, 0, 0, false);
    ESP_LOGI(TAG, "attached gpio=%d count=%u brightness=%u", gpio, count, brightness);
    return true;
}

bool LedStripService::detach(int gpio) {
    if (!hw_) {
        return false;
    }
    xSemaphoreTake(hw_->lock, portMAX_DELAY);
    Strip* s = find(gpio);
    if (!s) {
        xSemaphoreGive(hw_->lock);
        return false;
    }
    ESP_LOGI(TAG, "detach gpio=%d", gpio);
    destroyStrip(*s);
    xSemaphoreGive(hw_->lock);
    return true;
}

void LedStripService::detachAll() {
    if (!hw_) {
        return;
    }
    xSemaphoreTake(hw_->lock, portMAX_DELAY);
    for (uint8_t i = 0; i < hw_->cap; ++i) {
        if (hw_->strips[i].gpio >= 0) {
            destroyStrip(hw_->strips[i]);
        }
    }
    xSemaphoreGive(hw_->lock);
}

bool LedStripService::attached(int gpio) const {
    if (!hw_) {
        return false;
    }
    xSemaphoreTake(hw_->lock, portMAX_DELAY);
    const bool ok = find(gpio) != nullptr;
    xSemaphoreGive(hw_->lock);
    return ok;
}

uint16_t LedStripService::ledCount(int gpio) const {
    if (!hw_) {
        return 0;
    }
    xSemaphoreTake(hw_->lock, portMAX_DELAY);
    const Strip* s = find(gpio);
    const uint16_t n = s ? s->count : 0;
    xSemaphoreGive(hw_->lock);
    return n;
}

uint8_t LedStripService::brightness(int gpio) const {
    if (!hw_) {
        return 0;
    }
    xSemaphoreTake(hw_->lock, portMAX_DELAY);
    const Strip* s = find(gpio);
    const uint8_t b = s ? s->brightness : 0;
    xSemaphoreGive(hw_->lock);
    return b;
}

uint8_t LedStripService::stripCount() const {
    if (!hw_) {
        return 0;
    }
    xSemaphoreTake(hw_->lock, portMAX_DELAY);
    uint8_t n = 0;
    for (uint8_t i = 0; i < hw_->cap; ++i) {
        if (hw_->strips[i].gpio >= 0) {
            ++n;
        }
    }
    xSemaphoreGive(hw_->lock);
    return n;
}

uint8_t LedStripService::list(LedStripInfo* out, uint8_t max) const {
    if (!hw_ || !out || max == 0) {
        return 0;
    }
    xSemaphoreTake(hw_->lock, portMAX_DELAY);
    uint8_t n = 0;
    for (uint8_t i = 0; i < hw_->cap && n < max; ++i) {
        const Strip& s = hw_->strips[i];
        if (s.gpio < 0) {
            continue;
        }
        out[n++] = LedStripInfo{s.gpio, s.count, s.brightness};
    }
    xSemaphoreGive(hw_->lock);
    return n;
}

bool LedStripService::setBrightness(int gpio, uint8_t brightness) {
    if (!hw_) {
        return false;
    }
    xSemaphoreTake(hw_->lock, portMAX_DELAY);
    Strip* s = find(gpio);
    if (!s) {
        xSemaphoreGive(hw_->lock);
        return false;
    }
    s->brightness = brightness;
    const bool ok = refresh(*s);
    const Strip snap = *s;
    xSemaphoreGive(hw_->lock);
    emitChanged(snap, 0, 0, 0, brightness > 0);
    return ok;
}

bool LedStripService::setPixel(int gpio, uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (!hw_) {
        return false;
    }
    xSemaphoreTake(hw_->lock, portMAX_DELAY);
    Strip* s = find(gpio);
    if (!s || index >= s->count) {
        xSemaphoreGive(hw_->lock);
        return false;
    }
    s->pixels[index * 3 + 0] = r;
    s->pixels[index * 3 + 1] = g;
    s->pixels[index * 3 + 2] = b;
    xSemaphoreGive(hw_->lock);
    return true;
}

bool LedStripService::setRange(int gpio, uint16_t offset, uint16_t length, uint8_t r, uint8_t g, uint8_t b) {
    if (!hw_ || length == 0) {
        return false;
    }
    xSemaphoreTake(hw_->lock, portMAX_DELAY);
    Strip* s = find(gpio);
    if (!s || static_cast<uint32_t>(offset) + length > s->count) {
        xSemaphoreGive(hw_->lock);
        return false;
    }
    for (uint16_t i = 0; i < length; ++i) {
        const uint16_t idx = static_cast<uint16_t>(offset + i);
        s->pixels[idx * 3 + 0] = r;
        s->pixels[idx * 3 + 1] = g;
        s->pixels[idx * 3 + 2] = b;
    }
    const bool ok = refresh(*s);
    const Strip snap = *s;
    xSemaphoreGive(hw_->lock);
    emitChanged(snap, r, g, b, r || g || b);
    return ok;
}

bool LedStripService::fill(int gpio, uint8_t r, uint8_t g, uint8_t b) {
    if (!hw_) {
        return false;
    }
    xSemaphoreTake(hw_->lock, portMAX_DELAY);
    Strip* s = find(gpio);
    if (!s) {
        xSemaphoreGive(hw_->lock);
        return false;
    }
    for (uint16_t i = 0; i < s->count; ++i) {
        s->pixels[i * 3 + 0] = r;
        s->pixels[i * 3 + 1] = g;
        s->pixels[i * 3 + 2] = b;
    }
    const bool ok = refresh(*s);
    const Strip snap = *s;
    xSemaphoreGive(hw_->lock);
    emitChanged(snap, r, g, b, r || g || b);
    return ok;
}

bool LedStripService::clear(int gpio) {
    return fill(gpio, 0, 0, 0);
}

bool LedStripService::refresh(Strip& s) {
    if (!s.handle || !s.pixels) {
        return false;
    }
    const uint16_t bri = s.brightness;
    for (uint16_t i = 0; i < s.count; ++i) {
        const uint8_t r = static_cast<uint8_t>((s.pixels[i * 3 + 0] * bri) / 255);
        const uint8_t g = static_cast<uint8_t>((s.pixels[i * 3 + 1] * bri) / 255);
        const uint8_t b = static_cast<uint8_t>((s.pixels[i * 3 + 2] * bri) / 255);
        led_strip_set_pixel(s.handle, i, r, g, b);
    }
    return led_strip_refresh(s.handle) == ESP_OK;
}

void LedStripService::emitChanged(const Strip& s, uint8_t r, uint8_t g, uint8_t b, bool on) {
    onChanged.emit({s.gpio, s.count, s.brightness, r, g, b, on});
}

} // namespace embed
