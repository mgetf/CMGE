# WebSocket Message Protocol

## Message Format

All messages are JSON with the following structure:

```json
{
  "type": "MessageType",
  "payload": {
    // Message-specific data
  }
}
```

## Messages from TF2 Server → Tournament Manager

### ServerHello

Sent when a server/client connects.

```json
{
  "type": "ServerHello",
  "payload": {
    "apiKey": "", // Empty for game servers, "admin" for admin interface
    "serverNum": "1", // Server identifier
    "serverHost": "mge.tf", // Server hostname
    "serverPort": "27015", // Server port
    "stvPort": "" // STV port (optional)
  }
}
```

### UsersInServer

Sent to register all players for the tournament.

```json
{
  "type": "UsersInServer",
  "payload": {
    "players": [
      {
        "steamId": "76561198041183975",
        "name": "PlayerName",
        "elo": 1500
      }
    ]
  }
}
```

**Effect**:

- Adds all players to Challonge tournament
- Starts the tournament
- Assigns initial matches to arenas

### MatchBegan

Notification that a match has started.

```json
{
  "type": "MatchBegan",
  "payload": {
    "p1": "76561198041183975", // Also accepts "p1Id"
    "p2": "76561198306912450" // Also accepts "p2Id"
  }
}
```

### MatchResults

Report the result of a completed match.

```json
{
  "type": "MatchResults",
  "payload": {
    "winner": "76561198041183975",
    "loser": "76561198306912450",
    "finished": true, // true = match complete, false = forfeit/disconnect
    "arena": 1 // Arena number (1-16)
  }
}
```

**Effect**:

- Reports result to Challonge
- Clears the arena
- Assigns next pending matches

### MatchCancel

Cancel an ongoing match (e.g., player disconnected).

```json
{
  "type": "MatchCancel",
  "payload": {
    "delinquents": ["76561198041183975"], // Players who left
    "arrived": "76561198306912450", // Player who stayed
    "arena": 1
  }
}
```

**Effect**: Clears the arena

### TournamentStart

Manually start tournament mode (sent from admin).

```json
{
  "type": "TournamentStart",
  "payload": {}
}
```

### TournamentStop

Stop tournament mode and clear all arenas.

```json
{
  "type": "TournamentStop",
  "payload": {}
}
```

## Messages from Tournament Manager → TF2 Server

### MatchDetails

Assign players to an arena for a match.

```json
{
  "type": "MatchDetails",
  "payload": {
    "arenaId": 5,
    "p1Id": "76561198041183975",
    "p2Id": "76561198306912450"
  }
}
```

**Server Action**: Move both players to the specified arena and start the match.

### SetMatchScore

Update the score display for a match (sent from admin).

```json
{
  "type": "SetMatchScore",
  "payload": {
    "arenaId": 5,
    "p1Score": 15,
    "p2Score": 10
  }
}
```

### TournamentStart

Broadcast to all servers that tournament has started.

```json
{
  "type": "TournamentStart",
  "payload": {}
}
```

**Server Action**:

- Move all players to spectator
- Restrict joining to tournament participants
- Display tournament HUD

### TournamentStop

Broadcast to all servers that tournament has ended.

```json
{
  "type": "TournamentStop",
  "payload": {}
}
```

**Server Action**:

- Return all players to normal play
- Remove tournament restrictions
- Hide tournament HUD

### Error

Error message from the tournament manager.

```json
{
  "type": "Error",
  "payload": {
    "message": "Error description"
  }
}
```

## Complete Flow Example

### Starting a Tournament

```
1. TF2 Server → ServerHello
2. Admin → TournamentStart (optional, can be automatic)
3. TF2 Server → UsersInServer (with all players)
4. Tournament Manager → MatchDetails (assigns first matches)
5. Tournament Manager → MatchDetails (more matches if arenas available)
```

### During Tournament

```
1. TF2 Server → MatchBegan
2. ... match plays ...
3. TF2 Server → MatchResults
4. Tournament Manager → MatchDetails (assigns next match)
```

### Ending Tournament

```
1. Admin/Server → TournamentStop
2. Tournament Manager → TournamentStop (broadcast)
```

## Arena Assignment

The tournament manager assigns matches to arenas based on priority:

**Default Priority (Spire)**: 5, 6, 7, 1, 2, 3, 4, 8, 9, 10, 11, 12, 13, 14, 15, 16

Matches are assigned to the first available arena in this order.

## Important Notes

### Steam ID Format

Always use Steam64 ID format: `76561198041183975`

### Arena Numbering

- Arenas are numbered 1-16 in messages
- Internally stored as 0-15 (converted automatically)

### Match Assignment Rules

1. Players already in a match won't be assigned to another arena
2. Only "open" matches from Challonge are assigned
3. Matches are assigned in order received from Challonge API

### Error Handling

If a message fails to parse, an Error message is sent back:

```json
{
  "type": "Error",
  "payload": {
    "message": "Parse error: missing field 'steamId'"
  }
}
```

## Testing with WebSocket Client

### Using JavaScript Console

```javascript
const ws = new WebSocket("ws://localhost:8080");

ws.onopen = () => {
  console.log("Connected");

  // Send ServerHello
  ws.send(
    JSON.stringify({
      type: "ServerHello",
      payload: {
        apiKey: "admin",
        serverNum: "1",
        serverHost: "localhost",
        serverPort: "27015",
        stvPort: "",
      },
    })
  );
};

ws.onmessage = (event) => {
  console.log("Received:", event.data);
};

// Send test messages
ws.send(
  JSON.stringify({
    type: "TournamentStart",
    payload: {},
  })
);
```

### Using `websocat` (Command Line)

```bash
# Install websocat
sudo apt install websocat

# Connect
websocat ws://localhost:8080

# Then paste JSON messages
```

## Integration with SourceMod

Example SourceMod plugin code:

```sourcepawn
#include <sourcemod>
#include <websocket>

WebsocketHandle g_hWebsocket;

public void OnPluginStart() {
    g_hWebsocket = Websocket_Open("ws://your-server:8080", OnWebsocketOpen, OnWebsocketReceive, OnWebsocketDisconnect);
}

public void OnWebsocketOpen(WebsocketHandle websocket) {
    char buffer[512];
    Format(buffer, sizeof(buffer),
        "{\"type\":\"ServerHello\",\"payload\":{\"apiKey\":\"\",\"serverNum\":\"1\",\"serverHost\":\"mge.tf\",\"serverPort\":\"27015\",\"stvPort\":\"\"}}");
    Websocket_Send(websocket, SendType_Text, buffer);
}

public void OnWebsocketReceive(WebsocketHandle websocket, WebsocketSendType type, const char[] data, int size) {
    // Parse JSON and handle MatchDetails, TournamentStart, etc.
}

public void SendMatchResult(const char[] winner, const char[] loser, int arena) {
    char buffer[512];
    Format(buffer, sizeof(buffer),
        "{\"type\":\"MatchResults\",\"payload\":{\"winner\":\"%s\",\"loser\":\"%s\",\"finished\":true,\"arena\":%d}}",
        winner, loser, arena);
    Websocket_Send(g_hWebsocket, SendType_Text, buffer);
}
```
