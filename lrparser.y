%{

#include <stdio.h>
#include <string.h>
#include "ast.h"
extern int line_cnt;
int yylex(void);
void yyerror(char *);
ASTNode* root;
static char extern_temp[1024];
static int extern_temp_cnt = 0;
static char* GetTempExternParam() {
    sprintf(extern_temp, "temp%d", extern_temp_cnt++);
    return extern_temp;
}

%}
%token
    SYMBOL
    NUMBER
    CONST
    INT
    VOID
    IF
    ELSE
    BREAK
    CONTINUE
    WHILE
    RETURN
    ASSIGN
    COM
    SEM
    L_ROUND_BRACE
    R_ROUND_BRACE
    L_SQUARE_BRACE
    R_SQUARE_BRACE
    L_CURLY_BRACE
    R_CURLY_BRACE
    ADD
    SUB
    MUL
    DIV
    MOD
    E
    NE
    L
    G
    LE
    GE
    NOT
    AND
    OR
    END
    EXTERN
    VIRTUAL_IF

%union {
    char sVal[256];
    int iVal;
    struct _ASTNode* astNode;
};
%type <astNode> Program CompUnit Decl ConstDecl BType ConstDef ConstInitVal VarDecl VarDef InitVal FuncDef FuncType FuncFParams FuncFParam Block BlockItem Stmt Exp Cond LVal PrimaryExp Number UnaryExp UnaryOp FuncRParams MulExp AddExp RelExp EqExp LAndExp LOrExp ConstExp ConstDeclVarList ConstInitValVarList VarDeclVarList InitValVarList ConstDefArray VarDefArray FuncFParamArray LValArray MulOp AddOp RelOp EqOp BlockList Ident ExternFuncFParam ExternFuncFParams
%type <iVal> NUMBER
%type <sVal> SYMBOL

%left VIRTUAL_IF
%left ELSE

%%

Program: CompUnit { root = $$; }
       ;
CompUnit: CompUnit Decl { AddChildNode($1, $2); $$ = $1; }
        | CompUnit FuncDef { AddChildNode($1, $2); $$ = $1; }
        | { $$ = NewNode("CompUnit", 0); }
        ;
Decl: ConstDecl { $$ = $1; }
    | VarDecl { $$ = $1; }
    ;
ConstDecl: CONST BType ConstDeclVarList SEM { $$ = NewNode("ConstDecl", 2, $2, $3); }
         ;
ConstDeclVarList: ConstDef { $$ = NewNode("ConstDeclList", 1, $1); }
                     | ConstDeclVarList COM ConstDef { AddChildNode($1, $3); $$ = $1; }
                     ;
BType: INT { $$ = NewNode("Int", 0); }
     ;
Ident: SYMBOL { $$ = NewNode("Symbol", 0); $$->isSymbol = true; strcpy($$->symbolName, $1); }
     ;
ConstDef: Ident ConstDefArray ASSIGN ConstInitVal { $$ = NewNode("ConstDef", 3, $1, $2, $4); }
        | Ident ASSIGN ConstInitVal { $$ = NewNode("ConstDef", 2, $1, $3); }
        ;
ConstDefArray: L_SQUARE_BRACE ConstExp R_SQUARE_BRACE { $$ = NewNode("ConstDefArray", 1, $2); }
             | ConstDefArray L_SQUARE_BRACE ConstExp R_SQUARE_BRACE { AddChildNode($1, $3); $$ = $1; }
             ;
ConstInitVal: ConstExp { $$ = $1; }
            | L_CURLY_BRACE R_CURLY_BRACE { $$ = NewNode("Empty", 0); }
            | L_CURLY_BRACE ConstInitValVarList R_CURLY_BRACE { $$ = $2; }
            ;
ConstInitValVarList: ConstInitVal { $$ = NewNode("ConstInitVarList", 1, $1); }
                   | ConstInitValVarList COM ConstInitVal { AddChildNode($1, $3); $$ = $1; }
                   ;
VarDecl: BType VarDeclVarList SEM { $$ = NewNode("VarDecl", 2, $1, $2); }
       ;
VarDeclVarList: VarDef { $$ = NewNode("VarDeclList", 1, $1); }
              | VarDeclVarList COM VarDef { AddChildNode($1, $3); $$ = $1; }
              ;
VarDef: Ident { $$ = NewNode("VarDef", 1, $1); }
      | Ident VarDefArray { $$ = NewNode("VarDef", 2, $1, $2); }
      | Ident ASSIGN InitVal { $$ = NewNode("VarDef", 2, $1, $3); }
      | Ident VarDefArray ASSIGN InitVal { $$ = NewNode("VarDef", 3, $1, $2, $4); }
      ;
