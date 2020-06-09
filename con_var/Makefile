OBJ = main.o ../functions.o

all: con_var

con_var: $(OBJ)
	gcc $(OBJ) -o con_var -pthread

$(OBJ): ../functions.h

.PHONY: clean
clean:
	rm -f *.o


