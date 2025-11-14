RAYLIB = build/raylib/libraylib.a

build/bitwidgets: src/bitwidgets.c build $(RAYLIB)
	cc -Wall -Wextra -o build/bitwidgets src/bitwidgets.c -I include $(RAYLIB) -lm
 
$(RAYLIB): 
	cd build && cmake ../include/raylib && make -j24

build: 
	mkdir -p build

.PHONY: clean
clean:
	rm -rf build
