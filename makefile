CC = gcc
CFLAGS = -O0 -g3 -Iinclude -I./external/cJSON

target_folder = build
target = libhttp.so

example_target = example.out

SRC := $(wildcard src/*.c) 
OBJ := $(patsubst src/%.c,$(target_folder)/%.o,$(SRC))
SRC += ./external/cJSON/cJSON.c ./external/cJSON/cJSON_Utils.c
OBJ += $(target_folder)/cJSON.o $(target_folder)/cJSON_Utils.o

default: $(target_folder) $(target) example

$(target): $(OBJ)
	$(CC) $(CFLAGS) -shared -o $@ $^ -lssl -lcrypto

$(target_folder)/%.o: src/%.c
	$(CC) $(CFLAGS) -c -fPIC $< -o $@

$(target_folder)/%.o: external/cJSON/%.c
	$(CC) $(CFLAGS) -c -fPIC $< -o $@

$(target_folder):
	mkdir -p $(target_folder)

example:
	$(CC) $(CFLAGS) example.c -L. -Wl,-rpath=. -lhttp -o $(example_target)

cert:
	openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365 -nodes

san_cert:
	openssl req -x509 -nodes -newkey rsa:2048 \
	-keyout key.pem -out cert.pem \
	-days 365 \
	-subj "/CN=localhost" \
	-addext "subjectAltName=DNS:localhost,IP:127.0.0.1"


clean:
	rm -rf $(target_folder) $(target)
	rm -f $(example_target)