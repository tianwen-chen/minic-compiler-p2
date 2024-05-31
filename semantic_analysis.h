#ifndef SEMANTIC_ANALYSIS_H
#define SEMANTIC_ANALYSIS_H

#include <stdio.h>
#include "ast.h"
#include <stack>
#include <algorithm>
#include <cstring>

int semantic_analysis(astNode *root);

#endif // SEMANTIC_ANALYSIS_H