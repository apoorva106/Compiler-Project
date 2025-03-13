#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"

// Function to convert token type to string
const char* getTokenName(TokenType type) {
    switch(type) {
        case TK_ASSIGNOP: return "TK_ASSIGNOP";
        case TK_COMMENT: return "TK_COMMENT";
        case TK_FIELDID: return "TK_FIELDID";
        case TK_ID: return "TK_ID";
        case TK_NUM: return "TK_NUM";
        case TK_RNUM: return "TK_RNUM";
        case TK_FUNID: return "TK_FUNID";
        case TK_RUID: return "TK_RUID";
        case TK_WITH: return "TK_WITH";
        case TK_PARAMETERS: return "TK_PARAMETERS";
        case TK_END: return "TK_END";
        case TK_WHILE: return "TK_WHILE";
        case TK_ENDWHILE: return "TK_ENDWHILE";
        case TK_UNION: return "TK_UNION";
        case TK_ENDUNION: return "TK_ENDUNION";
        case TK_DEFINETYPE: return "TK_DEFINETYPE";
        case TK_AS: return "TK_AS";
        case TK_TYPE: return "TK_TYPE";
        case TK_MAIN: return "TK_MAIN";
        case TK_GLOBAL: return "TK_GLOBAL";
        case TK_PARAMETER: return "TK_PARAMETER";
        case TK_LIST: return "TK_LIST";
        case TK_SQL: return "TK_SQL";
        case TK_SQR: return "TK_SQR";
        case TK_INPUT: return "TK_INPUT";
        case TK_OUTPUT: return "TK_OUTPUT";
        case TK_INT: return "TK_INT";
        case TK_REAL: return "TK_REAL";
        case TK_COMMA: return "TK_COMMA";
        case TK_SEM: return "TK_SEM";
        case TK_COLON: return "TK_COLON";
        case TK_DOT: return "TK_DOT";
        case TK_OP: return "TK_OP";
        case TK_CL: return "TK_CL";
        case TK_PLUS: return "TK_PLUS";
        case TK_MINUS: return "TK_MINUS";
        case TK_MUL: return "TK_MUL";
        case TK_DIV: return "TK_DIV";
        case TK_CALL: return "TK_CALL";
        case TK_RECORD: return "TK_RECORD";
        case TK_ENDRECORD: return "TK_ENDRECORD";
        case TK_THEN: return "TK_THEN";
        case TK_AND: return "TK_AND";
        case TK_OR: return "TK_OR";
        case TK_NOT: return "TK_NOT";
        case TK_LT: return "TK_LT";
        case TK_LE: return "TK_LE";
        case TK_EQ: return "TK_EQ";
        case TK_GT: return "TK_GT";
        case TK_GE: return "TK_GE";
        case TK_NE: return "TK_NE";
        case TK_TRUE: return "TK_TRUE";
        case TK_FALSE: return "TK_FALSE";
        case TK_IF: return "TK_IF";
        case TK_ELSE: return "TK_ELSE";
        case TK_ENDIF: return "TK_ENDIF";
        case TK_READ: return "TK_READ";
        case TK_WRITE: return "TK_WRITE";
        case TK_RETURN: return "TK_RETURN";
        case TK_ERROR: return "TK_ERROR";
        default: return "UNKNOWN";
    }
}

// Function to print token with proper formatting
void printToken(Token* token) {
    if (token->type == TK_ERROR) {
        switch(token->errorType) {
            case 1:
                printf("Line No %d: Error :Variable Identifier is longer than the prescribed length of 20 characters.\n", 
                       token->lineNo);
                break;
            case 2:
                printf("Line No %d: Error : Unknown Symbol <%s>\n", 
                       token->lineNo, token->lexeme);
                break;
            case 3:
                printf("Line no: %d : Error: Unknown pattern <%s>\n", 
                       token->lineNo, token->lexeme);
                break;
            default:
                printf("Line no: %d : Lexical Error\n", token->lineNo);
        }
    } else {
        printf("Line no. %d\t Lexeme %s\t Token %s\n", 
               token->lineNo, 
               token->lexeme, 
               getTokenName(token->type));
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <source_file>\n", argv[0]);
        return 1;
    }

    FILE* fp = fopen(argv[1], "r");
    if (!fp) {
        printf("Error: Cannot open file %s\n", argv[1]);
        return 1;
    }

    initLexer(fp);

    Token* token;
    while ((token = getNextToken()) != NULL) {
        printToken(token);
        free(token->lexeme);
        free(token);
    }

    fclose(fp);
    return 0;
}