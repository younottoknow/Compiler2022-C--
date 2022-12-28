#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "header.h"
#include "symbolTable.h"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

void freeNode(AST_NODE *node);
void recursiveFree(AST_NODE *node);
void trimExprNode(AST_NODE *exprNode);
void trimFunctionCall(AST_NODE* functionCallNode);
void trimConst(AST_NODE *constValueNode);
void trimExprRelatedNode(AST_NODE* exprRelatedNode);
void trimStmtListNode(AST_NODE *stmtListNode);
void trimBlockNode(AST_NODE *blockNode);
void trimStmtNode(AST_NODE *stmtNode);
void trimReturnStmt(AST_NODE *returnNode);
void trimWhileStmt(AST_NODE* whileNode);
void trimIfStmt(AST_NODE* ifNode);
void trimForStmt(AST_NODE* forNode);
void trimNonemptyAssigExprListNode(AST_NODE *node);
void trimAssignOrExprNode(AST_NODE* assignOrExprRelatedNode);
void trimAssignmentStmt(AST_NODE* assignmentNode);
void trimNonemptyRelopExprListNode(AST_NODE *node);
void trimFuncDecl(AST_NODE *funcNode);

void freeNode(AST_NODE *node)
{
    switch(node->nodeType)
    {
    case CONST_VALUE_NODE:
        if(node->dataType == CONST_STRING_TYPE)
        {
            free(node->semantic_value.const1->const_u.sc);
            free(node->semantic_value.const1);
        }
        break;
    case IDENTIFIER_NODE:
        free(node->semantic_value.identifierSemanticValue.identifierName);
        break;
    default:
        break;
    }
}
void recursiveFree(AST_NODE *node)
{
    if(node == NULL) return;
    recursiveFree(node->rightSibling);
    recursiveFree(node->child);
    freeNode(node);
}

void trimExprNode(AST_NODE *exprNode)
{
    exprNode->terminateNormal = 1;
    if(exprNode->semantic_value.exprSemanticValue.isConstEval)
    {
        exprNode->containFunctionCall = 0;
    }
    else if(exprNode->semantic_value.exprSemanticValue.kind == UNARY_OPERATION)
    {
        trimExprRelatedNode(exprNode->child);
        exprNode->containFunctionCall = exprNode->child->containFunctionCall;
    }
    else
    {
        AST_NODE *leftOp = exprNode->child;
        AST_NODE *rightOp = leftOp->rightSibling;
        trimExprRelatedNode(leftOp);
        trimExprRelatedNode(rightOp);
        if(leftOp->containFunctionCall || rightOp->containFunctionCall)
        {
            exprNode->containFunctionCall = 1;
        }
        else
        {
            exprNode->containFunctionCall = 0;
        }
    }
}

void trimFunctionCall(AST_NODE* functionCallNode)
{
    functionCallNode->terminateNormal = 1;
    functionCallNode->containFunctionCall = 1;
}

void trimConst(AST_NODE *constValueNode)
{
    constValueNode->terminateNormal = 1;
    constValueNode->containFunctionCall = 0;
}

void trimVariable(AST_NODE *idNode)
{
    idNode->terminateNormal = 1;
    if(idNode->semantic_value.identifierSemanticValue.kind == NORMAL_ID)
    {
        idNode->containFunctionCall = 0;
    }
    else if(idNode->semantic_value.identifierSemanticValue.kind == ARRAY_ID)
    {
        AST_NODE *traverseDimList = idNode->child;
        while(traverseDimList)
        {
            trimExprRelatedNode(traverseDimList);
            if(traverseDimList->containFunctionCall)
            {
                idNode->containFunctionCall = 1;
            }
            traverseDimList = traverseDimList->rightSibling;
        }
    }
}

void trimExprRelatedNode(AST_NODE* exprRelatedNode)
{
    switch(exprRelatedNode->nodeType)
    {
    case EXPR_NODE:
        trimExprNode(exprRelatedNode);
        break;
    case STMT_NODE:
        //function call
        trimFunctionCall(exprRelatedNode);
        break;
    case IDENTIFIER_NODE:
        trimVariable(exprRelatedNode);
        break;
    case CONST_VALUE_NODE:
        trimConst(exprRelatedNode);
        break;
    default:
        break;
    }
}

void trimStmtListNode(AST_NODE *stmtListNode)
{
    AST_NODE *traverseChildren = stmtListNode->child;
    stmtListNode->terminateNormal = 1;
    stmtListNode->containFunctionCall = 0;
    while(traverseChildren)
    {
        if(traverseChildren->nodeType == BLOCK_NODE)
        {
            trimBlockNode(traverseChildren);
            if(!traverseChildren->terminateNormal)
            {
                stmtListNode->terminateNormal = 0;
            }
            if(traverseChildren->containFunctionCall)
            {
                stmtListNode->containFunctionCall = 1;
            }
        }
        else if(traverseChildren->nodeType == STMT_NODE)
        {
            trimStmtNode(traverseChildren);
            if(!traverseChildren->terminateNormal)
            {
                stmtListNode->terminateNormal = 0;
            }
            if(traverseChildren->containFunctionCall)
            {
                stmtListNode->containFunctionCall = 1;
            }
        }
        if(!traverseChildren->terminateNormal)
        {
            recursiveFree(traverseChildren->rightSibling);
            traverseChildren->rightSibling = NULL;
        }
        traverseChildren = traverseChildren->rightSibling;
    }
}

