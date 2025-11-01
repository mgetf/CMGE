# MGE Tournament Manager

A lightweight C++ WebSocket server for managing TF2 MGE tournaments with Challonge integration.

## Features

- Real-time tournament bracket management
- WebSocket API for TF2 server integration
- Web-based admin interface
- Automatic match assignment to 16 arenas
- Challonge API integration

## Building
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Configuration

Create `api_key.txt` with your Challonge API key.

Edit `main.cpp` to set your Challonge username.

## Running
```bash
./build/mge_tournament <tournament_url>
```

Access admin panel at `http://localhost:8080/admin`

## API

WebSocket endpoint: `ws://localhost:8080/tf2serverep`

Messages are JSON with format:
```json
{
  "type": "MessageType",
  "payload": { }
}
```

See source code for message types.

## Dependencies

- libwebsockets
- libcurl
- nlohmann-json

## License

MIT