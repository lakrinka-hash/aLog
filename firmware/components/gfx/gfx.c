/**
 * @file gfx.c
 * @brief Graphics engine implementation for software framebuffer management
 */

#include <stddef.h>         // for size_t
#include <string.h>         // for memset
#include "esp_log.h"        // for ESP_LOGE
#include "esp_heap_caps.h"  // for heap_caps_malloc
#include "gfx.h"

static const char *TAG = "gfx";

esp_err_t gfx_init(gfx_t *gfx, uint8_t width, uint8_t height, const font_mono_t *font)
{
    if (!gfx || !font || !font->data) return ESP_ERR_INVALID_ARG;

    size_t buffer_size = width * (height/8);
    uint8_t *buffer = (uint8_t*)heap_caps_malloc(buffer_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate frame buffer of %u bytes for gfx=%p",
                buffer_size,
                (void*)gfx);
        return ESP_ERR_NO_MEM;
    }
    memset(buffer, 0, buffer_size);
    gfx->width = width;
    gfx->height = height;
    gfx->buffer = buffer;
    gfx->font = font;
    ESP_LOGI(TAG, "New GFX engine (gfx=%p) initialized", (void*)gfx);
    ESP_LOGI(TAG, "gfx info ---> frame buffer (%ux%u) %u bytes allocated at %p",
            gfx->width,
            gfx->height,
            buffer_size,
            (void*)gfx->buffer);
    return ESP_OK;
}

void gfx_deinit(gfx_t *gfx)
{
    if (gfx->buffer) {
        heap_caps_free(gfx->buffer);
        gfx->buffer = NULL;
    }
    gfx->width = 0;
    gfx->height = 0;
    ESP_LOGI(TAG, "GFX engine (gfx=%p) deinitialized successfully",
            (void*)gfx);
}

esp_err_t gfx_set_font(gfx_t *gfx, const font_mono_t *font)
{
    if (!font || !font->data) return ESP_ERR_INVALID_ARG;
    gfx->font = font;
    return ESP_OK;
}

void gfx_clear(gfx_t *gfx, gfx_color_t color)
{
    size_t size = gfx->width * (gfx->height/8);
    memset (gfx->buffer, (color == WHITE) ? 0xFF : 0x00, size);
}

void gfx_draw_pixel(gfx_t *gfx, uint8_t x, uint8_t y, gfx_color_t color)
{
    if (x >= gfx->width || y >= gfx->height) return;

    size_t index = x + (y/8) * gfx->width;
    uint8_t bit = y%8;

    if (color == WHITE) gfx->buffer[index] |= (1<<bit);
    else gfx->buffer[index] &= ~(1<<bit);
}

void gfx_draw_char(gfx_t *gfx, uint8_t x, uint8_t y, char c, gfx_color_t color)
{
    if (c < gfx->font->first_char || c > gfx->font->last_char) c = '?';

    size_t char_index = c - gfx->font->first_char;
    const uint8_t *bitmap = &gfx->font->data[char_index * gfx->font->width];
    for (uint8_t col = 0; col < gfx->font->width; col++) {
        uint8_t glyph = bitmap[col];
        for (uint8_t row = 0; row < 8; row++) {
            if (glyph & (1<<row)) {
                gfx_draw_pixel(gfx, x+col, y+row, color);
            } else {
                gfx_draw_pixel(gfx, x+col, y+row, (color == WHITE) ? BLACK : WHITE);
            }
        }
    }
}

void gfx_draw_string(gfx_t *gfx, uint8_t x, uint8_t y, const char *str, gfx_color_t color)
{
    if(!str || !gfx->font) return;

    uint16_t pos = x; 
    uint8_t width = gfx->font->width;

    while (*str != '\0') {
        if (pos >= gfx->width) break;
        gfx_draw_char(gfx, pos, y, *str++, color);
        pos += width;
    }
    
}