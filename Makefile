PROGRAM = hp48emu
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
OPTIM = g
DEBUG = -DDEBUG
CFLAGS = -Werror -Wall
CFLAGS += -O$(OPTIM) -g
CFLAGS += $(DEBUG)
CC = gcc

HOST_ARCH=native
HOST_CPU=native
CFLAGS+=-march=$(HOST_ARCH) -mtune=$(HOST_CPU)

SUBDIRS :=
TOPTARGETS := all clean

all: $(PROGRAM)

$(TOPTARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

$(PROGRAM): $(OBJS)
	$(LINK.o) $(LDFLAGS) -o $(PROGRAM) $(OBJS) $(LDLIBS)

.c: *.h
.c.o:
	$(CC) $(CFLAGS) -c $<

mrproper: clean $(SUBDIRS)
	rm -f $(DEPS)

clean: $(SUBDIRS)
	rm -f $(OBJS) $(PROGRAM)

DEPS = $(SRCS:.c=.d)
include $(DEPS)

%.d : %.c
	$(CC) $(CCFLAGS) -MF"$@" -MG -MM -MP -MT"$@" -MT"$(<:.c=.o)" "$<"

# -MF  write the generated dependency rule to a file
# -MG  assume missing headers will be generated and don't stop with an error
# -MM  generate dependency rule for prerequisite, skipping system headers
# -MP  add phony target for each header to prevent errors when header is missing
# -MT  add a target to the generated dependency

.PHONY: $(TOPTARGETS) $(SUBDIRS)
