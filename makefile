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
	$(CC) $(CFLAGS) -shared -o $@ $^

$(target_folder)/%.o: src/%.c
	$(CC) $(CFLAGS) -c -fPIC $< -o $@

$(target_folder)/%.o: external/cJSON/%.c
	$(CC) $(CFLAGS) -c -fPIC $< -o $@

$(target_folder):
	mkdir -p $(target_folder)

example:
	$(CC) $(CFLAGS) example.c -L. -Wl,-rpath=. -lhttp -o $(example_target)

clean:
	rm -rf $(target_folder) $(target)
	rm -f $(example_target)