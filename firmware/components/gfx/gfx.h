/**
 * @file gfx.h
 * @brief high-level graphocs layer for monochromatic displays.
 */

#ifndef GFX_H
#define GFX_H

#include <stdint.h>   // for uint8_t
#include "esp_err.h"  // esp_err_t
#include "fonts.h"

/**
 * @brief Monochrome color enumeration.
 */
typedef enum {
    BLACK = 0,    ///< Pixel off (dark)
    WHITE = 1     ///< Pixel on (illuminated)
} gfx_color_t;

/**
 * @struct gfx_t
 * @brief Graphics context configuration structure.
 * Holds the pointer to the dynamically allocated framebuffer and current layout settings.
 */
typedef struct {
    uint8_t *buffer;            ///< Pointer to the framebuffer in RAM
    uint8_t width;              ///< Display width in pixels
    uint8_t height;             ///< Display height in pixels
    const font_mono_t *font;    ///< Currently selected font context
} gfx_t;

/**
 * @brief Initialize the graphics context and allocate memory for the framebuffer.
 *
 * @param[out]   gfx  Pointer to the graphics context structure
 * @param[in]  width  Width of the virtual screen area in pixels.
 * @param[in] height  Height of the virtual screen area in pixels.
 * @param[in]   font  Pointer to the default monochrome font structure.
 *
 * @return
 * - ESP_OK: Success, framebuffer allocated
 * - ESP_ERR_INVALID_ARG: Null pointer provided.
 * - ESP_ERR_NO_MEM: Failed to allocate framebuffer memory.
 */
esp_err_t gfx_init(gfx_t *gfx, uint8_t width, uint8_t height, const font_mono_t *font);

/**
 * @brief Free allocated framebuffer memory and reset the graphics context.
 * @param gfx Pointer to the graphics context structure.
 */
void gfx_deinit(gfx_t *gfx);

/**
 * @brief Change the current font used for drawing characters and strings.
 *
 * @param[in,out] gfx  Pointer to the graphics context structure.
 * @param[in]    font  Pointer to the new monochrome font structure.
 *
 * @return
 * - ESP_OK on success
 * - ESP_ERR_INVALID_ARG if font is invalid.
 */
esp_err_t gfx_set_font(gfx_t *gfx, const font_mono_t *font);

/**
 * @brief Fill the entire framebuffer with a specified color.
 *
 * @param[in,out] gfx  Pointer to the graphics context structure.
 * @param[in]   color  Color to fill the buffer with (BLACK or WHITE)
 */
void gfx_clear(gfx_t *gfx, gfx_color_t color);

/**
 * @brief Draw a single pixel in the framebuffer at specified coordinates.
 *
 * @param[in,out] gfx  Pointer to the graphics context structure.
 * @param[in]       x  Horizontal coordinate (0 to width-1).
 * @param[in]       y  Vertical coordinate (0 to height-1).
 * @param[in]   color  Color of the pixel (BLACK or WHITE).
 */
void gfx_draw_pixel(gfx_t *gfx, uint8_t x, uint8_t y, gfx_color_t color);

/**
 * @brief brief Draw a single character at the specified position.
 *
 * @param[in,out] gfx  Pointer to the graphics context structure.
 * @param[in]       x  Horizontal start coordinate for the character.
 * @param[in]       y  Vertical start coordinate for the character.
 * @param[in]       c  ASCII character to render.
 * @param[in]   color  Drawing color (BLACK or WHITE).
 */
void gfx_draw_char(gfx_t *gfx, uint8_t x, uint8_t y, char c, gfx_color_t color);

/**
 * @brief Draw a null-terminated string at the specified position.
 *
 * @param [in,out] gfx  Pointer to the graphics context structure.
 * @param[in]        x  Horizontal start coordinate for the text.
 * @param[in]        y  Vertical start coordinate for the text.
 * @param[in]      str  Pointer to the C-string to draw.
 * @param[in]    color  Drawing color (BLACK or WHITE).
 */
void gfx_draw_string(gfx_t *gfx, uint8_t x, uint8_t y, const char *str, gfx_color_t color);

#endif /* GFX_H */