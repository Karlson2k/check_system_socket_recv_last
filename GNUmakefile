uname_S := $(shell uname -s 2>/dev/null || echo NotAvl)

ifeq ($(uname_S),NotAvl)
  ifneq (,$(findstring MINGW,$(MSYSTEM)))
    uname_S := $(MSYSTEM)
  endif
endif

ifneq (,$(findstring MINGW,$(uname_S)))
  ifeq (cc,$(CC))
    ifneq (,$(findstring msys,$(shell $(CC) -dumpmachine 2>/dev/null || true)))
	  CC := gcc
	endif
  endif
  LDLIBS = -lws2_32
  EXEEXT := .exe
else
  EXEEXT :=
endif

all: server_part$(EXEEXT) client_part$(EXEEXT)

server_part$(EXEEXT): server_part.c

client_part$(EXEEXT): client_part.c

clean:
	rm -f server_part$(EXEEXT)
	rm -f client_part$(EXEEXT)

ifneq (,$(EXEEXT))
%$(EXEEXT) : %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(LOADLIBES) $(LDLIBS) -o $@
endif

.PHONY: all clean

