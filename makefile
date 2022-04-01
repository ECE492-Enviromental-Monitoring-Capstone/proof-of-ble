EXE=a.out
CFLAGS=-I/usr/include/glib-2.0 \
	-I/usr/lib/arm-linux-gnueabihf/glib-2.0/include
LIBS=-lglib-2.0

${EXE}: main.c
	cc main.c $(CFLAGS) $(LIBS) -o ${EXE}

.PHONY: run clean

run: ${EXE}
	./${EXE}

clean:
	rm -rf ${EXE}