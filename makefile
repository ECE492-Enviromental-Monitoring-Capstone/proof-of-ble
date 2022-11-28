# Adapted from https://github.com/clemedon/Makefile_tutor/blob/main/README.md

EXE:=bluetooth


CFLAGS= -Og -g\
	`pkg-config --cflags glib-2.0 gio-unix-2.0` \
	-I./xml_codegen/gen

LIBS=`pkg-config --libs glib-2.0 gio-unix-2.0`

OBJ_DIR=./build

SRCS:= \
	main.c \
	xml_codegen/gen/bluez_advertisement.c \
	xml_codegen/gen/bluez_characteristic.c \
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
	rm -rf $(OBJ_DIR)/*
	rmdir $(OBJ_DIR)