#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ast.h"
#include "lrparser.tab.h"
#include "genllvm.h"

extern ASTNode* root;

int main() {
    root = NULL;
    yyparse();
    FILE* fp = fopen("generated_ast.txt", "w");
    if (root != NULL) {
        PrintAST(root, 0, fp);
    }
    fclose(fp);
    GenerateIRCode(root);
    return 0;
}
