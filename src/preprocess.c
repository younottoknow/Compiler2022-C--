#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "header.h"
#include "symbolTable.h"

#define max(a, b) ((a) > (b) ? (a) : (b))

void preprocessExprNode(AST_NODE *exprNode);
void preprocessFunctionCall(AST_NODE* functionCallNode);
void preprocessVariable(AST_NODE* idNode);
void preprocessExprRelatedNode(AST_NODE* exprRelatedNode);
int preprocessVarDeclNode(AST_NODE *varDeclNode, int startOffset, int isGlobal);
int preprocessVarListNode(AST_NODE *varListNode, int startOffset, int isGlobal);
int preprocessStmtListNode(AST_NODE *stmtListNode, int startOffset);
int preprocessBlockNode(AST_NODE *blockNode, int startOffset);
int preprocessStmtNode(AST_NODE *stmtNode, int startOffset);
int preprocessWhileStmt(AST_NODE* whileNode, int startOffset);
int preprocessIfStmt(AST_NODE* ifNode, int startOffset);
int preprocessForStmt(AST_NODE* forNode, int startOffset);
int preprocessNonemptyAssigExprListNode(AST_NODE *node);
void preprocessAssignOrExprNode(AST_NODE* assignOrExprRelatedNode);
void preprocessAssignmentStmt(AST_NODE* assignmentNode);
int preprocessNonemptyRelopExprListNode(AST_NODE *node);
int preprocessFuncDecl(AST_NODE *funcDeclNode);
void preprocess(AST_NODE *root);

void preprocessExprNode(AST_NODE *exprNode)
{
    if(exprNode->semantic_value.exprSemanticValue.isConstEval)
    {
        if(exprNode->dataType == INT_TYPE)
        {
            exprNode->nIntRegs = 1;
        }
        else if(exprNode->dataType == FLOAT_TYPE)
        {
            exprNode->nFloatRegs = 1;
        }
    }
    else if(exprNode->semantic_value.exprSemanticValue.kind == UNARY_OPERATION)
    {
        preprocessExprRelatedNode(exprNode->child);
        if(exprNode->semantic_value.exprSemanticValue.op.unaryOp == TYPE_CONVERSION)
        {
            if(exprNode->dataType == INT_TYPE)
            {
                exprNode->nIntRegs = max(exprNode->child->nIntRegs, 1);
                exprNode->nFloatRegs = exprNode->child->nFloatRegs;
            }
            else
            {
                exprNode->nIntRegs = exprNode->child->nIntRegs;
                exprNode->nFloatRegs = max(exprNode->child->nFloatRegs, 1);
            }
        }
        else
        {
            exprNode->nIntRegs = exprNode->child->nIntRegs;
            exprNode->nFloatRegs = exprNode->child->nFloatRegs;
        }
    }
    else
    {
        AST_NODE *leftOp = exprNode->child;
        AST_NODE *rightOp = leftOp->rightSibling;
        preprocessExprRelatedNode(leftOp);
        preprocessExprRelatedNode(rightOp);
        if(exprNode->dataType == INT_TYPE)
        {
            exprNode->nIntRegs = max(leftOp->nIntRegs, rightOp->nIntRegs + 1);
            exprNode->nFloatRegs = max(leftOp->nFloatRegs, rightOp->nFloatRegs);
        }
        else
        {
            exprNode->nIntRegs = max(leftOp->nIntRegs, rightOp->nIntRegs);
            exprNode->nFloatRegs = max(leftOp->nFloatRegs, rightOp->nFloatRegs + 1);
        }
    }
}

void preprocessFunctionCall(AST_NODE* functionCallNode)
{
    AST_NODE* functionIDNode = functionCallNode->child;
    AST_NODE* actualParameterList = functionIDNode->rightSibling;
    int nIntRegs = 0, nFloatRegs = 0;
    if(actualParameterList->nodeType != NUL_NODE)
    {
        preprocessNonemptyRelopExprListNode(actualParameterList);
        nIntRegs = max(nIntRegs, actualParameterList->nIntRegs);
        nFloatRegs = max(nFloatRegs, actualParameterList->nFloatRegs);
    }
    functionCallNode->nIntRegs = nIntRegs;
    functionCallNode->nFloatRegs = nFloatRegs;
}

