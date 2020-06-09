OBJ = main.o ../functions.o

all: fryzjer_war

fryzjer_war: $(OBJ)
	gcc $(OBJ) -o fryzjer_war -pthread

$(OBJ): ../functions.h

.PHONY: clean
clean:
	rm -f *.o


