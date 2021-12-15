#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

void InitNode(ASTNode** rt, const char* info) {
    *rt = malloc(sizeof(ASTNode));
    (*rt)->nodeType = info;
    (*rt)->nodesCount = 0;
    (*rt)->allocatedLength = 2;
    (*rt)->isIntValue = false;
    (*rt)->isSymbol = false;
    (*rt)->nodes = malloc(sizeof(ASTNode*) * 2);
    (*rt)->symbolTable = NULL;
    (*rt)->parent = *rt;
}

void AddChildNode(ASTNode* rt, ASTNode* newNode) {
    if (rt->nodesCount + 1 > rt->allocatedLength) {
        ASTNode** newNodes = malloc(sizeof(ASTNode*) * (rt->allocatedLength) * 2);
        memcpy(newNodes, rt->nodes, sizeof(ASTNode*) * (rt->allocatedLength));
        rt->allocatedLength *= 2;
        free(rt->nodes);
        rt->nodes = newNodes;
    }
    rt->nodes[rt->nodesCount] = newNode;
    newNode->parent = rt;
    rt->nodesCount++;
}

ASTNode* NewNode(const char* info, int num, ...) {
    ASTNode* node;
    InitNode(&node, info);
    va_list va;
    va_start(va, num);
    for (int i = 0; i < num ; i++) {
        AddChildNode(node, va_arg(va, ASTNode*));
    }
    va_end(va);
    return node;
}

int symbolTableCount = 0;

static bool match(ASTNode* node, const char* s) {
    return !strcmp(node->nodeType, s);
}

int GetSymbolID(SymbolTable* table, char* s) {
    for (int i = 0; i < table->symbolCount; i++) {
        if (!strcmp(table->table[i].name, s)) {
            return i;
        }
    }
    return -1;
}

int CalcConst(ASTNode* expr);

int GetConstArrayVal(SymbolTableItem* var, ASTNode* array) {
    ASTNode* node = var->decl->parent->nodes[2];
    for (int i = 0; i < var->arrayCount; i++) {
        node = node->nodes[CalcConst(array->nodes[i])];
    }
    return CalcConst(node);
}

int CalcConst(ASTNode* expr) {
    if (match(expr, "ConstExp") || match(expr, "Exp")) {
        return CalcConst(expr->nodes[0]);
    } else if (match(expr, "ConstInt")) {
        return expr->intValue;
    } else if (match(expr, "Uncertain")) {
        return -1;
    } else if (match(expr, "Symbol")) {
        int tableID, symbolID;
        SymbolTableItem* item;
        QuerySymbol(expr, &tableID, &symbolID, &item);
        if (!item->isInited) {
            fprintf(stderr, "Not a const exp.\n");
        }
        if (item->arrayCount > 0) {
            return GetConstArrayVal(item, expr->nodes[0]);
        }
        return item->constVal;
    } else if (match(expr, "+")) {
        return CalcConst(expr->nodes[0]) + CalcConst(expr->nodes[1]);
    } else if (match(expr, "-")) {
        return CalcConst(expr->nodes[0]) - CalcConst(expr->nodes[1]);
    } else if (match(expr, "*")) {
        return CalcConst(expr->nodes[0]) * CalcConst(expr->nodes[1]);
    } else if (match(expr, "/")) {
        return CalcConst(expr->nodes[0]) / CalcConst(expr->nodes[1]);
    } else if (match(expr, "%")) {
        return CalcConst(expr->nodes[0]) % CalcConst(expr->nodes[1]);
    } else {
        fprintf(stderr, "Not a const exp.\n");
    }
    return 0;
}

bool is_global = true;

