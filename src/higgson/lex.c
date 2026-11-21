/*
 * lex.c - the Higgson lexer (see lex.h).
 *
 * hg_get_token skips spaces, newlines and comments, then looks at the
 * first character to decide what kind of token comes next: an operator or
 * punctuation, a string, a number, or a word (a keyword or a name).
 */
#include "higgson/lex.h"
#include "higgson/hg.h"
#include "higgson/util.h"

#define FIRST_PRINTABLE ' '
#define LAST_PRINTABLE  '~'

void hg_lexer_init(hg_lexer_t* hg_lex, const char* src, int length) {
    hg_lex->src        = src;
    hg_lex->length     = length;
    hg_lex->curr_char  = ' ';
    hg_lex->curr_pos   = -1;
    hg_lex->line       = 1;

    // loading the first char
    hg_lexer_next_char(hg_lex);
}

void hg_lexer_next_char(hg_lexer_t* hg_lex) {
    if (hg_lex->curr_char == '\n') {
        hg_lex->line++;     /* leaving a line behind */
    }
    ++hg_lex->curr_pos;

    if (hg_lex->curr_pos > hg_lex->length) {
        hg_lex->curr_char = '\0'; // end of file !
    }

    else {
        hg_lex->curr_char = hg_lex->src[hg_lex->curr_pos];
    }

}
char hg_lexer_peek(hg_lexer_t* hg_lex) {
    if (hg_lex->curr_pos + 1 >= hg_lex->length) {
        return '\0';
    }

    return hg_lex->src[hg_lex->curr_pos + 1];
}


// tokens
static bool hg_is_white_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/* Skips a // comment or a slash-star comment, if one starts here.
   Returns true if it did. */
static bool hg_skip_comment(hg_lexer_t* lexer) {
    if (lexer->curr_char == '/' && hg_lexer_peek(lexer) == '/') {
        while (lexer->curr_char != '\n' && lexer->curr_char != '\0')
            hg_lexer_next_char(lexer);
        return true;
    }
    if (lexer->curr_char == '/' && hg_lexer_peek(lexer) == '*') {
        int start_line = lexer->line;
        hg_lexer_next_char(lexer);
        hg_lexer_next_char(lexer);
        while (!(lexer->curr_char == '*' && hg_lexer_peek(lexer) == '/')) {
            if (lexer->curr_char == '\0') {
                hg_error(start_line, "this /* comment is never closed with */");
            }
            hg_lexer_next_char(lexer);
        }
        hg_lexer_next_char(lexer);      /* the '*'; the '/' goes below */
        hg_lexer_next_char(lexer);
        return true;
    }
    return false;
}

/* Spaces, newlines and comments, in any order. */
static void hg_skip_white_space(hg_lexer_t* lexer) {
    do {
        while (hg_is_white_space(lexer->curr_char))
            hg_lexer_next_char(lexer);
    } while (hg_skip_comment(lexer));
}

static char* hg_substr(const char* src, int start, int end) {
    char* text = hg_alloc(end - start + 1);
    for (int i = start; i < end; ++i) {
        text[i - start] = src[i];
    }
    text[end - start] = '\0';
    return text;
}

static hg_token_type_t hg_check_keyword(const char* text) {
    if (hg_util_strcmp(text, "let") == 0)       return TOK_LET;
    if (hg_util_strcmp(text, "if") == 0)        return TOK_IF;
    if (hg_util_strcmp(text, "else") == 0)      return TOK_ELSE;
    if (hg_util_strcmp(text, "while") == 0)     return TOK_WHILE;
    if (hg_util_strcmp(text, "print") == 0)     return TOK_PRINT;
    if (hg_util_strcmp(text, "func") == 0)      return TOK_FUNC;
    if (hg_util_strcmp(text, "return") == 0)    return TOK_RETURN;

    return TOK_IDENT;
}

/* An operator that may be followed by a second character: "<" or "<=". */
static void hg_one_or_two(hg_lexer_t* lexer, hg_token_t* token, char second,
                          const char* one, hg_token_type_t one_type,
                          const char* two, hg_token_type_t two_type) {
    if (hg_lexer_peek(lexer) == second) {
        hg_lexer_next_char(lexer);
        token->text = two;
        token->type = two_type;
    }
    else {
        token->text = one;
        token->type = one_type;
    }
}

/* An operator that is always two characters: "&&" and "||". */
static void hg_two(hg_lexer_t* lexer, hg_token_t* token, const char* text, hg_token_type_t type) {
    hg_lexer_next_char(lexer);
    token->text = text;
    token->type = type;
}

