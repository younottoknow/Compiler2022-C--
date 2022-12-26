#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "genCode.h"
#include "header.h"
#include "symbolTable.h"

FILE *fp;
Reg intRegs[31], floatRegs[32];
AST_NODE *curFuncNode;
RegFrame *regStackTop = NULL;
unsigned int nLabel = 0;

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

int findReg(DATA_TYPE type)
{
    if(type == INT_TYPE)
    {
        for(int i = 0; i < 31; i++)
        {
            if(intRegs[i].status == UNUSED)
            {
                return i;
            }
        }
    }
    else
    {
        for(int i = 0; i < 32; i++)
        {
            if(floatRegs[i].status == UNUSED)
            {
                return i;
            }
        }
    }
    fprintf(stderr, "Can't find available register.\n");
    return -1;
}

void saveRegs(int nIntRegs, int nFloatRegs)
{
    RegFrame *newRegFrame = (RegFrame *)malloc(sizeof(RegFrame));
    int offset = 0, availIntRegs = 0, availFloatRegs = 0;
    for(int i = 0; i < 31; i++)
    {
        if(intRegs[i].status == UNUSED)
        {
            availIntRegs++;
        }
        newRegFrame->intRegsNode[i] = NULL;
    }
    for(int i = 0; i < 32; i++)
    {
        if(floatRegs[i].status == UNUSED)
        {
            availFloatRegs++;
        }
        newRegFrame->floatRegsNode[i] = NULL;
    }
    for(int i = 0; i < 29 && availIntRegs < nIntRegs; i++)
    {
        if(intRegs[i].status == DIRTY)
        {
            offset -= 8;
            fprintf(fp, "str x%d, [sp, #%d]\n", i, offset);
            newRegFrame->intRegsNode[i] = intRegs[i].node;
            intRegs[i].status = UNUSED;
            availIntRegs++;
        }
    }
    for(int i = 0; i < 32 && availFloatRegs < nFloatRegs; i++)
    {
        if(floatRegs[i].status == DIRTY)
        {
            offset -= 8;
            fprintf(fp, "str s%d, [sp, #%d]\n", i, offset);
            newRegFrame->floatRegsNode[i] = floatRegs[i].node;
            floatRegs[i].status = UNUSED;
            availFloatRegs++;
        }
    }
    if(offset != 0)
    {
        fprintf(fp, "add sp, sp, #%d\n", offset);
    }
    newRegFrame->next = regStackTop;
    regStackTop = newRegFrame;
}

void restoreRegs()
{
    int offset = 0;

    for(int i = 31; i >= 0; i--)
    {
        if(regStackTop->floatRegsNode[i] != NULL)
        {
            fprintf(fp, "ldr s%d, [sp, #%d]\n", i, offset);
            floatRegs[i].status = DIRTY;
            floatRegs[i].node = regStackTop->floatRegsNode[i];
            offset += 8;
        }
    }
    for(int i = 30; i >= 0; i--)
    {
        if(regStackTop->intRegsNode[i] != NULL)
        {
            fprintf(fp, "ldr x%d, [sp, #%d]\n", i, offset);
            intRegs[i].status = DIRTY;
            intRegs[i].node = regStackTop->intRegsNode[i];
            offset += 8;
        }
    }
    if(offset != 0)
    {
        fprintf(fp, "add sp, sp, #%d\n", offset);
    }
    RegFrame *prevRegFrame = regStackTop;
    regStackTop = regStackTop->next;
    free(prevRegFrame);
}

int findPlace(AST_NODE *node)
{
    if(node->dataType == FLOAT_TYPE)
    {
        for(int i = 0; i < 32; i++)
        {
            if(floatRegs[i].status == DIRTY && floatRegs[i].node == node)
            {
                return i;
            }
        }
    }
    else
    {
        for(int i = 0; i < 31; i++)
        {
            if(intRegs[i].status == DIRTY && intRegs[i].node == node)
            {
                return i;
            }
        }
    }
    fprintf(stderr, "Can't find node in registers\n");
    return -1;
}

