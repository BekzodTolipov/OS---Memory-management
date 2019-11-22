CC=gcc
MASTER_CFILE = oss.c
CHILD_CFILE = user.c

MASTER_OBJ=$(MASTER_CFILE:.c=.o)
CHILD_OBJ=$(CHILD_CFILE:.c=.o)

MASTER_EXE = oss
CHILD_EXE = user

CFLAGS = -g -Wall
MATH = -lm

HEADER_FILE = shared_mem.h

SRC = global_constants.h shared.h
SHARE_OBJ = message_queue.o queue.o shared_memory.o
SHARE_HEAD = message_queue.h queue.h shared_memory.h

%.o: %.c $(SRC) $(SHARE_HEAD)
	$(CC) -c -o $@ $< $(CFLAGS)

all: $(MASTER_EXE) $(CHILD_EXE)

$(MASTER_EXE): $(MASTER_OBJ) $(SHARE_OBJ)
	gcc -o $(MASTER_EXE) $^ $(CFLAGS)
	
$(CHILD_EXE): $(CHILD_OBJ) $(SHARE_OBJ)
	gcc -o $(CHILD_EXE) $^ $(CFLAGS)

clean:
	rm $(MASTER_EXE) $(MASTER_OBJ) $(CHILD_EXE) $(CHILD_OBJ) $(SHARE_OBJ)
