#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"
#include "keyword_table.h"

#define BUFFER_SIZE 4096
#define MAX_LEXEME_LEN 50
#define MAX_ID_LEN 20
#define MAX_FUNID_LEN 30

typedef struct {
    char buffer1[BUFFER_SIZE];
    char buffer2[BUFFER_SIZE];
    int currentBuffer;
    int forward;
    int begin;
    FILE* fp;
    int lineNo;
    int eof;
} LexerBuffer;

// Global variables
static LexerBuffer* lexerBuffer = NULL;
static KeywordTable* keywordTable = NULL;
static int currentState = 0;

// Forward declarations
static FILE* getStream(FILE* fp);
static char getNextChar(void);
static void retract(int count);
static char* getLexeme(void);
static int isKeyword(const char* str);

// Initialize lexer
void initLexer(FILE* fp) {
    lexerBuffer = (LexerBuffer*)malloc(sizeof(LexerBuffer));
    lexerBuffer->fp = fp;
    lexerBuffer->currentBuffer = 1;
    lexerBuffer->forward = 0;
    lexerBuffer->begin = 0;
    lexerBuffer->lineNo = 1;
    lexerBuffer->eof = 0;
    
    memset(lexerBuffer->buffer1, EOF, BUFFER_SIZE);
    memset(lexerBuffer->buffer2, EOF, BUFFER_SIZE);
    
    keywordTable = initKeywordTable();
    getStream(lexerBuffer->fp);
}

// Get next chunk of file into buffer
static FILE* getStream(FILE* fp) {
    if (fp == NULL || lexerBuffer->eof) {
        return NULL;
    }

    char* targetBuffer = (lexerBuffer->currentBuffer == 1) ? 
                        lexerBuffer->buffer1 : lexerBuffer->buffer2;
    
    size_t bytesRead = fread(targetBuffer, sizeof(char), BUFFER_SIZE-1, fp);
    if (bytesRead < BUFFER_SIZE-1) {
        targetBuffer[bytesRead] = EOF;
        lexerBuffer->eof = 1;
    }
    
    return fp;
}

// Get next character from buffer
static char getNextChar(void) {
    char c;
    if (lexerBuffer->currentBuffer == 1) {
        c = lexerBuffer->buffer1[lexerBuffer->forward];
        if (lexerBuffer->forward >= BUFFER_SIZE-1) {
            getStream(lexerBuffer->fp);
            lexerBuffer->currentBuffer = 2;
            lexerBuffer->forward = 0;
        } else {
            lexerBuffer->forward++;
        }
    } else {
        c = lexerBuffer->buffer2[lexerBuffer->forward];
        if (lexerBuffer->forward >= BUFFER_SIZE-1) {
            getStream(lexerBuffer->fp);
            lexerBuffer->currentBuffer = 1;
            lexerBuffer->forward = 0;
        } else {
            lexerBuffer->forward++;
        }
    }
    
    if (c == '\n') {
        lexerBuffer->lineNo++;
    }
    
    return c;
}

// Retract the forward pointer
static void retract(int count) {
    while (count > 0) {
        if (lexerBuffer->forward == 0) {
            lexerBuffer->currentBuffer = (lexerBuffer->currentBuffer == 1) ? 2 : 1;
            lexerBuffer->forward = BUFFER_SIZE - 1;
        } else {
            lexerBuffer->forward--;
        }
        count--;
    }
}

// Get the current lexeme
// Add to getLexeme():
static char* getLexeme(void) {
    char* lexeme = (char*)malloc(MAX_LEXEME_LEN * sizeof(char));
    int i = 0;
    int bufferIndex = lexerBuffer->begin;
    int currentBuf = lexerBuffer->currentBuffer;
    
    while (bufferIndex != lexerBuffer->forward) {
        char c;
        if (currentBuf == 1) {
            c = lexerBuffer->buffer1[bufferIndex];
        } else {
            c = lexerBuffer->buffer2[bufferIndex];
        }
        
        // Check for token boundaries
        //if (i > 0 && (isspace(c) || isSpecialChar(c))) {
          //  break;
        //}
        
        lexeme[i++] = c;
        bufferIndex++;
        if (bufferIndex >= BUFFER_SIZE) {
            currentBuf = (currentBuf == 1) ? 2 : 1;
            bufferIndex = 0;
        }
    }
    lexeme[i] = '\0';
    return lexeme;
}

// Check if a string is a keyword
static int isKeyword(const char* str) {
    TokenType type = lookupKeyword(keywordTable, str);
    return (type != TK_ID);
}

