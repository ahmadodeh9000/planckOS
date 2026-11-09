/*
 * logo.c - the planckOS logo: an infinity sign wearing a fedora.
 *
 * Each row is one line of ASCII art plus the color it is drawn in, so the
 * picture is easy to see and edit right here. The rows are already
 * centered on each other; logo_print() only adds the left margin that
 * centers the whole logo on an 80-column screen.
 */
#include "shell/logo.h"

#define CONSOLE_COLUMNS 80
#define LOGO_WIDTH      50
#define LOGO_INDENT     ((CONSOLE_COLUMNS - LOGO_WIDTH) / 2)

typedef struct {
    const char* text;
    uint8_t color;
} logo_row;

static const logo_row logo[] = {
    /* the fedora: creased crown, red band, brim and its shadow */
    { "                  :-=+*#####*+=-:",                  CONSOLE_LIGHT_GREY },
    { "             :=*#*=-:  :=#=:  :-=*#*=:",             CONSOLE_LIGHT_GREY },
    { "           :*%+:       -%@%-       :+%*:",           CONSOLE_LIGHT_GREY },
    { "          =@+           :*:           +@=",          CONSOLE_LIGHT_GREY },
    { "          #@###########################@#",          CONSOLE_RED },
    { "  :-=+*##%@@###########################@@%##*+=-:",  CONSOLE_LIGHT_GREY },
    { " =%@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%=", CONSOLE_LIGHT_GREY },
    { "       :--==+++****#############****+++==--:",       CONSOLE_DARK_GREY },
    /* the infinity sign, brightest where the loops cross */
    { "      :-=+++++=-:              :-==++++==-:",        CONSOLE_BLUE },
    { "    :=**=--:::-=+**=:        :=**+=-::::-=+*+-",     CONSOLE_LIGHT_BLUE },
    { "  :+%=:  :-===-:  :+#+-:   -+#+-  :-====-  :-#*:",   CONSOLE_LIGHT_BLUE },
    { " :*#: :=#*=--==+*+-  =*- -*#=  -+**==--=+#+: :+%:",  CONSOLE_LIGHT_CYAN },
    { " +%: :#*:       :=*#-  -*#-  =#*=:       :*#:  #*",  CONSOLE_LIGHT_CYAN },
    { ":@=  =@:           ---*#=  =#+:           :@+  =@:", CONSOLE_WHITE },
    { ":@=  +@:           -+#=  -#*---           :@=  =@:", CONSOLE_WHITE },
    { " *#  :#*:       :=*#=  -#*-  -#*=:       :#*: :%+",  CONSOLE_LIGHT_CYAN },
    { " :%+: :+#+=--==**+-  =#*- -*=  -+*+==--=*#=  :#*:",  CONSOLE_LIGHT_CYAN },
    { "  :*#-:  --===-:  -+#+-   :-+#+:  :-===-:  :+%+:",   CONSOLE_LIGHT_BLUE },
    { "    -+*+=-::::-=+**=:        :=**+=-:::--=**=:",     CONSOLE_LIGHT_BLUE },
    { "      :-==++++==-:              :-=+++++=-:",        CONSOLE_BLUE },
};

#define LOGO_ROWS (sizeof(logo) / sizeof(logo[0]))

void logo_print(console_t* con) {
    for (uint32_t row = 0; row < LOGO_ROWS; row++) {
        for (int i = 0; i < LOGO_INDENT; i++) {
            console_putc(con, ' ');
        }
        console_print_color(con, logo[row].text, logo[row].color);
        console_putc(con, '\n');
    }
}
