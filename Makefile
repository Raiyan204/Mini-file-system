CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
TARGET = unmad_fs
SRC = unmad_fs.c
DISK = disk.img

.PHONY: all run test hex clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	bash run_tests.sh

hex:
	@if [ -f $(DISK) ]; then \
		xxd $(DISK) | head -n 10; \
	else \
		echo "Error: $(DISK) not found. Run 'make run' or 'make test' first."; \
	fi

clean:
	rm -f $(TARGET) $(DISK)