VarDefArray: L_SQUARE_BRACE ConstExp R_SQUARE_BRACE { $$ = NewNode("Array", 1, $2); }
           | VarDefArray L_SQUARE_BRACE ConstExp R_SQUARE_BRACE { AddChildNode($1, $3); $$ = $1; }
           ;
InitVal: Exp { $$ = $1; }
       | L_CURLY_BRACE InitValVarList R_CURLY_BRACE { $$ = $2; }
       ;
InitValVarList: InitVal { $$ = NewNode("InitValList", 1, $1); }
              | InitValVarList COM InitVal { AddChildNode($1, $3); $$ = $1; }
              ;
FuncDef: FuncType Ident L_ROUND_BRACE R_ROUND_BRACE Block { $$ = NewNode("FuncDef", 3, $1, $2, $5); }
       | FuncType Ident L_ROUND_BRACE FuncFParams R_ROUND_BRACE Block { $$ = NewNode("FuncDef", 4, $1, $2, $4, $6); }
       | BType Ident L_ROUND_BRACE R_ROUND_BRACE Block { $$ = NewNode("FuncDef", 3, $1, $2, $5); }
       | BType Ident L_ROUND_BRACE FuncFParams R_ROUND_BRACE Block { $$ = NewNode("FuncDef", 4, $1, $2, $4, $6); }
       | EXTERN FuncType Ident L_ROUND_BRACE R_ROUND_BRACE SEM { $$ = NewNode("Extern", 1, NewNode("FuncDef", 2, $2, $3)); }
       | EXTERN FuncType Ident L_ROUND_BRACE ExternFuncFParams R_ROUND_BRACE SEM { $$ = NewNode("Extern", 1, NewNode("FuncDef", 3, $2, $3, $5)); }
       | EXTERN BType Ident L_ROUND_BRACE R_ROUND_BRACE SEM { $$ = NewNode("Extern", 1, NewNode("FuncDef", 2, $2, $3)); }
       | EXTERN BType Ident L_ROUND_BRACE ExternFuncFParams R_ROUND_BRACE SEM { $$ = NewNode("Extern", 1, NewNode("FuncDef", 3, $2, $3, $5)); }
       ;
FuncType: VOID { $$ = NewNode("TypeVoid", 0); }
        ;
ExternFuncFParams: ExternFuncFParam { $$ = NewNode("FuncFParams", 1, $1); }
                 | ExternFuncFParams COM ExternFuncFParam { AddChildNode($1, $3); $$ = $1; }
                 ;
ExternFuncFParam: BType { ASTNode* var = NewNode("Symbol", 0); var->isSymbol = true; strcpy(var->symbolName, GetTempExternParam()); $$ = NewNode("FuncFParam", 2, $1, var); }
                | BType FuncFParamArray { ASTNode* var = NewNode("Symbol", 0); var->isSymbol = true; strcpy(var->symbolName, GetTempExternParam()); $$ = NewNode("FuncFParam", 3, $1, var, $2); }
                ;
FuncFParams: FuncFParam { $$ = NewNode("FuncFParams", 1, $1); }
           | FuncFParams COM FuncFParam { AddChildNode($1, $3); $$ = $1; }
           ;
FuncFParam: BType Ident { $$ = NewNode("FuncFParam", 2, $1, $2); }
          | BType Ident FuncFParamArray { $$ = NewNode("FuncFParam", 3, $1, $2, $3); }

FuncFParamArray: L_SQUARE_BRACE R_SQUARE_BRACE { $$ = NewNode("Array", 1, NewNode("Uncertain", 0)); }
               | FuncFParamArray L_SQUARE_BRACE ConstExp R_SQUARE_BRACE { AddChildNode($1, $3); $$ = $1; }
               ;
Block: L_CURLY_BRACE BlockList R_CURLY_BRACE { $$ = $2; }
     ;
BlockList: BlockItem { $$ = NewNode("Block", 1, $1); }
         | BlockList BlockItem { AddChildNode($1, $2); $$ = $1; }
         ;
BlockItem: Decl { $$ = $1; }
         | Stmt { $$ = $1; }
         ;
