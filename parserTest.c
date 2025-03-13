#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include "parser.h"

#define EPSILON_TOKEN "TK_EPS"
#define DOLLAR_TOKEN "TK_DOLLAR"

// Helper functions for symbol and rule management
Symbol* createSymbol(bool isTerminal, int id) {
    Symbol* symbol = (Symbol*)malloc(sizeof(Symbol));
    symbol->isTerminal = isTerminal;
    if (isTerminal) {
        symbol->id.terminal = id;
    } else {
        symbol->id.nonTerminal = id;
    }
    symbol->next = NULL;
    return symbol;
}

SymbolList* createSymbolList() {
    SymbolList* list = (SymbolList*)malloc(sizeof(SymbolList));
    list->head = NULL;
    list->tail = NULL;
    list->length = 0;
    return list;
}

void addSymbolToList(SymbolList* list, Symbol* symbol) {
    if (list->head == NULL) {
        list->head = symbol;
        list->tail = symbol;
    } else {
        list->tail->next = symbol;
        list->tail = symbol;
    }
    list->length++;
}

Rule* createRule(SymbolList* symbols, int ruleNumber) {
    Rule* rule = (Rule*)malloc(sizeof(Rule));
    rule->symbols = symbols;
    rule->ruleNumber = ruleNumber;
    return rule;
}

// Find the index of a terminal or non-terminal in the grammar
int findTerminalIndex(Grammar* grammar, const char* terminal) {
    for (int i = 0; i < grammar->numTerminals; i++) {
        if (strcmp(grammar->terminals[i], terminal) == 0) {
            return i;
        }
    }
    return -1;
}

int findNonTerminalIndex(Grammar* grammar, const char* nonTerminal) {
    for (int i = 0; i < grammar->numNonTerminals; i++) {
        if (strcmp(grammar->nonTerminals[i], nonTerminal) == 0) {
            return i;
        }
    }
    return -1;
}

// Read grammar from a file
Grammar* readGrammarFromFile(const char* filename) {
    Grammar* grammar = (Grammar*)malloc(sizeof(Grammar));
    grammar->numTerminals = 0;
    grammar->numNonTerminals = 0;
    grammar->numRules = 0;
    
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error opening grammar file: %s\n", filename);
        exit(1);
    }
    
    // First pass: count rules and identify symbols
    char line[256];
    int ruleCount = 0;
    
    while (fgets(line, sizeof(line), file)) {
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
            len--;
        }
        
        if (len == 0) continue; // Skip empty lines
        
        // Split into tokens
        char* tokens[MAX_RULE_LENGTH];
        int tokenCount = 0;
        
        char* token = strtok(line, " ");
        while (token != NULL && tokenCount < MAX_RULE_LENGTH) {
            tokens[tokenCount++] = token;
            token = strtok(NULL, " ");
        }
        
        if (tokenCount < 2) continue; // Skip invalid lines
        
        // First token is LHS non-terminal
        if (findNonTerminalIndex(grammar, tokens[0]) == -1) {
            strcpy(grammar->nonTerminals[grammar->numNonTerminals], tokens[0]);
            grammar->numNonTerminals++;
        }
        
        // Rest are RHS symbols
        for (int i = 1; i < tokenCount; i++) {
            // If starts with TK_, it's a terminal
            if (strncmp(tokens[i], "TK_", 3) == 0) {
                if (findTerminalIndex(grammar, tokens[i]) == -1) {
                    strcpy(grammar->terminals[grammar->numTerminals], tokens[i]);
                    grammar->numTerminals++;
                }
            } 
            // Otherwise, it's a non-terminal
            else if (strcmp(tokens[i], EPSILON_TOKEN) != 0) {
                if (findNonTerminalIndex(grammar, tokens[i]) == -1) {
                    strcpy(grammar->nonTerminals[grammar->numNonTerminals], tokens[i]);
                    grammar->numNonTerminals++;
                }
            }
        }
        
        ruleCount++;
    }
    
    // Add DOLLAR terminal if not already present
    if (findTerminalIndex(grammar, DOLLAR_TOKEN) == -1) {
        strcpy(grammar->terminals[grammar->numTerminals], DOLLAR_TOKEN);
        grammar->numTerminals++;
    }
    
    // Allocate rules array
    grammar->rules = (Rule**)malloc((ruleCount + 1) * sizeof(Rule*));
    grammar->rules[0] = NULL; // Rule 0 is empty for 1-based indexing
    
    // Second pass: create rules
    rewind(file);
    int currentRule = 1;
    
    // Create array to track rule ranges for non-terminals
    NonTerminalRules* ntRules = (NonTerminalRules*)malloc(grammar->numNonTerminals * sizeof(NonTerminalRules));
    for (int i = 0; i < grammar->numNonTerminals; i++) {
        ntRules[i].startRule = -1;
        ntRules[i].endRule = -1;
    }
    
    int lastNonTerminal = -1;
    
    while (fgets(line, sizeof(line), file)) {
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
            len--;
        }
        
        if (len == 0) continue; // Skip empty lines
        
        // Split into tokens
        char* tokens[MAX_RULE_LENGTH];
        int tokenCount = 0;
        
        char* token = strtok(line, " ");
        while (token != NULL && tokenCount < MAX_RULE_LENGTH) {
            tokens[tokenCount++] = token;
            token = strtok(NULL, " ");
        }
        
        if (tokenCount < 2) continue; // Skip invalid lines
        
        // Create symbol list for the rule
        SymbolList* symbolList = createSymbolList();
        
        // First token is LHS non-terminal
        int ntIndex = findNonTerminalIndex(grammar, tokens[0]);
        Symbol* lhs = createSymbol(false, ntIndex);
        addSymbolToList(symbolList, lhs);
        
        // Track rule ranges
        if (lastNonTerminal != ntIndex) {
            if (lastNonTerminal != -1) {
                ntRules[lastNonTerminal].endRule = currentRule - 1;
            }
            ntRules[ntIndex].startRule = currentRule;
            lastNonTerminal = ntIndex;
        }
        
        // Set start symbol to the first non-terminal in the grammar
        if (currentRule == 1) {
            strcpy(grammar->startSymbol, tokens[0]);
        }
        
        // Rest are RHS symbols
        for (int i = 1; i < tokenCount; i++) {
            Symbol* symbol;
            
            // If starts with TK_, it's a terminal
            if (strncmp(tokens[i], "TK_", 3) == 0) {
                int termIndex = findTerminalIndex(grammar, tokens[i]);
                symbol = createSymbol(true, termIndex);
            } 
            // Epsilon is a special terminal
            else if (strcmp(tokens[i], EPSILON_TOKEN) == 0) {
                int epsIndex = findTerminalIndex(grammar, EPSILON_TOKEN);
                symbol = createSymbol(true, epsIndex);
            }
            // Otherwise, it's a non-terminal
            else {
                int ntIdx = findNonTerminalIndex(grammar, tokens[i]);
                symbol = createSymbol(false, ntIdx);
            }
            
            addSymbolToList(symbolList, symbol);
        }
        
        // Create and store the rule
        Rule* rule = createRule(symbolList, currentRule);
        grammar->rules[currentRule] = rule;
        currentRule++;
    }
    
    // Set end rule for the last non-terminal
    if (lastNonTerminal != -1) {
        ntRules[lastNonTerminal].endRule = currentRule - 1;
    }
    
    grammar->numRules = currentRule - 1;
    
    fclose(file);
    
    // Return both grammar and rule ranges
    return grammar;
}

