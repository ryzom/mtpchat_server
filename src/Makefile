
# Mtp Makefile

all: mtp

clean:
	$(RM) *.o *~

# General

CC      = gcc
CFLAGS  = -pipe
LDFLAGS = -lm -lcrypt

# Warnings

# CFLAGS  += -pedantic -Wall

# Optimize

CFLAGS  += -O2 -fomit-frame-pointer
LDFLAGS += -s

# Debug

# CFLAGS  += -g

# Solaris

# LDFLAGS += -lnsl -lsocket

# Dependencies

OBJS= channel.o command.o crypt.o database.o group.o list.o mtp.o server.o \
      socket.o token.o user.o variable.o

mtp: $(OBJS)
	$(CC) -o mtp $(OBJS) $(LDFLAGS)

channel.o: channel.c channel.h object.h list.h user.h database.h \
           socket.h types.h group.h mtp.h token.h variable.h

command.o: command.c command.h user.h object.h database.h list.h \
           socket.h types.h channel.h crypt.h group.h mtp.h server.h token.h \
           variable.h

crypt.o: crypt.c crypt.h types.h command.h user.h object.h database.h \
         list.h socket.h server.h

database.o: database.c database.h types.h channel.h object.h list.h \
            user.h socket.h group.h mtp.h server.h token.h variable.h

group.o: group.c group.h object.h list.h types.h mtp.h server.h user.h \
         database.h socket.h token.h variable.h

list.o: list.c list.h object.h types.h mtp.h server.h user.h \
        database.h socket.h token.h variable.h

mtp.o: mtp.c mtp.h types.h

server.o: server.c server.h user.h object.h database.h list.h socket.h \
          types.h channel.h command.h group.h mtp.h token.h variable.h

socket.o: socket.c socket.h types.h mtp.h

token.o: token.c token.h object.h types.h server.h user.h database.h \
         list.h socket.h

user.o: user.c user.h object.h database.h list.h socket.h types.h \
        channel.h command.h crypt.h group.h mtp.h token.h variable.h

variable.o: variable.c variable.h types.h mtp.h token.h object.h
