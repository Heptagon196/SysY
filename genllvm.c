// 写得挺烂的，姑且能跑（
#include "genllvm.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

int tempVarCount = 0;
int tempLabelCount = 0;
int globalTempCount = 0;
int varTable[1000][1000];
char temp[1024];
char temp2[1024];
char type_name[1024];
char type_init[1024];
bool enable_buffer = false;
SymbolTableItem* undeclaredFuncTable[1024];
int undeclaredFuncCount = 0;
char buffer[1000000];
int buffer_pos = 0;
char buffer_defer[1000000];
int buffer_defer_pos = 0;

static bool match(ASTNode* node, const char* name) {
    return !strcmp(node->nodeType, name);
}

void PrintIRCodeInstant(const char* code) {
    printf("%s", code);
}

void PrintIRCode(const char* code) {
    if (!enable_buffer) {
        PrintIRCodeInstant(code);
        return;
    }
    sprintf(buffer + buffer_pos, "%s", code);
    buffer_pos += strlen(code);
}

void PrintIRCodeDefer(const char* code) {
    sprintf(buffer_defer + buffer_defer_pos, "%s", code);
    buffer_defer_pos += strlen(code);
}

void PrintIRCodeBuffered() {
    printf("%s", buffer);
    buffer[0] = '\0';
    buffer_pos = 0;
}

void PrintIRCodeDeferBuffered() {
    printf("%s", buffer_defer);
    buffer_defer[0] = '\0';
    buffer_defer_pos = 0;
}

int GetTempVarID() {
    return ++tempVarCount;
}

int GetTempLabelID() {
    return ++tempLabelCount;
}

int GetGlobalTempID() {
    return ++globalTempCount;
}

void GetVarName(const char* name, int symbolTableID) {
    if (symbolTableID == 0) {
        sprintf(temp, "%%%s", name);
    } else {
        sprintf(temp, "%%%s.%d", name, symbolTableID);
    }
}

void GetVarTypeName(SymbolTableItem* item, int end_pos, bool skip_first) {
    bool has_skipped = false;
    if (item->arrayCount == 0) {
        sprintf(type_name, "i32");
    } else {
        char temp[1024];
        sprintf(type_name, "i32");
        for (int i = item->arrayCount - 1; i >= end_pos; i--) {
            strcpy(temp, type_name);
            if (skip_first) {
                has_skipped = true;
                skip_first = false;
                continue;
            }
            if (item->arraySize[i] == -1) {
                sprintf(type_name, "%s*", temp);
            } else {
                sprintf(type_name, "[%d x %s]", item->arraySize[i], temp);
            }
        }
    }
    if (has_skipped) {
        strcpy(temp, type_name);
        sprintf(type_name, "%s*", temp);
    }
}

void LoadSymbolArray(ASTNode* arr, SymbolTableItem* item, char* out, bool use_const, int* array_pos) {
    char type[1024];
    for (int i = 0; i < arr->nodesCount; i++) {
        GetVarTypeName(item, i, false);
        strcpy(type, type_name);
        int next = GetTempVarID();
        if (use_const) {
            sprintf(temp2, "%%var%d = getelementptr inbounds %s, %s* %s, i32 0, i32 %d\n", next, type, type, out, array_pos[i]);
            PrintIRCode(temp2);
            sprintf(out, "%%var%d", next);
        } else {
            if (i == 0 && item->arraySize[0] == -1) {
                int idx = GenerateIRCode(arr->nodes[i]->nodes[0]);
                int load = GetTempVarID();
                int load2 = GetTempVarID();
                sprintf(temp2, "%%var%d = load %s, %s* %s\n", load, type, type, out);
                PrintIRCode(temp2);
                sprintf(temp2, "%%var%d = load i32, i32* %%var%d\n", load2, idx);
                PrintIRCode(temp2);
                char temp_type[1024];
                strcpy(temp_type, type);
                temp_type[strlen(temp_type) - 1] = '\0';
                sprintf(temp2, "%%var%d = getelementptr inbounds %s, %s* %%var%d, i32 %%var%d\n", next, temp_type, temp_type, load, load2);
                PrintIRCode(temp2);
            } else {
                int idx = GenerateIRCode(arr->nodes[i]);
                sprintf(temp2, "%%var%d = getelementptr inbounds %s, %s* %s, i32 0, i32 %%var%d\n", next, type, type, out, idx);
                PrintIRCode(temp2);
            }
            sprintf(out, "%%var%d", next);
        }
    }
}

