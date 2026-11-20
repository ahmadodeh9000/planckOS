/*
 * iris_avatar.c - Iris's face: a 48x48 portrait (eyes open and blinking)
 * and a 16x16 icon for the launcher and the taskbar.
 *
 * An original pixel-art drawing for planckOS. The format is made to be
 * edited by hand: every image is a list of rows, one string per row, and
 * every character in a row is one pixel. The character is an index into
 * the palette below ('h' = hair, 'g' = green eye, ...), and '.' is
 * transparent. To change a color, change the palette; to change the
 * drawing, change the letters (each row must keep its length).
 */
#include "apps/iris_avatar.h"
#include "gui/theme.h"

#define TRANSPARENT '.'

typedef struct {
    char key;
    uint32_t color;
} palette_entry_t;

static const palette_entry_t palette[] = {
    { 'o', RGB(0x4A, 0x30, 0x20) },     /* outline, dark brown */
    { 's', RGB(0xF8, 0xD5, 0xB8) },     /* skin */
    { 'S', RGB(0xEB, 0xB8, 0x98) },     /* skin in shadow */
    { 'h', RGB(0xF2, 0xC9, 0x4C) },     /* blonde hair */
    { 'H', RGB(0xD4, 0x9E, 0x2A) },     /* hair in shadow */
    { 'l', RGB(0xFB, 0xE7, 0x96) },     /* hair highlight */
    { 'g', RGB(0x3C, 0xA0, 0x55) },     /* green eyes */
    { 'G', RGB(0x1F, 0x5A, 0x30) },     /* dark green (pupils) */
    { 'w', RGB(0xFF, 0xFF, 0xFF) },     /* white (eye shine) */
    { 'm', RGB(0xC2, 0x4E, 0x4E) },     /* smile */
    { 'b', RGB(0xF5, 0xA8, 0xA0) },     /* blush */
    { 'e', RGB(0x7A, 0x4E, 0x30) },     /* eyebrows */
    { 'B', THEME_TITLE_ACTIVE },        /* top and hair clip: the theme blue */
    { 'L', THEME_SELECTION },           /* light blue collar */
};

#define PALETTE_SIZE ((int)(sizeof(palette) / sizeof(palette[0])))

/* Eyes open. */
static const char* const portrait_rows[] = {
    "................................................",
    "................................................",
    "................................................",
    "..................oooooooooooo..................",
    "................oohhhhhhhhhhhhoo................",
    "..............oohhhhhhhhhhhhhhhhoo..............",
    ".............ohhhhhhhhhhhhhhhhhhhho.............",
    "............ohhhllllhhhhhhhhhhhhhhho............",
    "...........ollllllllllllhhhhhhhhhhhho...........",
    "..........ohllllllllllllhhhhhhhhhhhhho..........",
    ".........ohhhhhhllllhhhhhhhhhhhhhhhhhho.........",
    ".........ohhhhhhhhhhhhhhhhhhhhBBBBBhhho.........",
    "........ohhhhhhhhhhhhhhhhhhhhhLLLLLhhhho........",
    "........ohhhhhhhhhhhhhhhhhhsshhBBBhhhhho........",
    ".......ohhhhhhhhhhhhhhhhhssssssshhhhhhhho.......",
    ".......ohhhhhhhhhhhhhhssssssssssshhhhhhho.......",
    ".......ohhhhhhhhhssshssssssssssssshhhhhho.......",
    "......ohhhhhhhsssssssssssssssssssshhhhhhho......",
    "......ohhhhhhhssseeeesssssseeeessshhhhhhho......",
    "......ohhhhhhSssssssssssssssssssssShhhhhho......",
    "......ohhhhhhSssooooossssssooooossShhhhhho......",
    "......ohhhhhhSsssgGwgsssssssgGwgssShhhhhho......",
    "......oHHHHhhSsssgGGgsssssssgGGgssShhHHHHo......",
    "......oHHHHhhSssswggwssssssswggwssShhHHHHo......",
    "......oHHHHhhSssssssssssssssssssssShhHHHHo......",
    "......oHHHHhhSssssssssssSsssssssssShhHHHHo......",
    "......oHHHHhhhSbbssssssSsssssssbbShhhHHHHo......",
    "......oHHHHhhhSssssssssssssssssssShhhHHHHo......",
    ".......oHHHhhhSsssssmssssssmsssssShhhHHHo.......",
    ".......oHHHhhhhSsssssmmmmmmsssssShhhhHHHo.......",
    ".......oHHHhhhhhSsssssmmmmsssssShhhhhHHHo.......",
    "........oHHhhhhhSSssssssssssssSShhhhhHHo........",
    "........oHHhhhhhhSSssssssssssSShhhhhhHHo........",
    ".........oHhhhhhhhhSSssssssSShhhhhhhhHo.........",
    ".........oHhhhhhhhhhhSSSSSShhhhhhhhhhHo.........",
    ".........oHhhhhhhhhhhSSSSSShhhhhhhhhhHo.........",
    ".........oHhhhhhhhhhhSSSSSShhhhhhhhhhHo.........",
    ".........oHhhhhhoohhhSSSSSShhhoohhhhhHo.........",
    ".........oHhhhhhBBBBBSSSSSSBBBBBhhhhhHo.........",
    "..........ohhhhBBBBBBSSSSSSBBBBBBhhhho..........",
    "........ooBhhhhBBBBBBLSSSSLBBBBBBhhhhBoo........",
    ".......oBBBhhhBBBBBBLLBBBBLLBBBBBBhhhBBBo.......",
    "......oBBBBhhhBBBBBBLBBBBBBLBBBBBBhhhBBBBo......",
    ".....oBBBBBhhBBBBBBLLBBBBBBLLBBBBBBhhBBBBBo.....",
    "....oBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBo....",
    "...oBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBo...",
    "...oBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBo...",
    "..oBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBo..",
};