void genExprNode(AST_NODE *exprNode)
{
    if(exprNode->semantic_value.exprSemanticValue.isConstEval)
    {
        if(exprNode->dataType == FLOAT_TYPE)
        {
            int index = findReg(FLOAT_TYPE);
            char hex[9] = {0};
            snprintf(hex, 9, "%08x", *(unsigned int*)&exprNode->semantic_value.exprSemanticValue.constEvalValue.fValue);
            fprintf(fp, "mov w27, #0x%4s\n", &(hex[4]));
            fprintf(fp, "movk w27, #0x%4.4s, lsl 16\n", hex);
            fprintf(fp, "fmov s%d, w27\n", index);
            floatRegs[index].status = DIRTY;
            floatRegs[index].node = exprNode;
        }
        else
        {
            int index = findReg(INT_TYPE);
            char hex[9] = {0};
            snprintf(hex, 9, "%08x", *(unsigned int*)&exprNode->semantic_value.exprSemanticValue.constEvalValue.iValue);
            fprintf(fp, "mov w27, #0x%4s\n", &(hex[4]));
            fprintf(fp, "movk w27, #0x%4.4s, lsl 16\n", hex);
            fprintf(fp, "sxtw x%d, w27\n", index);
            intRegs[index].status = DIRTY;
            intRegs[index].node = exprNode;
        }
    }
    else if(exprNode->semantic_value.exprSemanticValue.kind == UNARY_OPERATION)
    {
        int index;
        if(exprNode->dataType == INT_TYPE)
        {
            index = findReg(INT_TYPE);
        }
        else
        {
            index = findReg(FLOAT_TYPE);
        }
        saveRegs(exprNode->child->nIntRegs,exprNode->child->nFloatRegs);
        genExprRelatedNode(exprNode->child);
        int result = findPlace(exprNode->child);
        switch(exprNode->semantic_value.exprSemanticValue.op.unaryOp)
        {
        case TYPE_CONVERSION:
            if(exprNode->dataType == FLOAT_TYPE)
            {
                fprintf(fp, "scvtf s%d, x%d\n", index, result);
                intRegs[result].status = UNUSED;
                floatRegs[index].status = DIRTY;
                floatRegs[index].node = exprNode;
            }
            else
            {
                fprintf(fp, "fcvtzs x%d, s%d\n", index, result);
                floatRegs[result].status = UNUSED;
                intRegs[index].status = DIRTY;
                intRegs[index].node = exprNode;
            }
            break;
        case UNARY_OP_NEGATIVE:
            if(exprNode->dataType == INT_TYPE)
            {
                fprintf(fp, "ngc x%d, x%d\n", index, result);
                intRegs[result].status = UNUSED;
                intRegs[index].status = DIRTY;
                intRegs[index].node = exprNode;
            }
            else
            {
                fprintf(fp, "fneg s%d, s%d\n", index, result);
                floatRegs[result].status = UNUSED;
                floatRegs[index].status = DIRTY;
                floatRegs[index].node = exprNode;
            }
        case UNARY_OP_LOGICAL_NEGATION:
            if(exprNode->dataType == INT_TYPE)
            {
                fprintf(fp, "cmp x%d, #0\n", result);
                fprintf(fp, "cset x%d, eq\n", index);
                intRegs[result].status = UNUSED;
                intRegs[index].status = DIRTY;
                intRegs[index].node = exprNode;
            }
            else
            {
                fprintf(fp, "fcmp s%d, #0.0\n", result);
                fprintf(fp, "cset x27, eq\n");
                fprintf(fp, "ucvtf s%d, x27\n", index);
                floatRegs[result].status = UNUSED;
                floatRegs[index].status = DIRTY;
                floatRegs[index].node = exprNode;
            }
            break;
        case UNARY_OP_POSITIVE:
            if(exprNode->dataType == INT_TYPE)
            {
                if(result != index)
                {
                    fprintf(fp, "mov x%d, x%d\n", index, result);
                    intRegs[result].status = UNUSED;
                    intRegs[index].status = DIRTY;
                }
                intRegs[index].node = exprNode;
            }
            else
            {
                if(result != index)
                {
                    fprintf(fp, "fmov s%d, s%d\n", index, result);
                    floatRegs[result].status = UNUSED;
                    floatRegs[index].status = DIRTY;
                }
                floatRegs[index].node = exprNode;
            }
            break;
        default:
            fprintf(stderr, "Unhandle unary operation\n");
            break;
        }
        restoreRegs();
    }
    else
    {
        AST_NODE *leftOp = exprNode->child;
        AST_NODE *rightOp = leftOp->rightSibling;
        int leftIndex, rightIndex;
        if(leftOp->dataType == FLOAT_TYPE)
        {
            leftIndex = findReg(FLOAT_TYPE);
        }
        else
        {
            leftIndex = findReg(INT_TYPE);
        }
        saveRegs(leftOp->nIntRegs, leftOp->nFloatRegs);
        genExprRelatedNode(leftOp);
        int result = findPlace(leftOp);
        if(leftOp->dataType == FLOAT_TYPE)
        {
            if(result != leftIndex)
            {
                fprintf(fp, "fmov s%d, s%d\n", leftIndex, result);
                floatRegs[result].status = UNUSED;
            }
            floatRegs[leftIndex].status = DIRTY;
            floatRegs[leftIndex].node = leftOp;
        }
        else
        {
            if(result != leftIndex)
            {
                fprintf(fp, "mov x%d, x%d\n", leftIndex, result);
                intRegs[result].status = UNUSED;
            }
            intRegs[leftIndex].status = DIRTY;
            intRegs[leftIndex].node = leftOp;
        }
        restoreRegs();
        if(rightOp->dataType == FLOAT_TYPE)
        {
            rightIndex = findReg(FLOAT_TYPE);
        }
        else
        {
            rightIndex = findReg(INT_TYPE);
        }
        saveRegs(rightOp->nIntRegs, rightOp->nFloatRegs);
        genExprRelatedNode(rightOp);
        result = findPlace(rightOp);
        if(exprNode->dataType == FLOAT_TYPE)
        {
            if(result != rightIndex)
            {
                fprintf(fp, "mov s%d, s%d\n", rightIndex, result);
                floatRegs[result].status = UNUSED;
            }
            floatRegs[rightIndex].status = DIRTY;
            floatRegs[rightIndex].node = rightOp;
        }
        else
        {
            if(result != rightIndex)
            {
                fprintf(fp, "mov x%d, x%d\n", rightIndex, result);
                intRegs[result].status = UNUSED;
            }
            intRegs[rightIndex].status = DIRTY;
            intRegs[rightIndex].node = rightOp;
        }
        restoreRegs();
        if(exprNode->dataType == FLOAT_TYPE)
        {
            assert(floatRegs[leftIndex].node == leftOp);
            assert(floatRegs[rightIndex].node == rightOp);
        }
        else
        {
            assert(intRegs[leftIndex].node == leftOp);
            assert(intRegs[rightIndex].node == rightOp);
        }
        if(exprNode->dataType == FLOAT_TYPE)
        {
            switch(exprNode->semantic_value.exprSemanticValue.op.binaryOp)
            {
            case BINARY_OP_ADD:
                fprintf(fp, "fadd s%d, s%d, s%d\n", leftIndex, leftIndex, rightIndex);
                break;
            case BINARY_OP_SUB:
                fprintf(fp, "fsub s%d, s%d, s%d\n", leftIndex, leftIndex, rightIndex);
                break;
            case BINARY_OP_MUL:
                fprintf(fp, "fmul s%d, s%d, s%d\n", leftIndex, leftIndex, rightIndex);
                break;
            case BINARY_OP_DIV:
                fprintf(fp, "fdiv s%d, s%d, s%d\n", leftIndex, leftIndex, rightIndex);
                break;
            case BINARY_OP_EQ:
                fprintf(fp, "fcmp s%d, s%d\n", leftIndex, rightIndex);
                fprintf(fp, "cset x27, eq\n");
                fprintf(fp, "ucvtf s%d, x27\n", leftIndex);
                break;
            case BINARY_OP_GE:
                fprintf(fp, "fcmp s%d, s%d\n", leftIndex, rightIndex);
                fprintf(fp, "cset x27, ge\n");
                fprintf(fp, "ucvtf s%d, x27\n", leftIndex);
                break;
            case BINARY_OP_LE:
                fprintf(fp, "fcmp s%d, s%d\n", leftIndex, rightIndex);
                fprintf(fp, "cset x27, le\n");
                fprintf(fp, "ucvtf s%d, x27\n", leftIndex);
                break;
            case BINARY_OP_NE:
                fprintf(fp, "fcmp s%d, s%d\n", leftIndex, rightIndex);
                fprintf(fp, "cset x27, ne\n");
                fprintf(fp, "ucvtf s%d, x27\n", leftIndex);
                break;
            case BINARY_OP_GT:
                fprintf(fp, "fcmp s%d, s%d\n", leftIndex, rightIndex);
                fprintf(fp, "cset x27, gt\n");
                fprintf(fp, "ucvtf s%d, x27\n", leftIndex);
                break;
            case BINARY_OP_LT:
                fprintf(fp, "fcmp s%d, s%d\n", leftIndex, rightIndex);
                fprintf(fp, "cset x27, lt\n");
                fprintf(fp, "ucvtf s%d, x27\n", leftIndex);
                break;
            case BINARY_OP_AND:
                fprintf(fp, "fcmp s%d, #0.0\n", leftIndex);
                fprintf(fp, "cset x27, ne\n");
                fprintf(fp, "fcmp s%d, #0.0\n", rightIndex);
                fprintf(fp, "cset x28, ne\n");
                fprintf(fp, "and x27, x27, x28\n");
                fprintf(fp, "ucvtf s%d, x27\n", leftIndex);
                break;
            case BINARY_OP_OR:
                fprintf(fp, "fcmp s%d, #0.0\n", leftIndex);
                fprintf(fp, "cset x27, ne\n");
                fprintf(fp, "fcmp s%d, #0.0\n", rightIndex);
                fprintf(fp, "cset x28, ne\n");
                fprintf(fp, "orr x27, x27, x28\n");
                fprintf(fp, "ucvtf s%d, x27\n", leftIndex);
                break;
            default:
                fprintf(stderr, "Unhandle binary operation\n");
                break;
            }
            floatRegs[rightIndex].status = UNUSED;
            floatRegs[leftIndex].node = exprNode;
        }
        else
        {
            switch(exprNode->semantic_value.exprSemanticValue.op.binaryOp)
            {
            case BINARY_OP_ADD:
                fprintf(fp, "add x%d, x%d, x%d\n", leftIndex, leftIndex, rightIndex);
                break;
            case BINARY_OP_SUB:
                fprintf(fp, "sub x%d, x%d, x%d\n", leftIndex, leftIndex, rightIndex);
                break;
            case BINARY_OP_MUL:
                fprintf(fp, "mul x%d, x%d, x%d\n", leftIndex, leftIndex, rightIndex);
                break;
            case BINARY_OP_DIV:
                fprintf(fp, "sdiv x%d, x%d, x%d\n", leftIndex, leftIndex, rightIndex);
                break;
            case BINARY_OP_EQ:
                fprintf(fp, "cmp x%d, x%d\n", leftIndex, rightIndex);
                fprintf(fp, "cset x%d, eq\n", leftIndex);
                break;
            case BINARY_OP_GE:
                fprintf(fp, "cmp x%d, x%d\n", leftIndex, rightIndex);
                fprintf(fp, "cset x%d, ge\n", leftIndex);
                break;
            case BINARY_OP_LE:
                fprintf(fp, "cmp x%d, x%d\n", leftIndex, rightIndex);
                fprintf(fp, "cset x%d, le\n", leftIndex);
                break;
            case BINARY_OP_NE:
                fprintf(fp, "cmp x%d, x%d\n", leftIndex, rightIndex);
                fprintf(fp, "cset x%d, ne\n", leftIndex);
                break;
            case BINARY_OP_GT:
                fprintf(fp, "cmp x%d, x%d\n", leftIndex, rightIndex);
                fprintf(fp, "cset x%d, gt\n", leftIndex);
                break;
            case BINARY_OP_LT:
                fprintf(fp, "cmp x%d, x%d\n", leftIndex, rightIndex);
                fprintf(fp, "cset x%d, lt\n", leftIndex);
                break;
            case BINARY_OP_AND:
                fprintf(fp, "cmp x%d, #0\n", leftIndex);
                fprintf(fp, "cset x%d, ne\n", leftIndex);
                fprintf(fp, "cmp x%d, #0\n", rightIndex);
                fprintf(fp, "cset x%d, ne\n", rightIndex);
                fprintf(fp, "and x%d, x%d, x%d\n", leftIndex, leftIndex, rightIndex);
                break;
            case BINARY_OP_OR:
                fprintf(fp, "cmp x%d, #0\n", leftIndex);
                fprintf(fp, "cset x%d, ne\n", leftIndex);
                fprintf(fp, "cmp x%d, #0\n", rightIndex);
                fprintf(fp, "cset x%d, ne\n", rightIndex);
                fprintf(fp, "orr x%d, x%d, x%d\n", leftIndex, leftIndex, rightIndex);
                break;
            default:
                fprintf(stderr, "Unhandle binary operation\n");
                break;
            }
            intRegs[rightIndex].status = UNUSED;
            intRegs[leftIndex].node = exprNode;
        }
    }
}