Token* getNextToken(void) {
    currentState = 0;
    char c;
    Token* token = (Token*)malloc(sizeof(Token));
    char numBuffer[32] = {0};
    int numLen = 0;
    
    while (1) {
        c = getNextChar();
        
        switch (currentState) {
            case 0:  // Initial state
                if (c == EOF) {
                    free(token);
                    return NULL;
                }
                
                lexerBuffer->begin = lexerBuffer->forward - 1;
                
                if (isspace(c)) {
                    lexerBuffer->begin = lexerBuffer->forward;
                    continue;
                }
                
                // Handle all transitions from state 0
                if (c == '%') {
                    while (c != '\n' && c != EOF) {
                        c = getNextChar();
                    }
                    token->type = TK_COMMENT;
                    token->lexeme = strdup("%");
                    token->lineNo = lexerBuffer->lineNo - 1;
                    return token;
                }
                else if (c == '<') currentState = 16;
                else if (c == '>') currentState = 22;
                else if (c == '=') currentState = 25;
                else if (c == '!') currentState = 14;
                else if (c == '&') currentState = 30;
                else if (c == '@') currentState = 27;
                else if (c == '_') currentState = 47;
                else if (c == '#') currentState = 52;
                else if (isdigit(c)) {
                    numBuffer[numLen++] = c;
                    currentState = 42;
                }
                else if (c >= 'b' && c <= 'd') currentState = 35;
                else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) currentState = 40;
                else {
                    switch (c) {
                        case '+':
                            token->type = TK_PLUS;
                            token->lexeme = strdup("+");
                            break;
                        case '-':
                            token->type = TK_MINUS;
                            token->lexeme = strdup("-");
                            break;
                        case '*':
                            token->type = TK_MUL;
                            token->lexeme = strdup("*");
                            break;
                        case '/':
                            token->type = TK_DIV;
                            token->lexeme = strdup("/");
                            break;
                        case '(':
                            token->type = TK_OP;
                            token->lexeme = strdup("(");
                            break;
                        case ')':
                            token->type = TK_CL;
                            token->lexeme = strdup(")");
                            break;
                        case '[':
                            token->type = TK_SQL;
                            token->lexeme = strdup("[");
                            break;
                        case ']':
                            token->type = TK_SQR;
                            token->lexeme = strdup("]");
                            break;
                        case ',':
                            token->type = TK_COMMA;
                            token->lexeme = strdup(",");
                            break;
                        case ';':
                            token->type = TK_SEM;
                            token->lexeme = strdup(";");
                            break;
                        case ':':
                            token->type = TK_COLON;
                            token->lexeme = strdup(":");
                            break;
                        case '.':
                            token->type = TK_DOT;
                            token->lexeme = strdup(".");
                            break;
                        default:
                            token->type = TK_ERROR;
                            token->lexeme = (char*)malloc(2);
                            token->lexeme[0] = c;
                            token->lexeme[1] = '\0';
                            token->errorType = 2; // Unknown symbol
                    }
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;

            case 14:  // ! state
                c = getNextChar();
                if (c == '=') {
                    token->type = TK_NE;
                    token->lexeme = strdup("!=");
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                retract(1);
                token->type = TK_ERROR;
                token->lexeme = strdup("!");
                token->lineNo = lexerBuffer->lineNo;
                token->errorType = 2;  // Unknown symbol
                return token;

            case 16:  // < state
                c = getNextChar();
                if (c == '-') {
                    char* ahead = malloc(3);
                    for(int i = 0; i < 3; i++) {
                        ahead[i] = getNextChar();
                    }
                    if (ahead[0] == '-' && ahead[1] == '-' && ahead[2] == '-') {
                        token->type = TK_ASSIGNOP;
                        token->lexeme = strdup("<---");
                        free(ahead);
                        return token;
                    }
                    retract(3);
                    free(ahead);
                }
                retract(1);
                token->type = TK_LT;
                token->lexeme = strdup("<");
                return token;

            case 17:  // <- state
                c = getNextChar();
                if (c == '-') {
                    currentState = 18;
                }
                else {
                    retract(2);
                    token->type = TK_LT;
                    token->lexeme = strdup("<");
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;

            case 18:  // <-- state
                c = getNextChar();
                if (c == '-') {
                    token->type = TK_ASSIGNOP;
                    token->lexeme = strdup("<---");
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                retract(3);
                token->type = TK_LT;
                token->lexeme = strdup("<");
                token->lineNo = lexerBuffer->lineNo;
                return token;

            case 22:  // > state
                c = getNextChar();
                if (c == '=') {
                    token->type = TK_GE;
                    token->lexeme = strdup(">=");
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                retract(1);
                token->type = TK_GT;
                token->lexeme = strdup(">");
                token->lineNo = lexerBuffer->lineNo;
                return token;

            case 25:  // = state
                c = getNextChar();
                if (c == '=') {
                    token->type = TK_EQ;
                    token->lexeme = strdup("==");
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                token->type = TK_ERROR;
                token->lexeme = strdup("=");
                token->lineNo = lexerBuffer->lineNo;
                token->errorType = 2;  // Unknown symbol
                return token;

            case 27:  // @ state
                c = getNextChar();
                if (c == '@') {
                    currentState = 28;
                }
                else {
                    token->type = TK_ERROR;
                    token->lexeme = strdup("@");
                    token->lineNo = lexerBuffer->lineNo;
                    token->errorType = 2;
                    return token;
                }
                break;

            case 28:  // @@ state
                c = getNextChar();
                if (c == '@') {
                    token->type = TK_OR;
                    token->lexeme = strdup("@@@");
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                token->type = TK_ERROR;
                token->lexeme = strdup("@@");
                token->lineNo = lexerBuffer->lineNo;
                token->errorType = 2;
                return token;

            case 30:  // & state for &&&
                c = getNextChar();
                if (c == '&') {
                    currentState = 31;
                }
                else {
                    token->type = TK_ERROR;
                    token->lexeme = strdup("&");
                    token->lineNo = lexerBuffer->lineNo;
                    token->errorType = 2;
                    return token;
                }
                break;

            case 31:  // && state
                c = getNextChar();
                if (c == '&') {
                    token->type = TK_AND;
                    token->lexeme = strdup("&&&");
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                token->type = TK_ERROR;
                token->lexeme = strdup("&&");
                token->lineNo = lexerBuffer->lineNo;
                token->errorType = 2;
                return token;

            case 35:  // b-d state for identifiers
                c = getNextChar();
                if (c >= '2' && c <= '7') {
                    currentState = 36;
                }
                else if (isalpha(c)) {
                    currentState = 40;
                }
                else {
                    retract(1);
                    token->type = TK_ID;
                    token->lexeme = getLexeme();
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;

            case 36:  // Identifier with b-d followed by 2-7
                c = getNextChar();
                if (c >= 'b' && c <= 'd') {
                    continue;
                }
                else if (c >= '2' && c <= '7') {
                    currentState = 37;
                }
                else {
                    retract(1);
                    token->type = TK_ID;
                    token->lexeme = getLexeme();
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;

            case 37:  // Checking identifier length
                retract(1);
                {
                    char* lexeme = getLexeme();
                    int len = strlen(lexeme);
                    if (len > MAX_ID_LEN) {
                        token->type = TK_ERROR;
                        token->lexeme = lexeme;
                        token->lineNo = lexerBuffer->lineNo;
                        token->errorType = 1;  // Length error
                        return token;
                    }
                    token->type = TK_ID;
                    token->lexeme = lexeme;
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }

                case 40:  // Field identifier or keyword
                c = getNextChar();
                if (isalpha(c)) {
                    // Continue reading alphabets
                    continue;
                }
                else if (c == '_') {
                    // If we encounter an underscore, retract and return current token
                    retract(1);
                    token->lexeme = getLexeme();
                    TokenType keywordType = lookupKeyword(keywordTable, token->lexeme);
                    if (keywordType != TK_ID) {
                        token->type = keywordType;
                    }
                    else {
                        token->type = TK_FIELDID;
                    }
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                else {
                    retract(1);
                    token->lexeme = getLexeme();
                    TokenType keywordType = lookupKeyword(keywordTable, token->lexeme);
                    if (keywordType != TK_ID) {
                        token->type = keywordType;
                    }
                    else {
                        token->type = TK_FIELDID;
                    }
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;

            case 42:  // Number state
                if (isdigit(c)) {
                    numBuffer[numLen++] = c;
                }
                else if (c == '.') {
                    numBuffer[numLen++] = c;
                    currentState = 44;
                }
                else {
                    retract(1);
                    token->type = TK_NUM;
                    token->lexeme = strdup(numBuffer);
                    token->value.numValue = atoi(numBuffer);
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;

            case 44:  // Real number state (after decimal point)
                if (isdigit(c)) {
                    numBuffer[numLen++] = c;
                    currentState = 45;
                }
                else {
                    token->type = TK_ERROR;
                    token->lexeme = strdup(numBuffer);
                    token->lineNo = lexerBuffer->lineNo;
                    token->errorType = 3;  // Invalid real number
                    return token;
                }
                break;

            case 45:
                if (isdigit(c)) {
                    numBuffer[numLen++] = c;
                } else if (c == 'E' || c == 'e') {
                    numBuffer[numLen++] = c;
                    c = getNextChar();
                    if (c == '+' || c == '-') {
                        numBuffer[numLen++] = c;
                    } else {
                        retract(1);
                    }
                    currentState = 46;
                } else {
                    retract(1);
                    token->type = TK_RNUM;
                    token->lexeme = strdup(numBuffer);
                    token->value.realValue = atof(numBuffer);
                    return token;
                }
                break;

            case 46:  // Handle exponent part of real number
                if (c == '+' || c == '-' || isdigit(c)) {
                    numBuffer[numLen++] = c;
                    while ((c = getNextChar()) != EOF && isdigit(c)) {
                        numBuffer[numLen++] = c;
                    }
                    retract(1);
                    token->type = TK_RNUM;
                    token->lexeme = strdup(numBuffer);
                    token->value.realValue = atof(numBuffer);
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                token->type = TK_ERROR;
                token->lexeme = strdup(numBuffer);
                token->lineNo = lexerBuffer->lineNo;
                token->errorType = 3;
                return token;

            case 47:  // Function identifier (starts with _)
                c = getNextChar();
                if (isalpha(c)) {
                    printf("DEBUG: State 47 - Got alpha char: %c\n", c);
                    currentState = 48;
                } else {
                    token->type = TK_ERROR;
                    token->lexeme = getLexeme();
                    token->lineNo = lexerBuffer->lineNo;
                    token->errorType = 3;
                    return token;
                }
                break;
            
            case 48:  // Function identifier continued
                c = getNextChar();
                if (isalpha(c)) {
                    continue;
                } else {
                    retract(1);
                    char* lexeme = getLexeme();
                    printf("DEBUG: State 48 - Got lexeme: %s\n", lexeme);
                    TokenType keywordType = lookupKeyword(keywordTable, lexeme);
                    printf("DEBUG: State 48 - Keyword lookup returned: %d\n", keywordType);
                    if (strcmp(lexeme, "_main") == 0) {
                        token->type = TK_MAIN;
                    } else {
                        token->type = (keywordType != TK_ID) ? keywordType : TK_FUNID;
                    }
                    token->lexeme = lexeme;
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;

            case 49:  // Function identifier with numbers
                c = getNextChar();
                if (isdigit(c)) {
                    continue;  // Stay in state 49 for more digits
                } else {
                    retract(1);
                    char* lexeme = getLexeme();
                    if (strlen(lexeme) > MAX_FUNID_LEN) {
                        token->type = TK_ERROR;
                        token->lexeme = lexeme;
                        token->lineNo = lexerBuffer->lineNo;
                        token->errorType = 1;  // Length error
                        return token;
                    }
                    token->type = TK_FUNID;
                    token->lexeme = lexeme;
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;

            case 52:
                c = getNextChar();
                if (isalpha(c) && islower(c)) {
                    currentState = 53;
                    break;
                }
                token->type = TK_ERROR;
                token->lexeme = getLexeme();
                token->errorType = 3;
                return token;
            
            case 53:
                if (isalpha(c) && islower(c)) {
                    continue;
                }
                retract(1);
                char* lexeme = getLexeme();
                if (strcmp(lexeme, "#record") == 0) {
                    token->type = TK_RECORD;
                } else {
                    token->type = TK_RECORDID;
                }
                token->lexeme = lexeme;
                return token;

            default:
                token->type = TK_ERROR;
                token->lexeme = getLexeme();
                token->lineNo = lexerBuffer->lineNo;
                token->errorType = 3;  // Unknown pattern
                return token;
        }
    }
}
                
void removeComments(char* inputFile, char* cleanFile) {
    FILE* in = fopen(inputFile, "r");
    FILE* out = fopen(cleanFile, "w");
    
    if (!in || !out) {
        printf("Error opening files\n");
        return;
    }
    
    char c;
    int inComment = 0;
    
    while ((c = fgetc(in)) != EOF) {
        if (c == '%') {
            inComment = 1;
            continue;
        }
        
        if (c == '\n') {
            inComment = 0;
        }
        
        if (!inComment) {
            fputc(c, out);
        }
    }
    
    fclose(in);
    fclose(out);
}