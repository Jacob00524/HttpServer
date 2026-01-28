CC = gcc
MODE ?= release

TARGET = libhttp.so
EXAMPLE_TARGET = example.out
BUILD_FOLDER = build

CPPFLAGS = -Iexternal/cJSON
LDFLAGS = -shared -Wl,-soname,$(TARGET)
DEBUG_CFLAGS   = -g3 -O0 -Wall -Wextra -fPIC -fsanitize=address,undefined
RELEASE_CFLAGS = -O4 -Wall -Wextra -fPIC

CJSON_SO = external/cJSON/libcjson.so

ifeq ($(MODE),debug)
  CFLAGS = $(DEBUG_CFLAGS)
else
  CFLAGS = $(RELEASE_CFLAGS)
endif

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c, $(BUILD_FOLDER)/%.o, $(SRC))

default: $(CJSON_SO) $(TARGET) $(EXAMPLE_TARGET)

$(TARGET): $(OBJ) $(OBJ_ALGO)
	$(CC) $(OBJ) -o $(BUILD_FOLDER)/$@ $(LDFLAGS) \
	-L. -lcjson -lssl -lcrypto \
	-Wl,-rpath,'$$ORIGIN'
	cp $(BUILD_FOLDER)/$(TARGET) .

$(CJSON_SO):
	git submodule update --init --recursive
	$(MAKE) -C external/cJSON
	cp -L $(CJSON_SO) .

$(BUILD_FOLDER)/%.o: src/%.c | $(BUILD_FOLDER)
	$(CC) $(CFLAGS) $(CPPFLAGS) -Iinclude -c $< -o $@

$(BUILD_FOLDER):
	mkdir -p $@

$(EXAMPLE_TARGET): example.c $(TARGET)
	$(CC) -g3 -O0 -Wall -Wextra -fsanitize=address,undefined \
	-Iinclude $< -o $@ \
	-L. -lhttp \
	-Wl,-rpath,\$$ORIGIN

cert:
	openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365 -nodes

san_cert:
	openssl req -x509 -nodes -newkey rsa:2048 \
	-keyout key.pem -out cert.pem \
	-days 365 \
	-subj "/CN=localhost" \
	-addext "subjectAltName=DNS:localhost,IP:127.0.0.1"

clean:
	$(MAKE) -C external/cJSON clean
	rm -rf $(BUILD_FOLDER)
	rm -f libcjson.so
	rm -f $(EXAMPLE_TARGET)
