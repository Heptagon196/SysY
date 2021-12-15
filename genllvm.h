#ifndef LLVMGEN
#define LLVMGEN
#include "lrparser.tab.h"
#include "ast.h"
int GenerateIRCode(ASTNode* root, ...);
#endif