void trimBlockNode(AST_NODE *blockNode)
{
    AST_NODE *traverseChildren = blockNode->child;
    blockNode->terminateNormal = 1;
    blockNode->containFunctionCall = 0;
    while(traverseChildren)
    {
        if(traverseChildren->nodeType != VARIABLE_DECL_LIST_NODE)
        {
            trimStmtListNode(traverseChildren);
            if(!traverseChildren->terminateNormal)
            {
                blockNode->terminateNormal = 0;
            }
            if(traverseChildren->containFunctionCall)
            {
                blockNode->containFunctionCall = 1;
            }
            if(!blockNode->terminateNormal)
            {
                recursiveFree(traverseChildren->rightSibling);
                traverseChildren->rightSibling = NULL;
            }
        }
        traverseChildren = traverseChildren->rightSibling;
    }
}

void trimStmtNode(AST_NODE *stmtNode)
{
    if(stmtNode->nodeType == NUL_NODE)
    {
        stmtNode->terminateNormal = 1;
        stmtNode->containFunctionCall = 0;
        return;
    }
    else if(stmtNode->nodeType == BLOCK_NODE)
    {
        trimBlockNode(stmtNode);
        return;
    }
    else
    {
        switch(stmtNode->semantic_value.stmtSemanticValue.kind)
        {
        case WHILE_STMT:
            trimWhileStmt(stmtNode);
            break;
        case FOR_STMT:
            trimForStmt(stmtNode);
            break;
        case ASSIGN_STMT:
            trimAssignmentStmt(stmtNode);
            break;
        case IF_STMT:
            trimIfStmt(stmtNode);
            break;
        case FUNCTION_CALL_STMT:
            trimFunctionCall(stmtNode);
            break;
        case RETURN_STMT:
            trimReturnStmt(stmtNode);
            break;
        default:
            break;
        }
    }
}

void trimReturnStmt(AST_NODE *returnNode)
{
    returnNode->terminateNormal = 0;
    returnNode->containFunctionCall = 0;
    if(returnNode->child->nodeType != NUL_NODE)
    {
        trimExprRelatedNode(returnNode->child);
        if(returnNode->child->containFunctionCall)
        {
            returnNode->containFunctionCall = 1;
        }
    }
}

void trimWhileStmt(AST_NODE* whileNode)
{
    whileNode->terminateNormal = 1;
    whileNode->containFunctionCall = 0;
    AST_NODE* boolExpression = whileNode->child;
    trimExprRelatedNode(boolExpression);
    if(!boolExpression->terminateNormal)
    {
        whileNode->terminateNormal = 0;
    }
    if(boolExpression->containFunctionCall)
    {
        whileNode->containFunctionCall = 1;
    }
    AST_NODE* bodyNode = boolExpression->rightSibling;
    trimStmtNode(bodyNode);
    if(bodyNode->containFunctionCall)
    {
        whileNode->containFunctionCall = 1;
    }
}

void trimIfStmt(AST_NODE* ifNode)
{
    AST_NODE* boolExpression = ifNode->child;
    trimExprRelatedNode(boolExpression);
    if(boolExpression->containFunctionCall)
    {
        ifNode->containFunctionCall = 1;
    }
    AST_NODE* ifBodyNode = boolExpression->rightSibling;
    trimStmtNode(ifBodyNode);
    if(ifBodyNode->containFunctionCall)
    {
        ifNode->containFunctionCall = 1;
    }
    AST_NODE* elsePartNode = ifBodyNode->rightSibling;
    trimStmtNode(elsePartNode);
    if(elsePartNode->containFunctionCall)
    {
        ifNode->containFunctionCall = 1;
    }
    if(!ifBodyNode->terminateNormal && !elsePartNode->terminateNormal)
    {
        ifNode->terminateNormal = 0;
    }
    else
    {
        ifNode->terminateNormal = 1;
    }
}

void trimForStmt(AST_NODE* forNode)
{
    forNode->terminateNormal = 1;
    forNode->containFunctionCall = 0;
    AST_NODE* initExpression = forNode->child;
    if(initExpression->nodeType != NUL_NODE)
    {
        trimNonemptyAssigExprListNode(initExpression);
        if(initExpression->containFunctionCall)
        {
            forNode->containFunctionCall = 1;
        }
    }
    AST_NODE* conditionExpression = initExpression->rightSibling;
    if(conditionExpression->nodeType != NUL_NODE)
    {
        trimNonemptyRelopExprListNode(conditionExpression);
        if(conditionExpression->containFunctionCall)
        {
            forNode->containFunctionCall = 1;
        }
    }
    AST_NODE* loopExpression = conditionExpression->rightSibling;
    if(loopExpression->nodeType != NUL_NODE)
    {
        trimNonemptyAssigExprListNode(loopExpression);
        if(loopExpression->containFunctionCall)
        {
            forNode->containFunctionCall = 1;
        }
    }
    AST_NODE* bodyNode = loopExpression->rightSibling;
    trimStmtNode(bodyNode);
    if(bodyNode->containFunctionCall)
    {
        forNode->containFunctionCall = 1;
    }
}

