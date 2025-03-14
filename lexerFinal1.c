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

static void retract(int count) {
    while (count > 0) {
        if (lexerBuffer->forward == 0) {
            // Switch to the previous buffer
            lexerBuffer->currentBuffer = (lexerBuffer->currentBuffer == 1) ? 2 : 1;
            lexerBuffer->forward = BUFFER_SIZE - 1;
        } else {
            // Check which buffer is active before checking previous character
            char prevChar = (lexerBuffer->currentBuffer == 1) ? 
                lexerBuffer->buffer1[lexerBuffer->forward - 1] : 
                lexerBuffer->buffer2[lexerBuffer->forward - 1];

            if (prevChar == '\n') {
                lexerBuffer->lineNo--;  // Only decrement if crossing a newline
            }

            lexerBuffer->forward--;
        }
        count--;
    }
}


// Get the current lexeme
// Add to getLexeme():
static char* getLexeme() {
    char* lexeme = (char*)malloc(MAX_LEXEME_LEN * sizeof(char));
    int i = 0;
    int bufferIndex = lexerBuffer->begin;
    int bufferNum = lexerBuffer->currentBuffer;
    
    // Continue until we reach the current position or a token delimiter
    while (bufferIndex != lexerBuffer->forward && i < MAX_LEXEME_LEN - 1) {
        char c;
        if (bufferNum == 1) {
            c = lexerBuffer->buffer1[bufferIndex];
        } else {
            c = lexerBuffer->buffer2[bufferIndex];
        }
        
        // Check if we've reached a non-lexeme character
        // Don't break on alphanumeric chars or underscore, as they're valid in identifiers
        lexeme[i++] = c;
        
        bufferIndex++;
        if (bufferIndex >= BUFFER_SIZE) {
            bufferNum = (bufferNum == 1) ? 2 : 1;
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
                    //if (c == '\n') lexerBuffer->lineNo++;
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
                if (c == '<') {currentState = 22;  
                //goto case_22;
                }     // Less than branch
                else if (c == '>') currentState = 30;  // Greater than branch
                else if (c == '=') currentState = 20;  // Equals branch
                else if (c == '!') currentState = 21;  // Not equals branch
                else if (c == '&') currentState = 68;  // AND operator
                else if (c == '@') currentState = 37;  // @ for OR operator
                else if (c == '#') currentState = 33;  // # for record identifiers
                else if (c == '[') currentState = 41;  // Left square bracket
                else if (c == ']') currentState = 42;  // Right square bracket
                else if (c == '_') currentState = 2;  // Function identifier
                // First, modify the DFA transition in case 1:
                // else if (c >= 'a' && c <= 'z') {
                //     if (c >= 'b' && c <= 'd') {
                //         // Check if it's potentially a keyword starting with 'c'
                //         if (c == 'c') {
                //             char nextChar = getNextChar();
                //             if (nextChar == 'a') { // Potential keyword 'call'
                //                 retract(1);
                //                 currentState = 6; // Go to FIELDID path which handles keywords
                //             } else {
                //                 retract(1);
                //                 currentState = 8; // Go to the regular [b-d] identifier path
                //             }
                //         } else {
                //             currentState = 8;  // [b-d] identifiers (non-'c' case)
                //         }
                //     } else if (c >= 'a' && c <= 'z') {
                //         currentState = 6;  // Field identifiers and other identifiers
                //     }
                //     break;
                // }
                // Modified DFA transition in case 1 to handle both 'call' and 'chemistry'


                // // Fix compliant with exact language specifications
                // else if (c >= 'a' && c <= 'z') {
                //     if (c >= 'b' && c <= 'd') {
                //         // Special case for 'c' - check next character
                //         if (c == 'c') {
                //             // Peek at the next character
                //             char nextChar = getNextChar();
                //             // If next char is in range [2-7], treat as TK_ID pattern
                //             if (nextChar >= '2' && nextChar <= '7') {
                //                 retract(1);
                //                 currentState = 8;  // Regular identifier path for TK_ID
                //             } 
                //             // Otherwise, route to keyword/field ID path
                //             else {
                //                 retract(1);
                //                 currentState = 6;  // FIELDID/keyword path
                //             }
                //         } else {
                //             currentState = 8;  // Regular [b-d] identifier path
                //         }
                //     } else if (c >= 'a' && c <= 'z') {
                //         currentState = 6;  // Field identifiers and other identifiers
                //     }
                //     break;
                // }


                // Complete fix for handling words starting with b, c, and d
                else if (c >= 'a' && c <= 'z') {
                    if (c >= 'b' && c <= 'd') {
                        // Special handling for b, c, d characters which could be either keywords or identifiers
                        char nextChar = getNextChar();
                        
                        // For 'b' - check for keywords like "base", "beginpoint"
                        if (c == 'b') {
                            // If it's a letter, likely a keyword or field identifier, not an ID token
                            if ((nextChar >= 'a' && nextChar <= 'z') || nextChar == EOF || nextChar == '\n') {
                                retract(1);
                                currentState = 6;  // Route to FIELDID/keyword path
                            }
                            // If it's a valid digit for ID pattern, go to ID path
                            else if (nextChar >= '2' && nextChar <= '7') {
                                retract(1);
                                currentState = 8;  // Regular identifier path
                            }
                            // Default fallback
                            else {
                                retract(1);
                                currentState = 6;  // Default to FIELDID path
                            }
                        }
                        // For 'c' - check for keywords like "call"
                        else if (c == 'c') {
                            // If it's a letter, likely a keyword or field identifier, not an ID token
                            if ((nextChar >= 'a' && nextChar <= 'z') || nextChar == EOF || nextChar == '\n') {
                                retract(1);
                                currentState = 6;  // Route to FIELDID/keyword path
                            }
                            // If it's a valid digit for ID pattern, go to ID path
                            else if (nextChar >= '2' && nextChar <= '7') {
                                retract(1);
                                currentState = 8;  // Regular identifier path
                            }
                            // Default fallback
                            else {
                                retract(1);
                                currentState = 6;  // Default to FIELDID path
                            }
                        }
                        // For 'd' - check for keywords like "definetype"
                        else if (c == 'd') {
                            // If it's a letter, likely a keyword or field identifier, not an ID token
                            if ((nextChar >= 'a' && nextChar <= 'z') || nextChar == EOF || nextChar == '\n') {
                                retract(1);
                                currentState = 6;  // Route to FIELDID/keyword path
                            }
                            // If it's a valid digit for ID pattern, go to ID path
                            else if (nextChar >= '2' && nextChar <= '7') {
                                retract(1);
                                currentState = 8;  // Regular identifier path
                            }
                            // Default fallback
                            else {
                                retract(1);
                                currentState = 6;  // Default to FIELDID path
                            }
                        }
                    } else if (c >= 'a' && c <= 'z') {
                        currentState = 6;  // Field identifiers and other identifiers
                    }
                    break;
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
                        case '~':
                            token->type = TK_NOT;
                            token->lexeme = strdup("~");
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

            case 6: // First char of FIELDID [a-z]
                if (c >= 'a' && c <= 'z') {
                    currentState = 7;  // Continue FIELDID path
                } else {
                    retract(1);
                    char* lexeme = getLexeme();
                    TokenType keywordType = lookupKeyword(keywordTable, lexeme);
                    if (keywordType != TK_ID) {
                        token->type = keywordType;  // It's a keyword
                    } else {
                        token->type = TK_FIELDID;   // Not a keyword, so it's a FIELDID
                    }
                    token->lexeme = lexeme;
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;
            
            case 7: // Rest of FIELDID [a-z]*
                if (c >= 'a' && c <= 'z') {
                    currentState = 7;  // Stay in state 7 for more lowercase letters
                } else {
                    retract(1);
                    char* lexeme = getLexeme();
                    TokenType keywordType = lookupKeyword(keywordTable, lexeme);
                    if (keywordType != TK_ID) {
                        token->type = keywordType;  // It's a keyword
                    } else {
                        token->type = TK_FIELDID;   // Not a keyword, so it's a FIELDID
                    }
                    token->lexeme = lexeme;
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;
            
            case 8: // First char of TK_ID [b-d]
                if (c >= '2' && c <= '7') {
                    currentState = 9;
                } else {
                    retract(1);
                    token->type = TK_ERROR;
                    token->lexeme = getLexeme();
                    token->lineNo = lexerBuffer->lineNo;
                    token->errorType = 3;
                    return token;
                }
                break;
            
            case 9: // After [b-d][2-7] for TK_ID
                if ((c >= 'b' && c <= 'd')){ //|| (c >= '2' && c <= '7')) {
                    currentState = 10;
                } 
                else if ((c>='2'&&c<='7')){
                    currentState=11;
                }else {
                    retract(1);
                    char* lexeme = getLexeme();
                    if (strlen(lexeme) > MAX_ID_LEN) {
                        token->type = TK_ERROR;
                        token->errorType = 1;  // Length error
                    } else {
                        token->type = TK_ID;
                    }
                    token->lexeme = lexeme;
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;
            
            case 10: // Rest of TK_ID [b-d2-7]*
                if ((c >= 'b' && c <= 'd')){ //|| (c >= '2' && c <= '7')) {
                    currentState = 10;  // Stay in state 10 for [b-d2-7]*
                }
                else if ((c>='2'&&c<='7')){
                    currentState=11;
                } else {
                    retract(1);
                    char* lexeme = getLexeme();
                    if (strlen(lexeme) > MAX_ID_LEN) {
                        token->type = TK_ERROR;
                        token->errorType = 1;  // Length error
                    } else {
                        token->type = TK_ID;
                    }
                    token->lexeme = lexeme;
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;

            case 11:
                if ((c >= '2' && c <= '7')){ 
                    currentState = 11;  // Stay in state 10 for [b-d2-7]*
                }
                else{
                    retract(1);
                    char* lexeme = getLexeme();
                    if(strlen(lexeme)>MAX_ID_LEN){
                        token->type = TK_ERROR;
                        token->errorType = 1;
                    } else {
                        token->type = TK_ID;
                    }
                    token->lexeme = lexeme;
                    token->lineNo = lexerBuffer->lineNo;
                    return token;
                }
                break;

            // case 20: // Equals state
            //     //c = getNextChar();
            //     if (c == '=') {
            //         token->type = TK_EQ;
            //         token->lexeme = strdup("==");
            //         token->lineNo = lexerBuffer->lineNo;
            //         lexerBuffer->begin = lexerBuffer->forward;
            //         return token;
            //     }
            //     token->type = TK_ERROR;
            //     token->lexeme = strdup("=");
            //     token->lineNo = lexerBuffer->lineNo;
            //     token->errorType = 2;
            //     retract(2);
            //     return token;

            // case 21: // Not equals state
            //     //c = getNextChar();
            //     if (c == '=') {
            //         token->type = TK_NE;
            //         token->lexeme = strdup("!=");
            //         token->lineNo = lexerBuffer->lineNo;
            //         lexerBuffer->begin = lexerBuffer->forward;
            //         return token;
            //     }
            //     retract(1);
            //     token->type = TK_ERROR;
            //     token->lexeme = strdup("!");
            //     token->lineNo = lexerBuffer->lineNo;
            //     token->errorType = 2;
            //     return token;

            // case_22:
            // case 22: // Detect `<`
            //     c = getNextChar();
            //     if (c == '-')
            //     {
            //         c = getNextChar();
            //         if (c == '-')
            //         {
            //             c = getNextChar();
            //             if (c == '-')
            //             {
            //                 // ✅ Successfully found `<---`
            //                 token->type = TK_ASSIGNOP;
            //                 token->lexeme = strdup("<---");
            //                 token->lineNo = lexerBuffer->lineNo;
            //                 lexerBuffer->begin = lexerBuffer->forward;
            //                 return token;
            //             }
            //             // ❌ Found `<--` but no third `-`, retract 2 and return `<`
            //             retract(2);
            //         }
            //         else
            //         {
            //             // ❌ Found `<-` but no second `-`, retract 1 and return `<`
            //             retract(1);
            //         }
            //     }
            //     else if (c == '=')
            //     {
            //         // ✅ Successfully found `<=`
            //         token->type = TK_LE;
            //         token->lexeme = strdup("<=");
            //         token->lineNo = lexerBuffer->lineNo;
            //         lexerBuffer->begin = lexerBuffer->forward;
            //         return token;
            //     }
            //     else
            //     {
            //         // If none of the above matched, return `<` alone.
            //         retract(1);
            //     }
            //     lexerBuffer->begin = lexerBuffer->forward;
            //     token->type = TK_LT;
            //     token->lexeme = strdup("<");
            //     token->lineNo = lexerBuffer->lineNo;
            //     return token;

            case 20: // Equals state
                //c = getNextChar();
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
                retract(1);
                return token;

            case 21: // Not equals state
                //c = getNextChar();
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
                //c = getNextChar();
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
                //c = getNextChar();
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
                //c = getNextChar();
                    if (c == '-') {
                        token->type = TK_ASSIGNOP;
                        token->lexeme = strdup("<---");
                        token->lineNo = lexerBuffer->lineNo;
                        lexerBuffer->begin = lexerBuffer->forward;
                        return token;
                    }
                    else {
                        retract(3);
                        token->type=TK_LT;
                        token->lexeme = strdup("<");
                        token->lineNo = lexerBuffer->lineNo;
                        lexerBuffer->begin = lexerBuffer->forward; 
                        return token;
                    }
                    break;
                

            // case 26: // Second - of assignment
            //     c = getNextChar();
            //     if (c == '-') {
            //         token->type = TK_ASSIGNOP;
            //         token->lexeme = strdup("<---");
            //         token->lineNo = lexerBuffer->lineNo;
            //         lexerBuffer->begin = lexerBuffer->forward;
            //         return token;
            //     }
            //     retract(3);
            //     token->type = TK_LT;
            //     token->lexeme = strdup("<");
            //     token->lineNo = lexerBuffer->lineNo;
            //     return token;

            case 30: // Greater than state
                //c = getNextChar();
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

            case 33: // # state - start of record id
                if (isalpha(c) && islower(c)) {
                    numBuffer[numLen++] = '#';
                    numBuffer[numLen++] = c;
                    currentState = 35;
                } else {
                    token->type = TK_ERROR;
                    token->lexeme = strdup("#");
                    token->lineNo = lexerBuffer->lineNo;
                    token->errorType = 3;  // Unknown pattern
                    return token;
                }
                break;
            
            case 35: // Rest of record identifier [a-z]*
                if (isalpha(c) && islower(c)) {
                    numBuffer[numLen++] = c;
                    continue;
                } else {
                    retract(1);
                    numBuffer[numLen] = '\0';
                    token->type = TK_RUID;  // Changed from TK_RECORDID to TK_RUID
                    token->lexeme = strdup(numBuffer);
                    token->lineNo = lexerBuffer->lineNo;
                    numLen = 0;  // Reset numBuffer
                    return token;
                }
                break;
    
            

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
                retract(1);  
                return token;

            case 42: // ] state from DFA
                token->type = TK_SQR;
                token->lexeme = strdup("]");
                token->lineNo = lexerBuffer->lineNo;
                retract(1);  
                return token;

            case 43: // Number start state 
                if (isdigit(c)) {
                    numBuffer[numLen++] = c;
                }
                else if (c == '.') {
                    numBuffer[numLen++] = c;
                    currentState = 46;  // Move to decimal point state
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

            case 46: // First decimal digit state
                if (isdigit(c)) {
                    numBuffer[numLen++] = c;
                    currentState = 47;  // Move to second decimal digit state
                }
                else {
                    token->type = TK_ERROR;
                    token->lexeme = strdup(numBuffer);
                    token->lineNo = lexerBuffer->lineNo;
                    token->errorType = 3;  // Unknown pattern
                    return token;
                }
                break;

            case 47: // Second decimal digit state
                if (isdigit(c)) {
                    numBuffer[numLen++] = c;
                    currentState = 48;  // New state to check for E/e or end
                }
                else {
                    token->type = TK_ERROR;
                    token->lexeme = strdup(numBuffer);
                    token->lineNo = lexerBuffer->lineNo;
                    token->errorType = 3;  // Unknown pattern
                    retract(1);
                    return token;
                }
                break;
            
            case 48: // After two decimal places
                if (c == 'E' || c == 'e') {
                    numBuffer[numLen++] = c;
                    c = getNextChar();
                    if (c == '+' || c == '-') {
                        numBuffer[numLen++] = c;
                        //c = getNextChar();  // Read first exponent digit after sign
                    }
                    currentState = 52;  // Move to exponent state
                } else {
                    retract(1);
                    token->type = TK_RNUM;
                    token->lexeme = strdup(numBuffer);
                    token->value.realValue = atof(numBuffer);
                    token->lineNo = lexerBuffer->lineNo;
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

            case 52: { // Exponent part
                    int exponentDigits = 0;
                    if (!isdigit(c)) {
                        token->type = TK_ERROR;
                        token->lexeme = strdup(numBuffer);
                        token->lineNo = lexerBuffer->lineNo;
                        token->errorType = 3;
                        return token;
                    }
                    // Loop to accept exactly 2 digits
                    while (isdigit(c) && exponentDigits < 2) {
                        numBuffer[numLen++] = c;
                        c = getNextChar();
                        exponentDigits++;
                    }
                    
                    // If more than 2 digits, retract and stop processing exponent
                    retract(1);
    
                    // Finalize the token
                    numBuffer[numLen] = '\0';
                    token->type = TK_RNUM;
                    token->lexeme = strdup(numBuffer);
                    token->value.realValue = atof(numBuffer);
                    token->lineNo = lexerBuffer->lineNo;
                    lexerBuffer->begin = lexerBuffer->forward;
                    return token;
                }
                

            case 68: // First & state
                if (c == '&') {
                    currentState = 69;
                } else {
                    retract(1);
                    token->type = TK_ERROR;
                    token->lexeme = strdup("&");
                    token->lineNo = lexerBuffer->lineNo;
                    token->errorType = 2;  // Unknown symbol
                    lexerBuffer->begin = lexerBuffer->forward;  // Ensure begin is updated
                    return token;
                }
                break;
            
            case 69: // && state
                if (c == '&') {
                    token->type = TK_AND;
                    token->lexeme = strdup("&&&");
                    token->lineNo = lexerBuffer->lineNo;
                    lexerBuffer->begin = lexerBuffer->forward;  // Ensure begin is updated
                    return token;
                } else {
                    retract(1);
                    token->type = TK_ERROR;
                    token->lexeme = strdup("&&");
                    token->lineNo = lexerBuffer->lineNo;
                    token->errorType = 3;  // Unknown pattern
                    lexerBuffer->begin = lexerBuffer->forward;  // Ensure begin is updated for next token
                    return token;
                }
                break;
            
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