void PrintInitVal(SymbolTableItem* item, int p, ASTNode* declNode, char* out, bool isNonConstLocal, int* array_pos, char* var_name) {
    char temp_init[1024];
    sprintf(out, "i32");
    if (p == item->arrayCount) {
        strcpy(temp_init, out);
        if (isNonConstLocal) {
            char var[128];
            strcpy(var, var_name);
            LoadSymbolArray(item->decl->parent->nodes[1], item, var, true, array_pos);
            if (declNode != NULL) {
                int ans = GenerateIRCode(declNode);
                sprintf(temp, "store i32 %%var%d, i32* %s\n", ans, var);
            } else {
                sprintf(temp, "store i32 0, i32* %s\n", var);
            }
            PrintIRCode(temp);
        } else {
            sprintf(out, "%s %d", temp_init, declNode == NULL ? 0 : CalcConst(declNode));
        }
        return;
    }
    for (int i = item->arrayCount - 1; i >= p; i--) {
        strcpy(temp_init, out);
        sprintf(out, "[%d x %s]", item->arraySize[i], temp_init);
    }
    strcpy(temp_init, out);
    sprintf(out, "%s [", temp_init);
    if (p == 0) {
        sprintf(out, "[");
    }
    for (int i = 0; i < item->arraySize[p]; i++) {
        if (i != 0) {
            strcpy(temp_init, out);
            sprintf(out, "%s, ", temp_init);
        }
        array_pos[p] = i;
        ASTNode* next;
        if (declNode == NULL) {
            next = declNode;
        } else {
            next = declNode->nodesCount > i ? declNode->nodes[i] : NULL;
        }
        PrintInitVal(item, p + 1, next, out + strlen(out), isNonConstLocal, array_pos, var_name);
    }
    strcpy(temp_init, out);
    sprintf(out, "%s]", temp_init);
}

void GetVarInitVal(SymbolTableItem* item, bool isNonConstLocal, char* varname) {
    if (item->arrayCount == 0) {
        sprintf(type_init, "%d", item->constVal);
    } else {
        int array_pos[128];
        PrintInitVal(item, 0, item->decl->parent->nodes[2], type_init, isNonConstLocal, array_pos, varname);
    }
}

bool exp_ret_type = false;

int LoadVar(ASTNode* root, bool* isSymbol, char* type) {
    if (match(root, "Symbol")) {
        bool is_array = root->nodesCount > 0;
        int symbolTableID, symbolID;
        SymbolTableItem* item;
        QuerySymbol(root, &symbolTableID, &symbolID, &item);
        int end_pos = is_array ? root->nodes[0]->nodesCount : 0;
        char get_element_type_name[1024];
        GetVarTypeName(item, end_pos, true);
        strcpy(type, type_name);
        GetVarTypeName(item, end_pos, false);
        strcpy(get_element_type_name, type_name);
        GetVarName(root->symbolName, symbolTableID);
        if (symbolTableID == 0) {
            temp[0] = '@';
        }
        char var[128];
        strcpy(var, temp);
        if (is_array) {
            LoadSymbolArray(root->nodes[0], item, var, false, NULL);
        }
        int ret = GetTempVarID();
        if (!strcmp(type, "i32")) {
            sprintf(temp2, "%%var%d = load i32, i32* %s\n", ret, var);
        } else {
            sprintf(temp2, "%%var%d = getelementptr inbounds %s, %s* %s, i32 0, i32 0\n", ret, get_element_type_name, get_element_type_name, var);
        }
        PrintIRCode(temp2);
        *isSymbol = true;
        return ret;
    } else if (match(root, "ConstInt")) {
        strcpy(type, "i32");
        *isSymbol = false;
        return root->intValue;
    } else {
        *isSymbol = true;
        exp_ret_type = true;
        int ret = GenerateIRCode(root, type);
        exp_ret_type = false;
        return ret;
    }
}

void LoadVarAsString(ASTNode* root, char* out, char* type) {
    strcpy(type, "i32");
    bool isSymbol, isArray;
    int comp = LoadVar(root, &isSymbol, type);
    if (isSymbol) {
        sprintf(out, "%s %%var%d", type, comp);
    } else {
        sprintf(out, "i32 %d", comp);
    }
}

