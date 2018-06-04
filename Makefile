SRCS = main.c
OBJS = $(SRCS:.c=.o)
CFLAGS = -O2 -std=gnu99 `pkg-config --cflags gtk+-3.0`
LDFLAGS = -lcurl `pkg-config --libs gtk+-3.0`

all: $(OBJS)
	gcc -o faceappgtk $(OBJS) $(CFLAGS) $(LDFLAGS)

.c.o:
	gcc $(CFLAGS) -c $< -o $@
