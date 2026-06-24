files=$(wildcard *.c)

all: $(files:.c=)

%: %.c
	$(CC) `pkg-config --cflags --libs glib-2.0` $(CFLAGS) -o $@ $^ -lX11 -lXext -L/usr/X11R6/lib -I/usr/X11R6/include