void trimNonemptyAssigExprListNode(AST_NODE *node)
{
    node->terminateNormal = 1;
    node->containFunctionCall = 0;
    AST_NODE *traverseChildren = node->child, *lastChild = traverseChildren;
    while(traverseChildren)
    {
        trimAssignOrExprNode(traverseChildren);
        if(traverseChildren->containFunctionCall)
        {
            node->containFunctionCall = 1;
        }
        // if just a expression which don't contain function call
        if(traverseChildren->nodeType != STMT_NODE
           && !traverseChildren->containFunctionCall)
        {
            recursiveFree(traverseChildren->child);
            // not the leftmostSibling
            if(traverseChildren->leftmostSibling != traverseChildren)
            {
                lastChild->rightSibling = traverseChildren->rightSibling;
            }
            else if(traverseChildren->rightSibling)
            {
                node->child = traverseChildren->rightSibling;
                AST_NODE *tmp = traverseChildren->rightSibling;
                while(tmp)
                {
                    tmp->leftmostSibling = node->child;
                    tmp = tmp->rightSibling;
                }
            }
            else
            {
                node->child = NULL;
            }
            freeNode(traverseChildren);
        }
        lastChild = traverseChildren;
        traverseChildren = traverseChildren->rightSibling;
    }
}

void trimAssignOrExprNode(AST_NODE* assignOrExprRelatedNode)
{
    if(assignOrExprRelatedNode->nodeType == STMT_NODE)
    {
        if(assignOrExprRelatedNode->semantic_value.stmtSemanticValue.kind == ASSIGN_STMT)
        {
            trimAssignmentStmt(assignOrExprRelatedNode);
        }
        else if(assignOrExprRelatedNode->semantic_value.stmtSemanticValue.kind == FUNCTION_CALL_STMT)
        {
            trimFunctionCall(assignOrExprRelatedNode);
        }
    }
    else
    {
        trimExprRelatedNode(assignOrExprRelatedNode);
    }
}

void trimAssignmentStmt(AST_NODE* assignmentNode)
{
    assignmentNode->terminateNormal = 1;
    assignmentNode->containFunctionCall = 0;
    AST_NODE* leftOp = assignmentNode->child;
    AST_NODE* rightOp = leftOp->rightSibling;
    trimVariable(leftOp);
    trimExprRelatedNode(rightOp);
    if(leftOp->containFunctionCall || rightOp->containFunctionCall)
    {
        assignmentNode->containFunctionCall = 1;
    }
}

void trimNonemptyRelopExprListNode(AST_NODE *node)
{
    node->terminateNormal = 1;
    node->containFunctionCall = 0;
    AST_NODE *traverseChildren = node->child, *lastChild = node->child;
    while(traverseChildren)
    {
        trimExprRelatedNode(traverseChildren);
        // if not contain function call and not the last child
        if(!traverseChildren->containFunctionCall && traverseChildren->rightSibling != NULL)
        {
            recursiveFree(traverseChildren->child);
            // not the leftmostSibling
            if(traverseChildren->leftmostSibling != traverseChildren)
            {
                lastChild->rightSibling = traverseChildren->rightSibling;
            }
            else if(traverseChildren->rightSibling)
            {
                node->child = traverseChildren->rightSibling;
                AST_NODE *tmp = traverseChildren->rightSibling;
                while(tmp)
                {
                    tmp->leftmostSibling = node->child;
                    tmp = tmp->rightSibling;
                }
            }
            else
            {
                node->child = NULL;
            }
            freeNode(traverseChildren);
        }
        if(traverseChildren->containFunctionCall)
        {
            node->containFunctionCall = 1;
        }
        lastChild = traverseChildren;
        traverseChildren = traverseChildren->rightSibling;
    }
}

void trimFuncDecl(AST_NODE *funcNode)
{
    AST_NODE* returnTypeNode = funcNode->child;
    AST_NODE* functionNameID = returnTypeNode->rightSibling;
    AST_NODE *parameterListNode = functionNameID->rightSibling;
    AST_NODE *blockNode = parameterListNode->rightSibling;
    trimBlockNode(blockNode);
}

void trimProgram(AST_NODE *root)
{
    AST_NODE *traverseDeclaration = root->child;
    while(traverseDeclaration)
    {
        if(traverseDeclaration->nodeType != VARIABLE_DECL_LIST_NODE)
        {
            trimFuncDecl(traverseDeclaration);
        }

        traverseDeclaration = traverseDeclaration->rightSibling;
    }
}