void genFunctionCall(AST_NODE* functionCallNode)
{
    AST_NODE* functionIDNode = functionCallNode->child;
    AST_NODE* actualParameterList = functionIDNode->rightSibling;
    int offset, isLibcall;
    DATA_TYPE returnType;
    if(strcmp(functionIDNode->semantic_value.identifierSemanticValue.identifierName, "read") == 0
       || strcmp(functionIDNode->semantic_value.identifierSemanticValue.identifierName, "fread") == 0
       || strcmp(functionIDNode->semantic_value.identifierSemanticValue.identifierName, "write") == 0)
    {
        isLibcall = 1;
    }
    else
    {
        isLibcall = 0;
    }
    if(strcmp(functionIDNode->semantic_value.identifierSemanticValue.identifierName, "write") == 0)
    {
        offset = 0; // put parameter to x0
        returnType = VOID_TYPE;
    }
    else
    {
        offset = -8 * functionIDNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->attr.functionSignature->parametersCount;
        returnType = functionIDNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->attr.functionSignature->returnType;
    }
    int index = 27;
    if(returnType == FLOAT_TYPE)
    {
        index = findReg(FLOAT_TYPE);
    }
    else if(returnType == INT_TYPE)
    {
        index = findReg(INT_TYPE);
    }
    saveRegs(31, 32);
    if(offset != 0)
    {
        fprintf(fp, "add sp, sp, #%d\n", offset);
    }
    int argOffset = 0;
    if(isLibcall && actualParameterList->nodeType != NUL_NODE)
    {
        // only write has parameter
        AST_NODE *parameter = actualParameterList->child;
        // now all the register is stored and unused
        genExprRelatedNode(parameter);
        int result = findPlace(parameter);
        // only int, float, ad string can be print
        if(parameter->dataType == FLOAT_TYPE)
        {
            if(result != 0)
            {
                // no need to mark s0 is dirty since the next instruction is bl
                fprintf(fp, "fmov s0, s%d\n", result);
                floatRegs[result].status = UNUSED;
            }
        }
        else
        {
            if(result != 0)
            {
                // no need to mark x0 is dirty since the next instruction is bl
                fprintf(fp, "mov x0, x%d\n", result);
                intRegs[result].status = UNUSED;
            }
        }
    }
    else if(actualParameterList->nodeType != NUL_NODE)
    {
        AST_NODE *traverseChildren = actualParameterList->child;
        while(traverseChildren)
        {
            saveRegs(traverseChildren->nIntRegs, traverseChildren->nFloatRegs);
            if(traverseChildren->dataType == INT_PTR_TYPE || traverseChildren->dataType == FLOAT_PTR_TYPE)
            {
                if(traverseChildren->semantic_value.identifierSemanticValue.symbolTableEntry->isGlobal)
                {
                    // global
                    fprintf(fp, "ldr x27, =_g_%s\n", traverseChildren->semantic_value.identifierSemanticValue.symbolTableEntry->name);
                    fprintf(fp, "str x27, [sp, #%d]\n", argOffset);
                }
                else if(traverseChildren->semantic_value.identifierSemanticValue.symbolTableEntry->offset < 0)
                {
                    // local variable
                    fprintf(fp, "add x27, x29, #%d\n", traverseChildren->semantic_value.identifierSemanticValue.symbolTableEntry->offset);
                    fprintf(fp, "str x27, [sp, #%d]\n", argOffset);
                }
                else
                {
                    // parameter
                    fprintf(fp, "ldr x27, [x29, #%d]\n", traverseChildren->semantic_value.identifierSemanticValue.symbolTableEntry->offset);
                    fprintf(fp, "str x27, [sp, #%d]\n", argOffset);
                }
            }
            else
            {
                genExprRelatedNode(traverseChildren);
                int result = findPlace(traverseChildren);
                if(traverseChildren->dataType == FLOAT_TYPE)
                {
                    fprintf(fp, "str s%d, [sp, #%d]\n", result, argOffset);
                    floatRegs[result].status = UNUSED;
                }
                else
                {
                    fprintf(fp, "str x%d, [sp, #%d]\n", result, argOffset);
                    intRegs[result].status = UNUSED;
                }
            }
            restoreRegs();
            traverseChildren = traverseChildren->rightSibling;
            argOffset += 8;
        }
    }
    if(strcmp(functionIDNode->semantic_value.identifierSemanticValue.identifierName, "read") == 0)
    {
        fprintf(fp, "bl _read_int\n");
    }
    else if(strcmp(functionIDNode->semantic_value.identifierSemanticValue.identifierName, "fread") == 0)
    {
        fprintf(fp, "bl _read_float\n");
    }
    else if(strcmp(functionIDNode->semantic_value.identifierSemanticValue.identifierName, "write") == 0)
    {
        switch(actualParameterList->child->dataType)
        {
        case INT_TYPE:
            fprintf(fp, "bl _write_int\n");
            break;
        case FLOAT_TYPE:
            fprintf(fp, "bl _write_float\n");
            break;
        case CONST_STRING_TYPE:
            fprintf(fp, "bl _write_str\n");
            break;
        default:
            fprintf(stderr, "Unsupported type for write()\n");
            break;
        }
    }
    else
    {
        fprintf(fp, "bl _start_%s\n", functionIDNode->semantic_value.identifierSemanticValue.symbolTableEntry->name);
    }
    if(returnType == FLOAT_TYPE)
    {
        fprintf(fp, "fmov s%d, s0\n", index);
        floatRegs[0].status = UNUSED;
        floatRegs[index].status = DIRTY;
        floatRegs[index].node = functionCallNode;
    }
    else if(returnType == INT_TYPE)
    {
        fprintf(fp, "mov x%d, x0\n", index);
        intRegs[0].status = UNUSED;
        intRegs[index].status = DIRTY;
        intRegs[index].node = functionCallNode;
    }
    if(offset != 0)
    {
        fprintf(fp, "add sp, sp, #%d\n", -offset);
    }
    restoreRegs(31, 32);
}

