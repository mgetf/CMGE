# MGE Tournament Manager (C++)

A C++ application that acts as a bridge between a Team Fortress 2 MGE server plugin and the Challonge tournament platform, enabling fully automated tournaments.

This project is a modernized C++ replication of an older Rust-based system, featuring an improved communication protocol and robust testing tools.

## Core Architecture

The manager operates with two primary WebSocket connections:

1.  **WebSocket Server (Admin UI):** It hosts a server on port `8080` that serves a web-based admin dashboard for starting and stopping tournaments.
2.  **WebSocket Client (MGE Plugin):** It actively connects as a client to a WebSocket server provided by the MGE game server plugin (expected on port `9001`) to receive player data and send match commands.

## Features

- Real-time tournament management via the Challonge v1 API.
- Connects to an MGE server plugin to manage players and matches.
- Web-based admin dashboard for tournament control.
- Automatic match assignment to arenas based on a priority list.
- ELO-based seeding for initial player ranking in the tournament.
- Includes a Python-based mock MGE server for easy testing and development.

## Building

This project uses CMake.

```bash
# Clone the repository
git clone <your-repo-url>
cd mge-tournament-cpp

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
cmake --build .
```

## Configuration

1.  **Challonge API Key:** Create a file named `api_key.txt` in the executable's directory and place your Challonge API key inside it.
2.  **Challonge Username:** In `tournament_manager.cpp`, update the `ChallongeAPI` constructor with your Challonge username.

## Running the System

You need to run two components: the MGE server (or the mock server) and the tournament manager.

#### 1. Running the Mock MGE Server (for testing)

The mock server simulates the TF2 game server plugin, allowing you to test the full tournament flow.

```bash
# Make sure you have Python 3 and the websockets library
pip install websockets

# Run the mock server
python3 mock_mge_server.py
```

The mock server will start and wait for a connection on `ws://localhost:9001`.

#### 2. Running the Tournament Manager

The manager requires the URL slug of a pre-created Challonge tournament as a command-line argument.

```bash
./build/mge_tournament <your_challonge_tournament_url>
```

Example: `./build/mge_tournament my_weekly_cup_42`

The manager will start, connect to the mock server, and host the admin panel.

#### 3. Using the Admin Panel

Open a web browser and navigate to **`http://localhost:8080`**. From here, you can start and stop the tournament.

## Communication Protocols

#### Admin UI WebSocket API (Server)

- **Endpoint:** `ws://localhost:8080` (protocol is `tf2serverep`)
- **Direction:** Admin UI connects to the manager.
- **Purpose:** Receives commands like `TournamentStart` and `TournamentStop` from the admin.

#### MGE Plugin WebSocket API (Client)

- **Endpoint:** `ws://localhost:9001` (or your configured game server IP/port)
- **Direction:** The manager connects to the MGE server plugin.
- **Purpose:**
  - Sends commands like `get_players` and `add_player_to_arena`.
  - Receives events like `match_end_1v1` and responses with player data.

All messages use a JSON format:

```json
{
  "type": "MessageType",
  "payload": {
    /* ... message data ... */
  }
}
```

## Dependencies

- **libwebsockets**
- **libcurl**
- **nlohmann-json** (handled automatically via CMake's FetchContent)
- **CMake** (for building)

## License

MIT
