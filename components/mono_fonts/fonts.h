/**
 * font.h
 */

#ifndef FONTS_H_
#define FONTS_H_

#include <stdint.h>

/**
 * @struct font_mono_t
 * @brief Структура дескриптора моноширинного шрифта
 */
typedef struct {
    const uint8_t *data;    ///< Указатель на массив шрифта во flash
    uint8_t width;          ///< Ширина символа в пикселях
    uint8_t height;         ///< Высота символа в пикселях
    uint8_t first_char;     ///< Минимальный ASCII-код в таблице шрифта
    uint8_t last_char;      ///< Максимальный ASCII-код в таблице шрифта
} font_mono_t;

extern const font_mono_t font8x8uk;

#endif /* FONTS_H_ */