void genVariableR(AST_NODE* idNode)
{
    if(idNode->semantic_value.identifierSemanticValue.kind == NORMAL_ID)
    {
        int index;
        if(idNode->semantic_value.identifierSemanticValue.symbolTableEntry->isGlobal)
        {
            fprintf(fp, "ldr x27, =_g_%s\n", idNode->semantic_value.identifierSemanticValue.symbolTableEntry->name);
            if(idNode->dataType == FLOAT_TYPE)
            {
                index = findReg(FLOAT_TYPE);
                fprintf(fp, "ldr s%d, [x27]\n", index);
                floatRegs[index].status = DIRTY;
                floatRegs[index].node = idNode;
            }
            else
            {
                index = findReg(INT_TYPE);
                fprintf(fp, "ldr x%d, [x27]\n", index);
                intRegs[index].status = DIRTY;
                intRegs[index].node = idNode;
            }
        }
        else
        {

            if(idNode->dataType == FLOAT_TYPE)
            {
                index = findReg(FLOAT_TYPE);
                fprintf(fp, "ldr s%d, [x29, #%d]\n", index, idNode->semantic_value.identifierSemanticValue.symbolTableEntry->offset);
                floatRegs[index].status = DIRTY;
                floatRegs[index].node = idNode;
            }
            else
            {
                index = findReg(INT_TYPE);
                fprintf(fp, "ldr x%d, [x29, #%d]\n", index, idNode->semantic_value.identifierSemanticValue.symbolTableEntry->offset);
                intRegs[index].status = DIRTY;
                intRegs[index].node = idNode;
            }
        }
    }
    else if(idNode->semantic_value.identifierSemanticValue.kind == ARRAY_ID)
    {
        AST_NODE *traverseDimList = idNode->child;
        int *sizeInEachDimension = idNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->attr.typeDescriptor->properties.arrayProperties.sizeInEachDimension;
        int curDim = 0;
        int offsetIndex = findReg(INT_TYPE);
        intRegs[offsetIndex].status = DIRTY;
        intRegs[offsetIndex].node = idNode;
        fprintf(fp, "mov x%d, xzr\n", offsetIndex);
        while(traverseDimList)
        {
            int dimIndex = findReg(INT_TYPE);
            saveRegs(traverseDimList->nIntRegs, traverseDimList->nFloatRegs);
            genExprRelatedNode(traverseDimList);
            int result = findPlace(traverseDimList);
            if(dimIndex != result)
            {
                fprintf(fp, "mov x%d, x%d\n", dimIndex, result);
                intRegs[result].status = UNUSED;
                intRegs[dimIndex].status = DIRTY;
                intRegs[dimIndex].node = traverseDimList;
            }
            restoreRegs();
            fprintf(fp, "mov x27, #%d\n", sizeInEachDimension[curDim]);
            fprintf(fp, "mul x%d, x%d, x27\n", offsetIndex, offsetIndex);
            fprintf(fp, "add x%d, x%d, x%d\n", offsetIndex, offsetIndex, dimIndex);
            intRegs[dimIndex].status = UNUSED;
            curDim++;
            traverseDimList = traverseDimList->rightSibling;
        }
        if(idNode->semantic_value.identifierSemanticValue.symbolTableEntry->isGlobal)
        {
            // global
            fprintf(fp, "ldr x27, =_g_%s\n", idNode->semantic_value.identifierSemanticValue.symbolTableEntry->name);
        }
        else if(idNode->semantic_value.identifierSemanticValue.symbolTableEntry->offset < 0)
        {
            // local variable
            fprintf(fp, "add x27, x29, #%d\n", idNode->semantic_value.identifierSemanticValue.symbolTableEntry->offset);
        }
        else
        {
            // parameter
            fprintf(fp, "ldr x27, [x29, #%d]\n", idNode->semantic_value.identifierSemanticValue.symbolTableEntry->offset);
        }
        fprintf(fp, "lsl x%d, x%d, 3\n", offsetIndex, offsetIndex); // index * typeSize(=8)
        fprintf(fp, "add x%d, x%d, x27\n", offsetIndex, offsetIndex);
        if(idNode->dataType == FLOAT_TYPE)
        {
            int elementIndex = findReg(FLOAT_TYPE);
            fprintf(fp, "ldr s%d, [x%d]\n", elementIndex, offsetIndex);
            intRegs[offsetIndex].status = UNUSED;
            floatRegs[elementIndex].status = DIRTY;
            floatRegs[elementIndex].node = idNode;
        }
        else if(idNode->dataType == INT_TYPE)
        {
            fprintf(fp, "ldr x%d, [x%d]\n", offsetIndex, offsetIndex);
        }
    }
}