int InsertSymbolIfNotExists(SymbolTable* table, char* s, bool isConst, ASTNode* decl) {
    int id = GetSymbolID(table, s);
    if (id != -1) {
        return id;
    }
    id = table->symbolCount;
    SymbolTableItem* item = &(table->table[id]);
    item->id = table->symbolCount++;
    item->isConst = isConst;
    item->constVal = 0;
    item->decl = decl;
    item->arrayCount = 0;
    ASTNode* array = NULL;
    int p = -1;
    if (match(decl->parent, "FuncFParam")) {
        p = 2;
    } else if (match(decl->parent, "VarDef") && decl->parent->nodesCount >= 2 && match(decl->parent->nodes[1], "Array")) {
        p = 1;
    } else if (match(decl->parent, "ConstDef") && match(decl->parent->nodes[1], "ConstDefArray")) {
        p = 1;
    }
    if (p != -1 && p < decl->parent->nodesCount) {
        array = decl->parent->nodes[p];
    }
    if (array != NULL) {
        item->arrayCount = array->nodesCount;
        item->isInited = true;
        for (int i = 0; i < item->arrayCount; i++) {
            item->arraySize[i] = CalcConst(array->nodes[i]);
        }
    }
    if (isConst || is_global) {
        if (item->arrayCount == 0) {
            item->isInited = true;
            item->constVal = CalcConst(decl->parent->nodes[1]);
        }
    }
    strcpy(item->name, s);
    return id;
}

ASTNode* FindCodeBlock(ASTNode* rt) {
    if (match(rt->parent, "FuncFParam")) {
        return rt->parent->parent->parent;
    }
    while (!match(rt, "Block") && !match(rt, "If") && !match(rt, "Else") && !match(rt, "While") && rt->parent != rt) {
        rt = rt->parent;
    }
    return rt;
}

void BuildSymbolTableIfNotExists(ASTNode* rt) {
    if (rt->symbolTable == NULL) {
        rt->symbolTable = malloc(sizeof(SymbolTable));
        rt->symbolTable->symbolCount = 0;
        rt->symbolTable->symbolTableID = symbolTableCount++;
    }
}

void QuerySymbol(ASTNode* symbol, int* symbolTableID, int* symbolID, SymbolTableItem** info) {
    if (!symbol->isSymbol) {
        return;
    }
    bool isDecl = match(symbol->parent, "ConstDef") || match(symbol->parent, "VarDef") || match(symbol->parent, "FuncDef") || match(symbol->parent, "FuncFParam");
    bool isConst = match(symbol->parent, "ConstDef");
    ASTNode* rt = FindCodeBlock(symbol);
    if (isDecl) {
        BuildSymbolTableIfNotExists(rt);
    } else {
        while (rt->symbolTable == NULL || GetSymbolID(rt->symbolTable, symbol->symbolName) == -1) {
            rt = rt->parent;
            if (rt == rt->parent) {
                BuildSymbolTableIfNotExists(rt);
                break;
            }
        }
    }
    SymbolTable* table = rt->symbolTable;
    *symbolTableID = table->symbolTableID;
    if (isDecl) {
        *symbolID = InsertSymbolIfNotExists(table, symbol->symbolName, isConst, symbol);
    } else {
        *symbolID = GetSymbolID(table, symbol->symbolName);
        if (*symbolID == -1) {
            *symbolTableID = 0;
            *symbolID = InsertSymbolIfNotExists(table, symbol->symbolName, false, symbol);
            if (match(symbol->parent, "FuncCall")) {
                table->table[*symbolID].isUndeclaredFunc = true;
            }
            fprintf(stderr, "Warning: symbol %s is not declared before usage.\n", symbol->symbolName);
        }
    }
    *info = &(table->table[*symbolID]);
    (*info)->tableID = table->symbolTableID;
}

void PrintAST(ASTNode* node, int indent, FILE* fp) {
    if (node->parent == node) {
        BuildSymbolTableIfNotExists(node);
    }
    for (int i = 0; i < indent * 2; i++) {
        fprintf(fp, " ");
    }
    fprintf(fp, "%s", node->nodeType);
    if (node->isIntValue) {
        fprintf(fp, " %d\n", node->intValue);
    } else if (node->isSymbol) {
        int tableID, symbolID;
        SymbolTableItem* item;
        QuerySymbol(node, &tableID, &symbolID, &item);
        fprintf(fp, " name: %s symbolTable: %d symbolID: %d const: %d constVal: %d array: %d\n", node->symbolName, tableID, symbolID, item->isConst, item->constVal, item->arrayCount);
    } else {
        fprintf(fp, "\n");
    }
    if (match(node, "FuncDef")) {
        is_global = false;
    }
    for (int i = 0; i < node->nodesCount; i++) {
        PrintAST(node->nodes[i], indent + 1, fp);
    }
    if (match(node, "FuncDef")) {
        is_global = true;
    }
}