// Print the grammar for debugging
void printGrammar(Grammar* grammar) {
    printf("Grammar:\n");
    printf("Start Symbol: %s\n", grammar->startSymbol);
    
    printf("Non-terminals (%d): ", grammar->numNonTerminals);
    for (int i = 0; i < grammar->numNonTerminals; i++) {
        printf("%s ", grammar->nonTerminals[i]);
    }
    printf("\n");
    
    printf("Terminals (%d): ", grammar->numTerminals);
    for (int i = 0; i < grammar->numTerminals; i++) {
        printf("%s ", grammar->terminals[i]);
    }
    printf("\n");
    
    printf("Rules (%d):\n", grammar->numRules);
    for (int i = 1; i <= grammar->numRules; i++) {
        Rule* rule = grammar->rules[i];
        Symbol* symbol = rule->symbols->head;
        
        // Print LHS
        printf("%s -> ", grammar->nonTerminals[symbol->id.nonTerminal]);
        
        // Print RHS
        symbol = symbol->next;
        while (symbol != NULL) {
            if (symbol->isTerminal) {
                printf("%s ", grammar->terminals[symbol->id.terminal]);
            } else {
                printf("%s ", grammar->nonTerminals[symbol->id.nonTerminal]);
            }
            symbol = symbol->next;
        }
        printf("\n");
    }
}

// Initialize First and Follow sets
FirstAndFollow* initializeFirstAndFollow(Grammar* grammar) {
    FirstAndFollow* fafl = (FirstAndFollow*)malloc(sizeof(FirstAndFollow));
    
    // Allocate and initialize first sets
    fafl->first = (bool**)malloc(grammar->numNonTerminals * sizeof(bool*));
    fafl->firstHasEpsilon = (bool*)calloc(grammar->numNonTerminals, sizeof(bool));
    
    for (int i = 0; i < grammar->numNonTerminals; i++) {
        fafl->first[i] = (bool*)calloc(grammar->numTerminals, sizeof(bool));
    }
    
    // Allocate and initialize follow sets
    fafl->follow = (bool**)malloc(grammar->numNonTerminals * sizeof(bool*));
    
    for (int i = 0; i < grammar->numNonTerminals; i++) {
        fafl->follow[i] = (bool*)calloc(grammar->numTerminals, sizeof(bool));
    }
    
    return fafl;
}

// Compute First sets for all non-terminals
void computeFirst(Grammar* grammar, FirstAndFollow* fafl, bool* computed, int nonTerminalIndex) {
    // If already computed, return
    if (computed[nonTerminalIndex]) return;
    
    // Mark as computed to avoid infinite recursion
    computed[nonTerminalIndex] = true;
    
    // Find rules where this non-terminal is on the LHS
    for (int i = 1; i <= grammar->numRules; i++) {
        Rule* rule = grammar->rules[i];
        Symbol* lhs = rule->symbols->head;
        
        // Skip if not the non-terminal we're looking for
        if (lhs->id.nonTerminal != nonTerminalIndex) continue;
        
        // Process the first symbol of the RHS
        Symbol* symbol = lhs->next;
        
        // If RHS is ε, add it to FIRST set
        if (symbol != NULL && symbol->isTerminal && 
            strcmp(grammar->terminals[symbol->id.terminal], EPSILON_TOKEN) == 0) {
            fafl->firstHasEpsilon[nonTerminalIndex] = true;
            continue;
        }
        
        // Process symbols in the RHS
        bool allCanDeriveEpsilon = true;
        
        while (symbol != NULL && allCanDeriveEpsilon) {
            if (symbol->isTerminal) {
                // If it's a terminal, add it to FIRST set
                fafl->first[nonTerminalIndex][symbol->id.terminal] = true;
                allCanDeriveEpsilon = false;
            } else {
                // If it's a non-terminal, compute its FIRST set first
                int ntIndex = symbol->id.nonTerminal;
                if (!computed[ntIndex]) {
                    computeFirst(grammar, fafl, computed, ntIndex);
                }
                
                // Add all terminals in FIRST(ntIndex) to FIRST(nonTerminalIndex)
                for (int j = 0; j < grammar->numTerminals; j++) {
                    if (fafl->first[ntIndex][j]) {
                        fafl->first[nonTerminalIndex][j] = true;
                    }
                }
                
                // If this non-terminal cannot derive ε, we're done
                if (!fafl->firstHasEpsilon[ntIndex]) {
                    allCanDeriveEpsilon = false;
                }
            }
            
            symbol = symbol->next;
        }
        
        // If all symbols in RHS can derive ε, then the non-terminal can too
        if (allCanDeriveEpsilon) {
            fafl->firstHasEpsilon[nonTerminalIndex] = true;
        }
    }
}