void genVariableL(AST_NODE *idNode)
{
    if(idNode->semantic_value.identifierSemanticValue.kind == NORMAL_ID)
    {
        int index;
        index = findReg(INT_TYPE);
        if(idNode->semantic_value.identifierSemanticValue.symbolTableEntry->isGlobal)
        {
            fprintf(fp, "ldr x%d, =_g_%s\n", index, idNode->semantic_value.identifierSemanticValue.symbolTableEntry->name);
        }
        else
        {
            fprintf(fp, "add x%d, x29, #%d\n", index, idNode->semantic_value.identifierSemanticValue.symbolTableEntry->offset);
        }
        intRegs[index].status = DIRTY;
        intRegs[index].node = idNode;
    }
    else if(idNode->semantic_value.identifierSemanticValue.kind == ARRAY_ID)
    {
        AST_NODE *traverseDimList = idNode->child;
        int *sizeInEachDimension = idNode->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->attr.typeDescriptor->properties.arrayProperties.sizeInEachDimension;
        int curDim = 0;
        int offsetIndex = findReg(INT_TYPE);
        intRegs[offsetIndex].status = DIRTY;
        intRegs[offsetIndex].node = idNode;
        fprintf(fp, "mov x%d, xzr\n", offsetIndex);
        while(traverseDimList)
        {
            int dimIndex = findReg(INT_TYPE);
            saveRegs(traverseDimList->nIntRegs, traverseDimList->nFloatRegs);
            genExprRelatedNode(traverseDimList);
            int result = findPlace(traverseDimList);
            if(dimIndex != result)
            {
                fprintf(fp, "mov x%d, x%d\n", dimIndex, result);
                intRegs[result].status = UNUSED;
                intRegs[dimIndex].status = DIRTY;
                intRegs[dimIndex].node = traverseDimList;
            }
            restoreRegs();
            fprintf(fp, "mov x27, #%d\n", sizeInEachDimension[curDim]);
            fprintf(fp, "mul x%d, x%d, x27\n", offsetIndex, offsetIndex);
            fprintf(fp, "add x%d, x%d, x%d\n", offsetIndex, offsetIndex, dimIndex);
            intRegs[dimIndex].status = UNUSED;
            curDim++;
            traverseDimList = traverseDimList->rightSibling;
        }
        if(idNode->semantic_value.identifierSemanticValue.symbolTableEntry->isGlobal)
        {
            // global
            fprintf(fp, "ldr x27, =_g_%s\n", idNode->semantic_value.identifierSemanticValue.symbolTableEntry->name);
        }
        else if(idNode->semantic_value.identifierSemanticValue.symbolTableEntry->offset < 0)
        {
            // local variable
            fprintf(fp, "add x27, x29, #%d\n", idNode->semantic_value.identifierSemanticValue.symbolTableEntry->offset);
        }
        else
        {
            // parameter
            fprintf(fp, "ldr x27, [x29, #%d]\n", idNode->semantic_value.identifierSemanticValue.symbolTableEntry->offset);
        }
        fprintf(fp, "lsl x%d, x%d, 3\n", offsetIndex, offsetIndex); // index * typeSize(=8)
        fprintf(fp, "add x%d, x%d, x27\n", offsetIndex, offsetIndex);
    }
}

