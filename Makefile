CC = clang
CFLAGS = -Wall -Wextra -g $(shell llvm-config --cflags)
LDFLAGS = $(shell llvm-config --ldflags --libs core executionengine mcjit interpreter native --system-libs)

TARGET = main
SOURCE = main.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) *.ll *.o

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run