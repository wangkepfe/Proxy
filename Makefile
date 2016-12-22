IDIR =./
CC=gcc
CFLAGS=-I$(IDIR) -g -Wall -Wextra -lpthread

ODIR=obj
LDIR =../lib

_DEPS = serverside.h http.h util.h util_socket.h proxy_clientside.h midlayer.h proxy.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = main.o serverside.o http.o util.o util_socket.o proxy_clientside.o midlayer.o proxy.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

proxy: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ 
