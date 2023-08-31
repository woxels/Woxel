all:
	mkdir -p release
	gcc main.c -Ofast -lm -lz -lSDL2 -lGLESv2 -lEGL -o release/wox
	strip --strip-unneeded release/wox
	upx --lzma --best release/wox

test:
	gcc main.c -Ofast -lm -lz -lSDL2 -lGLESv2 -lEGL -o wox_test
	./wox_test
	rm wox_test

clean:
	rm -r release
