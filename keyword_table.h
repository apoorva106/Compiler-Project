// keyword_table.h
#ifndef KEYWORD_TABLE_H
#define KEYWORD_TABLE_H

#include "lexer.h"

// Keyword entry structure
typedef struct KeywordEntry {
    char* keyword;
    TokenType token;
    struct KeywordEntry* next;
} KeywordEntry;

// Keyword table structure
typedef struct {
    KeywordEntry* entries[128];  // Simple hash table
} KeywordTable;

KeywordTable* initKeywordTable(void);
TokenType lookupKeyword(KeywordTable* table, const char* keyword);
void freeKeywordTable(KeywordTable* table);

#endif