int GenerateBinaryOperator(ASTNode* root, char* symbol, bool isBool) {
    int ret = GetTempVarID();
    char var1[128], var2[128];
    char type[1024];
    LoadVarAsString(root->nodes[0], var1, type);
    LoadVarAsString(root->nodes[1], var2, type);
    sprintf(temp, "%%var%d = %s %s, %s\n", ret, symbol, var1, var2 + 4);
    PrintIRCode(temp);
    if (isBool) {
        int zext = GetTempVarID();
        sprintf(temp, "%%var%d = zext i1 %%var%d to i32\n", zext, ret);
        PrintIRCode(temp);
        ret = zext;
    }
    return ret;
}

int while_comp[1024], while_end[1024];
int while_comp_cnt = 0;
int while_end_cnt = 0;
void AddWhileComp(int id) {
    while_comp[while_comp_cnt++] = id;
}
void RemoveWhileComp() {
    while_comp_cnt--;
}
int GetWhileComp() {
    return while_comp[while_comp_cnt - 1];
}
void AddWhileEnd(int id) {
    while_end[while_end_cnt++] = id;
}
void RemoveWhileEnd() {
    while_end_cnt--;
}
int GetWhileEnd() {
    return while_end[while_end_cnt - 1];
}

int return_id;
int return_pos;
int jump_to_return[1024];
int jump_to_return_cnt = 0;
int current_label = 0;
bool add_suffix = false;
SymbolTableItem* param_list[128];
int param_count = 0;
bool is_global_var = true;
bool print_symbol = true;

void DefVar(ASTNode* root, bool isConst) {
    SymbolTableItem* current_var;
    char current_var_name[1024];
    bool buffer = enable_buffer;
    enable_buffer = false;
    print_symbol = false;
    GenerateIRCode(root->nodes[0], current_var_name, &current_var);
    print_symbol = true;
    GetVarTypeName(current_var, 0, false);
    if (is_global_var || isConst) {
        GetVarInitVal(current_var, false, current_var_name);
    }
    if (is_global_var) {
        sprintf(temp, "%s = dso_local %s %s %s\n", current_var_name, isConst ? "constant" : "global", type_name, type_init);
        PrintIRCode(temp);
    } else if (isConst) {
        int gval = GetGlobalTempID();
        sprintf(temp, "@tempconst.%d.0 = dso_local constant %s %s\n", gval, type_name, type_init);
        PrintIRCodeDefer(temp);
        sprintf(temp, "%s = alloca %s\n", current_var_name, type_name);
        PrintIRCode(temp);
        enable_buffer = true;
        int tmpvar = GetTempVarID();
        sprintf(temp, "%%var%d = load %s, %s* @tempconst.%d.0\n", tmpvar, type_name, type_name, gval);
        PrintIRCode(temp);
        sprintf(temp, "store %s %%var%d, %s* %s\n", type_name, tmpvar, type_name, current_var_name);
        PrintIRCode(temp);
        enable_buffer = false;
    } else {
        sprintf(temp, "%s = alloca %s\n", current_var_name, type_name);
        PrintIRCode(temp);
        if (current_var->arrayCount == 0) {
            if (root->nodesCount > 1) {
                int ans = GenerateIRCode(root->nodes[1]);
                sprintf(temp, "store i32 %%var%d, i32* %s\n", ans, current_var_name);
                PrintIRCode(temp);
            }
        } else {
            if (root->nodesCount == 3) {
                GetVarInitVal(current_var, true, current_var_name);
            }
        }
    }
    enable_buffer = buffer;
}

