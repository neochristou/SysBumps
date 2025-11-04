CC = clang
TARGET = attacker

VPATH = $(CURDIR) $(COMDIR)

COMDIR = ../common

CFLAGS = -g -w -I./ -I$(COMDIR)

OBJS = main.o memory.o

all : $(TARGET)
	rm -f $(OBJS)
	
$(TARGET) : $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) -lpthread

.c.o :
	$(CC) $(CFLAGS) -c -o $@ $< 

clean : 
	rm -f $(OBJS) $(TARGET)


