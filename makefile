CC = gcc
CFLAGS = -O0 -g3 -Iinclude -I./external/cJSON

target_folder = build
target = http.out

SRC := $(wildcard src/*.c) 
OBJ := $(patsubst src/%.c,$(target_folder)/%.o,$(SRC))
SRC += ./external/cJSON/cJSON.c ./external/cJSON/cJSON_Utils.c
OBJ += $(target_folder)/cJSON.o $(target_folder)/cJSON_Utils.o

default: $(target_folder) $(target)

$(target): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(target_folder)/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(target_folder)/%.o: external/cJSON/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(target_folder):
	mkdir -p $(target_folder)

clean:
	rm -rf $(target_folder) $(target)