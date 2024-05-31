#include <stdio.h>
#include "ast.h"
#include <stack>
#include <algorithm>
#include <cstring>

void traverse(astNode *root, bool isFuncBody = false);
void printSymbolTable(vector<char*>* sym_table);
void printSymbolTableStack(stack<vector<char*>*> sym_table_stack);

// global var
stack<vector<char*>*> tableStack;
bool semanticError = false;

int semantic_analysis(astNode *root){
	
	#ifdef YYDEBUG
  	yydebug = 0;
	#endif 
	
    int res = 0;

	if (root != NULL) {
		// printNode(root);
		traverse(root, false);
        if (semanticError){
            printf("Semantic analysis failed\n");
            res = 1;
        } else {
            printf("Semantic analysis passed\n");
            res = 0;
        }
	}
	
	// yylex_destroy();
	
	return res;
}

void traverse(astNode *root, bool isFuncBody){
    if (root == NULL){
        return;
    }
    if (root->type == ast_stmt) {
        if (root->stmt.type == ast_block){
            if (isFuncBody){
                // visit all nodes in the statement list of block statement
                for(int i = 0; i < root->stmt.block.stmt_list->size(); i++){
                    traverse(root->stmt.block.stmt_list->at(i), false);
                }
                return;
            } else {
                // create a new symbol table curr_sym_table and push it to the symbol table stack
                vector<char*> *curr_sym_table = new vector<char*>();
                tableStack.push(curr_sym_table);
                // visit all nodes in the statement list of block statement
                for(int i = 0; i < root->stmt.block.stmt_list->size(); i++){
                    traverse(root->stmt.block.stmt_list->at(i), false);
                }
                // free the symbol table at the top of the stack
                curr_sym_table = tableStack.top();
                // pop top of the stack
                tableStack.pop();
                delete curr_sym_table;
                return;
            }
        } else if (root->stmt.type == ast_decl){
            // loop through all elements in the symbol table at the top of the stack, check if the variable name is already declared
            for(std::vector<char*>::iterator it = tableStack.top()->begin(); it != tableStack.top()->end(); ++it){
                // printf("Comparing %s with %s\n", *it, root->stmt.decl.name);
                if (strcmp(*it, root->stmt.decl.name) == 0){
                    // emit an error message
                    printf("Error: Variable %s already declared\n", root->stmt.decl.name);
                    semanticError = true;
                    return;
                }
            }

            // add the variable to the symbol table at the top of the stack
            tableStack.top()->push_back(root->stmt.decl.name);
            return;
        }
    } else if (root->type == ast_func) {
        // push a symbol table to stack
        vector<char*> *new_table = new vector<char*>();
        tableStack.push(new_table);
        // add parameter to symbol table
        if (root->func.param != NULL){
            tableStack.top()->push_back(root->func.param->var.name);
        }
        // visit body node
        traverse(root->func.body, true);
        // free the symbol table at the top of the stack
        new_table = tableStack.top();
        // pop top of the stack
        tableStack.pop();
        delete new_table;       

        return;
    } else if (root->type == ast_var) {
        // check if it appears in one of the symbol tables on the stack
        bool found = false;
        std::stack<std::vector<char*>*> tempStack;
        while (!tableStack.empty()) {
            std::vector<char*>* top = tableStack.top();
            tableStack.pop();
            tempStack.push(top);
            // loop over every element in top, print them out and compare it with the variable name
            for(std::vector<char*>::iterator it = top->begin(); it != top->end(); ++it){
                // printf("Comparing %s with %s\n", *it, root->var.name);
                if (strcmp(*it, root->var.name) == 0){
                    found = true;
                    break;
                }
            }

        }
        // restore the original stack
        while (!tempStack.empty()) {
            tableStack.push(tempStack.top());
            tempStack.pop();
        }
        if (!found){
            // emit an error message with name of the variable
            printf("Error: Variable %s not declared\n", root->var.name);
            semanticError = true;
            return;
        }
 
        return;

    } else if (root->type == ast_extern) {
        // ignore the extern nodes
        return;
    }

    // all other cases, visit children
    switch(root->type){
        case ast_stmt:
            switch(root->stmt.type){
                case ast_asgn:
                    traverse(root->stmt.asgn.lhs, false);
                    traverse(root->stmt.asgn.rhs, false);
                    return;
                case ast_call:
                    if (root->stmt.call.param != NULL){
                        traverse(root->stmt.call.param, false);
                    }
                    return;
                case ast_ret:
                    traverse(root->stmt.ret.expr, false);
                    return;
                case ast_if:
                    traverse(root->stmt.ifn.cond, false);
                    traverse(root->stmt.ifn.if_body, false);
                    if (root->stmt.ifn.else_body != NULL){
                        traverse(root->stmt.ifn.else_body, false);
                    }
                    return;
                case ast_while:
                    traverse(root->stmt.whilen.cond, false);
                    traverse(root->stmt.whilen.body, false);
                    return;
            }
        
        case ast_prog:
            traverse(root->prog.ext1, false);
            traverse(root->prog.ext2, false);
            traverse(root->prog.func, false);
            return;
        
        case ast_cnst:
            return;
        
        case ast_rexpr:
            traverse(root->rexpr.lhs, false);
            traverse(root->rexpr.rhs, false);
            return;
        
        case ast_bexpr:
            traverse(root->bexpr.lhs, false);
            traverse(root->bexpr.rhs, false);
            return;
        
        case ast_uexpr:
            traverse(root->uexpr.expr, false);
            return;
    }

}

void printSymbolTable(vector<char*>* sym_table){
    for (int i = 0; i < sym_table->size(); i++){
        printf("%s\n", sym_table->at(i));
    }
}

void printSymbolTableStack(stack<vector<char*>*> sym_table_stack){
    while (!sym_table_stack.empty()){
        printSymbolTable(sym_table_stack.top());
        sym_table_stack.pop();
        printf("----------\n");
    }
}