/* Eyes closed: the same drawing with rows 20-23 changed. */
static const char* const blink_rows[] = {
    "................................................",
    "................................................",
    "................................................",
    "..................oooooooooooo..................",
    "................oohhhhhhhhhhhhoo................",
    "..............oohhhhhhhhhhhhhhhhoo..............",
    ".............ohhhhhhhhhhhhhhhhhhhho.............",
    "............ohhhllllhhhhhhhhhhhhhhho............",
    "...........ollllllllllllhhhhhhhhhhhho...........",
    "..........ohllllllllllllhhhhhhhhhhhhho..........",
    ".........ohhhhhhllllhhhhhhhhhhhhhhhhhho.........",
    ".........ohhhhhhhhhhhhhhhhhhhhBBBBBhhho.........",
    "........ohhhhhhhhhhhhhhhhhhhhhLLLLLhhhho........",
    "........ohhhhhhhhhhhhhhhhhhsshhBBBhhhhho........",
    ".......ohhhhhhhhhhhhhhhhhssssssshhhhhhhho.......",
    ".......ohhhhhhhhhhhhhhssssssssssshhhhhhho.......",
    ".......ohhhhhhhhhssshssssssssssssshhhhhho.......",
    "......ohhhhhhhsssssssssssssssssssshhhhhhho......",
    "......ohhhhhhhssseeeesssssseeeessshhhhhhho......",
    "......ohhhhhhSssssssssssssssssssssShhhhhho......",
    "......ohhhhhhSssssssssssssssssssssShhhhhho......",
    "......ohhhhhhSssssssssssssssssssssShhhhhho......",
    "......oHHHHhhSssosssossssssosssossShhHHHHo......",
    "......oHHHHhhSsssooossssssssooosssShhHHHHo......",
    "......oHHHHhhSssssssssssssssssssssShhHHHHo......",
    "......oHHHHhhSssssssssssSsssssssssShhHHHHo......",
    "......oHHHHhhhSbbssssssSsssssssbbShhhHHHHo......",
    "......oHHHHhhhSssssssssssssssssssShhhHHHHo......",
    ".......oHHHhhhSsssssmssssssmsssssShhhHHHo.......",
    ".......oHHHhhhhSsssssmmmmmmsssssShhhhHHHo.......",
    ".......oHHHhhhhhSsssssmmmmsssssShhhhhHHHo.......",
    "........oHHhhhhhSSssssssssssssSShhhhhHHo........",
    "........oHHhhhhhhSSssssssssssSShhhhhhHHo........",
    ".........oHhhhhhhhhSSssssssSShhhhhhhhHo.........",
    ".........oHhhhhhhhhhhSSSSSShhhhhhhhhhHo.........",
    ".........oHhhhhhhhhhhSSSSSShhhhhhhhhhHo.........",
    ".........oHhhhhhhhhhhSSSSSShhhhhhhhhhHo.........",
    ".........oHhhhhhoohhhSSSSSShhhoohhhhhHo.........",
    ".........oHhhhhhBBBBBSSSSSSBBBBBhhhhhHo.........",
    "..........ohhhhBBBBBBSSSSSSBBBBBBhhhho..........",
    "........ooBhhhhBBBBBBLSSSSLBBBBBBhhhhBoo........",
    ".......oBBBhhhBBBBBBLLBBBBLLBBBBBBhhhBBBo.......",
    "......oBBBBhhhBBBBBBLBBBBBBLBBBBBBhhhBBBBo......",
    ".....oBBBBBhhBBBBBBLLBBBBBBLLBBBBBBhhBBBBBo.....",
    "....oBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBo....",
    "...oBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBo...",
    "...oBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBo...",
    "..oBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBo..",
};

/* The 16x16 icon: just her face. */
static const char* const icon_rows[] = {
    "................",
    ".....oooooo.....",
    "...oohhllhhoo...",
    "..ohhllhhhhBBo..",
    "..ohhhhhhhhhho..",
    ".ohhhhhhsssshho.",
    ".ohhhssssssssho.",
    ".ohhsggssggshho.",
    ".ohhsGwssGwshho.",
    ".ohhbssssssbhho.",
    ".ohhssmssmsshho.",
    ".ohhsssmmssshho.",
    "..ohhSssssShho..",
    "..ohhhSSSShhho..",
    ".oBBBBLSSLBBBBo.",
    "oBBBBBBLLBBBBBBo",
};

const avatar_image_t iris_portrait = { IRIS_PORTRAIT_SIZE, IRIS_PORTRAIT_SIZE, portrait_rows };
const avatar_image_t iris_portrait_blink = { IRIS_PORTRAIT_SIZE, IRIS_PORTRAIT_SIZE, blink_rows };
const avatar_image_t iris_icon = { IRIS_ICON_SIZE, IRIS_ICON_SIZE, icon_rows };

static bool palette_color(char key, uint32_t* color) {
    for (int i = 0; i < PALETTE_SIZE; i++) {
        if (palette[i].key == key) {
            *color = palette[i].color;
            return true;
        }
    }
    return false;
}

void avatar_draw(surface_t* s, int x, int y, const avatar_image_t* image) {
    for (int row = 0; row < image->height; row++) {
        for (int col = 0; col < image->width; col++) {
            uint32_t color;
            char key = image->rows[row][col];
            if (key != TRANSPARENT && palette_color(key, &color)) {
                surface_put_pixel(s, x + col, y + row, color);
            }
        }
    }
}
