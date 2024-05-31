MAIN = main
PARSER = minic
BUILDER = ir_builder

$(MAIN): $(MAIN).c $(BUILDER).cpp $(PARSER).y $(PARSER).l ast.c ast.h semantic_analysis.c semantic_analysis.h
	g++ -g -I /usr/include/llvm-c-15/ -c $(MAIN).c
	g++ -g -I /usr/include/llvm-c-15/ -c $(BUILDER).cpp
	g++ -g -c semantic_analysis.c
	g++ -g -c ast.c
	yacc -d -t $(PARSER).y
	lex $(PARSER).l
	g++ -g -c lex.yy.c -o lex.o
	g++ -g -c y.tab.c -o yacc.o
	g++ $(MAIN).o $(BUILDER).o ast.o lex.o yacc.o semantic_analysis.o `llvm-config-15 --cxxflags --ldflags --libs core` -I /usr/include/llvm-c-15/ -o $@

clean:
	rm -f $(PARSER).out $(PARSER).tab.c $(PARSER).tab.h lex.yy.c $(MAIN) $(MAIN).o $(BUILDER).o ast.o lex.o yacc.o