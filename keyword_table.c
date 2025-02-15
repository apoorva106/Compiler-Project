#include <string.h>
#include <stdlib.h>
#include "keyword_table.h"

// Simple hash function for keywords
static unsigned int hash(const char* str) {
    unsigned int hash = 0;
    while (*str) {
        hash = (hash * 31 + *str) % 128;
        str++;
    }
    return hash;
}

// Initialize keyword table with all keywords
KeywordTable* initKeywordTable(void) {
    KeywordTable* table = (KeywordTable*)malloc(sizeof(KeywordTable));
    memset(table->entries, 0, sizeof(table->entries));

    // Define all keywords and their corresponding tokens
    struct {
        const char* keyword;
        TokenType token;
    } keywords[] = {
        {"_main", TK_MAIN},
        {"call", TK_CALL},
        {"else", TK_ELSE},
        {"end", TK_END},
        {"endif", TK_ENDIF},
        {"endrecord", TK_ENDRECORD},
        {"endunion", TK_ENDUNION},
        {"global", TK_GLOBAL},
        {"if", TK_IF},
        {"input", TK_INPUT},
        {"int", TK_INT},
        {"list", TK_LIST},
        {"output", TK_OUTPUT},
        {"parameter", TK_PARAMETER},
        {"parameters", TK_PARAMETERS},
        {"read", TK_READ},
        {"real", TK_REAL},
        {"record", TK_RECORD},
        {"return", TK_RETURN},
        {"then", TK_THEN},
        {"type", TK_TYPE},
        {"union", TK_UNION},
        {"with", TK_WITH},
        {"write", TK_WRITE},
        {NULL, 0}
    };

    // Insert all keywords into the table
    for (int i = 0; keywords[i].keyword != NULL; i++) {
        unsigned int h = hash(keywords[i].keyword);
        KeywordEntry* entry = (KeywordEntry*)malloc(sizeof(KeywordEntry));
        entry->keyword = strdup(keywords[i].keyword);
        entry->token = keywords[i].token;
        entry->next = table->entries[h];
        table->entries[h] = entry;
    }

    return table;
}

// Lookup a keyword in the table
TokenType lookupKeyword(KeywordTable* table, const char* keyword) {
    printf("DEBUG: Looking up keyword: %s\n", keyword);
    if (strcmp(keyword, "_main") == 0) {
        printf("DEBUG: Found _main match\n");
        return TK_MAIN;
    }

    unsigned int h = hash(keyword);
    KeywordEntry* entry = table->entries[h];
    
    while (entry) {
        if (strcmp(entry->keyword, keyword) == 0) {
            return entry->token;
        }
        entry = entry->next;
    }
    
    return TK_ID;  // Not a keyword
}

// Free the keyword table
void freeKeywordTable(KeywordTable* table) {
    for (int i = 0; i < 128; i++) {
        KeywordEntry* entry = table->entries[i];
        while (entry) {
            KeywordEntry* next = entry->next;
            free(entry->keyword);
            free(entry);
            entry = next;
        }
    }
    free(table);
}