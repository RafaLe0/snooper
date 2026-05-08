CC     = gcc
CFLAGS = -Wall -Wextra -g
TARGET = memscan
SRC    = memscan.c
IMAGE  = usb.img
WL     = wordlist.txt

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

image: make_test_image.py
	python3 make_test_image.py

run: $(TARGET) image
	./$(TARGET) $(IMAGE) $(WL)

clean:
	rm -f $(TARGET) $(IMAGE)

.PHONY: all image run clean
