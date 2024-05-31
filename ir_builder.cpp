#include"ast.h"
#include"Core.h"
#include <stdio.h>
#include <stdbool.h>
#include <unordered_map>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>
#include <set>
#include <stack>

using namespace std;

LLVMValueRef ret_ref;
LLVMBasicBlockRef retBB;
unordered_map<astNode*, string> name_map;   
unordered_map<string, LLVMValueRef> var_map;
LLVMValueRef readFunc;
LLVMValueRef printFunc;
set<LLVMBasicBlockRef> all_bb_set;

// subroutines
int rename_vars(astNode *root, int counter);
void remove_unreachable_blocks(LLVMBasicBlockRef startBB, set<LLVMBasicBlockRef> all_bb_set);

/*
    subroutine genIRExpr
    input: ASTNode of expr, builder
    output: LLVMValueRef of the expression
*/
LLVMValueRef genIRExpr(astNode *expr, LLVMBuilderRef &builder){
    LLVMValueRef res = NULL;
    LLVMValueRef lhs = NULL;
    LLVMValueRef rhs = NULL;
    LLVMValueRef temp_expr = NULL;
    switch (expr->type)
    {
    case ast_cnst:{
        res = LLVMConstInt(LLVMInt32Type(), expr->cnst.value, 0);
        printf("Generating constant\n");
        break;
    }
    case ast_var:{
        printf("build load\n");
        //Generate a load instruction that loads from the memory location (alloc instruction in var_map) corresponding to the variable name in the node.
        LLVMValueRef alloc;
        if (name_map.find(expr) == name_map.end()){
            // meaning it is a parameter
            printf("Alloc for param: %s\n", expr->var.name);
            alloc = var_map[string(expr->var.name)];
        } else {
            string new_name = name_map[expr];
            printf("Alloc for var with new name: %s\n", new_name.c_str());
            alloc = var_map[new_name];
        }
        res = LLVMBuildLoad2(builder, LLVMInt32Type(), alloc, "");
        break;
    }
    case ast_uexpr:{
        temp_expr = genIRExpr(expr->uexpr.expr, builder);
        /* Generate a subtraction instruction with constant 0 as the first operand and 
        LLVMValueRef from step A above as the second operand.*/
        res = LLVMBuildSub(builder, LLVMConstInt(LLVMInt32Type(), 0, 0), temp_expr, "");
        break;
    }
    case ast_bexpr:{
        // generate LLVMValueRef of LHS and RHS by calling the genIRExpr recursively
        lhs = genIRExpr(expr->bexpr.lhs, builder);
        rhs = genIRExpr(expr->bexpr.rhs, builder);
        // generate based on operator
        if(expr->bexpr.op == add){
            printf("Generating add\n");
            res = LLVMBuildAdd(builder, lhs, rhs, "");
        } else if (expr->bexpr.op == sub){
            res = LLVMBuildSub(builder, lhs, rhs, "");
        } else if (expr->bexpr.op == mul){
            res = LLVMBuildMul(builder, lhs, rhs, "");
        } 
        break;
    }
    case ast_rexpr:{
        lhs = genIRExpr(expr->rexpr.lhs, builder);
        rhs = genIRExpr(expr->rexpr.rhs, builder);
        // generate compare instruction based on operator
        if(expr->rexpr.op == lt){
            res = LLVMBuildICmp(builder, LLVMIntSLT, lhs, rhs, "");
        } else if (expr->rexpr.op == gt){
            res = LLVMBuildICmp(builder, LLVMIntSGT, lhs, rhs, "");
        } else if (expr->rexpr.op == le){
            res = LLVMBuildICmp(builder, LLVMIntSLE, lhs, rhs, "");
        } else if (expr->rexpr.op == ge){
            res = LLVMBuildICmp(builder, LLVMIntSGE, lhs, rhs, "");
        } else if (expr->rexpr.op == eq){
            res = LLVMBuildICmp(builder, LLVMIntEQ, lhs, rhs, "");
        } else if (expr->rexpr.op == neq){
            res = LLVMBuildICmp(builder, LLVMIntNE, lhs, rhs, "");
        }
        break;
    }
    case ast_call:{
        // call must be a read function, generate call instruction
        LLVMTypeRef readFuncType = LLVMFunctionType(LLVMInt32Type(), nullptr, 0, 0);
        
        res = LLVMBuildCall2(builder, readFuncType, readFunc, NULL, 0, "");
        break;
    }
    }

    return res;
}