// Function to get the first set of a sequence of symbols
void getFirstOfSequence(Grammar* grammar, FirstAndFollow* fafl, Symbol* startSymbol, 
                        bool* firstSet, bool* derivesEpsilon) {
    // Initialize result
    *derivesEpsilon = true;
    memset(firstSet, 0, grammar->numTerminals * sizeof(bool));
    
    Symbol* symbol = startSymbol;
    
    while (symbol != NULL && *derivesEpsilon) {
        if (symbol->isTerminal) {
            // For terminal, just add it to the result
            if (strcmp(grammar->terminals[symbol->id.terminal], EPSILON_TOKEN) != 0) {
                firstSet[symbol->id.terminal] = true;
                *derivesEpsilon = false;
            }
        } else {
            // For non-terminal, add its FIRST set
            for (int i = 0; i < grammar->numTerminals; i++) {
                if (fafl->first[symbol->id.nonTerminal][i]) {
                    firstSet[i] = true;
                }
            }
            
            // If it can't derive ε, we're done
            if (!fafl->firstHasEpsilon[symbol->id.nonTerminal]) {
                *derivesEpsilon = false;
            }
        }
        
        symbol = symbol->next;
    }
}

// Compute Follow sets for all non-terminals
void computeFollow(Grammar* grammar, FirstAndFollow* fafl) {
    // Add $ to FOLLOW of the start symbol
    int startSymbolIndex = findNonTerminalIndex(grammar, grammar->startSymbol);
    int dollarIndex = findTerminalIndex(grammar, DOLLAR_TOKEN);
    fafl->follow[startSymbolIndex][dollarIndex] = true;
    
    bool changed;
    do {
        changed = false;
        
        // For each rule
        for (int i = 1; i <= grammar->numRules; i++) {
            Rule* rule = grammar->rules[i];
            Symbol* lhs = rule->symbols->head;
            Symbol* symbols = lhs->next;
            
            // For each non-terminal B in the RHS
            Symbol* symbol = symbols;
            while (symbol != NULL) {
                // Skip terminals
                if (symbol->isTerminal) {
                    symbol = symbol->next;
                    continue;
                }
                
                int B = symbol->id.nonTerminal;
                
                // Get the sequence of symbols after B
                Symbol* beta = symbol->next;
                
                // If there are symbols after B
                if (beta != NULL) {
                    // Compute FIRST(beta)
                    bool firstSet[grammar->numTerminals];
                    bool derivesEpsilon;
                    getFirstOfSequence(grammar, fafl, beta, firstSet, &derivesEpsilon);
                    
                    // Add FIRST(beta) - {ε} to FOLLOW(B)
                    for (int j = 0; j < grammar->numTerminals; j++) {
                        if (firstSet[j] && !fafl->follow[B][j]) {
                            fafl->follow[B][j] = true;
                            changed = true;
                        }
                    }
                    
                    // If beta =>* ε, add FOLLOW(A) to FOLLOW(B)
                    if (derivesEpsilon) {
                        for (int j = 0; j < grammar->numTerminals; j++) {
                            if (fafl->follow[lhs->id.nonTerminal][j] && !fafl->follow[B][j]) {
                                fafl->follow[B][j] = true;
                                changed = true;
                            }
                        }
                    }
                } 
                // If B is the last symbol, add FOLLOW(A) to FOLLOW(B)
                else {
                    for (int j = 0; j < grammar->numTerminals; j++) {
                        if (fafl->follow[lhs->id.nonTerminal][j] && !fafl->follow[B][j]) {
                            fafl->follow[B][j] = true;
                            changed = true;
                        }
                    }
                }
                
                symbol = symbol->next;
            }
        }
    } while (changed);
    
    // Remove epsilon from all follow sets (epsilon should never be in a follow set)
    int epsilonIndex = findTerminalIndex(grammar, EPSILON_TOKEN);
    if (epsilonIndex != -1) {
        for (int i = 0; i < grammar->numNonTerminals; i++) {
            fafl->follow[i][epsilonIndex] = false;
        }
    }
}

// Main function to compute First and Follow sets
FirstAndFollow* computeFirstAndFollowSets(Grammar* grammar) {
    FirstAndFollow* fafl = initializeFirstAndFollow(grammar);
    
    // Compute FIRST sets
    bool* computed = (bool*)calloc(grammar->numNonTerminals, sizeof(bool));
    for (int i = 0; i < grammar->numNonTerminals; i++) {
        if (!computed[i]) {
            computeFirst(grammar, fafl, computed, i);
        }
    }
    free(computed);
    
    // Compute FOLLOW sets
    computeFollow(grammar, fafl);
    
    return fafl;
}

// Function to print FIRST sets
void printFirstSets(Grammar* grammar, FirstAndFollow* fafl) {
    printf("\nFIRST Sets:\n");
    for (int i = 0; i < grammar->numNonTerminals; i++) {
        printf("FIRST(%s) = { ", grammar->nonTerminals[i]);
        
        bool isEmpty = true;
        for (int j = 0; j < grammar->numTerminals; j++) {
            if (fafl->first[i][j]) {
                printf("%s ", grammar->terminals[j]);
                isEmpty = false;
            }
        }
        
        if (fafl->firstHasEpsilon[i]) {
            printf("%s ", EPSILON_TOKEN);
            isEmpty = false;
        }
        
        if (isEmpty) {
            printf("∅");
        }
        
        printf("}\n");
    }
}

// Function to print FOLLOW sets
void printFollowSets(Grammar* grammar, FirstAndFollow* fafl) {
    printf("\nFOLLOW Sets:\n");
    for (int i = 0; i < grammar->numNonTerminals; i++) {
        printf("FOLLOW(%s) = { ", grammar->nonTerminals[i]);
        
        bool isEmpty = true;
        for (int j = 0; j < grammar->numTerminals; j++) {
            if (fafl->follow[i][j]) {
                printf("%s ", grammar->terminals[j]);
                isEmpty = false;
            }
        }
        
        if (isEmpty) {
            printf("∅");
        }
        
        printf("}\n");
    }
}

