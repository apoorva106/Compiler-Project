
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

static int isIdChar(char c) {
    return (isalnum(c) || c == '_');
}

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

    while (bufferIndex != lexerBuffer->forward && i < MAX_LEXEME_LEN - 1) {
        char c;
        if (currentBuf == 1) {
            c = lexerBuffer->buffer1[bufferIndex];
        } else {
            c = lexerBuffer->buffer2[bufferIndex];
        }

        // Break if we hit a non-identifier character
        if (!isalnum(c) && c != '_') {
            break;
        }

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
    currentState = 1; // Initial state from DFA
    char c;
    Token* token = (Token*)malloc(sizeof(Token));
    char numBuffer[32] = {0};
    int numLen = 0;
    
    while (1) {
        c = getNextChar();
        
        switch (currentState) {
            case 1: // Initial/Start state (center of DFA)
                if (c == EOF) {
                    free(token);
                    return NULL;
                }
                
                lexerBuffer->begin = lexerBuffer->forward - 1;
                
                if (isspace(c)) {
                    lexerBuffer->begin = lexerBuffer->forward;
                    continue;
                }

                // Comment handling (shown in DFA)
                if (c == '%') {
                    while ((c = getNextChar()) != '\n' && c != EOF);
                    token->type = TK_COMMENT;
                    token->lexeme = strdup("%");
                    token->lineNo = lexerBuffer->lineNo - 1;
                    lexerBuffer->begin = lexerBuffer->forward;
                    return token;
                }

                // Follow exact DFA transitions:
                if (c == '<') currentState = 22;       // Less than branch
                else if (c == '>') currentState = 30;  // Greater than branch
                else if (c == '=') currentState = 20;  // Equals branch
                else if (c == '!') currentState = 21;  // Not equals branch
                else if (c == '&') currentState = 68;  // AND operator
                else if (c == '@') currentState = 37;  // @ for OR operator
                else if (c == '#') currentState = 33;  // # for record identifiers
                else if (c == '[') currentState = 41;  // Left square bracket
                else if (c == ']') currentState = 42;  // Right square bracket
                else if (c == '_') currentState = 2;  // Function identifier
                else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                    if (c >= 'b' && c <= 'd') {
                        currentState = 8;  // [b-d] identifiers
                    } else {
                        currentState = 6;  // Field identifiers and other identifiers
                    }
                }
                else if (isdigit(c)) {
                    numBuffer[numLen++] = c;
                    currentState = 43;  // Number recognition
                }
                else {
                    // Single character tokens matching DFA leaves (12-15 in DFA)
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
                            token->errorType = 2;  // Unknown symbol
                    }
                    token->lineNo = lexerBuffer->lineNo;
                    lexerBuffer->begin = lexerBuffer->forward;
                    return token;
                }
                break;

            case 2: // Function identifier (starts with _)
                c = getNextChar();
                if (isalpha(c)) {
                    currentState = 3;  // Move to function ID part
                } else {
                    token->type = TK_ERROR;
                    token->lexeme = getLexeme();
                    token->lineNo = lexerBuffer->lineNo;
                    token->errorType = 3;
                    return token;
                }
                break;

            case 3: // Function identifier continued (matches DFA)
                if (isalnum(c)) {
                    continue;  // Keep accumulating alphanumeric chars
                } else {
                    retract(1);
                    char* lexeme = getLexeme();
                    
                    // Special handling for _main as shown in DFA
                    if (strcmp(lexeme, "_main") == 0) {
                        token->type = TK_MAIN;
                    } else {
                        // Check function ID length
                        if (strlen(lexeme) > MAX_FUNID_LEN) {
                            token->type = TK_ERROR;
                            token->errorType = 1;  // Length error
                        } else {
                            token->type = TK_FUNID;
                        }
                    }
                    token->lexeme = lexeme;
                    token->lineNo = lexerBuffer->lineNo;
                    lexerBuffer->begin = lexerBuffer->forward;
                    return token;
                }
                break;

            case 4: // Function identifier with numbers
                if (isdigit(c)) {
                    continue;  // Stay in state 49 as per DFA
                } else {
                    retract(1);
                    char* lexeme = getLexeme();
                    if (strlen(lexeme) > MAX_FUNID_LEN) {
                        token->type = TK_ERROR;
                        token->lexeme = lexeme;
                        token->lineNo = lexerBuffer->lineNo;
                        token->errorType = 1;
                        return token;
                    }
                    token->type = TK_FUNID;
                    token->lexeme = lexeme;
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;

            case 6: // Field identifier state
                if (isalnum(c)) {
                    currentState = 6;  // Stay in state 6 for alphanumeric characters
                    continue;
                } else {
                    retract(1);
                    char* lexeme = getLexeme();
                    TokenType keywordType = lookupKeyword(keywordTable, lexeme);
                    
                    // Check if it's a keyword first
                    if (keywordType != TK_ID) {
                        token->type = keywordType;
                    } else {
                        // Not a keyword, check if it starts with lowercase letter
                        if (islower(lexeme[0])) {
                            token->type = TK_FIELDID;
                        } else {
                            token->type = TK_ID;
                        }
                    }
                    token->lexeme = lexeme;
                    token->lineNo = lexerBuffer->lineNo;
                    lexerBuffer->begin = lexerBuffer->forward;
                    return token;
                }
                break;

            case 8: // [b-d] identifier start
                c = getNextChar();
                if (c >= '2' && c <= '7') {
                    currentState = 9;  // Matches DFA transition [2-7]
                }
                else if (isalpha(c)) {
                    currentState = 6;  // Matches DFA transition to field identifier
                }
                else {
                    retract(1);
                    token->type = TK_ID;
                    token->lexeme = getLexeme();
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;


            case 9: // [b-d][2-7] identifier state
                c = getNextChar();
                if (c >= 'b' && c <= 'd') {
                    continue;  // Stay in state 6 as per DFA loop
                }
                else if (c >= '2' && c <= '7') {
                    currentState = 8;  // Move to state 8 as per DFA
                }
                else {
                    retract(1);
                    token->type = TK_ID;
                    token->lexeme = getLexeme();
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;

            case 10: // Final identifier check state
                retract(1);
                {
                    char* lexeme = getLexeme();
                    if (strlen(lexeme) > MAX_ID_LEN) {
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
                break;

            case 20: // Equals state
                c = getNextChar();
                if (c == '=') {
                    token->type = TK_EQ;
                    token->lexeme = strdup("==");
                    token->lineNo = lexerBuffer->lineNo;
                    lexerBuffer->begin = lexerBuffer->forward;
                    return token;
                }
                token->type = TK_ERROR;
                token->lexeme = strdup("=");
                token->lineNo = lexerBuffer->lineNo;
                token->errorType = 2;
                return token;

            case 21: // Not equals state
                c = getNextChar();
                if (c == '=') {
                    token->type = TK_NE;
                    token->lexeme = strdup("!=");
                    token->lineNo = lexerBuffer->lineNo;
                    lexerBuffer->begin = lexerBuffer->forward;
                    return token;
                }
                retract(1);
                token->type = TK_ERROR;
                token->lexeme = strdup("!");
                token->lineNo = lexerBuffer->lineNo;
                token->errorType = 2;
                return token;

            case 22: // Less than state
                c = getNextChar();
                if (c == '-') {
                    currentState = 24;  // Start of assignment operator
                } else if (c == '=') {
                    token->type = TK_LE;
                    token->lexeme = strdup("<=");
                    token->lineNo = lexerBuffer->lineNo;
                    lexerBuffer->begin = lexerBuffer->forward;
                    return token;
                } else {
                    retract(1);
                    token->type = TK_LT;
                    token->lexeme = strdup("<");
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;

            case 24: // First - of assignment
                c = getNextChar();
                if (c == '-') {
                    currentState = 25;
                } else {
                    retract(2);
                    token->type = TK_LT;
                    token->lexeme = strdup("<");
                    token->lineNo = lexerBuffer->lineNo;
                    lexerBuffer->begin = lexerBuffer->forward;
                    return token;
                }
                break;

            case 25:
                c = getNextChar();
                    if (c == '-') {
                        currentState=26;
                    }
                    else {
                        retract(2);
                        token->type=TK_LT;
                        token->lexeme = strdup("<");
                        token->lineNo = lexerBuffer->lineNo;
                        lexerBuffer->begin = lexerBuffer->forward; 
                        return token;
                    }
                    break;
                

            case 26: // Second - of assignment
                c = getNextChar();
                if (c == '-') {
                    token->type = TK_ASSIGNOP;
                    token->lexeme = strdup("<---");
                    token->lineNo = lexerBuffer->lineNo;
                    lexerBuffer->begin = lexerBuffer->forward;
                    return token;
                }
                retract(3);
                token->type = TK_LT;
                token->lexeme = strdup("<");
                token->lineNo = lexerBuffer->lineNo;
                return token;

            case 30: // Greater than state
                c = getNextChar();
                if (c == '=') {
                    token->type = TK_GE;
                    token->lexeme = strdup(">=");
                    token->lineNo = lexerBuffer->lineNo;
                    lexerBuffer->begin = lexerBuffer->forward;
                    return token;
                }
                retract(1);
                token->type = TK_GT;
                token->lexeme = strdup(">");
                token->lineNo = lexerBuffer->lineNo;
                return token;

            case 33: // # state
                c = getNextChar();
                if (isalpha(c) && islower(c)) {
                    currentState = 35;
                } else {
                    token->type = TK_ERROR;
                    token->lexeme = getLexeme();
                    token->errorType = 3;
                    return token;
                }
                break;

            case 35: // Record identifier state
                if (isalpha(c) && islower(c)) {
                    continue;
                }
                retract(1);
                {
                    char* lexeme = getLexeme();
                    if (strcmp(lexeme, "#record") == 0) {
                        token->type = TK_RECORD;
                    } else {
                        token->type = TK_RECORDID;
                    }
                    token->lexeme = lexeme;
                    token->lineNo = lexerBuffer->lineNo;
                    lexerBuffer->begin = lexerBuffer->forward;
                    return token;
                }
            

            case 37: // @ state
                c = getNextChar();
                if (c == '@') {
                    currentState = 38;
                } else {
                    token->type = TK_ERROR;
                    token->lexeme = strdup("@");
                    token->lineNo = lexerBuffer->lineNo;
                    token->errorType = 2;
                    return token;
                }
                break;

            case 38: // @@ state
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

            
            case 41: // [ state from DFA
                token->type = TK_SQL;
                token->lexeme = strdup("[");
                token->lineNo = lexerBuffer->lineNo;
                return token;

            case 42: // ] state from DFA
                token->type = TK_SQR;
                token->lexeme = strdup("]");
                token->lineNo = lexerBuffer->lineNo;
                return token;

            case 43: // Number start state
                if (isdigit(c)) {
                    numBuffer[numLen++] = c;
                }
                else if (c == '.') {
                    numBuffer[numLen++] = c;
                    currentState = 46;
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

            case 46: // Decimal point state
                if (isdigit(c)) {
                    numBuffer[numLen++] = c;
                    currentState = 47;
                }
                else {
                    token->type = TK_ERROR;
                    token->lexeme = strdup(numBuffer);
                    token->lineNo = lexerBuffer->lineNo;
                    token->errorType = 3;
                    return token;
                }
                break;

            case 47: // After decimal digits
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
                    currentState = 52;
                } else {
                    retract(1);
                    token->type = TK_ERROR;
                    token->lexeme = strdup(numBuffer);
                    token->lineNo = lexerBuffer->lineNo;
                    token->errorType = 3;
                    lexerBuffer->begin = lexerBuffer->forward;
                    return token;
                }
                break;

            case 50: // Real number state (matches DFA)
                retract(1);
                token->type = TK_RNUM;
                token->lexeme = strdup(numBuffer);
                token->value.realValue = atof(numBuffer);
                token->lineNo = lexerBuffer->lineNo;
                return token;

            case 51: // Error state for real numbers (shown with * in DFA)
                token->type = TK_ERROR;
                token->lexeme = strdup(numBuffer);
                token->lineNo = lexerBuffer->lineNo;
                token->errorType = 3;  // Unknown pattern
                return token;

            case 52: // Exponent part
                if (isdigit(c)) {
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

            case 68: // & state
                c = getNextChar();
                if (c == '&') {
                    currentState = 69;
                } else {
                    retract(1);
                    token->type = TK_ERROR;
                    token->lexeme = strdup("&");
                    token->lineNo = lexerBuffer->lineNo;
                    token->errorType = 2;
                    return token;
                }
                break;

            case 69: // && state
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


            default: // Error state (shown with * in DFA)
                token->type = TK_ERROR;
                token->lexeme = getLexeme();
                token->lineNo = lexerBuffer->lineNo;
                token->errorType = 3;  // Unknown pattern
                return token;
        }
    }
}

// Comment removal function (matches original requirements)
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