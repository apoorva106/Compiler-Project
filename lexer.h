#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>

typedef enum {
    TK_ASSIGNOP,    // <---
    TK_COMMENT,     // %
    TK_FIELDID,     // field identifier
    TK_ID,          // identifier
    TK_NUM,         // integer
    TK_RNUM,        // real number
    TK_FUNID,       // function identifier
    TK_RECORDID,    // record identifier
    TK_WITH,        // with
    TK_PARAMETERS,  // parameters
    TK_END,         // end
    TK_WHILE,       // while
    TK_UNION,       // union
    TK_ENDUNION,    // endunion
    TK_DEFINITETYPE,// definitetype
    TK_AS,          // as
    TK_TYPE,        // type
    TK_MAIN,        // main
    TK_GLOBAL,      // global
    TK_PARAMETER,   // parameter
    TK_LIST,        // list
    TK_SQL,         // [
    TK_SQR,         // ]
    TK_INPUT,       // input
    TK_OUTPUT,      // output
    TK_INT,         // int
    TK_REAL,        // real
    TK_COMMA,       // ,
    TK_SEM,         // ;
    TK_COLON,       // :
    TK_DOT,         // .
    TK_OP,          // (
    TK_CL,          // )
    TK_PLUS,        // +
    TK_MINUS,       // -
    TK_MUL,         // *
    TK_DIV,         // /
    TK_CALL,        // call
    TK_RECORD,      // record
    TK_ENDRECORD,   // endrecord
    TK_THEN,        // then
    TK_AND,         // &&&
    TK_OR,          // @@@
    TK_NOT,         // ~
    TK_LT,          // <
    TK_LE,          // <=
    TK_EQ,          // ==
    TK_GT,          // >
    TK_GE,          // >=
    TK_NE,          // !=
    TK_TRUE,        // true
    TK_FALSE,       // false
    TK_IF,          // if
    TK_ELSE,        // else
    TK_ENDIF,       // endif
    TK_READ,        // read
    TK_WRITE,       // write
    TK_RETURN,      // return
    TK_ERROR        // error
} TokenType;

typedef struct {
    TokenType type;
    char* lexeme;
    int lineNo;
    union {
        int numValue;
        float realValue;
    } value;
    int errorType;  // 1: Length error, 2: Unknown symbol, 3: Unknown pattern
} Token;

void initLexer(FILE* fp);
Token* getNextToken(void);
void removeComments(char* inputFile, char* cleanFile);

#endif