/*
    subroutine genIRStmt
    input: ASTNode of stmt, builder, startBB
    output: endBB
*/
LLVMBasicBlockRef genIRStmt(astNode *stmt, LLVMBuilderRef &builder, LLVMBasicBlockRef &startBB, LLVMValueRef &func){
    LLVMBasicBlockRef endBB = NULL;

    switch (stmt->stmt.type)
    {
    case ast_asgn:{
        LLVMPositionBuilderAtEnd(builder, startBB);
        //Generate LLVMValueRef of RHS by calling the genIRExpr subroutine
        LLVMValueRef rhs = genIRExpr(stmt->stmt.asgn.rhs, builder);
        string lhs_name(stmt->stmt.asgn.lhs->var.name);
        // pass in the alloc as third parameter
        LLVMValueRef alloc;
        if (name_map.find(stmt->stmt.asgn.lhs) == name_map.end()){
            printf("in ast_asgn, ERR or it's a param: %s\n", lhs_name.c_str());
            // print out type of the node
            printNode(stmt->stmt.asgn.lhs);
            // meaning it is a parameter
            alloc = var_map[lhs_name];
        } else {
            string new_name = name_map[stmt->stmt.asgn.lhs];
            alloc = var_map[new_name];
        }
        if(alloc == NULL){
            printf("Variable not found\n");
        }
        LLVMBuildStore(builder, rhs, alloc);
        endBB = startBB;
        break;
    }

    
    case ast_call:{
        LLVMPositionBuilderAtEnd(builder, startBB);
        LLVMTypeRef printParamTypes[] = {LLVMInt32Type()}; // One integer parameter
	    LLVMTypeRef printFuncType = LLVMFunctionType(LLVMVoidType(), printParamTypes, 1, 0);
        LLVMValueRef call_param = genIRExpr(stmt->stmt.call.param, builder);
        // generate call instruction to the print function 
        LLVMValueRef call_inst = LLVMBuildCall2(builder, printFuncType, printFunc, &call_param, 1, "");
        endBB = startBB;
        break;
    }

    
    case ast_while:{
        LLVMPositionBuilderAtEnd(builder, startBB);
        LLVMBasicBlockRef condBB = LLVMAppendBasicBlock(func, "");
        // generate unconditional branch to condBB
        LLVMBuildBr(builder, condBB);
        LLVMPositionBuilderAtEnd(builder, condBB);
        LLVMValueRef cond = genIRExpr(stmt->stmt.whilen.cond, builder);
        LLVMBasicBlockRef trueBB = LLVMAppendBasicBlock(func, "");
        LLVMBasicBlockRef falseBB = LLVMAppendBasicBlock(func, "");
        // generate conditional branch based on cond
        LLVMBuildCondBr(builder, cond, trueBB, falseBB);
        // generate for the loop body by calling genIRStmt recursively
        LLVMBasicBlockRef trueExitBB = genIRStmt(stmt->stmt.whilen.body, builder, trueBB, func);
        LLVMPositionBuilderAtEnd(builder, trueExitBB);
        // generate unconditional branch to condBB
        LLVMBuildBr(builder, condBB);
        endBB = falseBB;
        break;
    }

    
    case ast_if:{
        LLVMPositionBuilderAtEnd(builder, startBB);
        LLVMValueRef if_cond = genIRExpr(stmt->stmt.ifn.cond, builder);
        LLVMBasicBlockRef trueBB = LLVMAppendBasicBlock(func, "");
        LLVMBasicBlockRef falseBB = LLVMAppendBasicBlock(func, "");
        // conditional branch based on cond
        LLVMBuildCondBr(builder, if_cond, trueBB, falseBB);
        // if there is no else part
        if(stmt->stmt.ifn.else_body == NULL){
            // generate for the if body by calling genIRStmt recursively
            LLVMBasicBlockRef ifExitBB = genIRStmt(stmt->stmt.ifn.if_body, builder, trueBB, func);
            LLVMPositionBuilderAtEnd(builder, ifExitBB);
            // unconditional branch to falseBB
            LLVMBuildBr(builder, falseBB);
            endBB = falseBB;
        } else {
            // generate for the if and else body by calling genIRStmt recursively
            LLVMBasicBlockRef ifExitBB = genIRStmt(stmt->stmt.ifn.if_body, builder, trueBB, func);
            LLVMBasicBlockRef elseExitBB = genIRStmt(stmt->stmt.ifn.else_body, builder, falseBB, func);
            // new endBB
            endBB = LLVMAppendBasicBlock(func, "");
            LLVMPositionBuilderAtEnd(builder, ifExitBB);
            // unconditional branch to endBB
            LLVMBuildBr(builder, endBB);
            LLVMPositionBuilderAtEnd(builder, elseExitBB);
            // unconditional branch to endBB
            LLVMBuildBr(builder, endBB);
        }
        break;
    }


    case ast_ret:{
        LLVMPositionBuilderAtEnd(builder, startBB);
        LLVMValueRef ret_val = genIRExpr(stmt->stmt.ret.expr, builder);
        // generate store inst of return value to the mem location pointed by ret_ref
        LLVMBuildStore(builder, ret_val, ret_ref);
        LLVMBuildBr(builder, retBB);
        // create a new endBB
        endBB = LLVMAppendBasicBlock(func, "");
        break;
    }


    case ast_block:{
        printf("ast_block\n");
        LLVMBasicBlockRef prevBB = startBB;
        for(int i = 0; i < stmt->stmt.block.stmt_list->size(); i++){
            // generate for each stmt in the block by calling genIRStmt recursively
            prevBB = genIRStmt(stmt->stmt.block.stmt_list->at(i), builder, prevBB, func);
        }
        endBB = prevBB;
        break;
    }

    default:{
        endBB = startBB;
        break;
    }

    }

    all_bb_set.insert(endBB);

    return endBB;
    
}

