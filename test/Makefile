all: test test_x11

test: test.c
	gcc -Wall -o $@ $^ -lEGL -lGLESv2

test_x11: test.c
	gcc -Wall -DUSE_X11 -o $@ $^ -lEGL -lGLESv2 -lX11

clean:
	rm -f test test_x11