void preprocessVariable(AST_NODE* idNode)
{
    if(idNode->semantic_value.identifierSemanticValue.kind == NORMAL_ID)
    {
        if(idNode->dataType == FLOAT_TYPE)
        {
            idNode->nFloatRegs = 1;
        }
        else
        {
            idNode->nIntRegs = 1;
        }
    }
    else if(idNode->semantic_value.identifierSemanticValue.kind == ARRAY_ID)
    {
        AST_NODE *traverseDimList = idNode->child;
        int nIntRegs = 0, nFloatRegs = 0;
        while(traverseDimList)
        {
            preprocessExprRelatedNode(traverseDimList);
            nIntRegs = max(nIntRegs, traverseDimList->nIntRegs);
            nFloatRegs = max(nFloatRegs, traverseDimList->nFloatRegs);
            traverseDimList = traverseDimList->rightSibling;
        }
        if(idNode->dataType == FLOAT_TYPE)
        {
            idNode->nIntRegs = nIntRegs + 1; // 1 register for arrary base address
            idNode->nFloatRegs = max(1, nFloatRegs);
        }
        else
        {
            idNode->nIntRegs = nIntRegs + 1; // 1 register for arrary base address
            idNode->nFloatRegs = nFloatRegs;
        }
    }
}

void preprocessExprRelatedNode(AST_NODE* exprRelatedNode)
{
    switch(exprRelatedNode->nodeType)
    {
    case EXPR_NODE:
        preprocessExprNode(exprRelatedNode);
        break;
    case STMT_NODE:
        //function call
        preprocessFunctionCall(exprRelatedNode);
        break;
    case IDENTIFIER_NODE:
        preprocessVariable(exprRelatedNode);
        break;
    case CONST_VALUE_NODE:
        if(exprRelatedNode->dataType == INT_TYPE)
        {
            exprRelatedNode->nIntRegs = 1;
        }
        else
        {
            exprRelatedNode->nFloatRegs = 1;
        }
        break;
    default:
        break;
    }
}

int preprocessVarDeclNode(AST_NODE *varDeclNode, int startOffset, int isGlobal)
{
    AST_NODE *traverseChildren = varDeclNode->child;
    int size = 0, typeSize = 8, nIntRegs = 0, nFloatRegs = 0;
    // the first child is type
    traverseChildren = traverseChildren->rightSibling;
    while(traverseChildren)
    {
        int varSize = typeSize;
        if(traverseChildren->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->attr.typeDescriptor->kind == ARRAY_TYPE_DESCRIPTOR)
        {
            for(int idx = 0; idx < traverseChildren->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->attr.typeDescriptor->properties.arrayProperties.dimension; idx++)
            {
                varSize *= traverseChildren->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->attr.typeDescriptor->properties.arrayProperties.sizeInEachDimension[idx];
            }
        }
        size += varSize;
        if(isGlobal)
        {
            traverseChildren->semantic_value.identifierSemanticValue.symbolTableEntry->isGlobal = isGlobal;
        }
        else
        {
            traverseChildren->semantic_value.identifierSemanticValue.symbolTableEntry->offset = startOffset - size;
        }
        if(traverseChildren->semantic_value.identifierSemanticValue.kind == WITH_INIT_ID)
        {
            preprocessExprNode(traverseChildren->child);
            traverseChildren->nIntRegs = traverseChildren->child->nIntRegs;
            traverseChildren->nFloatRegs = traverseChildren->child->nFloatRegs;
        }
        nIntRegs = max(nIntRegs, traverseChildren->nIntRegs);
        nFloatRegs = max(nFloatRegs, traverseChildren->nFloatRegs);
        traverseChildren = traverseChildren->rightSibling;
    }
    varDeclNode->nIntRegs = nIntRegs;
    varDeclNode->nFloatRegs = nFloatRegs;
    return size;
}

