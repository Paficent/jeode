CC := i686-w64-mingw32-gcc

CFLAGS  := -Wall -Wextra -Os -std=c11 -DWIN32_LEAN_AND_MEAN
LDFLAGS := -shared -static -static-libgcc -Wl,--strip-all

TARGET := native_example.dll

$(TARGET): main.c jeode_native.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ main.c

clean:
	rm -f $(TARGET)

.PHONY: clean