/*
    input: AST root node
    output: LLVM module reference for the program
    assumption: renaming routine has been called on the AST
*/
LLVMModuleRef generate_IR(astNode *root) {

    //REVIEW - module name?
    LLVMModuleRef res_module = LLVMModuleCreateWithName("miniC");
    LLVMSetTarget(res_module, "x86_64-pc-linux-gnu");

    // REVIEW - global variables
    readFunc = LLVMGetNamedFunction(res_module, "read");
    printFunc = LLVMGetNamedFunction(res_module, "print");

    // rename variables
    rename_vars(root, 0);

    // print out all entries in name_map
    printf("Printing name_map\n");
    for(auto it = name_map.begin(); it != name_map.end(); it++){
        astNode* node = it->first;
        // print out the type of the node
        printNode(node);
        printf("value: %s\n", it->second.c_str());
    }
    
    // Generate LLVM functions without bodies for print and read extern function declarations
    LLVMAddFunction(res_module, "print", LLVMFunctionType(LLVMVoidType(), NULL, 0, false));
    LLVMAddFunction(res_module, "read", LLVMFunctionType(LLVMInt32Type(), NULL, 0, false));

    LLVMBuilderRef builder = LLVMCreateBuilder();
    LLVMTypeRef ret_type = LLVMInt32Type();
    LLVMTypeRef param_types[] = {LLVMInt32Type()};
    LLVMTypeRef func_type = LLVMFunctionType(ret_type, param_types, 1, 0);
    LLVMValueRef func = LLVMAddFunction(res_module, "main", func_type);
    // create entry bb
    LLVMBasicBlockRef entryBB = LLVMAppendBasicBlock(func, "");
    all_bb_set.insert(entryBB);

    LLVMPositionBuilderAtEnd(builder, entryBB);

    // NOTE: initialze var_map, string new_name -> LLVMValueRef Alloc instruction
    // FIXME: alloc for return value!!!
    unordered_map<string, LLVMValueRef> var_map = unordered_map<string, LLVMValueRef>();
    // build alloc instructions for function parameter
    // get the function parameter if there is one
    printf("building var_map...\n");
    LLVMValueRef param = LLVMGetParam(func, 0);
    if(param){
        string var_name(root->func.param->var.name);
        LLVMValueRef alloc = LLVMBuildAlloca(builder, LLVMInt32Type(), "");
        var_map[var_name] = alloc;
    }
    for (auto it = name_map.begin(); it != name_map.end(); it++) {
        string var_name = it->second;
        LLVMValueRef alloc = LLVMBuildAlloca(builder, LLVMInt32Type(), "");
        var_map[var_name] = alloc;
    }
    // print out the content of var_map
    for(auto it = var_map.begin(); it != var_map.end(); it++){
        printf("key: %s\n", it->first.c_str());
        LLVMDumpValue(it->second);
        printf("\n");
    }

    // F - generate alloc instruction for return value and use that as global ret_ref
    ret_ref = LLVMBuildAlloca(builder, LLVMInt32Type(), "");
    
    // G - generate a store instruction for function parameter
    if(param){
        LLVMBuildStore(builder, LLVMGetParam(func, 0), var_map[string(root->func.param->var.name)]);
    }    
    
    // H - keep retBB global
    retBB = LLVMAppendBasicBlock(func, "");
    LLVMPositionBuilderAtEnd(builder, retBB);
    all_bb_set.insert(retBB);
    
    // J - I. add a load instruction from the ret_ref
    LLVMValueRef ret_inst = LLVMBuildLoad2(builder, LLVMInt32Type(), ret_ref, "");
    // J - II. add return instruction with operand as the load instruction created above
    LLVMBuildRet(builder, ret_inst);

    // K. call genIRStmt
    LLVMBasicBlockRef exitBB = genIRStmt(root->prog.func->func.body, builder, entryBB, func);
    // L. get the terminator instruction of exitBB
    LLVMValueRef term_inst = LLVMGetBasicBlockTerminator(exitBB);
    if(term_inst == NULL){
        LLVMPositionBuilderAtEnd(builder, exitBB);
        // generate an unconditional branch to retBB
        LLVMBuildBr(builder, retBB);
    }
    all_bb_set.insert(exitBB);

    // TODO - M. remove all basic blocks that do not have any predecessors
    // remove_unreachable_blocks(entryBB, all_bb_set);

    // TODO - clean up: maps, sets, builder
    LLVMDisposeBuilder(builder);
    // clean up all global variables
    name_map.clear();
    var_map.clear();
    // var_set.clear();
    // free ret_ref, retBB, readFunc, printFunc
    ret_ref = NULL;
    retBB = NULL;
    readFunc = NULL;
    printFunc = NULL;

    return res_module;
}