// Create parsing table based on First and Follow sets
// with error recovery using synchronizing tokens
void createParseTable(FirstAndFollow* fafl, ParseTable* parseTable, Grammar* grammar) {
    // Initialize parse table
    parseTable->table = (int**)malloc(grammar->numNonTerminals * sizeof(int*));
    for (int i = 0; i < grammar->numNonTerminals; i++) {
        parseTable->table[i] = (int*)calloc(grammar->numTerminals, sizeof(int));
        // Initialize with -1 (error)
        for (int j = 0; j < grammar->numTerminals; j++) {
            parseTable->table[i][j] = -1;
        }
    }
    
    // Find epsilon index
    int epsilonIndex = findTerminalIndex(grammar, EPSILON_TOKEN);
    
    // Build the table
    for (int i = 1; i <= grammar->numRules; i++) {
        Rule* rule = grammar->rules[i];
        Symbol* lhs = rule->symbols->head;
        int A = lhs->id.nonTerminal;
        Symbol* rhsStart = lhs->next;
        
        // Case 1: If α -> ε, add A -> α to M[A, b] for each b in FOLLOW(A)
        if (rhsStart != NULL && rhsStart->isTerminal && 
            rhsStart->id.terminal == epsilonIndex) {
            for (int j = 0; j < grammar->numTerminals; j++) {
                if (fafl->follow[A][j]) {
                    parseTable->table[A][j] = i;
                }
            }
        } 
        // Case 2: If α does not derive ε, add A -> α to M[A, b] for each b in FIRST(α)
        else {
            bool firstSet[grammar->numTerminals];
            bool derivesEpsilon;
            getFirstOfSequence(grammar, fafl, rhsStart, firstSet, &derivesEpsilon);
            
            for (int j = 0; j < grammar->numTerminals; j++) {
                if (firstSet[j]) {
                    parseTable->table[A][j] = i;
                }
            }
            
            // If α can derive ε, add A -> α to M[A, b] for each b in FOLLOW(A)
            if (derivesEpsilon) {
                for (int j = 0; j < grammar->numTerminals; j++) {
                    if (fafl->follow[A][j]) {
                        parseTable->table[A][j] = i;
                    }
                }
            }
        }
    }
    
    // Add synchronizing tokens for error recovery
    // For each non-terminal, a "synch" entry is added for any terminal in its FOLLOW set
    // where there is currently an error entry
    for (int i = 0; i < grammar->numNonTerminals; i++) {
        for (int j = 0; j < grammar->numTerminals; j++) {
            // Skip epsilon terminal for synch entries
            if (j == epsilonIndex) continue;
            
            // Only mark as synch if it's currently an error (-1) and is in the FOLLOW set
            if (parseTable->table[i][j] == -1 && fafl->follow[i][j]) {
                // Use a special value to mark synch entries: -2
                parseTable->table[i][j] = -2;
            }
        }
    }
}

// Function to print the parse table
void printParseTable(ParseTable* parseTable, Grammar* grammar) {
    printf("\nParse Table:\n");
    
    // Print column headers (terminals)
    printf("%20s", "");
    for (int j = 0; j < grammar->numTerminals; j++) {
        if (strcmp(grammar->terminals[j], EPSILON_TOKEN) != 0) {
            printf("%-15s", grammar->terminals[j]);
        }
    }
    printf("\n");
    
    // Print separator
    for (int i = 0; i < 20 + 15 * (grammar->numTerminals - 1); i++) {
        printf("-");
    }
    printf("\n");
    
    // Print rows
    for (int i = 0; i < grammar->numNonTerminals; i++) {
        printf("%-20s", grammar->nonTerminals[i]);
        
        for (int j = 0; j < grammar->numTerminals; j++) {
            if (strcmp(grammar->terminals[j], EPSILON_TOKEN) == 0) continue;
            
            if (parseTable->table[i][j] == -1) {
                printf("%-15s", "error");
            } else if (parseTable->table[i][j] == -2) {
                printf("%-15s", "synch");
            } else {
                Rule* rule = grammar->rules[parseTable->table[i][j]];
                Symbol* lhs = rule->symbols->head;
                Symbol* rhs = lhs->next;
                
                printf("%s -> ", grammar->nonTerminals[lhs->id.nonTerminal]);
                
                if (rhs != NULL && rhs->isTerminal && 
                    strcmp(grammar->terminals[rhs->id.terminal], EPSILON_TOKEN) == 0) {
                    printf("ε%-12s", "");
                } else {
                    while (rhs != NULL) {
                        if (rhs->isTerminal) {
                            printf("%s ", grammar->terminals[rhs->id.terminal]);
                        } else {
                            printf("%s ", grammar->nonTerminals[rhs->id.nonTerminal]);
                        }
                        rhs = rhs->next;
                    }
                    printf("%-4s", "");
                }
            }
        }
        printf("\n");
    }
}

// Function to write the parse table to a text file
void writeParseTableToFile(ParseTable* parseTable, Grammar* grammar, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Error opening file %s for writing\n", filename);
        return;
    }
    
    // Write column headers (terminals)
    fprintf(file, "%-20s", "");
    for (int j = 0; j < grammar->numTerminals; j++) {
        if (strcmp(grammar->terminals[j], EPSILON_TOKEN) != 0) {
            fprintf(file, "%-15s", grammar->terminals[j]);
        }
    }
    fprintf(file, "\n");
    
    // Write separator
    for (int i = 0; i < 20 + 15 * (grammar->numTerminals - 1); i++) {
        fprintf(file, "-");
    }
    fprintf(file, "\n");
    
    // Write rows
    for (int i = 0; i < grammar->numNonTerminals; i++) {
        fprintf(file, "%-20s", grammar->nonTerminals[i]);
        
        for (int j = 0; j < grammar->numTerminals; j++) {
            if (strcmp(grammar->terminals[j], EPSILON_TOKEN) == 0) continue;
            
            if (parseTable->table[i][j] == -1) {
                fprintf(file, "%-15s", "error");
            } else {
                char buffer[100] = {0};
                Rule* rule = grammar->rules[parseTable->table[i][j]];
                Symbol* lhs = rule->symbols->head;
                Symbol* rhs = lhs->next;
                
                int offset = snprintf(buffer, sizeof(buffer), "%s -> ", grammar->nonTerminals[lhs->id.nonTerminal]);
                
                if (rhs != NULL && rhs->isTerminal && 
                    strcmp(grammar->terminals[rhs->id.terminal], EPSILON_TOKEN) == 0) {
                    snprintf(buffer + offset, sizeof(buffer) - offset, "ε");
                } else {
                    while (rhs != NULL) {
                        if (rhs->isTerminal) {
                            offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s ", 
                                             grammar->terminals[rhs->id.terminal]);
                        } else {
                            offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s ", 
                                             grammar->nonTerminals[rhs->id.nonTerminal]);
                        }
                        rhs = rhs->next;
                    }
                }
                fprintf(file, "%-15s", buffer);
            }
        }
        fprintf(file, "\n");
    }
    
    fclose(file);
    printf("Parse table has been written to %s\n", filename);
}

