EXE=a.out
CFLAGS=-I/usr/include/glib-2.0 \
	-I/usr/lib/arm-linux-gnueabihf/glib-2.0/include \
	-pthread -I/usr/include/gio-unix-2.0 \
	-I/usr/include/libmount -I/usr/include/blkid \
	-I/usr/include/glib-2.0 \
	-I/usr/lib/arm-linux-gnueabihf/glib-2.0/include

LIBS=-lglib-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0

${EXE}: main.c bluez_gatt.o bluez_gatt.h
	cc main.c bluez_gatt.o $(CFLAGS) $(LIBS) -o ${EXE}

bluez_gatt.o: bluez_gatt.h
	cc -c bluez_gatt.c $(CFLAGS) $(LIBS)

.PHONY: run clean

run: ${EXE}
	./${EXE}

clean:
	rm -rf ${EXE}