/* Operators and punctuation. False if curr_char isn't one. */
static bool hg_operator_token(hg_lexer_t* lexer, hg_token_t* token) {
    static const struct { char c; const char* text; hg_token_type_t type; } singles[] = {
        { '+', "+", TOK_PLUS },    { '-', "-", TOK_MINUS },     { '*', "*", TOK_STAR },
        { '/', "/", TOK_SLASH },   { '%', "%", TOK_PERCENT },   { '(', "(", TOK_LPAREN },
        { ')', ")", TOK_RPAREN },  { '{', "{", TOK_LBRACE },    { '}', "}", TOK_RBRACE },
        { ',', ",", TOK_COMMA },   { ';', ";", TOK_SEMICOLON },
    };
    char c = lexer->curr_char;

    for (uint32_t i = 0; i < sizeof(singles) / sizeof(singles[0]); i++) {
        if (singles[i].c == c) {
            token->text = singles[i].text;
            token->type = singles[i].type;
            return true;
        }
    }
    if (c == '=')       hg_one_or_two(lexer, token, '=', "=", TOK_EQ, "==", TOK_EQEQ);
    else if (c == '>')  hg_one_or_two(lexer, token, '=', ">", TOK_GT, ">=", TOK_GTE);
    else if (c == '<')  hg_one_or_two(lexer, token, '=', "<", TOK_LT, "<=", TOK_LTE);
    else if (c == '!')  hg_one_or_two(lexer, token, '=', "!", TOK_NOT, "!=", TOK_NOTE);
    else if (c == '&' && hg_lexer_peek(lexer) == '&')  hg_two(lexer, token, "&&", TOK_AND);
    else if (c == '|' && hg_lexer_peek(lexer) == '|')  hg_two(lexer, token, "||", TOK_OR);
    else return false;
    return true;
}

/* "text": printable characters only, on one line. There are no escape
   codes, so a backslash is refused rather than silently kept. */
static void hg_string_token(hg_lexer_t* lexer, hg_token_t* token) {
    hg_lexer_next_char(lexer);
    int start = lexer->curr_pos;

    while (lexer->curr_char != '\"') {
        char c = lexer->curr_char;
        if (c == '\n' || c == '\0') {
            hg_error(token->line, "this string is never closed with \"");
        }
        if (c == '\\') {
            hg_error(lexer->line, "strings can't contain '\\' (there are no escape codes)");
        }
        if (c < FIRST_PRINTABLE || c > LAST_PRINTABLE) {
            hg_error(lexer->line, "strings can only contain printable characters");
        }
        hg_lexer_next_char(lexer);
    }
    if (lexer->curr_pos - start > HG_STRING_MAX) {
        hg_error(token->line, "string too long (at most %d characters)", HG_STRING_MAX);
    }

    token->text = hg_substr(lexer->src, start, lexer->curr_pos);
    token->type = TOK_STRING;
}

static bool hg_is_hex_digit(char c) {
    return hg_util_is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* 123 or 0xFF8800. The parser works out the value and checks its size. */
static void hg_number_token(hg_lexer_t* lexer, hg_token_t* token) {
    int start = lexer->curr_pos;
    char next = hg_lexer_peek(lexer);

    if (lexer->curr_char == '0' && (next == 'x' || next == 'X')) {
        hg_lexer_next_char(lexer);
        if (!hg_is_hex_digit(hg_lexer_peek(lexer)))
            hg_error(lexer->line, "expected hex digits after '0x'");
        while (hg_is_hex_digit(hg_lexer_peek(lexer)))
            hg_lexer_next_char(lexer);
    }
    else {
        while (hg_util_is_digit(hg_lexer_peek(lexer)))
            hg_lexer_next_char(lexer);
        if (hg_lexer_peek(lexer) == '.')
            hg_error(lexer->line, "Higgson has only whole numbers (no decimal point)");
    }
    if (hg_util_is_alpha(hg_lexer_peek(lexer)) || hg_lexer_peek(lexer) == '_')
        hg_error(lexer->line, "a name can't start with a digit");

    token->text = hg_substr(lexer->src, start, lexer->curr_pos + 1);
    token->type = TOK_NUMBER;
}

static bool hg_is_name_char(char c) {
    return hg_util_is_alnum(c) || c == '_';
}

/* A keyword, or a name: a letter or '_', then letters, digits and '_'. */
static void hg_word_token(hg_lexer_t* lexer, hg_token_t* token) {
    int start = lexer->curr_pos;

    while (hg_is_name_char(hg_lexer_peek(lexer)))
        hg_lexer_next_char(lexer);

    if (lexer->curr_pos + 1 - start > HG_NAME_MAX) {
        hg_error(lexer->line, "name too long (at most %d characters)", HG_NAME_MAX);
    }
    token->text = hg_substr(lexer->src, start, lexer->curr_pos + 1);
    token->type = hg_check_keyword(token->text);
}

hg_token_t hg_get_token(hg_lexer_t* lexer) {
    hg_token_t token;

    hg_skip_white_space(lexer);
    token.line = lexer->line;

    if (lexer->curr_char == '\0') {
        token.text = "end of file";
        token.type = TOK_EOF;
        return token;       /* stay on the end: no next character to read */
    }
    if (hg_operator_token(lexer, &token)) {
        hg_lexer_next_char(lexer);
        return token;
    }

    if (lexer->curr_char == '\"') {
        hg_string_token(lexer, &token);
    }
    else if (hg_util_is_digit(lexer->curr_char)) {
        hg_number_token(lexer, &token);
    }
    else if (hg_util_is_alpha(lexer->curr_char) || lexer->curr_char == '_') {
        hg_word_token(lexer, &token);
    }
    else if (lexer->curr_char >= FIRST_PRINTABLE && lexer->curr_char <= LAST_PRINTABLE) {
        hg_error(lexer->line, "unknown character '%c'", lexer->curr_char);
    }
    else {
        hg_error(lexer->line, "unknown character (code %d)", (unsigned char)lexer->curr_char);
    }

    hg_lexer_next_char(lexer);
    return token;
}
