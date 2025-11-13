RAYLIB := include/raylib/build/raylib/libraylib.a

build/bitwidgets: src/bitwidgets.c build $(RAYLIB)
	cc -Wall -Wextra -o build/bitwidgets src/bitwidgets.c -I include $(RAYLIB) -lm

$(RAYLIB):
	cd include/raylib && mkdir build && cd build && cmake .. && make -j4

build: 
	mkdir -p build

.PHONY: clean
clean:
	rm -rf build