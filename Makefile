CC      := clang
CFLAGS  := -O2 -g -Wall -Wextra
CFLAGS += -Iinclude
CFLAGS  += $(shell pkg-config --cflags libbpf)
LDFLAGS := $(shell pkg-config --libs libbpf)

SRCS := \
  src/main.c \
  src/umem.c \
  src/rx.c \
  src/tx.c \
  src/util.c \
  src/config.c

OBJS := $(SRCS:.c=.o)

TARGET := afxdp-userapp

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