void genConst(AST_NODE *constValueNode)
{
    if(constValueNode->dataType == CONST_STRING_TYPE)
    {
        int index = findReg(INT_TYPE), len = strlen(constValueNode->semantic_value.const1->const_u.sc);
        fprintf(fp, ".data\n");
        fprintf(fp, "_CONSTANT_%d: .ascii %.*s", nLabel, len - 1, constValueNode->semantic_value.const1->const_u.sc); // remove the right quote
        fprintf(fp, "\\000\"\n"); // append null byte and add right quote back
        fprintf(fp, ".text\n");
        fprintf(fp, "ldr x%d, =_CONSTANT_%d\n", index, nLabel);
        intRegs[index].status = DIRTY;
        intRegs[index].node = constValueNode;
        nLabel++;
    }
    else if(constValueNode->dataType == FLOAT_TYPE)
    {
        int index = findReg(FLOAT_TYPE);
        char hex[9] = {0};
        snprintf(hex, 9, "%08x", *(unsigned int*)&constValueNode->semantic_value.exprSemanticValue.constEvalValue.fValue);
        fprintf(fp, "mov w27, #0x%4s\n", &(hex[4]));
        fprintf(fp, "movk w27, #0x%4.4s, lsl 16\n", hex);
        fprintf(fp, "fmov s%d, w27\n", index);
        floatRegs[index].status = DIRTY;
        floatRegs[index].node = constValueNode;
    }
    else if(constValueNode->dataType == INT_TYPE)
    {
        int index = findReg(INT_TYPE);
        char hex[9] = {0};
        snprintf(hex, 9, "%08x", *(unsigned int*)&constValueNode->semantic_value.exprSemanticValue.constEvalValue.iValue);
        fprintf(fp, "mov w27, #0x%4s\n", &(hex[4]));
        fprintf(fp, "movk w27, #0x%4.4s, lsl 16\n", hex);
        fprintf(fp, "sxtw x%d, w27\n", index);
        intRegs[index].status = DIRTY;
        intRegs[index].node = constValueNode;
    }
    else
    {
        fprintf(stderr, "Unknown constant type\n");
    }
}
void genExprRelatedNode(AST_NODE* exprRelatedNode)
{
    switch(exprRelatedNode->nodeType)
    {
    case EXPR_NODE:
        genExprNode(exprRelatedNode);
        break;
    case STMT_NODE:
        //function call
        genFunctionCall(exprRelatedNode);
        break;
    case IDENTIFIER_NODE:
        genVariableR(exprRelatedNode);
        break;
    case CONST_VALUE_NODE:
        genConst(exprRelatedNode);
        break;
    default:
        break;
    }
}

void genVarDeclNode(AST_NODE *varDeclNode)
{
    AST_NODE *typeNode = varDeclNode->child;
    AST_NODE *traverseChildren = typeNode->rightSibling;
    while(traverseChildren)
    {
        if(traverseChildren->semantic_value.identifierSemanticValue.kind == WITH_INIT_ID)
        {
            genExprNode(traverseChildren->child);
            int index = findPlace(traverseChildren->child);
            if(traverseChildren->child->dataType == INT_TYPE)
            {
                fprintf(fp, "str x%d, [x29, %d]\n", index, traverseChildren->semantic_value.identifierSemanticValue.symbolTableEntry->offset);
                intRegs[index].status = UNUSED;
            }
            else if(traverseChildren->child->dataType == FLOAT_TYPE)
            {
                fprintf(fp, "str s%d, [x29, %d]\n", index, traverseChildren->semantic_value.identifierSemanticValue.symbolTableEntry->offset);
                floatRegs[index].status = UNUSED;
            }
        }
        traverseChildren = traverseChildren->rightSibling;
    }
}

void genVarListNode(AST_NODE *varListNode)
{
    AST_NODE *traverseChildren = varListNode->child;
    while(traverseChildren)
    {
        if(traverseChildren->semantic_value.declSemanticValue.kind == VARIABLE_DECL)
        {
            genVarDeclNode(traverseChildren);
        }
        traverseChildren = traverseChildren->rightSibling;
    }
}

void genStmtListNode(AST_NODE *stmtListNode)
{
    AST_NODE *traverseChildren = stmtListNode->child;
    while(traverseChildren)
    {
        if(traverseChildren->nodeType == BLOCK_NODE)
        {
            genBlockNode(traverseChildren);
        }
        else if(traverseChildren->nodeType == STMT_NODE)
        {
            genStmtNode(traverseChildren);
        }
        traverseChildren = traverseChildren->rightSibling;
    }
}

void genBlockNode(AST_NODE *blockNode)
{
    initRegs();
    AST_NODE *traverseChildren = blockNode->child;
    while(traverseChildren)
    {
        if(traverseChildren->nodeType == VARIABLE_DECL_LIST_NODE)
        {
            genVarListNode(traverseChildren);
        }
        else
        {
            genStmtListNode(traverseChildren);
        }
        traverseChildren = traverseChildren->rightSibling;
    }
}

void genStmtNode(AST_NODE *stmtNode)
{
    initRegs();
    if(stmtNode->nodeType == NUL_NODE)
    {
        return;
    }
    else if(stmtNode->nodeType == BLOCK_NODE)
    {
        genBlockNode(stmtNode);
        return;
    }
    else
    {
        switch(stmtNode->semantic_value.stmtSemanticValue.kind)
        {
        case WHILE_STMT:
            genWhileStmt(stmtNode);
            break;
        case FOR_STMT:
            genForStmt(stmtNode);
            break;
        case ASSIGN_STMT:
            genAssignmentStmt(stmtNode);
            break;
        case IF_STMT:
            genIfStmt(stmtNode);
            break;
        case FUNCTION_CALL_STMT:
            genFunctionCall(stmtNode);
            break;
        case RETURN_STMT:
            genReturnStmt(stmtNode);
            break;
        default:
            break;
        }
    }
}

