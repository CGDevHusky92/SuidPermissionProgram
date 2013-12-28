.SUFFIXES: .c .o
ASSIGNMENT=sume
CC=gcc
CHM=chmod
PER_ONE=711
PER_TWO=u+s
EXEC=sume
CFLAGS= -Wall -g
OBJS=$(SRC:.c=.o)
SRC=sume.c
VAL=valgrind
VALFLAGS=-v --track-origins=yes --leak-check=full
ARGS=/home/campus24/crgorect/ 1

all: $(SRC) $(EXEC)
	
$(EXEC): $(OBJS) 
	$(CC) $(CFLAGS) $(OBJS) -o $@

set:
	make
	$(CHM) $(PER_ONE) $(EXEC)
	$(CHM) $(PER_TWO) $(EXEC)

test:
	clear
	make
	$(VAL) $(VALFLAGS) ./$(EXEC) $(ARGS)

clean:
	rm -f ./*~
	rm -f ./$(EXEC) 

prepare:
	rm -f ./$(ASSIGNMENT).tgz
	gtar -zcvf $(ASSIGNMENT).tgz Makefile README $(SRC)

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@