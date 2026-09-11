CC = gcc
AR = ar

MODE ?= release

TARGET = libhttp.a
EXAMPLE_TARGET = example.out
BUILD_FOLDER = build

CPPFLAGS = -Iinclude -Iexternal/cJSON

DEBUG_CFLAGS   = -g3 -O0 -Wall -Wextra -fsanitize=address,undefined
RELEASE_CFLAGS = -O3 -Wall -Wextra

CJSON_LIB = external/cJSON/libcjson.a

LDLIBS = $(CJSON_LIB) -lssl -lcrypto

ifeq ($(MODE),debug)
	CFLAGS = $(DEBUG_CFLAGS)
else
	CFLAGS = $(RELEASE_CFLAGS)
endif

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,$(BUILD_FOLDER)/%.o,$(SRC))


.PHONY: default clean cert san_cert

default: $(TARGET) $(EXAMPLE_TARGET)


$(TARGET): $(OBJ)
	$(AR) rcs $(BUILD_FOLDER)/$@ $(OBJ)
	cp $(BUILD_FOLDER)/$@ $@


$(CJSON_LIB):
	git submodule update --init --recursive
	$(MAKE) -C external/cJSON


$(BUILD_FOLDER)/%.o: src/%.c | $(BUILD_FOLDER)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@


$(BUILD_FOLDER):
	mkdir -p $@


$(EXAMPLE_TARGET): example.c $(TARGET) $(CJSON_LIB)
	$(CC) $(DEBUG_CFLAGS) $(CPPFLAGS) \
		example.c \
		$(TARGET) \
		$(LDLIBS) \
		-o $@


cert:
	openssl req -x509 -newkey rsa:2048 \
		-keyout key.pem \
		-out cert.pem \
		-days 365 \
		-nodes


san_cert:
	openssl req -x509 -nodes -newkey rsa:2048 \
		-keyout key.pem \
		-out cert.pem \
		-days 365 \
		-subj "/CN=localhost" \
		-addext "subjectAltName=DNS:localhost,IP:127.0.0.1"


clean:
	$(MAKE) -C external/cJSON clean
	rm -rf $(BUILD_FOLDER)
	rm -f $(TARGET)
	rm -f $(EXAMPLE_TARGET)