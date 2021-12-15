#ifndef AST_H
#define AST_H
#include <stdbool.h>
#include <stdio.h>

typedef struct _ASTNode ASTNode;

typedef struct {
    int tableID;
    int id;
    bool isConst;
    bool isInited;
    char name[256];
    int constVal;
    int arraySize[100];
    int arrayCount;
    bool isUndeclaredFunc;
    ASTNode* decl;
} SymbolTableItem;

typedef struct {
    int symbolTableID;
    int symbolCount;
    SymbolTableItem table[1024];
} SymbolTable;

struct _ASTNode {
    const char* nodeType;
    bool isIntValue;
    int intValue;
    bool isSymbol;
    char symbolName[256];
    int nodesCount;
    int allocatedLength;
    SymbolTable* symbolTable;
    ASTNode** nodes;
    ASTNode* parent;
};

void InitNode(ASTNode** rt, const char* info);
void AddChildNode(ASTNode* rt, ASTNode* newNode);
ASTNode* NewNode(const char* info, int num, ...);
void PrintAST(ASTNode* node, int indent, FILE* fp);
void QuerySymbol(ASTNode* symbol, int* symbolTableID, int* symbolID, SymbolTableItem** info);
int CalcConst(ASTNode* expr);

#endif
