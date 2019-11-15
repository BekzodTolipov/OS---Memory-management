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

SHARE_OBJ = message_queue.o shared.o queue.o
SHARE_HEAD = global_constants.h message_queue.h shared.h queue.h


%.o: %.c $(SHARE_HEAD)
	$(CC) -c -o $@ $< $(CFLAGS)

all: $(MASTER_EXE) $(CHILD_EXE)

$(MASTER_EXE): $(MASTER_OBJ) $(SHARE_OBJ)
	gcc -o $(MASTER_EXE) $^ $(CFLAGS)
	
$(CHILD_EXE): $(CHILD_OBJ) $(SHARE_OBJ)
	gcc -o $(CHILD_EXE) $^ $(CFLAGS)

clean:
	rm $(MASTER_EXE) $(MASTER_OBJ) $(CHILD_EXE) $(CHILD_OBJ) $(SHARE_OBJ)