int GenerateIRCode(ASTNode* root, ...) {
    if (match(root, "CompUnit")) {
        for (int i = 0; i < root->nodesCount; i++) {
            GenerateIRCode(root->nodes[i]);
        }
    } else if (match(root, "Int")) {
        PrintIRCode("i32");
    } else if (match(root, "Symbol")) {
        int symbolTableID, symbolID;
        SymbolTableItem* item;
        QuerySymbol(root, &symbolTableID, &symbolID, &item);
        GetVarName(root->symbolName, symbolTableID);
        if (match(root->parent, "FuncDef") || symbolTableID == 0) {
            temp[0] = '@';
        }
        char var[128];
        strcpy(var, temp);
        if (root->nodesCount > 0) {
            LoadSymbolArray(root->nodes[0], item, var, false, NULL);
        }
        strcpy(temp, var);
        if (print_symbol) {
            PrintIRCode(temp);
        }
        va_list va;
        va_start(va, root);
        char* current_var_name = va_arg(va, char*);
        SymbolTableItem** current_var = va_arg(va, SymbolTableItem**);
        va_end(va);
        strcpy(current_var_name, temp);
        *current_var = item;
        if (add_suffix) {
            if (print_symbol) {
                PrintIRCode(".param");
            }
            param_list[param_count++] = item;
        }
    } else if (match(root, "ConstDecl")) {
        GenerateIRCode(root->nodes[1]);
    } else if (match(root, "ConstDeclList")) {
        for (int i = 0; i < root->nodesCount; i++) {
            GenerateIRCode(root->nodes[i]);
        }
    } else if (match(root, "VarDecl")) {
        GenerateIRCode(root->nodes[1]);
    } else if (match(root, "VarDeclList")) {
        for (int i = 0; i < root->nodesCount; i++) {
            GenerateIRCode(root->nodes[i]);
        }
    } else if (match(root, "ConstDef")) {
        DefVar(root, true);
    } else if (match(root, "VarDef")) {
        DefVar(root, false);
    } else if (match(root, "Extern")) {
        SymbolTableItem* item;
        root = root->nodes[0];
        bool is_int = match(root->nodes[0], "Int");
        PrintIRCode("declare ");
        GenerateIRCode(root->nodes[0]);
        PrintIRCode(" ");
        GenerateIRCode(root->nodes[1], temp, &item);
        PrintIRCode("(");
        if (root->nodesCount == 2) {
            PrintIRCode(");\n");
        } else {
            root = root->nodes[2];
            for (int i = 0; i < root->nodesCount; i++) {
                int symbolTableID, symbolID;
                SymbolTableItem* item;
                QuerySymbol(root->nodes[i]->nodes[1], &symbolTableID, &symbolID, &item);
                GetVarTypeName(item, 0, false);
                if (i != 0) {
                    PrintIRCode(", ");
                }
                PrintIRCode(type_name);
            }
            PrintIRCode(");\n");
        }
    } else if (match(root, "FuncDef")) {
        SymbolTableItem* item;
        int begin = GetTempLabelID();
        return_pos = GetTempLabelID();
        const char* return_type = root->nodes[0]->nodeType;
        if (!strcmp(return_type, "Int")) {
            return_id = GetTempVarID();
        }
        PrintIRCode("define dso_local ");
        GenerateIRCode(root->nodes[0]);
        PrintIRCode(" ");
        GenerateIRCode(root->nodes[1], temp, &item);
        PrintIRCode("(");
        is_global_var = false;
        int block_id;
        param_count = 0;
        if (root->nodesCount == 3) {
            PrintIRCode(") #0 {\n");
            block_id = 2;
        } else {
            add_suffix = true;
            GenerateIRCode(root->nodes[2]);
            add_suffix = false;
            PrintIRCode(") #0 {\n");
            block_id = 3;
        }
        sprintf(temp, "label%d:\n", begin);
        PrintIRCodeInstant(temp);
        if (!strcmp(return_type, "Int")) {
            sprintf(temp, "%%var%d = alloca i32\n", return_id);
            PrintIRCodeInstant(temp);
        }
        for (int i = 0; i < param_count; i++) {
            SymbolTableItem* symbol = param_list[i];
            GetVarName(symbol->name, symbol->tableID);
            GetVarTypeName(symbol, 0, false);
            sprintf(temp2, "%s = alloca %s\n", temp, type_name);
            PrintIRCodeInstant(temp2);
            sprintf(temp2, "store %s %s.param, %s* %s\n", type_name, temp, type_name, temp);
            PrintIRCode(temp2);
        }
        enable_buffer = true;
        jump_to_return_cnt = 0;
        GenerateIRCode(root->nodes[block_id]);
        sprintf(temp, "br label %%label%d\n", return_pos);
        PrintIRCode(temp);
        if (jump_to_return_cnt != 0) {
            int val = jump_to_return[jump_to_return_cnt - 1];
            sprintf(temp, "label%d: ; preds = %%label%d", return_pos, val);
            for (int i = jump_to_return_cnt - 2; i >= 0; i--) {
                strcpy(temp2, temp);
                val = jump_to_return[i];
                sprintf(temp, "%s, %%label%d", temp2, val);
            }
            PrintIRCode(temp);
            PrintIRCode("\n");
        } else {
            sprintf(temp, "label%d: ; preds = %%label%d\n", return_pos, begin);
            PrintIRCode(temp);
        }
        if (!strcmp(return_type, "Int")) {
            int ret = GetTempVarID();
            sprintf(temp, "%%var%d = load i32, i32* %%var%d\n", ret, return_id);
            PrintIRCode(temp);
            sprintf(temp, "ret i32 %%var%d\n", ret);
            PrintIRCode(temp);
        } else {
            PrintIRCode("ret void\n");
        }
        jump_to_return_cnt = 0;
        enable_buffer = false;
        PrintIRCodeBuffered();
        PrintIRCode("}\n");
        current_label = 0;
        tempVarCount = 0;
        tempLabelCount = 0;
        is_global_var = true;
        PrintIRCodeDeferBuffered();
    } else if (match(root, "TypeVoid")) {
        PrintIRCode("void");
    } else if (match(root, "FuncFParams")) {
        GenerateIRCode(root->nodes[0]);
        for (int i = 1; i < root->nodesCount; i++) {
            PrintIRCode(", ");
            GenerateIRCode(root->nodes[i]);
        }
    } else if (match(root, "FuncFParam")) {
        char var_name[128];
        SymbolTableItem* item;
        print_symbol = false;
        GenerateIRCode(root->nodes[1], var_name, &item);
        print_symbol = true;
        GetVarTypeName(item, 0, false);
        sprintf(temp, "%s %s.param", type_name, var_name);
        PrintIRCode(temp);
    } else if (match(root, "Block")) {
        for (int i = 0; i < root->nodesCount; i++) {
            GenerateIRCode(root->nodes[i]);
        }
    } else if (match(root, "Assign")) {
        char current_var_name[1024];
        SymbolTableItem* current_var;
        print_symbol = false;
        GenerateIRCode(root->nodes[0], current_var_name, &current_var);
        if (current_var->isConst) {
            fprintf(stderr, "Warning: assigning to a local constant.\n");
        }
        print_symbol = true;
        int ans = GenerateIRCode(root->nodes[1]);
        sprintf(temp, "store i32 %%var%d, i32* %s\n", ans, current_var_name);
        PrintIRCode(temp);
    } else if (match(root, "Exp") || match(root, "Cond")) {
        va_list va;
        va_start(va, root);
        char* exp_type = va_arg(va, char*);
        va_end(va);
        char var[128];
        char type[1024];
        LoadVarAsString(root->nodes[0], var, type);
        if (exp_ret_type) {
            strcpy(exp_type, type);
        }
        int ret = GetTempVarID();
        sprintf(temp, "%%var%d = alloca %s\n", ret, type);
        PrintIRCodeInstant(temp);
        sprintf(temp, "store %s, %s* %%var%d\n", var, type, ret);
        PrintIRCode(temp);
        int load = GetTempVarID();
        sprintf(temp, "%%var%d = load %s, %s* %%var%d\n", load, type, type, ret);
        PrintIRCode(temp);
        return load;
    } else if (match(root, "If")) {
        int start_pos = current_label;
        bool hasElse = root->nodesCount == 3;
        int cond = GenerateIRCode(root->nodes[0]);
        int jump_true = GetTempLabelID();
        int jump_false = GetTempLabelID();
        int jump_end;
        if (hasElse) {
            jump_end = GetTempLabelID();
        }
        int conv = GetTempVarID();
        sprintf(temp, "%%var%d = trunc i32 %%var%d to i1\n", conv, cond);
        PrintIRCode(temp);
        cond = conv;
        sprintf(temp, "br i1 %%var%d, label %%label%d, label %%label%d\n", cond, jump_true, jump_false);
        PrintIRCode(temp);
        sprintf(temp, "label%d: ; preds = %%label%d\n", jump_true, start_pos);
        PrintIRCode(temp);
        current_label = jump_true;
        GenerateIRCode(root->nodes[1]);
        sprintf(temp, "br label %%label%d\n", hasElse ? jump_end : jump_false);
        PrintIRCode(temp);
        if (hasElse) {
            sprintf(temp, "label%d: ; preds = %%label%d\n", jump_false, start_pos);
            PrintIRCode(temp);
            current_label = jump_false;
            GenerateIRCode(root->nodes[2]);
            sprintf(temp, "br label %%label%d\n", jump_end);
            PrintIRCode(temp);
            sprintf(temp, "label%d: ; preds = %%label%d, %%label%d\n", jump_end, jump_false, jump_true);
            PrintIRCode(temp);
            current_label = jump_end;
        } else {
            sprintf(temp, "label%d: ; preds = %%label%d, %%label%d\n", jump_false, jump_true, start_pos);
            PrintIRCode(temp);
            current_label = jump_false;
        }
    } else if (match(root, "Else")) {
        GenerateIRCode(root->nodes[0]);
    } else if (match(root, "While")) {
        int start_pos = current_label;
        int comp = GetTempLabelID();
        int begin = GetTempLabelID();
        int end = GetTempLabelID();
        sprintf(temp, "br label %%label%d\n", comp);
        PrintIRCode(temp);
        sprintf(temp, "label%d: ; preds = %%label%d, %%label%d\n", comp, begin, start_pos);
        PrintIRCode(temp);
        current_label = comp;
        int result = GenerateIRCode(root->nodes[0]);
        int conv = GetTempVarID();
        sprintf(temp, "%%var%d = trunc i32 %%var%d to i1\n", conv, result);
        PrintIRCode(temp);
        result = conv;
        sprintf(temp, "br i1 %%var%d, label %%label%d, label %%label%d\n", result, begin, end);
        PrintIRCode(temp);
        sprintf(temp, "label%d: ; preds = %%label%d\n", begin, comp);
        PrintIRCode(temp);
        current_label = begin;

        AddWhileComp(comp);
        AddWhileEnd(end);
        GenerateIRCode(root->nodes[1]);
        RemoveWhileComp();
        RemoveWhileEnd();

        sprintf(temp, "br label %%label%d\n", comp);
        PrintIRCode(temp);
        sprintf(temp, "label%d: ; preds = %%label%d\n", end, comp);
        PrintIRCode(temp);
        current_label = end;
    } else if (match(root, "Break")) {
        sprintf(temp, "br label %%label%d\n", GetWhileEnd());
        PrintIRCode(temp);
    } else if (match(root, "Continue")) {
        sprintf(temp, "br label %%label%d\n", GetWhileComp());
        PrintIRCode(temp);
    } else if (match(root, "Return")) {
        if (root->nodesCount == 1) {
            int ret = GenerateIRCode(root->nodes[0]);
            sprintf(temp, "store i32 %%var%d, i32* %%var%d\n", ret, return_id);
            PrintIRCode(temp);
        }
        sprintf(temp, "br label %%label%d\n", return_pos);
        PrintIRCode(temp);
        jump_to_return[jump_to_return_cnt++] = current_label;
    } else if (match(root, "EmptyStatement")) {
    } else if (match(root, "ConstInt")) {
        int ret = GetTempVarID();
        sprintf(temp, "%%var%d = alloca i32\n", ret);
        PrintIRCodeInstant(temp);
        sprintf(temp, "store i32 %d, i32* %%var%d\n", root->intValue, ret);
        PrintIRCode(temp);
        return ret;
    } else if (match(root, "FuncCall")) {
        char callParams[128][128];
        char type[128][1024];
        int params = 0;
        if (root->nodesCount == 2) {
            params = root->nodes[1]->nodesCount;
            for (int i = 0; i < params; i++) {
                LoadVarAsString(root->nodes[1]->nodes[i], callParams[i], type[i]);
            }
        }
        int symbolTableID, symbolID;
        SymbolTableItem* item;
        QuerySymbol(root->nodes[0], &symbolTableID, &symbolID, &item);
        GetVarName(root->nodes[0]->symbolName, symbolTableID);
        temp[0] = '@';
        bool isUndeclared = item->isUndeclaredFunc;
        bool hasDeclared = false;
        int ret = 0;
        ASTNode* funcDef;
        if (isUndeclared) {
            for (int i = 0; i < undeclaredFuncCount; i++) {
                if (!strcmp(undeclaredFuncTable[i]->name, item->name)) {
                    hasDeclared = true;
                    break;
                }
            }
            if (!hasDeclared) {
                undeclaredFuncTable[undeclaredFuncCount++] = item;
                sprintf(temp2, "declare i32 %s(", temp);
                PrintIRCodeDefer(temp2);
            }
        } else {
            funcDef = item->decl->parent;
        }
        bool has_ret;
        if (isUndeclared || match(funcDef->nodes[0], "Int")) {
            has_ret = true;
            ret = GetTempVarID();
            sprintf(temp2, "%%var%d = call i32 %s(", ret, temp);
            PrintIRCode(temp2);
        } else {
            has_ret = false;
            sprintf(temp2, "call void %s(", temp);
            PrintIRCode(temp2);
        }
        if (hasDeclared) {
            isUndeclared = false;
        }
        if (params > 0) {
            if (isUndeclared) {
                PrintIRCodeDefer(type[0]);
            }
            PrintIRCode(callParams[0]);
            for (int i = 1; i < params; i++) {
                if (isUndeclared) {
                    PrintIRCodeDefer(", ");
                    PrintIRCodeDefer(type[i]);
                }
                PrintIRCode(", ");
                PrintIRCode(callParams[i]);
            }
        }
        if (isUndeclared) {
            PrintIRCodeDefer(")\n");
        }
        PrintIRCode(")\n");
        if (!has_ret) {
            int zero = GetTempVarID();
            ret = GetTempVarID();
            sprintf(temp, "%%var%d = alloca i32\n", zero);
            PrintIRCodeInstant(temp);
            sprintf(temp, "store i32 0, i32* %%var%d\n", zero);
            PrintIRCode(temp);
            sprintf(temp, "%%var%d = load i32, i32* %%var%d\n", ret, zero);
            PrintIRCode(temp);
        }
        return ret;
    } else if (match(root, "Positive")) {
        return GenerateIRCode(root->nodes[0]);
    } else if (match(root, "Negative")) {
        int ret = GetTempVarID();
        char var[128];
        char type[1024];
        LoadVarAsString(root->nodes[0], var, type);
        sprintf(temp, "%%var%d = sub nsw 0, %s\n", ret, var);
        PrintIRCode(temp);
        return ret;
    } else if (match(root, "Not")) {
        int ret = GetTempVarID();
        char var[128];
        char type[1024];
        LoadVarAsString(root->nodes[0], var, type);
        sprintf(temp, "%%var%d = icmp eq 0, %s\n", ret, var);
        PrintIRCode(temp);
        int zext = GetTempVarID();
        sprintf(temp, "%%var%d = zext i1 %%var%d to i32\n", zext, ret);
        PrintIRCode(temp);
        return zext;
    } else if (match(root, "+")) {
        return GenerateBinaryOperator(root, "add nsw", false);
    } else if (match(root, "-")) {
        return GenerateBinaryOperator(root, "sub nsw", false);
    } else if (match(root, "*")) {
        return GenerateBinaryOperator(root, "mul nsw", false);
    } else if (match(root, "/")) {
        return GenerateBinaryOperator(root, "sdiv", false);
    } else if (match(root, "%")) {
        return GenerateBinaryOperator(root, "srem", false);
    } else if (match(root, "<")) {
        return GenerateBinaryOperator(root, "icmp slt", true);
    } else if (match(root, ">")) {
        return GenerateBinaryOperator(root, "icmp sgt", true);
    } else if (match(root, ">=")) {
        return GenerateBinaryOperator(root, "icmp sge", true);
    } else if (match(root, "<=")) {
        return GenerateBinaryOperator(root, "icmp sle", true);
    } else if (match(root, "==")) {
        return GenerateBinaryOperator(root, "icmp eq", true);
    } else if (match(root, "!=")) {
        return GenerateBinaryOperator(root, "icmp ne", true);
    } else if (match(root, "&&")) {
        return GenerateBinaryOperator(root, "and", true);
    } else if (match(root, "||")) {
        return GenerateBinaryOperator(root, "or", true);
    }
    return 0;
}
