#include <stdio.h>
#include <stdlib.h>
#include "ir_builder.h"
#include "y.tab.h"
#include "ast.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include "semantic_analysis.h"

extern astNode *root;
extern FILE *yyin;

int main(int argc, char **argv) {
    // define extern FILE *yyin
    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    yyin = fopen(argv[1], "r");
    int p = yyparse();
    // print out root
    // printNode(root);
    // do semantic analysis
    int semantic_res = semantic_analysis(root);
    if (semantic_res != 0){
        return 1;
    }
    // generate IR
    LLVMModuleRef module = generate_IR(root);

    // TODO - yydestory()
    // print out IR
    printf("Generated IR:\n");
    LLVMDumpModule(module);
    printf("\n");

    fclose(yyin);
}