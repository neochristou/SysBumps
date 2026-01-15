CC = gcc
TARGET = test-user

VPATH = $(CURDIR) $(COMDIR)

COMDIR = ../common

CFLAGS = -g -w -I./ -I$(COMDIR)

#SRCS = $(foreach dir, ./ $(COMDIR), $(wildcard $(dir)/*.c))
#SRCS := $(notdir $(SRCS))
#OBJS = $(SRCS:.c=.o)
OBJS = main.o memory.o timing.o

all : $(TARGET)
	rm -f $(OBJS)
	
$(TARGET) : $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) -lpthread

.c.o :
	$(CC) $(CFLAGS) -c -o $@ $< 

clean : 
	rm -f $(OBJS) $(TARGET)


