# Makefile for MGE Tournament Manager
# Alternative to CMake for simpler builds

CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
TARGET = mge_tournament

# Include paths
INCLUDES = -I/usr/include/nlohmann

# Library flags
LIBS = -lwebsockets -lcurl -lssl -lcrypto

# Source files
SOURCES = main.cpp tournament_manager.cpp
OBJECTS = $(SOURCES:.cpp=.o)
HEADERS = tournament_manager.hpp

# Default target
all: check_deps $(TARGET)

# Check for required dependencies
check_deps:
	@echo "Checking dependencies..."
	@pkg-config --exists libwebsockets || (echo "Error: libwebsockets not found. Install with: sudo apt install libwebsockets-dev" && exit 1)
	@pkg-config --exists libcurl || (echo "Error: libcurl not found. Install with: sudo apt install libcurl4-openssl-dev" && exit 1)
	@test -f /usr/include/nlohmann/json.hpp || (echo "Warning: nlohmann-json not found. Downloading..." && $(MAKE) install_json)
	@echo "All dependencies found!"

# Install nlohmann-json if missing
install_json:
	@echo "Downloading nlohmann-json..."
	@mkdir -p include/nlohmann
	@wget -q https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp -O include/nlohmann/json.hpp
	$(eval INCLUDES += -Iinclude)

# Build target
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)
	@echo "Build complete! Run with: ./$(TARGET) <tournament_url>"

# Compile source files
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)
	rm -rf include/

# Install to system
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	@echo "Installed to /usr/local/bin/$(TARGET)"

# Uninstall from system
uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

# Run the program (requires tournament URL argument)
run: $(TARGET)
	@if [ -z "$(TOURNAMENT)" ]; then \
		echo "Error: Please specify TOURNAMENT variable"; \
		echo "Example: make run TOURNAMENT=mge1"; \
		exit 1; \
	fi
	./$(TARGET) $(TOURNAMENT)

# Debug build
debug: CXXFLAGS += -g -DDEBUG
debug: clean $(TARGET)

# Help target
help:
	@echo "MGE Tournament Manager - Build Targets:"
	@echo ""
	@echo "  make              - Build the project"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make install      - Install to /usr/local/bin"
	@echo "  make uninstall    - Remove from /usr/local/bin"
	@echo "  make run TOURNAMENT=mge1 - Build and run"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make check_deps   - Check for required dependencies"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Dependencies:"
	@echo "  - libwebsockets-dev"
	@echo "  - libcurl4-openssl-dev"
	@echo "  - nlohmann-json (auto-downloaded if missing)"
	@echo ""
	@echo "Example usage:"
	@echo "  make"
	@echo "  ./mge_tournament mge1"

.PHONY: all clean install uninstall run debug help check_deps install_json