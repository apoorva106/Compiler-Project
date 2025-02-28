/* parserDef.h */
#ifndef PARSER_DEF_H
#define PARSER_DEF_H

#include <stdbool.h>

#define MAX_SYMBOLS 150
#define MAX_RULES 100
#define MAX_RULE_LENGTH 25
#define MAX_SYMBOL_LENGTH 30

// Structure for a grammar symbol (terminal or non-terminal)
typedef struct Symbol {
    bool isTerminal;
    union {
        int terminal;
        int nonTerminal;
    } id;
    struct Symbol* next;
} Symbol;

// Structure for a linked list of symbols
typedef struct {
    Symbol* head;
    Symbol* tail;
    int length;
} SymbolList;

// Structure for a grammar rule
typedef struct {
    SymbolList* symbols;
    int ruleNumber;
} Rule;

// Structure to track rule ranges for non-terminals
typedef struct {
    int startRule;
    int endRule;
} NonTerminalRules;

// Structure for the grammar
typedef struct {
    Rule** rules;
    int numRules;
    char terminals[MAX_SYMBOLS][MAX_SYMBOL_LENGTH];
    char nonTerminals[MAX_SYMBOLS][MAX_SYMBOL_LENGTH];
    int numTerminals;
    int numNonTerminals;
    char startSymbol[MAX_SYMBOL_LENGTH];
} Grammar;

// Structure for First and Follow sets
typedef struct {
    bool** first;    // first[i][j] is true if terminal j is in FIRST(nonTerminal i)
    bool* firstHasEpsilon;   // firstHasEpsilon[i] is true if FIRST(nonTerminal i) contains epsilon
    bool** follow;   // follow[i][j] is true if terminal j is in FOLLOW(nonTerminal i)
} FirstAndFollow;

// Structure for the parsing table
typedef struct {
    int** table;     // table[i][j] contains rule number to apply for nonTerminal i and terminal j
} ParseTable;

// Forward declaration for parse tree node
typedef struct ParseTreeNode ParseTreeNode;

// Parse tree node structure
struct ParseTreeNode {
    bool isLeaf;
    int lineNo;
    
    union {
        struct { 
            int symbolId;
            char lexeme[MAX_SYMBOL_LENGTH];
            double value;
            bool isNumber;
        } leaf;
        
        struct {
            int symbolId;
            ParseTreeNode* firstChild;
        } nonLeaf;
    } data;
    
    ParseTreeNode* parent;
    ParseTreeNode* sibling;
};

// Parse tree structure
typedef struct {
    ParseTreeNode* root;
} ParseTree;

#endif /* PARSER_DEF_H */