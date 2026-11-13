/*******************************************************************************
 * Size: 11 px
 * Bpp: 4
 * Opts: --bpp 4 --format lvgl --no-compress --lv-include lvgl.h --size 11 --font /Volumes/DATA/Zenix-ESP32/xiaozhi-esp32/managed_components/lvgl__lvgl/scripts/built_in_font/Montserrat-Medium.ttf --symbols 0123456789/ -o font_tiny_11.c --lv-font-name font_tiny_11
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef FONT_TINY_11
#define FONT_TINY_11 1
#endif

#if FONT_TINY_11

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+002F "/" */
    0x0, 0x0, 0x61, 0x0, 0x0, 0xe0, 0x0, 0x5,
    0x90, 0x0, 0xb, 0x30, 0x0, 0x1d, 0x0, 0x0,
    0x68, 0x0, 0x0, 0xc3, 0x0, 0x1, 0xd0, 0x0,
    0x7, 0x80, 0x0, 0xc, 0x20, 0x0, 0x2d, 0x0,
    0x0,

    /* U+0030 "0" */
    0x1, 0xbf, 0xd4, 0x0, 0xd8, 0x14, 0xe3, 0x4d,
    0x0, 0x8, 0x96, 0xa0, 0x0, 0x5c, 0x6a, 0x0,
    0x5, 0xc4, 0xd0, 0x0, 0x89, 0xd, 0x81, 0x4e,
    0x30, 0x2b, 0xfd, 0x50,

    /* U+0031 "1" */
    0xef, 0xe0, 0x2e, 0x2, 0xe0, 0x2e, 0x2, 0xe0,
    0x2e, 0x2, 0xe0, 0x2e,

    /* U+0032 "2" */
    0x2b, 0xfe, 0x90, 0x76, 0x11, 0xc7, 0x0, 0x0,
    0x89, 0x0, 0x0, 0xd4, 0x0, 0xc, 0x80, 0x0,
    0xc8, 0x0, 0xc, 0x80, 0x0, 0x9f, 0xff, 0xfe,

    /* U+0033 "3" */
    0x9f, 0xff, 0xf7, 0x0, 0x5, 0xd0, 0x0, 0x2e,
    0x20, 0x0, 0xcd, 0x50, 0x0, 0x25, 0xd7, 0x0,
    0x0, 0x5c, 0x84, 0x11, 0xb9, 0x5c, 0xff, 0xa0,

    /* U+0034 "4" */
    0x0, 0x1, 0xe3, 0x0, 0x0, 0xc, 0x60, 0x0,
    0x0, 0x8a, 0x0, 0x0, 0x4, 0xd0, 0x28, 0x0,
    0x1e, 0x30, 0x4c, 0x0, 0x8f, 0xff, 0xff, 0xf3,
    0x0, 0x0, 0x4c, 0x0, 0x0, 0x0, 0x4c, 0x0,

    /* U+0035 "5" */
    0xe, 0xff, 0xf7, 0xf, 0x0, 0x0, 0x1e, 0x0,
    0x0, 0x3f, 0xfe, 0xa1, 0x0, 0x1, 0xaa, 0x0,
    0x0, 0x3e, 0x76, 0x11, 0x9a, 0x3b, 0xef, 0xb1,

    /* U+0036 "6" */
    0x0, 0x9e, 0xfb, 0x0, 0xc9, 0x10, 0x20, 0x3d,
    0x0, 0x0, 0x6, 0xb9, 0xed, 0x60, 0x6f, 0x60,
    0x2e, 0x44, 0xf0, 0x0, 0xa7, 0xd, 0x50, 0x1e,
    0x30, 0x2c, 0xee, 0x60,

    /* U+0037 "7" */
    0xaf, 0xff, 0xff, 0x2a, 0x60, 0x5, 0xd0, 0x42,
    0x0, 0xc6, 0x0, 0x0, 0x3e, 0x0, 0x0, 0xa,
    0x80, 0x0, 0x1, 0xf1, 0x0, 0x0, 0x7a, 0x0,
    0x0, 0xe, 0x30, 0x0,

    /* U+0038 "8" */
    0x6, 0xde, 0xd7, 0x2, 0xf2, 0x1, 0xe3, 0x1e,
    0x20, 0x1e, 0x20, 0x8f, 0xef, 0x90, 0x4e, 0x20,
    0x2d, 0x57, 0xa0, 0x0, 0x88, 0x4e, 0x20, 0x1d,
    0x50, 0x6d, 0xee, 0x70,

    /* U+0039 "9" */
    0x8, 0xee, 0xa1, 0x7, 0xb0, 0x9, 0xa0, 0xa6,
    0x0, 0x2f, 0x17, 0xc1, 0x9, 0xf3, 0x8, 0xed,
    0x8e, 0x30, 0x0, 0x1, 0xf0, 0x2, 0x2, 0xb8,
    0x1, 0xdf, 0xe7, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 62, .box_w = 6, .box_h = 11, .ofs_x = -1, .ofs_y = -1},
    {.bitmap_index = 33, .adv_w = 117, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 61, .adv_w = 65, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 73, .adv_w = 101, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 97, .adv_w = 101, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 121, .adv_w = 118, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 153, .adv_w = 101, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 177, .adv_w = 109, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 205, .adv_w = 105, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 233, .adv_w = 113, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 261, .adv_w = 109, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 47, .range_length = 11, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Map glyph_ids to kern left classes*/
static const uint8_t kern_left_class_mapping[] =
{
    0, 1, 2, 0, 3, 4, 5, 6,
    7, 8, 0, 2
};

/*Map glyph_ids to kern right classes*/
static const uint8_t kern_right_class_mapping[] =
{
    0, 1, 2, 3, 4, 5, 6, 7,
    2, 8, 9, 10
};

/*Kern values between classes*/
static const int8_t kern_class_values[] =
{
    -12, -4, 5, 0, 0, -12, 0, 2,
    -4, 0, -4, 0, -1, -1, -2, 0,
    0, -1, 0, 0, 2, -1, 0, 0,
    0, -3, 0, -1, 0, 0, 0, 0,
    0, -1, -1, 0, -2, -2, 0, 0,
    5, 0, -4, -1, -2, 0, -1, -8,
    2, -1, 0, 0, 0, -1, -1, 0,
    -1, -2, 0, 0, 2, 0, 0, 0,
    0, 0, 0, -1, 0, 0, -7, -4,
    2, 0, -1, -11, -3, 0, -3, 0
};


/*Collect the kern class' data in one place*/
static const lv_font_fmt_txt_kern_classes_t kern_classes =
{
    .class_pair_values   = kern_class_values,
    .left_class_mapping  = kern_left_class_mapping,
    .right_class_mapping = kern_right_class_mapping,
    .left_class_cnt      = 8,
    .right_class_cnt     = 10,
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_classes,
    .kern_scale = 16,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 1,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t font_tiny_11 = {
#else
lv_font_t font_tiny_11 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 11,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if FONT_TINY_11*/