// Function to write the parse table to a CSV file
void writeParseTableToCsv(ParseTable* parseTable, Grammar* grammar, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Error opening file %s for writing\n", filename);
        return;
    }
    
    // Write column headers (terminals)
    fprintf(file, ","); // Empty cell for the corner
    for (int j = 0; j < grammar->numTerminals; j++) {
        if (strcmp(grammar->terminals[j], EPSILON_TOKEN) != 0) {
            fprintf(file, "\"%s\",", grammar->terminals[j]);
        }
    }
    fprintf(file, "\n");
    
    // Write rows
    for (int i = 0; i < grammar->numNonTerminals; i++) {
        // Write row header (non-terminal)
        fprintf(file, "\"%s\",", grammar->nonTerminals[i]);
        
        for (int j = 0; j < grammar->numTerminals; j++) {
            if (strcmp(grammar->terminals[j], EPSILON_TOKEN) == 0) continue;
            
            if (parseTable->table[i][j] == -1) {
                fprintf(file, "\"error\",");
            } else if (parseTable->table[i][j] == -2) {
                fprintf(file, "\"synch\",");
            } else {
                Rule* rule = grammar->rules[parseTable->table[i][j]];
                Symbol* lhs = rule->symbols->head;
                Symbol* rhs = lhs->next;
                
                // Start with a quote
                fprintf(file, "\"");
                
                // Write the rule
                fprintf(file, "%s -> ", grammar->nonTerminals[lhs->id.nonTerminal]);
                
                if (rhs != NULL && rhs->isTerminal && 
                    strcmp(grammar->terminals[rhs->id.terminal], EPSILON_TOKEN) == 0) {
                    fprintf(file, "ε");
                } else {
                    while (rhs != NULL) {
                        if (rhs->isTerminal) {
                            fprintf(file, "%s ", grammar->terminals[rhs->id.terminal]);
                        } else {
                            fprintf(file, "%s ", grammar->nonTerminals[rhs->id.nonTerminal]);
                        }
                        rhs = rhs->next;
                    }
                }
                
                // End with a quote and comma
                fprintf(file, "\",");
            }
        }
        fprintf(file, "\n");
    }
    
    fclose(file);
    printf("Parse table has been written to %s (CSV format for Excel)\n", filename);
}

// Function to write the parse table to an HTML file
void writeParseTableToHtml(ParseTable* parseTable, Grammar* grammar, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Error opening file %s for writing\n", filename);
        return;
    }
    
    // Write HTML header
    fprintf(file, "<!DOCTYPE html>\n");
    fprintf(file, "<html lang=\"en\">\n");
    fprintf(file, "<head>\n");
    fprintf(file, "  <meta charset=\"UTF-8\">\n");
    fprintf(file, "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    fprintf(file, "  <title>Parse Table</title>\n");
    fprintf(file, "  <style>\n");
    fprintf(file, "    body { font-family: Arial, sans-serif; margin: 20px; }\n");
    fprintf(file, "    table { border-collapse: collapse; width: 100%%; }\n");
    fprintf(file, "    th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n");
    fprintf(file, "    th { background-color: #f2f2f2; }\n");
    fprintf(file, "    tr:nth-child(even) { background-color: #f9f9f9; }\n");
    fprintf(file, "    tr:hover { background-color: #eef; }\n");
    fprintf(file, "    .error { color: red; }\n");
    fprintf(file, "    .synch { color: orange; font-weight: bold; }\n");
    fprintf(file, "    .rule { color: blue; }\n");
    fprintf(file, "  </style>\n");
    fprintf(file, "</head>\n");
    fprintf(file, "<body>\n");
    fprintf(file, "  <h1>Parse Table</h1>\n");
    fprintf(file, "  <table>\n");
    
    // Write table header
    fprintf(file, "    <tr>\n");
    fprintf(file, "      <th></th>\n"); // Empty corner cell
    
    // Add terminal symbols as column headers
    for (int j = 0; j < grammar->numTerminals; j++) {
        if (strcmp(grammar->terminals[j], EPSILON_TOKEN) != 0) {
            fprintf(file, "      <th>%s</th>\n", grammar->terminals[j]);
        }
    }
    fprintf(file, "    </tr>\n");
    
    // Write table rows
    for (int i = 0; i < grammar->numNonTerminals; i++) {
        fprintf(file, "    <tr>\n");
        
        // Row header (non-terminal)
        fprintf(file, "      <th>%s</th>\n", grammar->nonTerminals[i]);
        
        // Table cells
        for (int j = 0; j < grammar->numTerminals; j++) {
            if (strcmp(grammar->terminals[j], EPSILON_TOKEN) == 0) continue;
            
            if (parseTable->table[i][j] == -1) {
                fprintf(file, "      <td class=\"error\">error</td>\n");
            } else if (parseTable->table[i][j] == -2) {
                fprintf(file, "      <td class=\"synch\">synch</td>\n");
            } else {
                fprintf(file, "      <td class=\"rule\">");
                
                Rule* rule = grammar->rules[parseTable->table[i][j]];
                Symbol* lhs = rule->symbols->head;
                Symbol* rhs = lhs->next;
                
                // Write the rule
                fprintf(file, "%s &rarr; ", grammar->nonTerminals[lhs->id.nonTerminal]);
                
                if (rhs != NULL && rhs->isTerminal && 
                    strcmp(grammar->terminals[rhs->id.terminal], EPSILON_TOKEN) == 0) {
                    fprintf(file, "&epsilon;");
                } else {
                    while (rhs != NULL) {
                        if (rhs->isTerminal) {
                            fprintf(file, "%s ", grammar->terminals[rhs->id.terminal]);
                        } else {
                            fprintf(file, "%s ", grammar->nonTerminals[rhs->id.nonTerminal]);
                        }
                        rhs = rhs->next;
                    }
                }
                
                fprintf(file, "</td>\n");
            }
        }
        
        fprintf(file, "    </tr>\n");
    }
    
    // Write HTML footer
    fprintf(file, "  </table>\n");
    fprintf(file, "  <div style=\"margin-top: 20px;\">\n");
    fprintf(file, "    <p><strong>Legend:</strong></p>\n");
    fprintf(file, "    <ul>\n");
    fprintf(file, "      <li><span class=\"rule\">Rule</span>: Normal grammar production rule</li>\n");
    fprintf(file, "      <li><span class=\"error\">error</span>: Error state - invalid input</li>\n");
    fprintf(file, "      <li><span class=\"synch\">synch</span>: Synchronizing entry for error recovery</li>\n");
    fprintf(file, "    </ul>\n");
    fprintf(file, "  </div>\n");
    fprintf(file, "</body>\n");
    fprintf(file, "</html>\n");
    
    fclose(file);
    printf("Parse table has been written to %s (HTML format for browser viewing)\n", filename);
}