int preprocessVarListNode(AST_NODE *varListNode, int startOffset, int isGlobal)
{
    AST_NODE *traverseChildren = varListNode->child;
    int size = 0, nIntRegs = 0, nFloatRegs = 0;
    while(traverseChildren)
    {
        if(traverseChildren->semantic_value.declSemanticValue.kind == VARIABLE_DECL)
        {
            size += preprocessVarDeclNode(traverseChildren, startOffset - size, isGlobal);
        }
        nIntRegs = max(nIntRegs, traverseChildren->nIntRegs);
        nFloatRegs = max(nFloatRegs, traverseChildren->nFloatRegs);
        traverseChildren = traverseChildren->rightSibling;
    }
    varListNode->nIntRegs = nIntRegs;
    varListNode->nFloatRegs = nFloatRegs;
    return size;
}

int preprocessStmtListNode(AST_NODE *stmtListNode, int startOffset)
{
    AST_NODE *traverseChildren = stmtListNode->child;
    int size = 0, nIntRegs = 0, nFloatRegs = 0;
    while(traverseChildren)
    {
        if(traverseChildren->nodeType == BLOCK_NODE)
        {
            size = max(size, preprocessBlockNode(traverseChildren, startOffset));
        }
        else if(traverseChildren->nodeType == STMT_NODE)
        {
            size = max(size, preprocessStmtNode(traverseChildren, startOffset));
        }
        nIntRegs = max(nIntRegs, traverseChildren->nIntRegs);
        nFloatRegs = max(nFloatRegs, traverseChildren->nFloatRegs);
        traverseChildren = traverseChildren->rightSibling;
    }
    stmtListNode->nIntRegs = nIntRegs;
    stmtListNode->nFloatRegs = nFloatRegs;
    return size;
}

int preprocessBlockNode(AST_NODE *blockNode, int startOffset)
{
    AST_NODE *traverseChildren = blockNode->child;
    int size = 0, nIntRegs = 0, nFloatRegs = 0;
    while(traverseChildren)
    {
        if(traverseChildren->nodeType == VARIABLE_DECL_LIST_NODE)
        {
            size += preprocessVarListNode(traverseChildren, startOffset, 0);

        }
        else
        {
            size += preprocessStmtListNode(traverseChildren, startOffset);
        }
        nIntRegs = max(nIntRegs, traverseChildren->nIntRegs);
        nFloatRegs = max(nFloatRegs, traverseChildren->nFloatRegs);
        traverseChildren = traverseChildren->rightSibling;
    }
    blockNode->nIntRegs = nIntRegs;
    blockNode->nFloatRegs = nFloatRegs;
    return size;
}

int preprocessStmtNode(AST_NODE *stmtNode, int startOffset)
{
    int size = 0;
    if(stmtNode->nodeType == NUL_NODE)
    {
        return size;
    }
    else if(stmtNode->nodeType == BLOCK_NODE)
    {
        size += preprocessBlockNode(stmtNode, startOffset);
        return size;
    }
    else
    {
        switch(stmtNode->semantic_value.stmtSemanticValue.kind)
        {
        case WHILE_STMT:
            size += preprocessWhileStmt(stmtNode, startOffset);
            break;
        case FOR_STMT:
            size += preprocessForStmt(stmtNode, startOffset);
            break;
        case ASSIGN_STMT:
            preprocessAssignmentStmt(stmtNode);
            break;
        case IF_STMT:
            size += preprocessIfStmt(stmtNode, startOffset);
            break;
        case FUNCTION_CALL_STMT:
            preprocessFunctionCall(stmtNode);
            break;
        case RETURN_STMT:
            preprocessExprRelatedNode(stmtNode->child);
            stmtNode->nIntRegs = stmtNode->child->nIntRegs;
            stmtNode->nFloatRegs = stmtNode->child->nFloatRegs;
            break;
        default:
            break;
        }
        return size;
    }
}

