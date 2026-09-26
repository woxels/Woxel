.PHONY: all test cean

all:
	cc main.c -Ofast -lm -lz -lSDL2 -lGLESv2 -lEGL -o wox
	strip --strip-unneeded wox
	upx --lzma --best wox

test:
	cc main.c -Ofast -lm -lz -lSDL2 -lGLESv2 -lEGL -o wox_test
	./wox_test
	rm wox_test

clean:
	rm wox
