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
    fprintf(file, "</body>\n");
    fprintf(file, "</html>\n");
    
    fclose(file);
    printf("Parse table has been written to %s (HTML format for browser viewing)\n", filename);
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
    
    return 0;
}