int preprocessWhileStmt(AST_NODE* whileNode, int startOffset)
{
    int size = 0, nIntRegs = 0, nFloatRegs = 0;
    AST_NODE* boolExpression = whileNode->child;
    preprocessExprRelatedNode(boolExpression);
    nIntRegs = max(nIntRegs, boolExpression->nIntRegs);
    nFloatRegs = max(nFloatRegs, boolExpression->nFloatRegs);
    AST_NODE* bodyNode = boolExpression->rightSibling;
    size += preprocessStmtNode(bodyNode, startOffset);
    nIntRegs = max(nIntRegs, bodyNode->nIntRegs);
    nFloatRegs = max(nFloatRegs, bodyNode->nFloatRegs);
    whileNode->nIntRegs = nIntRegs;
    whileNode->nFloatRegs = nFloatRegs;
    return size;
}

int preprocessIfStmt(AST_NODE* ifNode, int startOffset)
{
    int size = 0, nIntRegs = 0, nFloatRegs = 0;
    AST_NODE* boolExpression = ifNode->child;
    preprocessExprRelatedNode(boolExpression);
    nIntRegs = max(nIntRegs, boolExpression->nIntRegs);
    nFloatRegs = max(nFloatRegs, boolExpression->nFloatRegs);
    AST_NODE* ifBodyNode = boolExpression->rightSibling;
    size = max(size, preprocessStmtNode(ifBodyNode, startOffset));
    nIntRegs = max(nIntRegs, ifBodyNode->nIntRegs);
    nFloatRegs = max(nFloatRegs, ifBodyNode->nFloatRegs);
    AST_NODE* elsePartNode = ifBodyNode->rightSibling;
    size = max(size, preprocessStmtNode(elsePartNode, startOffset));
    nIntRegs = max(nIntRegs, elsePartNode->nIntRegs);
    nFloatRegs = max(nFloatRegs, elsePartNode->nFloatRegs);
    ifNode->nIntRegs = nIntRegs;
    ifNode->nFloatRegs = nFloatRegs;
    return size;
}

int preprocessForStmt(AST_NODE* forNode, int startOffset)
{
    int size = 0, nIntRegs = 0, nFloatRegs = 0;
    AST_NODE* initExpression = forNode->child;
    if(initExpression->nodeType != NUL_NODE)
    {
        preprocessNonemptyAssigExprListNode(initExpression);
        nIntRegs = max(nIntRegs, initExpression->nIntRegs);
        nFloatRegs = max(nFloatRegs, initExpression->nFloatRegs);
    }
    AST_NODE* conditionExpression = initExpression->rightSibling;
    if(conditionExpression->nodeType != NUL_NODE)
    {
        preprocessNonemptyRelopExprListNode(conditionExpression);
        nIntRegs = max(nIntRegs, conditionExpression->nIntRegs);
        nFloatRegs = max(nFloatRegs, conditionExpression->nFloatRegs);
    }
    AST_NODE* loopExpression = conditionExpression->rightSibling;
    if(loopExpression->nodeType != NUL_NODE)
    {
        preprocessNonemptyAssigExprListNode(loopExpression);
        nIntRegs = max(nIntRegs, loopExpression->nIntRegs);
        nFloatRegs = max(nFloatRegs, loopExpression->nFloatRegs);
    }
    AST_NODE* bodyNode = loopExpression->rightSibling;
    size += preprocessStmtNode(bodyNode, startOffset);
    nIntRegs = max(nIntRegs, bodyNode->nIntRegs);
    nFloatRegs = max(nFloatRegs, bodyNode->nFloatRegs);
    forNode->nIntRegs = nIntRegs;
    forNode->nFloatRegs = nFloatRegs;
    return size;
}

int preprocessNonemptyAssigExprListNode(AST_NODE *node)
{
    int size = 0, nIntRegs = 0, nFloatRegs = 0;
    AST_NODE *traverseChildren = node->child;
    while(traverseChildren)
    {
        preprocessAssignOrExprNode(traverseChildren);
        nIntRegs = max(nIntRegs, traverseChildren->nIntRegs);
        nFloatRegs = max(nFloatRegs, traverseChildren->nFloatRegs);
        traverseChildren = traverseChildren->rightSibling;
    }
    node->nIntRegs = nIntRegs;
    node->nFloatRegs = nFloatRegs;
    return size;
}

