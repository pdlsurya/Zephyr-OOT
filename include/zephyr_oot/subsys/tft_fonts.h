#ifndef ZEPHYR_SUBSYS_TFT_FONTS_H_
#define ZEPHYR_SUBSYS_TFT_FONTS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FONT1 Arial_Narrow8x12
#define FONT2 Arial_Narrow10x13
#define FONT3 Arial_Narrow12x16
#define FONT4 Arial_Narrow15x19
#define FONT5 Proggy_Mono12x26
#define FONT6 Cute_Font16x21

typedef struct
{
    uint8_t width;
    uint8_t height;
    uint8_t offset;
    uint8_t bpl;
    const uint8_t *data;
} tft_font_t;

extern const uint8_t Arial_Narrow8x12[];
extern const uint8_t Arial_Narrow10x13[];
extern const uint8_t Arial_Narrow12x16[];
extern const uint8_t Arial_Narrow15x19[];
extern const uint8_t Proggy_Mono12x26[];
extern const uint8_t Cute_Font16x21[];

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SUBSYS_TFT_FONTS_H_ */
