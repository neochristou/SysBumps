CC = gcc
TARGET = tlbtest

CFLAGS = -g -O2

OBJS = main.o memory.o

all : $(TARGET)
	rm -f $(OBJS)

$(TARGET) : $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) -lpthread

.c.o :
	$(CC) $(CFLAGS) -c -o $@ $<

clean :
	rm -f $(OBJS) $(TARGET)
