#ifndef IR_BUILDER_H
#define IR_BUILDER_H

// Include any necessary headers here
#include "ast.h"
#include <llvm-c/Core.h>

LLVMModuleRef generate_IR(astNode *root);

#endif // IR_BUILDER_H