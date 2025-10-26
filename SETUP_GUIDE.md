# MGE Tournament Manager - C++ Setup Guide

This guide will help you build and run the MGE Tournament Manager on Windows using WSL (Ubuntu).

## Prerequisites

### 1. Install WSL2 (if not already installed)

On Windows, open PowerShell as Administrator:

```powershell
wsl --install
```

Restart your computer if prompted, then set up Ubuntu.

### 2. Update WSL and Install Dependencies

Open WSL (Ubuntu) terminal:

```bash
# Update package list
sudo apt update && sudo apt upgrade -y

# Install build tools
sudo apt install -y build-essential cmake git pkg-config

# Install libwebsockets
sudo apt install -y libwebsockets-dev

# Install libcurl
sudo apt install -y libcurl4-openssl-dev

# Install OpenSSL (for HTTPS)
sudo apt install -y libssl-dev
```

## Project Setup

### 1. Create Project Directory

```bash
cd ~
mkdir mge_tournament_cpp
cd mge_tournament_cpp
```

### 2. Create Source Files

Create the following files in your project directory:

**tournament_manager.hpp** - Copy the header file content
**tournament_manager.cpp** - Copy the implementation file content
**main.cpp** - Copy the main file content
**CMakeLists.txt** - Copy the CMake configuration

You can create these files using nano or vim:

```bash
nano tournament_manager.hpp  # Paste content, Ctrl+X to save
nano tournament_manager.cpp
nano main.cpp
nano CMakeLists.txt
```

### 3. Create API Key File

Create a file named `api_key.txt` in your project directory with your Challonge API key:

```bash
echo "your_challonge_api_key_here" > api_key.txt
```

To get your Challonge API key:

1. Go to https://challonge.com/settings/developer
2. Copy your API key

### 4. Update Configuration

In `tournament_manager.cpp`, update the subdomain if needed (line with `subdomain` variable).

In `main.cpp`, change the Challonge username (line with `challongeUser`):

```cpp
std::string challongeUser = "your_challonge_username";
```

## Building the Project

### 1. Create Build Directory

```bash
mkdir build
cd build
```

### 2. Configure with CMake

```bash
cmake ..
```

If you get errors about missing packages, install them:

```bash
# If libwebsockets is missing
sudo apt install -y libwebsockets-dev

# If CURL is missing
sudo apt install -y libcurl4-openssl-dev
```

### 3. Compile

```bash
cmake --build .
```

This will create the `mge_tournament` executable.

## Running the Server

### 1. Basic Usage

```bash
./mge_tournament <tournament_url>
```

Example:

```bash
./mge_tournament mge1
```

This will:

- Connect to the Challonge tournament at `subdomain-mge1`
- Start a WebSocket server on port 8080
- Wait for TF2 servers to connect

### 2. Testing Locally

You can test the server with the admin interface by accessing:

```
http://localhost:8080
```

### 3. Accessing from Windows

If you want to access the server from your Windows browser:

1. Find your WSL IP address:

```bash
ip addr show eth0 | grep 'inet ' | awk '{print $2}' | cut -d/ -f1
```

2. Access from Windows using that IP:

```
http://<wsl_ip>:8080
```

Or use port forwarding from Windows PowerShell (as Administrator):

```powershell
netsh interface portproxy add v4tov4 listenport=8080 listenaddress=0.0.0.0 connectport=8080 connectaddress=<wsl_ip>
```

## HTML Admin Interface (Optional)

Create a `static` directory and add these files:

### static/admin.html

```bash
mkdir -p static
```

Copy the admin.html content from the original Rust project, but update the WebSocket URL if needed.

To serve static files, you'll need to add HTTP serving capability. For now, the WebSocket endpoint works at:

```
ws://localhost:8080
```

## Tournament Workflow

### 1. Create Tournament on Challonge

1. Go to https://challonge.com
2. Create a new tournament with URL format: `subdomain-mge1` (or whatever you named it)
3. Set tournament type to "Single Elimination"
4. Don't add participants yet - the server will do this