// global variable stack for renaming
stack<vector<string>> var_stack;
// return value: counter for the next unique variable name
int rename_vars(astNode *root, int counter) {
    // post-order traversal of root
    if (root == NULL) {
        return 0;
    }
    if (root->type == ast_stmt){
        if (root->stmt.type == ast_block){
            int curr_ct = counter;
            for(int i = 0; i < root->stmt.block.stmt_list->size(); i++){
                curr_ct = rename_vars(root->stmt.block.stmt_list->at(i), curr_ct);
            }
            return curr_ct;
        } else if (root->stmt.type == ast_decl){
            char* temp = root->stmt.decl.name;
            string old_name(temp);
            string new_name = old_name + to_string(counter);
            // print out the old name and new name
            printf("Renaming %s to %s\n", old_name.c_str(), new_name.c_str());
            // NOTE - what is the key for name_map
            name_map[root] = new_name;
            return counter + 1;
            
        } else if (root->stmt.type == ast_call){
            return rename_vars(root->stmt.call.param, counter);
        } else if (root->stmt.type == ast_asgn){
            int curr_ct;
            curr_ct = rename_vars(root->stmt.asgn.lhs, counter);
            return rename_vars(root->stmt.asgn.rhs, curr_ct);
        } else if (root->stmt.type == ast_if){
            int curr_ct = counter;
            curr_ct = rename_vars(root->stmt.ifn.cond, curr_ct);
            curr_ct = rename_vars(root->stmt.ifn.if_body, curr_ct);
            curr_ct = rename_vars(root->stmt.ifn.else_body, curr_ct);
            return curr_ct;
        } else if (root->stmt.type == ast_while){
            int curr_ct = counter;
            curr_ct = rename_vars(root->stmt.whilen.cond, curr_ct);
            curr_ct = rename_vars(root->stmt.whilen.body, curr_ct);
            return curr_ct;
        } else if (root->stmt.type == ast_ret){
            return rename_vars(root->stmt.ret.expr, counter);
        }
    } else if (root->type == ast_func){
        // ANCHOR: do we need name of the function
        int curr_ct = counter;
        curr_ct = rename_vars(root->func.body, curr_ct);
        curr_ct = rename_vars(root->func.param, curr_ct);
        return curr_ct;
    } else if (root->type == ast_var){
        // rename the variable
        return counter;

    } else if (root->type == ast_prog){
        int curr_ct = counter;
        curr_ct = rename_vars(root->prog.func, curr_ct);
        curr_ct = rename_vars(root->prog.ext1, curr_ct);
        curr_ct = rename_vars(root->prog.ext2, curr_ct);
        return curr_ct;
    } else if (root->type == ast_extern){
        // nothing to traverse
        return counter;
    } else if (root->type == ast_cnst){
        // nothing to traverse
        return counter;
    } else if (root->type == ast_rexpr){
        int curr_ct = counter;
        curr_ct = rename_vars(root->rexpr.lhs, curr_ct);
        curr_ct = rename_vars(root->rexpr.rhs, curr_ct);
        return curr_ct;
    } else if (root->type == ast_bexpr){
        int curr_ct = counter;
        curr_ct = rename_vars(root->bexpr.lhs, curr_ct);
        curr_ct = rename_vars(root->bexpr.rhs, curr_ct);
        return curr_ct;
    } else if (root->type == ast_uexpr){
        return rename_vars(root->uexpr.expr, counter);
    }
    return 0;
}