// Define parse tree node structure
typedef struct ParseTreeNode {
    bool isTerminal;
    int symbolIndex;
    int lineNumber;
    char lexeme[100];
    struct ParseTreeNode* parent;
    struct ParseTreeNode* firstChild;
    struct ParseTreeNode* nextSibling;
} ParseTreeNode;

// Define parser stack element
typedef struct StackElement {
    bool isTerminal;
    int symbolIndex;
    ParseTreeNode* node;
    struct StackElement* next;
} StackElement;

// Define stack structure
typedef struct {
    StackElement* top;
} ParserStack;

// Define token from lexer
typedef struct {
    char lexeme[100];
    char token[50];
    int lineNumber;
} Token;

// Function prototypes
ParserStack* createStack();
void push(ParserStack* stack, bool isTerminal, int symbolIndex, ParseTreeNode* node);
StackElement* pop(ParserStack* stack);
ParseTreeNode* createNode(bool isTerminal, int symbolIndex, int lineNumber, const char* lexeme);
void addChild(ParseTreeNode* parent, ParseTreeNode* child);
void printStackContents(ParserStack* stack, Grammar* grammar);
void printParseTree(ParseTreeNode* node, Grammar* grammar, int depth);
void inorderTraversal(ParseTreeNode* node, Grammar* grammar, FILE* outFile);
Token* readTokensFromFile(const char* filename, int* numTokens);
void parseTokens(Grammar* grammar, ParseTable* parseTable, Token* tokens, int numTokens, const char* parseTreeFile);

// Initialize the parser stack
ParserStack* createStack() {
    ParserStack* stack = (ParserStack*)malloc(sizeof(ParserStack));
    stack->top = NULL;
    return stack;
}

// Push an element onto the stack
void push(ParserStack* stack, bool isTerminal, int symbolIndex, ParseTreeNode* node) {
    StackElement* element = (StackElement*)malloc(sizeof(StackElement));
    element->isTerminal = isTerminal;
    element->symbolIndex = symbolIndex;
    element->node = node;
    element->next = stack->top;
    stack->top = element;
}

// Pop an element from the stack
StackElement* pop(ParserStack* stack) {
    if (stack->top == NULL) {
        return NULL;
    }
    
    StackElement* element = stack->top;
    stack->top = element->next;
    element->next = NULL;
    return element;
}

// Create a parse tree node
ParseTreeNode* createNode(bool isTerminal, int symbolIndex, int lineNumber, const char* lexeme) {
    ParseTreeNode* node = (ParseTreeNode*)malloc(sizeof(ParseTreeNode));
    node->isTerminal = isTerminal;
    node->symbolIndex = symbolIndex;
    node->lineNumber = lineNumber;
    if (lexeme != NULL) {
        strncpy(node->lexeme, lexeme, sizeof(node->lexeme) - 1);
        node->lexeme[sizeof(node->lexeme) - 1] = '\0'; // Ensure null termination
    } else {
        node->lexeme[0] = '\0';
    }
    node->parent = NULL;
    node->firstChild = NULL;
    node->nextSibling = NULL;
    return node;
}

// Add a child node to a parent node
void addChild(ParseTreeNode* parent, ParseTreeNode* child) {
    if (parent == NULL || child == NULL) return;
    
    child->parent = parent;
    
    if (parent->firstChild == NULL) {
        parent->firstChild = child;
    } else {
        ParseTreeNode* sibling = parent->firstChild;
        while (sibling->nextSibling != NULL) {
            sibling = sibling->nextSibling;
        }
        sibling->nextSibling = child;
    }
}

// Print the contents of the stack (for debugging)
void printStackContents(ParserStack* stack, Grammar* grammar) {
    printf("Stack: ");
    StackElement* current = stack->top;
    while (current != NULL) {
        if (current->isTerminal) {
            printf("%s ", grammar->terminals[current->symbolIndex]);
        } else {
            printf("%s ", grammar->nonTerminals[current->symbolIndex]);
        }
        current = current->next;
    }
    printf("\n");
}

// Print the parse tree (for debugging)
void printParseTree(ParseTreeNode* node, Grammar* grammar, int depth) {
    if (node == NULL) return;
    
    // Print indentation
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
    
    // Print node info
    if (node->isTerminal) {
        printf("%s", grammar->terminals[node->symbolIndex]);
        if (node->lexeme[0] != '\0') {
            printf(" (Lexeme: %s, Line: %d)", node->lexeme, node->lineNumber);
        }
    } else {
        printf("%s", grammar->nonTerminals[node->symbolIndex]);
    }
    printf("\n");
    
    // Print children
    ParseTreeNode* child = node->firstChild;
    while (child != NULL) {
        printParseTree(child, grammar, depth + 1);
        child = child->nextSibling;
    }
}

