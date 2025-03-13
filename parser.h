#ifndef PARSER_H
#define PARSER_H

#define MAX_SYMBOL_LENGTH 50
#define MAX_RULE_LENGTH 100

// Symbol Structure
typedef union {
    int terminal;
    int nonTerminal;
} SymbolID;

typedef struct Symbol {
    bool isTerminal;
    SymbolID id;
    struct Symbol* next;
} Symbol;

// Symbol List Structure
typedef struct {
    Symbol* head;
    Symbol* tail;
    int length;
} SymbolList;

// Rule Structure
typedef struct {
    SymbolList* symbols;
    int ruleNumber;
} Rule;

// Non-Terminal Rules Range
typedef struct {
    int startRule;
    int endRule;
} NonTerminalRules;

// Grammar Structure
typedef struct {
    char terminals[100][MAX_SYMBOL_LENGTH];
    char nonTerminals[100][MAX_SYMBOL_LENGTH];
    char startSymbol[MAX_SYMBOL_LENGTH];
    int numTerminals;
    int numNonTerminals;
    Rule** rules;
    int numRules;
} Grammar;

// First and Follow Sets Structure
typedef struct {
    bool** first;
    bool* firstHasEpsilon;
    bool** follow;
} FirstAndFollow;

// Parse Table Structure
typedef struct {
    int** table;
} ParseTable;

// Function prototypes
Grammar* readGrammarFromFile(const char* filename);
void printGrammar(Grammar* grammar);
FirstAndFollow* computeFirstAndFollowSets(Grammar* grammar);
void printFirstSets(Grammar* grammar, FirstAndFollow* fafl);
void printFollowSets(Grammar* grammar, FirstAndFollow* fafl);
void createParseTable(FirstAndFollow* fafl, ParseTable* parseTable, Grammar* grammar);
void printParseTable(ParseTable* parseTable, Grammar* grammar);
void writeParseTableToCsv(ParseTable* parseTable, Grammar* grammar, const char* filename);
void writeParseTableToHtml(ParseTable* parseTable, Grammar* grammar, const char* filename);
void parseSourceCode(Grammar* grammar, ParseTable* parseTable, const char* tokenFile, const char* parseTreeFile);
int findTerminalIndex(Grammar* grammar, const char* terminal);
int findNonTerminalIndex(Grammar* grammar, const char* nonTerminal);

#endif // PARSER_H