#!/usr/bin/env python3
import asyncio
import websockets
import json
import time
import random

connected_clients = set()

arena_players = {i: set() for i in range(1, 17)}

async def simulate_match(websocket, arena_id, players_in_arena):
    """
    Simulates a match and sends the result.
    This is now a separate function to be called only when an arena is full.
    """
    print(f"   🔥 Arena {arena_id} is full. Simulating match between players {players_in_arena}...")
    await asyncio.sleep(5)  # Shorter simulation time for faster testing

    player_list = list(players_in_arena)
    winner_id = random.choice(player_list)
    loser_id = player_list[0] if player_list[1] == winner_id else player_list[1]

    # Mock player names based on ID
    players_map = {
        1: "Shaaden", 2: "BobAmmomod", 3: "CharlieSpire", 4: "DaveShotgunStall"
    }

    print(f"   🏆 Match in Arena {arena_id} ended! Winner: Player {winner_id}")
    event = {
        "type": "event",
        "event": "match_end_1v1",
        "arena_id": arena_id,
        "winner_id": winner_id,
        "loser_id": loser_id,
        "winner_name": players_map.get(winner_id, f"Player{winner_id}"),
        "loser_name": players_map.get(loser_id, f"Player{loser_id}"),
        "winner_score": 20,
        "loser_score": random.randint(0, 19),
        "timestamp": int(time.time())
    }
    await websocket.send(json.dumps(event))

    # Clear the arena for the next match
    print(f"   🧹 Clearing arena {arena_id}")
    arena_players[arena_id].clear()


async def handler(websocket):
    connected_clients.add(websocket)
    print(f"✅ Client connected from {websocket.remote_address}")
    
    # Reset state on new connection for clean tests
    for arena_id in arena_players:
        arena_players[arena_id].clear()

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
                        {"id": 1, "name": "Shaaden", "elo": 1800, "arena": 0, "inArena": False},
                        {"id": 2, "name": "BobAmmomod", "elo": 1600, "arena": 0, "inArena": False},
                        {"id": 3, "name": "CharlieSpire", "elo": 1700, "arena": 0, "inArena": False},
                        {"id": 4, "name": "DaveShotgunStall", "elo": 1500, "arena": 0, "inArena": False},
                    ]
                }
                await websocket.send(json.dumps(response))

            elif data.get("command") == "add_player_to_arena":
                player_id = data.get("player_id")
                arena_id = data.get("arena_id")
                
                print(f"   → Adding player {player_id} to arena {arena_id}")
                
                # Add player to our state
                arena_players[arena_id].add(player_id)
                
                response = { "type": "success", "message": "Player added to arena" }
                await websocket.send(json.dumps(response))
                
                # Check if the arena is now full
                if len(arena_players[arena_id]) == 2:
                    # If full, start the simulation for the correct players
                    await simulate_match(websocket, arena_id, arena_players[arena_id])

    except websockets.exceptions.ConnectionClosed:
        print(f"❌ Client disconnected")
    finally:
        connected_clients.remove(websocket)

async def main():
    print("=" * 60)
    print("🎮 Mock MGE Server (Stateful Version)")
    print("=" * 60)
    print("Listening on: ws://localhost:9001")
    print("Waiting for tournament manager to connect...\n")
    
    async with websockets.serve(handler, "0.0.0.0", 9001):
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())