void remove_unreachable_blocks(LLVMBasicBlockRef startBB, set<LLVMBasicBlockRef> all_bb_set){
    // start from startBB, visit all reachable BBs using BFS
    // mark all visited BBs
    // remove all unmarked BBs
    set<LLVMBasicBlockRef> visited;
    vector<LLVMBasicBlockRef> queue;
    queue.push_back(startBB);
    visited.insert(startBB);
    while(!queue.empty()){
        LLVMBasicBlockRef currBB = queue.back();
        queue.pop_back();
        // get the terminator instruction of currBB
        LLVMValueRef term_inst = LLVMGetBasicBlockTerminator(currBB);
        if(term_inst == NULL){
            // unconditional branch
            LLVMBasicBlockRef nextBB = LLVMGetNextBasicBlock(currBB);
            if(visited.find(nextBB) == visited.end()){
                visited.insert(nextBB);
                queue.push_back(nextBB);
            }
        } else {
            // conditional branch
            for(int i = 0; i < LLVMGetNumSuccessors(term_inst); i++){
                LLVMBasicBlockRef nextBB = LLVMGetSuccessor(term_inst, i);
                if(visited.find(nextBB) == visited.end()){
                    visited.insert(nextBB);
                    queue.push_back(nextBB);
                }
            }
        }
    }
}