// Correct inorder traversal for n-ary trees
void inorderTraversal(ParseTreeNode* node, Grammar* grammar, FILE* outFile) {
    if (node == NULL) return;
    
    // Process leftmost child first
    if (node->firstChild != NULL) {
        inorderTraversal(node->firstChild, grammar, outFile);
    }
    
    // Process current node
    if (node->isTerminal) {
        if (node->lexeme[0] != '\0') {
            fprintf(outFile, "%-20s", grammar->terminals[node->symbolIndex]);
            fprintf(outFile, "Line: %-4d", node->lineNumber);
            fprintf(outFile, "Lexeme: %-20s\n", node->lexeme);
        }
    } else {
        fprintf(outFile, "%-20s", grammar->nonTerminals[node->symbolIndex]);
        fprintf(outFile, "Line: ---");
        fprintf(outFile, "   Internal Node\n");
    }
    
    // Process remaining siblings of the leftmost child
    if (node->firstChild != NULL) {
        ParseTreeNode* sibling = node->firstChild->nextSibling;
        while (sibling != NULL) {
            inorderTraversal(sibling, grammar, outFile);
            sibling = sibling->nextSibling;
        }
    }
}

// Read tokens from lexer output file
Token* readTokensFromFile(const char* filename, int* numTokens) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error opening token file: %s\n", filename);
        exit(1);
    }
    
    // First pass: count tokens (excluding comments)
    int count = 0;
    char line[1024];
    
    while (fgets(line, sizeof(line), file)) {
        char token[50];
        if (sscanf(line, "%*[^T]Token %s", token) == 1) {
            // Skip comment tokens
            if (strcmp(token, "TK_COMMENT") != 0) {
                count++;
            }
        }
    }
    
    // Allocate memory for tokens
    Token* tokens = (Token*)malloc((count + 1) * sizeof(Token)); // +1 for end marker
    
    // Reset file pointer
    rewind(file);
    
    // Second pass: read tokens
    int index = 0;
    int line_no;
    
    while (fgets(line, sizeof(line), file) && index < count) {
        char lexeme[100];
        char token[50];
        
        if (sscanf(line, "Line no. %d Lexeme %s Token %s", &line_no, lexeme, token) == 3) {
            // Skip comment tokens
            if (strcmp(token, "TK_COMMENT") != 0) {
                tokens[index].lineNumber = line_no;
                strncpy(tokens[index].lexeme, lexeme, sizeof(tokens[index].lexeme) - 1);
                tokens[index].lexeme[sizeof(tokens[index].lexeme) - 1] = '\0'; // Ensure null termination
                strncpy(tokens[index].token, token, sizeof(tokens[index].token) - 1);
                tokens[index].token[sizeof(tokens[index].token) - 1] = '\0'; // Ensure null termination
                index++;
            }
        }
    }
    
    // Add end marker token (TK_DOLLAR)
    strncpy(tokens[index].lexeme, "$", sizeof(tokens[index].lexeme) - 1);
    strncpy(tokens[index].token, "TK_DOLLAR", sizeof(tokens[index].token) - 1);
    tokens[index].lineNumber = tokens[index-1].lineNumber;
    index++;
    
    *numTokens = index;
    fclose(file);
    return tokens;
}

