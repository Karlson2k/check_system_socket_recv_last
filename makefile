CC ?= cc

all: server_part client_part

server_part: server_part.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) server_part.c $(LDLIBS) $(LIBS) -o server_part

client_part: client_part.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) client_part.c $(LDLIBS) $(LIBS) -o client_part

clean:
	rm -f server_part
	rm -f client_part

.PHONY: all clean
