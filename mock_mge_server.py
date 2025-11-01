#!/usr/bin/env python3
import asyncio
import websockets
import json
import time

connected_clients = set()

async def handler(websocket):
    connected_clients.add(websocket)
    print(f"✅ Client connected from {websocket.remote_address}")
    
    welcome = {
        "type": "welcome",
        "message": "Connected to Mock MGE Server",
        "arenas": 16
    }
    await websocket.send(json.dumps(welcome))
    
    try:
        async for message in websocket:
            print(f"📩 Received: {message}")
            data = json.loads(message)
            
            if data.get("command") == "get_players":
                print("   → Sending 4 test players")
                response = {
                    "type": "response",
                    "command": "get_players",
                    "players": [
                        {"id": 1, "name": "AliceTheScout", "elo": 1800, "arena": 0, "inArena": False},
                        {"id": 2, "name": "BobTheSoldier", "elo": 1600, "arena": 0, "inArena": False},
                        {"id": 3, "name": "CharlieDemoman", "elo": 1700, "arena": 0, "inArena": False},
                        {"id": 4, "name": "DaveMedic", "elo": 1500, "arena": 0, "inArena": False},
                    ]
                }
                await websocket.send(json.dumps(response))
                
            elif data.get("command") == "get_arenas":
                print("   → Sending arena list")
                response = {
                    "type": "response",
                    "command": "get_arenas",
                    "arenas": [
                        {"id": i, "name": f"Spire {i}", "players": 0, "max": 2, 
                         "status": 0, "is2v2": False, "game_mode": 1}
                        for i in range(1, 17)
                    ]
                }
                await websocket.send(json.dumps(response))
                
            elif data.get("command") == "add_player_to_arena":
                player_id = data.get("player_id")
                arena_id = data.get("arena_id")
                print(f"   → Adding player {player_id} to arena {arena_id}")
                
                response = {
                    "type": "success",
                    "message": "Player added to arena",
                    "player_id": player_id,
                    "arena_id": arena_id
                }
                await websocket.send(json.dumps(response))
                
                print(f"   ⏳ Simulating 10 second match...")
                await asyncio.sleep(10)
                
                winner_id = player_id
                loser_id = player_id + 1 if player_id % 2 == 1 else player_id - 1
                
                players = {
                    1: "AliceTheScout",
                    2: "BobTheSoldier", 
                    3: "CharlieDemoman",
                    4: "DaveMedic"
                }
                
                print(f"   🏆 Match ended! Winner: Player {winner_id}")
                event = {
                    "type": "event",
                    "event": "match_end_1v1",
                    "arena_id": arena_id,
                    "winner_id": winner_id,
                    "loser_id": loser_id,
                    "winner_name": players.get(winner_id, f"Player{winner_id}"),
                    "loser_name": players.get(loser_id, f"Player{loser_id}"),
                    "winner_score": 20,
                    "loser_score": 15,
                    "timestamp": int(time.time())
                }
                await websocket.send(json.dumps(event))
                
    except websockets.exceptions.ConnectionClosed:
        print(f"❌ Client disconnected")
    finally:
        connected_clients.remove(websocket)

async def main():
    print("=" * 60)
    print("🎮 Mock MGE Server Starting")
    print("=" * 60)
    print("Listening on: ws://localhost:9001")
    print("Waiting for tournament manager to connect...\n")
    
    async with websockets.serve(handler, "0.0.0.0", 9001):
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())