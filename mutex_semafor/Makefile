OBJ = main.o ../functions.o

all: fryzjer_sem

fryzjer_sem: $(OBJ)
	gcc $(OBJ) -o fryzjer_sem -pthread

$(OBJ): ../functions.h

.PHONY: clean
clean:
	rm -f *.o


