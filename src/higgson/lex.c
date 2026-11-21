/*
 * lex.c - the Higgson lexer (see lex.h).
 */
#include "higgson/lex.h"
#include "higgson/hg.h"
#include "higgson/util.h"

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
static void hg_skip_white_space(hg_lexer_t* lexer) {
    while (lexer->curr_char == ' ' || lexer->curr_char == '\t' || lexer->curr_char == '\r')
        hg_lexer_next_char(lexer);
}

static void hg_skip_comment(hg_lexer_t* lexer) {
    if (lexer->curr_char == '#') {
        while (lexer->curr_char != '\n' && lexer->curr_char != '\0')
            hg_lexer_next_char(lexer);
    }
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
    if (hg_util_strcmp(text, "LET") == 0)       return TOK_LET;
    if (hg_util_strcmp(text, "GOTO") == 0)      return TOK_GOTO;
    if (hg_util_strcmp(text, "LABEL") == 0)     return TOK_LABEL;
    if (hg_util_strcmp(text, "PRINT") == 0)     return TOK_PRINT;
    if (hg_util_strcmp(text, "IF") == 0)        return TOK_IF;
    if (hg_util_strcmp(text, "ELSE") == 0)      return TOK_ELSE;
    if (hg_util_strcmp(text, "THEN") == 0)      return TOK_THEN;
    if (hg_util_strcmp(text, "ENDIF") == 0)     return TOK_ENDIF;
    if (hg_util_strcmp(text, "WHILE") == 0)     return TOK_WHILE;
    if (hg_util_strcmp(text, "ENDWHILE") == 0)  return TOK_ENDWHILE;
    if (hg_util_strcmp(text, "INPUT") == 0)     return TOK_INPUT;
    if (hg_util_strcmp(text,"REPEAT") == 0)     return TOK_REPEAT;
    
    return TOK_IDENT;
}

hg_token_t hg_get_token(hg_lexer_t* lexer) {
    hg_token_t token;

    hg_skip_white_space(lexer);
    hg_skip_comment(lexer);
    token.line = lexer->line;

    if (lexer->curr_char == '+') {
        token.text = "+";
        token.type = TOK_PLUS;     
    }
    else if (lexer->curr_char == '-') {
        token.text = "-";
        token.type = TOK_MINUS;     
    }
    else if (lexer->curr_char == '\n') {
        token.text = "\n";
        token.type = TOK_NEWLINE;
    }
    else if (lexer->curr_char == '*') {
        token.text = "*";
        token.type = TOK_STAR;     
    }
    else if (lexer->curr_char == '\0') {
        token.text = "\0";
        token.type = TOK_EOF;     
    }
    else if (lexer->curr_char == '/') {
        token.text = "/";
        token.type = TOK_SLASH;     
    }

    else if (lexer->curr_char == '=') {
        if (hg_lexer_peek(lexer) == '=') {
            hg_lexer_next_char(lexer);
            token.text = "==";
            token.type = TOK_EQEQ;
        }
        else {
            token.text = "=";
            token.type = TOK_EQ;
        }
    }

    else if (lexer->curr_char == '>') {
        if (hg_lexer_peek(lexer) == '=') {
            hg_lexer_next_char(lexer);
            token.text = ">=";
            token.type = TOK_GTE;
        }
        else {
            token.text = ">";
            token.type = TOK_GT;
        }
    }

    else if (lexer->curr_char == '<') {
        if (hg_lexer_peek(lexer) == '=') {
            hg_lexer_next_char(lexer);
            token.text = "<=";
            token.type = TOK_LTE;
        }
        else {
            token.text = "<";
            token.type = TOK_LT;
        }
    }

    else if (lexer->curr_char == '!') {
        if (hg_lexer_peek(lexer) == '=') {
            hg_lexer_next_char(lexer);
            token.text = "!=";
            token.type = TOK_NOTE;
        }
        else {
            hg_error(lexer->line, "EXPECTED !=, GOT !");
        }
    }

    else if (lexer->curr_char == '\"') {
        hg_lexer_next_char(lexer);
        int start = lexer->curr_pos;

        while (lexer->curr_char != '\"') {
            if (lexer->curr_char == '\r' || lexer->curr_char == '\n' || lexer->curr_char == '\t' ||
                lexer->curr_char == '\\' || lexer->curr_char == '%' || lexer->curr_char == '\0') {
                hg_error(lexer->line, "ILLEGAL CHARACTER IN STRING");
            }
            hg_lexer_next_char(lexer);
        }

        token.text = hg_substr(lexer->src, start, lexer->curr_pos);
        token.type = TOK_STRING;
    }

    else if (hg_util_is_digit(lexer->curr_char)) {
        int start = lexer->curr_pos;

        while (hg_util_is_digit(hg_lexer_peek(lexer)))
            hg_lexer_next_char(lexer);

        if (hg_lexer_peek(lexer) == '.') {
            hg_lexer_next_char(lexer);

            if (!hg_util_is_digit(hg_lexer_peek(lexer)))
                hg_error(lexer->line, "ILLEGAL CHARACTER IN NUMBER");

            while (hg_util_is_digit(hg_lexer_peek(lexer)))
                hg_lexer_next_char(lexer);
        }

        token.text = hg_substr(lexer->src, start, lexer->curr_pos + 1);
        token.type = TOK_NUMBER;
    }

    else if (hg_util_is_alpha(lexer->curr_char)) {
        int start = lexer->curr_pos;

        while (hg_util_is_alnum(hg_lexer_peek(lexer)))
            hg_lexer_next_char(lexer);

        token.text = hg_substr(lexer->src, start, lexer->curr_pos + 1);
        token.type = hg_check_keyword(token.text);
    }

    else {
        hg_error(lexer->line, "UNKNOWN TOKEN");
    }

    hg_lexer_next_char(lexer);
    return token;
}