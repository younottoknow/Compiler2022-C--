TARGET = parser
OBJECT = parser.tab.c parser.tab.o lex.yy.c alloc.o functions.o semanticAnalysis.o symbolTable.o genCode.o preprocess.o
OUTPUT = parser.output parser.tab.h
CC = gcc -g -Wall
LEX = flex
YACC = bison -v
YACCFLAG = -d
LIBS = -lfl

parser: parser.tab.o alloc.o functions.o symbolTable.o semanticAnalysis.o genCode.o preprocess.o
	$(CC) -o $(TARGET) parser.tab.o alloc.o functions.o symbolTable.o semanticAnalysis.o genCode.o preprocess.o $(LIBS)

parser.tab.o: parser.tab.c lex.yy.c alloc.o functions.c symbolTable.o semanticAnalysis.o
	$(CC) -c parser.tab.c

semanticAnalysis.o: semanticAnalysis.c header.h symbolTable.h
	$(CC) -c semanticAnalysis.c

symbolTable.o: symbolTable.c symbolTable.h
	$(CC) -c symbolTable.c

lex.yy.c: lexer3.l
	$(LEX) lexer3.l

parser.tab.c: parser.y
	$(YACC) $(YACCFLAG) parser.y

alloc.o: alloc.c header.h
	$(CC) -c alloc.c
	
functions.o: functions.c header.h
	$(CC) -c functions.c

preprocess.o: preprocess.c header.h symbolTable.h
	$(CC) -c preprocess.c

genCode.o: genCode.c genCode.h header.h symbolTable.h
	$(CC) -c genCode.c

clean:
	rm -f $(TARGET) $(OBJECT) $(OUTPUT)