void preprocessAssignOrExprNode(AST_NODE* assignOrExprRelatedNode)
{
    if(assignOrExprRelatedNode->nodeType == STMT_NODE)
    {
        if(assignOrExprRelatedNode->semantic_value.stmtSemanticValue.kind == ASSIGN_STMT)
        {
            preprocessAssignmentStmt(assignOrExprRelatedNode);
        }
        else if(assignOrExprRelatedNode->semantic_value.stmtSemanticValue.kind == FUNCTION_CALL_STMT)
        {
            preprocessFunctionCall(assignOrExprRelatedNode);
        }
    }
    else
    {
        preprocessExprRelatedNode(assignOrExprRelatedNode);
    }
}

void preprocessAssignmentStmt(AST_NODE* assignmentNode)
{
    AST_NODE* leftOp = assignmentNode->child;
    AST_NODE* rightOp = leftOp->rightSibling;
    int nIntRegs = 0, nFloatRegs = 0;
    preprocessExprRelatedNode(rightOp);
    nIntRegs = max(nIntRegs, rightOp->nIntRegs);
    nFloatRegs = max(nFloatRegs, rightOp->nFloatRegs);
    preprocessVariable(leftOp);
    if(rightOp->dataType == INT_TYPE)
    {
        nIntRegs = max(nIntRegs, rightOp->nIntRegs + 1);
        nFloatRegs = max(nFloatRegs, rightOp->nFloatRegs);
    }
    else
    {
        nIntRegs = max(nIntRegs, rightOp->nIntRegs);
        nFloatRegs = max(nFloatRegs, rightOp->nFloatRegs + 1);
    }
    assignmentNode->nIntRegs = nIntRegs;
    assignmentNode->nFloatRegs = nFloatRegs;
}

int preprocessNonemptyRelopExprListNode(AST_NODE *node)
{
    AST_NODE *traverseChildren = node->child;
    int size = 0, nIntRegs = 0, nFloatRegs = 0;
    while(traverseChildren)
    {
        preprocessExprRelatedNode(traverseChildren);
        nIntRegs = max(nIntRegs, traverseChildren->nIntRegs);
        nFloatRegs = max(nFloatRegs, traverseChildren->nFloatRegs);
        traverseChildren = traverseChildren->rightSibling;
    }
    node->nIntRegs = nIntRegs;
    node->nFloatRegs = nFloatRegs;
    return size;
}

int preprocessFuncDecl(AST_NODE *funcNode)
{
    AST_NODE* returnTypeNode = funcNode->child;
    AST_NODE* functionNameID = returnTypeNode->rightSibling;
    AST_NODE *parameterListNode = functionNameID->rightSibling;
    AST_NODE *traverseParameter = parameterListNode->child;
    int size = 0, nIntRegs = 0, nFloatRegs = 0, parameterOffset = 16;
    while(traverseParameter)
    {
        traverseParameter->child->rightSibling->semantic_value.identifierSemanticValue.symbolTableEntry->offset = parameterOffset;
        parameterOffset += 8;
        traverseParameter = traverseParameter->rightSibling;
    }
    AST_NODE *blockNode = parameterListNode->rightSibling;
    size += preprocessBlockNode(blockNode, 0);
    nIntRegs = max(nIntRegs, blockNode->nIntRegs);
    nFloatRegs = max(nFloatRegs, blockNode->nFloatRegs);
    funcNode->nIntRegs = nIntRegs;
    funcNode->nFloatRegs = nFloatRegs;
    return size;
}

void preprocess(AST_NODE *root)
{
    AST_NODE *traverseDeclaration = root->child;
    while(traverseDeclaration)
    {
        if(traverseDeclaration->nodeType == VARIABLE_DECL_LIST_NODE)
        {
            preprocessVarListNode(traverseDeclaration, 0, 1);
        }
        else
        {
            AST_NODE *func_id = traverseDeclaration->child->rightSibling;
            assert(func_id->nodeType == IDENTIFIER_NODE);
            assert(func_id->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->attributeKind == FUNCTION_SIGNATURE);
            func_id->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->attr.functionSignature->frameSize = preprocessFuncDecl(traverseDeclaration);
        }

        traverseDeclaration = traverseDeclaration->rightSibling;
    }
}
