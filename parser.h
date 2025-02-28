/* parser.h */
#ifndef PARSER_H
#define PARSER_H

#include "parserDef.h"
#include "lexer.h"

// Function prototypes
Grammar* readGrammarFromFile(const char* filename);
void printGrammar(Grammar* grammar);

FirstAndFollow* computeFirstAndFollowSets(Grammar* grammar);
void printFirstSets(Grammar* grammar, FirstAndFollow* fafl);
void printFollowSets(Grammar* grammar, FirstAndFollow* fafl);

void createParseTable(FirstAndFollow* fafl, ParseTable* parseTable, Grammar* grammar);
void printParseTable(ParseTable* parseTable, Grammar* grammar);

ParseTree* parseInputSourceCode(char* testcaseFile, ParseTable* parseTable, Grammar* grammar);
void printParseTree(ParseTree* parseTree, char* outfile);

#endif /* PARSER_H */