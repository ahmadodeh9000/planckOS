#include "lex.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void pcl_abort(const char* err_msg) {
    fprintf(stderr, "PCL LEXER ERROR: %s\n",err_msg);
    exit(-1);
}

void pcl_lexer_init(pcl_lexer_t* pcl_lex, const char* src) {
    pcl_lex->src        = src;
    pcl_lex->curr_char  = ' ';
    pcl_lex->curr_pos   = -1;
    
    // loading the first char
    pcl_lexer_next_char(pcl_lex);
}

void pcl_lexer_next_char(pcl_lexer_t* pcl_lex) {
    ++pcl_lex->curr_pos;

    if (pcl_lex->curr_pos > pcl_util_strlen(pcl_lex->src)) {
        pcl_lex->curr_char = '\0'; // end of file !
    }

    else {
        pcl_lex->curr_char = pcl_lex->src[pcl_lex->curr_pos];
    }

}
char pcl_lexer_peek(pcl_lexer_t* pcl_lex) {
    if (pcl_lex->curr_pos + 1 >= pcl_util_strlen(pcl_lex->src)) {
        return '\0';
    }

    return pcl_lex->src[pcl_lex->curr_pos + 1];
}


// tokens
static void pcl_skip_white_space(pcl_lexer_t* lexer) {
    while (lexer->curr_char == ' ' || lexer->curr_char == '\t' || lexer->curr_char == '\r')
        pcl_lexer_next_char(lexer);
}

static void pcl_skip_comment(pcl_lexer_t* lexer) {
    if (lexer->curr_char == '#') {
        while (lexer->curr_char != '\n' && lexer->curr_char != '\0')
            pcl_lexer_next_char(lexer);
    }
}

static char* pcl_substr(const char* src, int start, int end) {
    char* text = malloc(end - start + 1);
    if (!text) {
        pcl_abort("OUT OF MEMORY");
    }
    for (int i = start; i < end; ++i) {
        text[i - start] = src[i];
    }
    text[end - start] = '\0';
    return text;
}

static pcl_token_type_t pcl_check_keyword(const char* text) {
    if (strcmp(text, "LET") == 0)       return TOK_LET;
    if (strcmp(text, "GOTO") == 0)      return TOK_GOTO;
    if (strcmp(text, "LABEL") == 0)     return TOK_LABEL;
    if (strcmp(text, "PRINT") == 0)     return TOK_PRINT;
    if (strcmp(text, "IF") == 0)        return TOK_IF;
    if (strcmp(text, "ELSE") == 0)      return TOK_ELSE;
    if (strcmp(text, "THEN") == 0)      return TOK_THEN;
    if (strcmp(text, "ENDIF") == 0)     return TOK_ENDIF;
    if (strcmp(text, "WHILE") == 0)     return TOK_WHILE;
    if (strcmp(text, "ENDWHILE") == 0)  return TOK_ENDWHILE;
    if (strcmp(text, "INPUT") == 0)     return TOK_INPUT;
    if (strcmp(text,"REPEAT") == 0)     return TOK_REPEAT;
    
    return TOK_IDENT;
}

pcl_token_t pcl_get_token(pcl_lexer_t* lexer) {
    pcl_token_t token;

    pcl_skip_white_space(lexer);
    pcl_skip_comment(lexer);

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
        if (pcl_lexer_peek(lexer) == '=') {
            char last_char = lexer->curr_char;
            pcl_lexer_next_char(lexer);
            token.text = "==";
            token.type = TOK_EQEQ;
        }
        else {
            token.text = "=";
            token.type = TOK_EQ;
        }
    }

    else if (lexer->curr_char == '>') {
        if (pcl_lexer_peek(lexer) == '=') {
            pcl_lexer_next_char(lexer);
            token.text = ">=";
            token.type = TOK_GTE;
        }
        else {
            token.text = ">";
            token.type = TOK_GT;
        }
    }

    else if (lexer->curr_char == '<') {
        if (pcl_lexer_peek(lexer) == '=') {
            pcl_lexer_next_char(lexer);
            token.text = "<=";
            token.type = TOK_LTE;
        }
        else {
            token.text = "<";
            token.type = TOK_LT;
        }
    }

    else if (lexer->curr_char == '!') {
        if (pcl_lexer_peek(lexer) == '=') {
            pcl_lexer_next_char(lexer);
            token.text = "!=";
            token.type = TOK_NOTE;
        }
        else {
            pcl_abort("EXPECTED !=, GOT !");
        }
    }

    else if (lexer->curr_char == '\"') {
        pcl_lexer_next_char(lexer);
        int start = lexer->curr_pos;

        while (lexer->curr_char != '\"') {
            if (lexer->curr_char == '\r' || lexer->curr_char == '\n' || lexer->curr_char == '\t' ||
                lexer->curr_char == '\\' || lexer->curr_char == '%' || lexer->curr_char == '\0') {
                pcl_abort("ILLEGAL CHARACTER IN STRING");
            }
            pcl_lexer_next_char(lexer);
        }

        token.text = pcl_substr(lexer->src, start, lexer->curr_pos);
        token.type = TOK_STRING;
    }

    else if (isdigit((unsigned char)lexer->curr_char)) {
        int start = lexer->curr_pos;

        while (isdigit((unsigned char)pcl_lexer_peek(lexer)))
            pcl_lexer_next_char(lexer);

        if (pcl_lexer_peek(lexer) == '.') {
            pcl_lexer_next_char(lexer);

            if (!isdigit((unsigned char)pcl_lexer_peek(lexer)))
                pcl_abort("ILLEGAL CHARACTER IN NUMBER");

            while (isdigit((unsigned char)pcl_lexer_peek(lexer)))
                pcl_lexer_next_char(lexer);
        }

        token.text = pcl_substr(lexer->src, start, lexer->curr_pos + 1);
        token.type = TOK_NUMBER;
    }

    else if (isalpha((unsigned char)lexer->curr_char)) {
        int start = lexer->curr_pos;

        while (isalnum((unsigned char)pcl_lexer_peek(lexer)))
            pcl_lexer_next_char(lexer);

        token.text = pcl_substr(lexer->src, start, lexer->curr_pos + 1);
        token.type = pcl_check_keyword(token.text);
    }

    else {
        pcl_abort("UNKNOWN TOKEN");
    }

    pcl_lexer_next_char(lexer);
    return token;
}