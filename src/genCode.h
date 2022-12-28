#ifndef __GEN_CODE_H__
#define __GEN_CODE_H__

#include "header.h"
#include "symbolTable.h"

typedef enum {
    UNUSED,
    DIRTY,
    PRESERVED
} RegStatus;

typedef struct {
    RegStatus status;
    AST_NODE *node;
} Reg;

typedef struct RegFrame{
    AST_NODE *intRegsNode[31];
    AST_NODE *floatRegsNode[32];
    struct RegFrame *next;
} RegFrame;

int findReg(DATA_TYPE type);
void saveRegs(int nIntRegs, int nFloatRegs);
void restoreRegs();
int findPlace(AST_NODE *node);
void initRegs();
void genExprNode(AST_NODE *exprNode);
void genFunctionCall(AST_NODE* functionCallNode);
void genVariableR(AST_NODE* idNode);
void genVariableL(AST_NODE *idNode);
void genConst(AST_NODE *constValueNode);
void genExprRelatedNode(AST_NODE* exprRelatedNode);
void genVarDeclNode(AST_NODE *varDeclNode);
void genVarListNode(AST_NODE *varListNode);
void genStmtListNode(AST_NODE *stmtListNode);
void genBlockNode(AST_NODE *blockNode);
void genStmtNode(AST_NODE *stmtNode);
void genReturnStmt(AST_NODE *returnNode);
void genWhileStmt(AST_NODE* whileNode);
void genIfStmt(AST_NODE* ifNode);
void genForStmt(AST_NODE* forNode);
void genNonemptyAssigExprListNode(AST_NODE *node);
void genAssignOrExprNode(AST_NODE* assignOrExprRelatedNode);
void genAssignmentStmt(AST_NODE* assignmentNode);
void genNonemptyRelopExprListNode(AST_NODE *node);
void genFuncDecl(AST_NODE *funcDeclNode);
void genGlobalVarDeclNode(AST_NODE *varDeclNode);
void genGlobalVarListNode(AST_NODE *varListNode);
void genProgram(AST_NODE *root);

#endif