Stmt: LVal ASSIGN Exp SEM { $$ = NewNode("Assign", 2, $1, $3); }
    | Exp SEM { $$ = $1; }
    | Block { $$ = $1; }
    | IF L_ROUND_BRACE Cond R_ROUND_BRACE Stmt %prec VIRTUAL_IF { $$ = NewNode("If", 2, $3, $5); }
    | IF L_ROUND_BRACE Cond R_ROUND_BRACE Stmt ELSE Stmt { $$ = NewNode("If", 3, $3, $5, NewNode("Else", 1, $7)); }
    | WHILE L_ROUND_BRACE Cond R_ROUND_BRACE Stmt { $$ = NewNode("While", 2, $3, $5); }
    | BREAK SEM { $$ = NewNode("Break", 0); }
    | CONTINUE SEM { $$ = NewNode("Continue", 0); }
    | RETURN SEM { $$ = NewNode("Return", 0); }
    | RETURN Exp SEM { $$ = NewNode("Return", 1, $2); }
    | SEM { $$ = NewNode("EmptyStatement", 0); }
    ;
Exp: AddExp { $$ = NewNode("Exp", 1, $1); }
   ;
Cond: LOrExp { $$ = NewNode("Cond", 1, $1); }
    ;
LVal: Ident { $$ = $1; }
    | Ident LValArray { AddChildNode($1, $2); $$ = $1; }
    ;
LValArray: L_SQUARE_BRACE Exp R_SQUARE_BRACE{ $$ = NewNode("Array", 1, $2); }
         | LValArray L_SQUARE_BRACE Exp R_SQUARE_BRACE { AddChildNode($1, $3); $$ = $1; }
         ;
PrimaryExp: L_ROUND_BRACE Exp R_ROUND_BRACE { $$ = $2; }
          | LVal { $$ = $1; }
          | Number { $$ = $1; }
          ;
Number: NUMBER { $$ = NewNode("ConstInt", 0); $$->isIntValue = true; $$->intValue = $1; }
     ;
UnaryExp: PrimaryExp { $$ = $1; }
        | Ident L_ROUND_BRACE R_ROUND_BRACE { $$ = NewNode("FuncCall", 1, $1); }
        | Ident L_ROUND_BRACE FuncRParams R_ROUND_BRACE { $$ = NewNode("FuncCall", 2, $1, $3); }
        | UnaryOp UnaryExp { AddChildNode($1, $2); $$ = $1; }
        ;
UnaryOp: ADD { $$ = NewNode("Positive", 0); }
       | SUB { $$ = NewNode("Negative", 0); }
       | NOT { $$ = NewNode("Not", 0); }
       ;
FuncRParams: Exp { $$ = NewNode("FuncCallParams", 1, $1); }
           | FuncRParams COM Exp { AddChildNode($1, $3); $$ = $1; }
           ;
MulExp: UnaryExp { $$ = $1; }
      | MulExp MulOp UnaryExp { AddChildNode($2, $1); AddChildNode($2, $3); $$ = $2; }
      ;
MulOp: MUL { $$ = NewNode("*", 0); }
     | DIV { $$ = NewNode("/", 0); }
     | MOD { $$ = NewNode("%", 0); }
     ;
AddExp: MulExp { $$ = $1; }
      | AddExp AddOp MulExp { AddChildNode($2, $1); AddChildNode($2, $3); $$ = $2; }
      ;
AddOp: ADD { $$ = NewNode("+", 0); }
     | SUB { $$ = NewNode("-", 0); }
     ;
RelExp: AddExp
      | RelExp RelOp AddExp { AddChildNode($2, $1); AddChildNode($2, $3); $$ = $2; }
      ;
RelOp: L { $$ = NewNode("<", 0); }
     | G { $$ = NewNode(">", 0); }
     | GE { $$ = NewNode(">=", 0); }
     | LE { $$ = NewNode("<=", 0); }
     ;
EqExp: RelExp { $$ = $1; }
     | EqExp EqOp RelExp { AddChildNode($2, $1); AddChildNode($2, $3); $$ = $2; }
     ;
EqOp: E { $$ = NewNode("==", 0); }
    | NE { $$ = NewNode("!=", 0); }
    ;
LAndExp: EqExp { $$ = $1; }
       | LAndExp AND EqExp { $$ = NewNode("&&", 2, $1, $3); }
       ;
LOrExp: LAndExp { $$ = $1; }
      | LOrExp OR LAndExp { $$ = NewNode("||", 2, $1, $3); }
      ;
ConstExp: AddExp { $$ = NewNode("ConstExp", 1, $1); }

%%

void yyerror(char *s) 
{
	fprintf(stderr, "%d: %s\n", line_cnt, s);
}
