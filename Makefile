CPPFLAGS := -Wall -Wextra -O3 -Iinclude
LDFLAGS  := -static-libgcc -static-libstdc++ -lopengl32 -mwindows

BUILD_DIR  := build
SOURCE_DIR := src

OBJ := $(BUILD_DIR)/common.o $(BUILD_DIR)/Logger.o \
	$(BUILD_DIR)/OpenGL.o $(BUILD_DIR)/GLWindow.o \
	$(BUILD_DIR)/Texture.o $(BUILD_DIR)/main.o

BIN := lesson03.exe

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	g++ -c $(CPPFLAGS) $< -o $@

all: $(BIN)

$(BIN): $(OBJ)
	g++ -o $(BIN) $(OBJ) $(LDFLAGS)

clean:
	rm -f $(OBJ) $(BIN)
