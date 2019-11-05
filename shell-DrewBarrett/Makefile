SRCS = simple_shell.c
OBJS = $(SRCS:.c=.o)
BIN = simple_shell
CFLAGS = -g -Wall
LDFLAGS = -g -Wall


# You shouldn't need to touch anything below this line...
all: $(BIN)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

$(BIN): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	-rm -f $(OBJS)
	-rm -f $(BIN)
