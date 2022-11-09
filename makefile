# Adapted from https://github.com/clemedon/Makefile_tutor/blob/main/README.md

EXE:=bluetooth

CFLAGS= -Og -g \
	-I/usr/include/glib-2.0 \
	-I/usr/lib/arm-linux-gnueabihf/glib-2.0/include \
	-pthread -I/usr/include/gio-unix-2.0 \
	-I/usr/include/libmount -I/usr/include/blkid \
	-I/usr/include/glib-2.0 \
	-I/usr/lib/arm-linux-gnueabihf/glib-2.0/include \
	-I./xml_codegen/gen

LIBS=-lglib-2.0 -lgio-2.0 -lgobject-2.0

OBJ_DIR=./build

SRCS:= \
	main.c \
	xml_codegen/gen/bluez_advertisement.c \
	xml_codegen/gen/bluez_characteristic.c \
	xml_codegen/gen/bluez_hci.c \
	xml_codegen/gen/bluez_service.c \

OBJS:= $(SRCS:%.c=$(OBJ_DIR)/%.o)

DIR_DUP=mkdir -p $(@D)

# DEFAULT TARGET -> Execute
all: $(EXE)

$(EXE): $(OBJS)
	cc $(OBJS) $(LIBS) -o $(EXE)
	$(info CREATED $(EXE))

$(OBJ_DIR)/%.o: %.c
	$(DIR_DUP)
	cc $(CFLAGS) -c -o $@ $<
	$(info CREATED $@)

.PHONY: clean

clean:
	rm -f $(EXE)
	rm -f $(OBJ_DIR)/*
	rmdir $(OBJ_DIR)