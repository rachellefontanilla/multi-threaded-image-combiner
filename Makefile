
# Makefile, ECE252  
# Rachelle Fontanilla

CC = gcc
CFLAGS = -Wall -g -std=c99 
LD = gcc
LDFLAGS = -g -std=c99
LDLIBS = -pthread -lcurl -lz

OBJ_DIR = obj
SRC_DIR = lib
LIB_UTIL = $(OBJ_DIR)/zutil.o $(OBJ_DIR)/crc.o $(OBJ_DIR)/lab_png.o
SRCS   = png-combiner.c crc.c zutil.c lab_png.c
OBJS   = $(OBJ_DIR)/png-combiner.o $(LIB_UTIL) 
TARGETS= png-combiner

all: ${TARGETS}

png-combiner: $(OBJS)
	mkdir -p $(OBJ_DIR)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

$(OBJ_DIR)/png-combiner.o: png-combiner.c 
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c 
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *~ *.d *.o all.png $(TARGETS) 