### 2. Start the Server

```bash
./mge_tournament mge1
```

### 3. Connect TF2 Server

Your TF2 server should send:

```json
{
  "type": "ServerHello",
  "payload": {
    "apiKey": "",
    "serverNum": "1",
    "serverHost": "your.server.com",
    "serverPort": "27015",
    "stvPort": ""
  }
}
```

### 4. Send Player List

```json
{
  "type": "UsersInServer",
  "payload": {
    "players": [
      { "steamId": "76561198041183975", "name": "player1", "elo": 1500 },
      { "steamId": "76561198306912450", "name": "player2", "elo": 1400 }
    ]
  }
}
```

This will:

- Add all players to Challonge
- Start the tournament
- Assign first matches to arenas

### 5. Report Match Results

When a match finishes, send:

```json
{
  "type": "MatchResults",
  "payload": {
    "winner": "76561198041183975",
    "loser": "76561198306912450",
    "finished": true,
    "arena": 1
  }
}
```

## Troubleshooting

### Port Already in Use

If port 8080 is already in use:

```bash
# Find what's using the port
sudo lsof -i :8080

# Kill the process
sudo kill -9 <PID>
```

### Compilation Errors

**Missing nlohmann/json:**

The CMake file will automatically download it. If it fails:

```bash
cd build
rm -rf _deps
cmake ..
cmake --build .
```

**Libwebsockets version issues:**

```bash
# Check version
pkg-config --modversion libwebsockets

# Should be 4.0+, if not:
sudo apt remove libwebsockets-dev
sudo apt install -y libwebsockets-dev
```

### Connection Issues

**Challonge API not responding:**

- Check your API key in `api_key.txt`
- Verify tournament URL matches Challonge
- Check subdomain is correct

**WebSocket not connecting:**

- Ensure firewall allows port 8080
- Check server is running: `netstat -tulpn | grep 8080`

### Debug Mode

Add debug output in `main.cpp`:

```cpp
std::cout << "Debug: Message received: " << message << std::endl;
```

## Advanced Configuration

### Change Arena Priority

Edit `tournament_manager.cpp`, find the `arenaPriority` initialization:

```cpp
// Spire configuration (default)
arenaPriority = {5, 6, 7, 1, 2, 3, 4, 8, 9, 10, 11, 12, 13, 14, 15, 16};

// Blands mid configuration
// arenaPriority = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
```

### Change Port

In `main.cpp`, modify:

```cpp
info.port = 8080;  // Change to your preferred port
```

## Production Deployment

### Run as Service

Create a systemd service file:

```bash
sudo nano /etc/systemd/system/mge-tournament.service
```

```ini
[Unit]
Description=MGE Tournament Manager
After=network.target

[Service]
Type=simple
User=your_username
WorkingDirectory=/home/your_username/mge_tournament_cpp/build
ExecStart=/home/your_username/mge_tournament_cpp/build/mge_tournament mge1
Restart=always

[Install]
WantedBy=multi-user.target
```

Enable and start:

```bash
sudo systemctl enable mge-tournament
sudo systemctl start mge-tournament
sudo systemctl status mge-tournament
```

### View Logs

```bash
sudo journalctl -u mge-tournament -f
```

## Next Steps

1. Integrate with your TF2 server plugin (SourceMod)
2. Add admin web interface for manual control
3. Add database logging for match history
4. Implement ELO calculation system

## Support

If you encounter issues:

1. Check the console output for error messages
2. Verify all dependencies are installed
3. Ensure API key and tournament URL are correct
4. Check Challonge API rate limits

## Differences from Rust Version

Key improvements in the C++ version:

1. **Fixed field name handling** - Handles both `p1`/`p2` and `p1Id`/`p2Id`
2. **Arena conflict prevention** - Checks if players are already in a match
3. **Better error handling** - More descriptive error messages
4. **Cleaner message queue** - Prevents message duplication

The C++ version should resolve the bugs seen in the Rust logs (mge1.log, mge2.log, etc.).
