CFLAGS=-Wall -g
LFLAGS=`pkg-config fuse --cflags --libs`

%: %.c
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

vmufs:
