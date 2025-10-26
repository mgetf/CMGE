#!/bin/bash
# Quick setup script for MGE Tournament Manager

set -e

echo "======================================"
echo "MGE Tournament Manager - Quick Setup"
echo "======================================"
echo ""

# Check if running in WSL/Linux
if [[ ! -f /etc/os-release ]]; then
    echo "Error: This script is for Linux/WSL only"
    exit 1
fi

# Update package list
echo "[1/6] Updating package list..."
sudo apt update

# Install dependencies
echo "[2/6] Installing dependencies..."
sudo apt install -y build-essential pkg-config libwebsockets-dev libcurl4-openssl-dev libssl-dev wget

# Check for CMake (optional)
if command -v cmake &> /dev/null; then
    USE_CMAKE=true
    echo "CMake found - will use CMake build"
else
    USE_CMAKE=false
    echo "CMake not found - will use Makefile build"
    echo "To install CMake: sudo apt install cmake"
fi

# Download nlohmann-json if not present
if [[ ! -f /usr/include/nlohmann/json.hpp ]]; then
    echo "[3/6] Downloading nlohmann-json..."
    mkdir -p include/nlohmann
    wget -q https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp -O include/nlohmann/json.hpp
    INCLUDES_FLAG="-Iinclude"
else
    echo "   make"
fi
echo ""
echo "4. Run the server:"
echo "   ./run_tournament.sh mge1"
echo ""
echo "Or run directly:"
echo "   $BIN_PATH mge1"
echo ""
echo "The server will start on ws://localhost:8080"
echo ""
echo "For more information, see SETUP_GUIDE.md"
echo "======================================"[3/6] nlohmann-json already installed"
    INCLUDES_FLAG=""
fi

# Create api_key.txt if it doesn't exist
if [[ ! -f api_key.txt ]]; then
    echo "[4/6] Creating api_key.txt..."
    echo "your_challonge_api_key_here" > api_key.txt
    echo "⚠️  WARNING: Please edit api_key.txt and add your Challonge API key!"
    echo "   Get your key from: https://challonge.com/settings/developer"
else
    echo "[4/6] api_key.txt already exists"
fi

# Build the project
echo "[5/6] Building project..."

if [[ "$USE_CMAKE" == true ]]; then
    mkdir -p build
    cd build
    cmake ..
    cmake --build .
    cd ..
    BIN_PATH="build/mge_tournament"
else
    make
    BIN_PATH="./mge_tournament"
fi

# Create run script
echo "[6/6] Creating run script..."
cat > run_tournament.sh << 'EOF'
#!/bin/bash

if [ $# -eq 0 ]; then
    echo "Usage: ./run_tournament.sh <tournament_url>"
    echo "Example: ./run_tournament.sh mge1"
    exit 1
fi

TOURNAMENT_URL=$1
EOF

if [[ "$USE_CMAKE" == true ]]; then
    echo "./build/mge_tournament \$TOURNAMENT_URL" >> run_tournament.sh
else
    echo "./mge_tournament \$TOURNAMENT_URL" >> run_tournament.sh
fi

chmod +x run_tournament.sh

echo ""
echo "======================================"
echo "✅ Setup Complete!"
echo "======================================"
echo ""
echo "Next steps:"
echo ""
echo "1. Edit api_key.txt with your Challonge API key:"
echo "   nano api_key.txt"
echo ""
echo "2. Update your Challonge username in main.cpp (line ~80):"
echo "   nano main.cpp"
echo "   Change: std::string challongeUser = \"tommylt3\";"
echo ""
echo "3. Rebuild after editing:"
if [[ "$USE_CMAKE" == true ]]; then
    echo "   cd build && cmake --build .. && cd .."
else
    echo "