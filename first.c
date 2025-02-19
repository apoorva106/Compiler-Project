#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Set structure and functions
typedef struct {
    char** symbols;
    int count;
    int capacity;
} Set;

typedef struct {
    char* left;  // Left-hand side of the production
    char** right;  // Right-hand side of the production
    int rightCount;  // Number of symbols on the right-hand side
} Production;

typedef struct {
    Production* productions; // Array of production rules
    int productionCount; // Number of productions
    char** nonTerminals; // List of non-terminal symbols
    int nonTerminalCount; // Number of non-terminals
    char** terminals; // List of terminal symbols
    int terminalCount; // Number of terminals
} Grammar;


// yaha best test krne k liye I have hardcoded a grammar S->AB,A->a|epsilon, B-> b|epsion
Grammar* createTestGrammar() {
    Grammar* grammar = (Grammar*)malloc(sizeof(Grammar));
    
   // Defining non-terminals
    grammar->nonTerminalCount = 3;
    grammar->nonTerminals = (char**)malloc(sizeof(char*) * grammar->nonTerminalCount);
    grammar->nonTerminals[0] = strdup("S");
    grammar->nonTerminals[1] = strdup("A");
    grammar->nonTerminals[2] = strdup("B");

    // Defining terminals
    grammar->terminalCount = 2;
    grammar->terminals = (char**)malloc(sizeof(char*) * grammar->terminalCount);
    grammar->terminals[0] = strdup("a");
    grammar->terminals[1] = strdup("b");

    // Defining production rules
    grammar->productionCount = 5;
    grammar->productions = (Production*)malloc(sizeof(Production) * grammar->productionCount);

    // S -> AB
    grammar->productions[0].left = strdup("S");
    grammar->productions[0].right = (char**)malloc(sizeof(char*) * 2);
    grammar->productions[0].right[0] = strdup("A");
    grammar->productions[0].right[1] = strdup("B");
    grammar->productions[0].rightCount = 2;

    // A-> a | ε
    grammar->productions[1].left = strdup("A");
    grammar->productions[1].right = (char**)malloc(sizeof(char*) * 1);
    grammar->productions[1].right[0] = strdup("a"); 
    grammar->productions[1].rightCount = 1;

    grammar->productions[4].left = strdup("A");
    grammar->productions[4].right = (char**)malloc(sizeof(char*));
    grammar->productions[4].right[0] = strdup("ε");
    grammar->productions[4].rightCount = 1;

    // b -> b | ε
    grammar->productions[2].left = strdup("B");
    grammar->productions[2].right = (char**)malloc(sizeof(char*) * 1);
    grammar->productions[2].right[0] = strdup("b");
    grammar->productions[2].rightCount = 1;

    grammar->productions[3].left = strdup("B");
    grammar->productions[3].right = (char**)malloc(sizeof(char*));
    grammar->productions[3].right[0] = strdup("ε");
    grammar->productions[3].rightCount = 1;


    return grammar;
}

void printSet(Set* set) {
    printf("{ ");
    for (int i = 0; i < set->count; i++) {
        printf("%s ", set->symbols[i]);
        if (i < set->count - 1) {
            printf(", ");
        }
    }
    printf("}");
}

Set* createSet() {
    Set* set = (Set*)malloc(sizeof(Set));
    set->capacity = 10;
    set->symbols = (char**)malloc(sizeof(char*) * set->capacity);
    set->count = 0;
    return set;
}

void addToSet(Set* set, char* symbol) {
    for (int i = 0; i < set->count; i++) {
        if (strcmp(set->symbols[i], symbol) == 0) {
            return;  // Symbol already in set
        }
    }
    
    if (set->count == set->capacity) {
        set->capacity *= 2;
        set->symbols = (char**)realloc(set->symbols, sizeof(char*) * set->capacity);
    }
    
    set->symbols[set->count] = strdup(symbol);
    set->count++;
}

void addSetToSet(Set* dest, Set* src) {
    for (int i = 0; i < src->count; i++) {
        addToSet(dest, src->symbols[i]);
    }
}

void freeSet(Set* set) {
    for (int i = 0; i < set->count; i++) {
        free(set->symbols[i]);
    }
    free(set->symbols);
    free(set);
}

// Grammar helper functions
bool isTerminal(Grammar* grammar, char* symbol) {
    for (int i = 0; i < grammar->terminalCount; i++) {
        if (strcmp(grammar->terminals[i], symbol) == 0) {
            return true;
        }
    }
    return false;
}

bool containsEpsilon(Set* set) {
    for (int i = 0; i < set->count; i++) {
        if (strcmp(set->symbols[i], "ε") == 0) {
            return true;
        }
    }
    return false;
}

// Memory deallocation for Grammar
void freeGrammar(Grammar* grammar) {
    for (int i = 0; i < grammar->nonTerminalCount; i++) {
        free(grammar->nonTerminals[i]);
    }
    free(grammar->nonTerminals);

    for (int i = 0; i < grammar->terminalCount; i++) {
        free(grammar->terminals[i]);
    }
    free(grammar->terminals);

    for (int i = 0; i < grammar->productionCount; i++) {
        free(grammar->productions[i].left);
        for (int j = 0; j < grammar->productions[i].rightCount; j++) {
            free(grammar->productions[i].right[j]);
        }
        free(grammar->productions[i].right);
    }
    free(grammar->productions);

    free(grammar);
}

// FIRST set calculation (implementation of the function we discussed earlier)
Set* calculateFirst(Grammar* grammar, char* symbol) {
    Set* firstSet = createSet();
    
    if (isTerminal(grammar, symbol) || strcmp(symbol, "ε") == 0) {
        addToSet(firstSet, symbol);
        return firstSet;
    }
    
    for (int i = 0; i < grammar->productionCount; i++) {
        if (strcmp(grammar->productions[i].left, symbol) == 0) {
            char** right = grammar->productions[i].right;
            int rightCount = grammar->productions[i].rightCount;
            
            if (rightCount == 0 || strcmp(right[0], "ε") == 0) {
                addToSet(firstSet, "ε");
            } else {
                for (int j = 0; j < rightCount; j++) {
                    Set* symbolFirst = calculateFirst(grammar, right[j]);
                    addSetToSet(firstSet, symbolFirst);
                    
                    if (!containsEpsilon(symbolFirst)) {
                        freeSet(symbolFirst);
                        break;
                    }
                    
                    if (j == rightCount - 1 && containsEpsilon(symbolFirst)) {
                        addToSet(firstSet, "ε");
                    }
                    freeSet(symbolFirst);
                }
            }
        }
    }
    
    return firstSet;
}


int main() {
    Grammar* grammar = createTestGrammar();
    
    // Test FIRST set calculation for each non-terminal
    for (int i = 0; i < grammar->nonTerminalCount; i++) {
        char* symbol = grammar->nonTerminals[i];
        printf("FIRST(%s) = ", symbol);
        Set* firstSet = calculateFirst(grammar, symbol);
        printSet(firstSet);
        printf("\n");
        // Don't forget to free the set after use
        freeSet(firstSet);
    }

    // Free the grammar
    freeGrammar(grammar);

    return 0;
}

