all:
	gcc ../main.c -Ofast -lm -lz -lSDL2 -lGLESv2 -lEGL -o woxel

install:
	cp woxel $(DESTDIR)

uninstall:
	rm $(DESTDIR)/woxel

clean:
	rm woxel
