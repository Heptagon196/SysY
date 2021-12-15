default:
	bison -d lrparser.y -Wcounterexamples
	flex lrlex.l
	gcc lex.yy.c lrparser.tab.c ast.c main.c genllvm.c -o parser -g
clean:
	rm lex.yy.c lrparser.tab.c lrparser.tab.h ir.* generated_ast.txt ir
compile-test:
	./parser < test/test.sy > ir.ll
	clang test/print.ll ir.ll -o ir