// The main parsing function
void parseTokens(Grammar* grammar, ParseTable* parseTable, Token* tokens, int numTokens, const char* parseTreeFile) {
    ParserStack* stack = createStack();
    int currentToken = 0;
    
    // Create parse tree root node
    ParseTreeNode* root = createNode(false, findNonTerminalIndex(grammar, grammar->startSymbol), 0, NULL);
    
    // Initialize stack with $ and start symbol
    int dollarIndex = findTerminalIndex(grammar, "TK_DOLLAR");
    push(stack, true, dollarIndex, NULL);
    push(stack, false, findNonTerminalIndex(grammar, grammar->startSymbol), root);
    
    FILE* logFile = fopen("parsing_log.txt", "w");
    if (!logFile) {
        printf("Error opening parsing log file\n");
        return;
    }
    
    fprintf(logFile, "Starting Predictive LL(1) Parsing\n");
    fprintf(logFile, "=========================================\n\n");
    
    bool error = false;
    
    while (stack->top != NULL) {
        StackElement* X = stack->top;
        
        // Print current status
        fprintf(logFile, "Current Token: %s, Lexeme: %s, Line: %d\n", 
                tokens[currentToken].token, tokens[currentToken].lexeme, tokens[currentToken].lineNumber);
        
        fprintf(logFile, "Top of Stack: ");
        if (X->isTerminal) {
            fprintf(logFile, "%s (Terminal)\n", grammar->terminals[X->symbolIndex]);
        } else {
            fprintf(logFile, "%s (Non-terminal)\n", grammar->nonTerminals[X->symbolIndex]);
        }
        
        // Case 1: X is a terminal
        if (X->isTerminal) {
            if (strcmp(grammar->terminals[X->symbolIndex], tokens[currentToken].token) == 0) {
                // Match found, pop X and advance input
                StackElement* popped = pop(stack);
                if (popped->node != NULL) {
                    // Update node with token information
                    strncpy(popped->node->lexeme, tokens[currentToken].lexeme, sizeof(popped->node->lexeme) - 1);
                    popped->node->lineNumber = tokens[currentToken].lineNumber;
                }
                free(popped);
                
                fprintf(logFile, "Matched terminal %s. Advancing input.\n\n", tokens[currentToken].token);
                currentToken++;
            } else {
                // Error: X doesn't match current input token
                error = true;
                fprintf(logFile, "Error: Expected %s but found %s at line %d\n", 
                        grammar->terminals[X->symbolIndex], tokens[currentToken].token, tokens[currentToken].lineNumber);
                
                // Skip X (error recovery)
                StackElement* popped = pop(stack);
                free(popped);
                
                fprintf(logFile, "Error recovery: Popping %s from stack\n\n", grammar->terminals[X->symbolIndex]);
            }
        }
        // Case 2: X is a non-terminal
        else {
            int a_idx = -1;
            
            // Find terminal index for current token
            for (int i = 0; i < grammar->numTerminals; i++) {
                if (strcmp(grammar->terminals[i], tokens[currentToken].token) == 0) {
                    a_idx = i;
                    break;
                }
            }
            
            if (a_idx == -1) {
                fprintf(logFile, "Error: Unknown token %s at line %d\n", 
                        tokens[currentToken].token, tokens[currentToken].lineNumber);
                currentToken++;
                continue;
            }
            
            int rule_num = parseTable->table[X->symbolIndex][a_idx];
            
            // Case 2.1: M[X,a] = valid rule
            if (rule_num > 0) {
                StackElement* popped = pop(stack);
                ParseTreeNode* parentNode = popped->node;
                free(popped);
                
                Rule* rule = grammar->rules[rule_num];
                Symbol* rhs = rule->symbols->head->next;
                
                fprintf(logFile, "Using rule %d: %s -> ", rule_num, grammar->nonTerminals[X->symbolIndex]);
                
                // Create a linked list of RHS symbols in reverse order (for stack)
                StackElement* rhsList = NULL;
                int symbolCount = 0;
                
                while (rhs != NULL) {
                    if (rhs->isTerminal) {
                        fprintf(logFile, "%s ", grammar->terminals[rhs->id.terminal]);
                    } else {
                        fprintf(logFile, "%s ", grammar->nonTerminals[rhs->id.nonTerminal]);
                    }
                    
                    StackElement* element = (StackElement*)malloc(sizeof(StackElement));
                    element->isTerminal = rhs->isTerminal;
                    element->symbolIndex = rhs->isTerminal ? rhs->id.terminal : rhs->id.nonTerminal;
                    
                    // Create parse tree node
                    ParseTreeNode* childNode = createNode(rhs->isTerminal, element->symbolIndex, 0, NULL);
                    addChild(parentNode, childNode);
                    element->node = childNode;
                    
                    // Add to reversed list
                    element->next = rhsList;
                    rhsList = element;
                    symbolCount++;
                    
                    rhs = rhs->next;
                }
                fprintf(logFile, "\n");
                
                // Special case: Epsilon rule
                if (symbolCount == 1 && rhsList->isTerminal && 
                    strcmp(grammar->terminals[rhsList->symbolIndex], "TK_EPS") == 0) {
                    // For epsilon, just free the element without pushing
                    free(rhsList);
                } else {
                    // Push RHS symbols onto stack (already in reverse order)
                    while (rhsList != NULL) {
                        StackElement* element = rhsList;
                        rhsList = rhsList->next;
                        
                        push(stack, element->isTerminal, element->symbolIndex, element->node);
                        free(element);
                    }
                }
                
                fprintf(logFile, "Stack after rule application:\n");
                StackElement* current = stack->top;
                while (current != NULL) {
                    if (current->isTerminal) {
                        fprintf(logFile, "%s ", grammar->terminals[current->symbolIndex]);
                    } else {
                        fprintf(logFile, "%s ", grammar->nonTerminals[current->symbolIndex]);
                    }
                    current = current->next;
                }
                fprintf(logFile, "\n\n");
            }
            // Case 2.2: M[X,a] = synch (error recovery)
            else if (rule_num == -2) {
                error = true;
                fprintf(logFile, "Error recovery: Synch entry found for %s and %s. Popping non-terminal.\n\n", 
                        grammar->nonTerminals[X->symbolIndex], tokens[currentToken].token);
                
                StackElement* popped = pop(stack);
                free(popped);
            }
            // Case 2.3: M[X,a] = error
            else {
                error = true;
                fprintf(logFile, "Error: No rule for %s with input %s at line %d\n", 
                        grammar->nonTerminals[X->symbolIndex], tokens[currentToken].token, tokens[currentToken].lineNumber);
                
                // Skip current input token (error recovery)
                fprintf(logFile, "Error recovery: Skipping input token %s\n\n", tokens[currentToken].token);
                currentToken++;
            }
        }
    }
    
    if (currentToken < numTokens - 1) {
        fprintf(logFile, "Error: Extra tokens in input starting at line %d\n", tokens[currentToken].lineNumber);
    } else if (!error) {
        fprintf(logFile, "Parsing completed successfully!\n");
    } else {
        fprintf(logFile, "Parsing completed with errors!\n");
    }
    
    // Print parse tree for debugging
    fprintf(logFile, "\nParse Tree:\n");
    printParseTree(root, grammar, 0);
    
    // Generate parse tree inorder traversal
    FILE* traversalFile = fopen(parseTreeFile, "w");
    if (!traversalFile) {
        printf("Error opening parse tree file\n");
        fclose(logFile);
        return;
    }
    
    fprintf(traversalFile, "Parse Tree Inorder Traversal:\n");
    fprintf(traversalFile, "============================\n\n");
    fprintf(traversalFile, "%-20s%-15s%-20s\n", "Token/Non-Terminal", "Line Number", "Lexeme/Type");
    fprintf(traversalFile, "------------------------------------------------------------\n");
    
    inorderTraversal(root, grammar, traversalFile);
    
    fclose(traversalFile);
    fclose(logFile);
    
    printf("Parsing completed. Check parsing_log.txt for details and %s for parse tree.\n", parseTreeFile);
}

// Add parsing functionality to main function
void parseSourceCode(Grammar* grammar, ParseTable* parseTable, const char* tokenFile, const char* parseTreeFile) {
    int numTokens;
    Token* tokens = readTokensFromFile(tokenFile, &numTokens);
    
    printf("Read %d tokens from %s\n", numTokens, tokenFile);
    parseTokens(grammar, parseTable, tokens, numTokens, parseTreeFile);
    
    free(tokens);
}

// Main function to demonstrate functionality
int main() {
    Grammar* grammar = readGrammarFromFile("grammar.txt");
    printGrammar(grammar);
    
    FirstAndFollow* fafl = computeFirstAndFollowSets(grammar);
    printFirstSets(grammar, fafl);
    printFollowSets(grammar, fafl);
    
    ParseTable* parseTable = (ParseTable*)malloc(sizeof(ParseTable));
    createParseTable(fafl, parseTable, grammar);
    printParseTable(parseTable, grammar);

    // Write parse table to files in different formats for better visualization
    writeParseTableToCsv(parseTable, grammar, "parse_table.csv");
    writeParseTableToHtml(parseTable, grammar, "parse_table.html");

    // Parse source code using lexer output with hardcoded file names
    printf("\nParsing source code from lexer output lexemesandtokens_t2.txt...\n");
    parseSourceCode(grammar, parseTable, "lexemesandtokens_t2.txt", "parse_tree.txt");
    
    return 0;
}