void genReturnStmt(AST_NODE *returnNode)
{
    initRegs();
    if(returnNode->child->nodeType != NUL_NODE)
    {
        genExprRelatedNode(returnNode->child);
        int index = findPlace(returnNode->child);
        if(curFuncNode->child->dataType == FLOAT_TYPE)
        {
            if(index != 0)
            {
                fprintf(fp, "fmov s0, s%d\n", index);
            }
        }
        else if(curFuncNode->child->dataType == INT_TYPE)
        {
            if(index != 0)
            {
                fprintf(fp, "mov x0, x%d\n", index);
            }
        }
    }
    if(strcmp(curFuncNode->child->rightSibling->semantic_value.identifierSemanticValue.symbolTableEntry->name, "main") == 0)
    {
        fprintf(fp, "b _end_MAIN\n");
    }
    else
    {
        fprintf(fp, "b _end_%s\n", curFuncNode->child->rightSibling->semantic_value.identifierSemanticValue.symbolTableEntry->name);
    }
}

void genWhileStmt(AST_NODE* whileNode)
{
    unsigned int label_count = nLabel;
    nLabel++;
    initRegs();
    AST_NODE* boolExpression = whileNode->child;
    fprintf(fp, "_startWhile_%u:\n", label_count);
    genExprRelatedNode(boolExpression);
    int result = findPlace(boolExpression);
    if(boolExpression->dataType == FLOAT_TYPE)
    {
        fprintf(fp, "fcmp s%d, #0.0\n", result);
        fprintf(fp, "beq _exitWhile_%u\n", label_count);
        floatRegs[result].status = UNUSED;
    }
    else
    {
        fprintf(fp, "cmp x%d, #0\n", result);
        fprintf(fp, "beq _exitWhile_%u\n", label_count);
        intRegs[result].status = UNUSED;
    }
    AST_NODE* bodyNode = boolExpression->rightSibling;
    genStmtNode(bodyNode);
    fprintf(fp, "b _startWhile_%u\n", label_count);
    fprintf(fp, "_exitWhile_%u:\n", label_count);
}

void genIfStmt(AST_NODE* ifNode)
{
    unsigned int label_count = nLabel;
    nLabel++;
    initRegs();
    AST_NODE* boolExpression = ifNode->child;
    genExprRelatedNode(boolExpression);
    int result = findPlace(boolExpression);
    if(boolExpression->dataType == FLOAT_TYPE)
    {
        fprintf(fp, "fcmp s%d, #0.0\n", result);
        fprintf(fp, "beq _else_%u\n", label_count);
        floatRegs[result].status = UNUSED;
    }
    else
    {
        fprintf(fp, "cmp x%d, #0\n", result);
        fprintf(fp, "beq _else_%u\n", label_count);
        floatRegs[result].status = UNUSED;
    }
    AST_NODE* ifBodyNode = boolExpression->rightSibling;
    genStmtNode(ifBodyNode);
    fprintf(fp, "b _exitIF_%u\n", label_count);
    fprintf(fp, "_else_%u:\n", label_count);
    AST_NODE* elsePartNode = ifBodyNode->rightSibling;
    genStmtNode(elsePartNode);
    fprintf(fp, "_exitIF_%u:\n", label_count);
}

void genForStmt(AST_NODE* forNode)
{
    unsigned int label_count = nLabel;
    nLabel++;
    initRegs();
    AST_NODE* initExpression = forNode->child;
    if(initExpression->nodeType != NUL_NODE)
    {
        genNonemptyAssigExprListNode(initExpression);
    }
    AST_NODE* conditionExpression = initExpression->rightSibling;
    fprintf(fp, "_startFor_%d:\n", label_count);
    if(conditionExpression->nodeType != NUL_NODE)
    {
        genNonemptyRelopExprListNode(conditionExpression);
        AST_NODE *traverseChildren = conditionExpression->child, *lastChild = NULL;
        while(traverseChildren)
        {
            lastChild = traverseChildren;
            traverseChildren = traverseChildren->rightSibling;
        }
        int result = -1;
        if(lastChild != NULL)
        {
            result = findPlace(lastChild);
        }
        if(result >= 0)
        {
            if(lastChild->dataType == FLOAT_TYPE)
            {
                fprintf(fp, "fcmp s%d, #0.0\n", result);
                fprintf(fp, "beq _exitFor_%d\n", label_count);
                floatRegs[result].status = UNUSED;
            }
            else
            {
                fprintf(fp, "cmp x%d, #0\n", result);
                fprintf(fp, "beq _exitFor_%d\n", label_count);
                intRegs[result].status = UNUSED;
            }
        }
    }
    AST_NODE* loopExpression = conditionExpression->rightSibling;
    AST_NODE* bodyNode = loopExpression->rightSibling;
    genStmtNode(bodyNode);
    if(loopExpression->nodeType != NUL_NODE)
    {
        genNonemptyAssigExprListNode(loopExpression);
    }
    fprintf(fp, "b _startFor_%d\n", label_count);
    fprintf(fp, "_exitFor_%d:\n", label_count);
}

void genNonemptyAssigExprListNode(AST_NODE *node)
{
    AST_NODE *traverseChildren = node->child;
    while(traverseChildren)
    {
        initRegs();
        genAssignOrExprNode(traverseChildren);
        traverseChildren = traverseChildren->rightSibling;
    }
    initRegs(); //clear the result which might not be stored
}

void genAssignOrExprNode(AST_NODE* assignOrExprRelatedNode)
{
    if(assignOrExprRelatedNode->nodeType == STMT_NODE)
    {
        if(assignOrExprRelatedNode->semantic_value.stmtSemanticValue.kind == ASSIGN_STMT)
        {
            genAssignmentStmt(assignOrExprRelatedNode);
        }
        else if(assignOrExprRelatedNode->semantic_value.stmtSemanticValue.kind == FUNCTION_CALL_STMT)
        {
            genFunctionCall(assignOrExprRelatedNode);
        }
    }
    else
    {
        genExprRelatedNode(assignOrExprRelatedNode);
    }
}

