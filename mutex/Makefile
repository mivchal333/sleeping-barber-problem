OBJ = main.o ../functions.o

all: mutex

mutex: $(OBJ)
	gcc $(OBJ) -o mutex -pthread

$(OBJ): ../functions.h

.PHONY: clean
clean:
	rm -f *.o