void genAssignmentStmt(AST_NODE* assignmentNode)
{
    AST_NODE* leftOp = assignmentNode->child;
    AST_NODE* rightOp = leftOp->rightSibling;
    int leftIndex, rightIndex;
    initRegs();
    if(rightOp->dataType == FLOAT_TYPE)
    {
        rightIndex = findReg(FLOAT_TYPE);
    }
    else
    {
        rightIndex = findReg(INT_TYPE);
    }
    saveRegs(rightOp->nIntRegs, rightOp->nFloatRegs);
    genExprRelatedNode(rightOp);
    int result = findPlace(rightOp);
    if(rightOp->dataType == FLOAT_TYPE)
    {
        if(result != rightIndex)
        {
            fprintf(fp, "fmov s%d, s%d\n", rightIndex, result);
            floatRegs[result].status = UNUSED;
            floatRegs[rightIndex].status = DIRTY;
            floatRegs[rightIndex].node = rightOp;
        }
    }
    else
    {
        if(result != rightIndex)
        {
            fprintf(fp, "mov x%d, x%d\n", rightIndex, result);
            intRegs[result].status = UNUSED;
            intRegs[rightIndex].status = DIRTY;
            intRegs[rightIndex].node = rightOp;
        }
    }
    restoreRegs();
    leftIndex = findReg(INT_TYPE);
    saveRegs(leftOp->nIntRegs, leftOp->nFloatRegs);
    genVariableL(leftOp);
    restoreRegs();
    for(leftIndex = 0; leftIndex < 31; leftIndex++)
    {
        if(intRegs[leftIndex].status == DIRTY && intRegs[leftIndex].node == leftOp)
        {
            break;
        }
    }
    if(rightOp->dataType == FLOAT_TYPE)
    {
        fprintf(fp, "str s%d, [x%d]\n", rightIndex, leftIndex);
        floatRegs[rightIndex].status = UNUSED;
    }
    else
    {
        fprintf(fp, "str x%d, [x%d]\n", rightIndex, leftIndex);
        intRegs[rightIndex].status = UNUSED;
    }
    intRegs[leftIndex].status = UNUSED;
}

void genNonemptyRelopExprListNode(AST_NODE *node)
{
    AST_NODE *traverseChildren = node->child;
    while(traverseChildren)
    {
        initRegs();
        genExprRelatedNode(traverseChildren);
        traverseChildren = traverseChildren->rightSibling;
    }
    // keep the result of last child for parent
}

void genFuncDecl(AST_NODE *funcNode)
{
    AST_NODE* returnTypeNode = funcNode->child;
    AST_NODE* functionNameID = returnTypeNode->rightSibling;
    AST_NODE *parameterListNode = functionNameID->rightSibling;
    AST_NODE *blockNode = parameterListNode->rightSibling;
    curFuncNode = funcNode;
    fprintf(fp, ".text\n");
    if(strcmp(functionNameID->semantic_value.identifierSemanticValue.symbolTableEntry->name, "main") == 0)
    {
        fprintf(fp, "_start_MAIN:\n");
    }
    else
    {
        fprintf(fp, "_start_%s:\n", functionNameID->semantic_value.identifierSemanticValue.symbolTableEntry->name);
    }
    fprintf(fp, "str x30, [sp, #-8]\n");
    fprintf(fp, "str x29, [sp, #-16]\n");
    fprintf(fp, "add sp, sp, #-16\n");
    fprintf(fp, "mov x29, sp\n");
    fprintf(fp, "add sp, sp, #%d\n", -functionNameID->semantic_value.identifierSemanticValue.symbolTableEntry->attribute->attr.functionSignature->frameSize);
    genBlockNode(blockNode);
    if(strcmp(functionNameID->semantic_value.identifierSemanticValue.symbolTableEntry->name, "main") == 0)
    {
        fprintf(fp, "_end_MAIN:\n");
    }
    else
    {
        fprintf(fp, "_end_%s:\n", functionNameID->semantic_value.identifierSemanticValue.symbolTableEntry->name);
    }
    fprintf(fp, "mov sp, x29\n");
    fprintf(fp, "ldr x30, [sp, #8]\n");
    fprintf(fp, "ldr x29, [sp, #0]\n");
    fprintf(fp, "add sp, sp, #16\n");
    fprintf(fp, "RET x30\n");
}

void genGlobalVarDeclNode(AST_NODE *varDeclNode)
{
    AST_NODE *traverseChildren = varDeclNode->child;
    int typeSize = 8;
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
        if(traverseChildren->semantic_value.identifierSemanticValue.kind == WITH_INIT_ID)
        {
            fprintf(stderr, "Initialization for global variable isn't supported and is ignored.\n");
        }
        fprintf(fp, "_g_%s: .zero %d\n", traverseChildren->semantic_value.identifierSemanticValue.symbolTableEntry->name, varSize);
        traverseChildren = traverseChildren->rightSibling;
    }
}
void genGlobalVarListNode(AST_NODE *varListNode)
{
    AST_NODE *traverseChildren = varListNode->child;
    if(traverseChildren)
    {
        fprintf(fp, ".bss\n");
    }
    while(traverseChildren)
    {
        if(traverseChildren->semantic_value.declSemanticValue.kind == VARIABLE_DECL)
        {
            genGlobalVarDeclNode(traverseChildren);
        }
        traverseChildren = traverseChildren->rightSibling;
    }
}
void genProgram(AST_NODE *root)
{
    AST_NODE *traverseDeclaration = root->child;
    while(traverseDeclaration)
    {
        if(traverseDeclaration->nodeType == VARIABLE_DECL_LIST_NODE)
        {
            genGlobalVarListNode(traverseDeclaration);
        }
        else
        {
            genFuncDecl(traverseDeclaration);
        }

        traverseDeclaration = traverseDeclaration->rightSibling;
    }
}

void initRegs()
{
    for(int i = 0; i < 27; i++) intRegs[i].status = UNUSED;
    for(int i = 0; i < 32; i++) floatRegs[i].status = UNUSED;
    // for logical operation and calculate address
    intRegs[27].status = PRESERVED;
    intRegs[28].status = PRESERVED;
    // for function call
    intRegs[29].status = PRESERVED;
    intRegs[30].status = PRESERVED;
}

void genCode(AST_NODE *root, char *filename)
{
    if (filename == NULL) {
        filename = "output.s";
    }

    fp = fopen(filename, "w");
    if (!fp) {
        printf("Cannot open file \"%s\"\n", filename);
        return;
    }
    preprocess(root);
    initRegs();
    genProgram(root);

    fclose(fp);
}
