#!/usr/bin/env python3
"""
Test client for MGE Tournament Manager
Simulates a TF2 server connecting to the tournament system
"""

import asyncio
import websockets
import json
import sys

class TournamentClient:
    def __init__(self, uri="ws://localhost:8080"):
        self.uri = uri
        self.ws = None
    
    async def connect(self):
        """Connect to the tournament server"""
        print(f"Connecting to {self.uri}...")
        self.ws = await websockets.connect(self.uri)
        print("✅ Connected!")
    
    async def send_message(self, msg_type, payload):
        """Send a message to the server"""
        message = {
            "type": msg_type,
            "payload": payload
        }
        msg_str = json.dumps(message)
        print(f"📤 Sending: {msg_str}")
        await self.ws.send(msg_str)
    
    async def receive_message(self):
        """Receive a message from the server"""
        message = await self.ws.recv()
        print(f"📥 Received: {message}")
        return json.loads(message)
    
    async def server_hello(self, is_admin=False):
        """Send ServerHello message"""
        await self.send_message("ServerHello", {
            "apiKey": "admin" if is_admin else "",
            "serverNum": "1",
            "serverHost": "test.server.com",
            "serverPort": "27015",
            "stvPort": ""
        })
    
    async def send_players(self, players):
        """Send UsersInServer message"""
        player_list = []
        for name, steam_id, elo in players:
            player_list.append({
                "steamId": steam_id,
                "name": name,
                "elo": elo
            })
        
        await self.send_message("UsersInServer", {
            "players": player_list
        })
    
    async def tournament_start(self):
        """Send TournamentStart message"""
        await self.send_message("TournamentStart", {})
    
    async def tournament_stop(self):
        """Send TournamentStop message"""
        await self.send_message("TournamentStop", {})
    
    async def match_result(self, winner_id, loser_id, arena, finished=True):
        """Send MatchResults message"""
        await self.send_message("MatchResults", {
            "winner": winner_id,
            "loser": loser_id,
            "finished": finished,
            "arena": arena
        })
    
    async def match_began(self, p1_id, p2_id):
        """Send MatchBegan message"""
        await self.send_message("MatchBegan", {
            "p1": p1_id,
            "p2": p2_id
        })
    
    async def listen(self):
        """Listen for messages from server"""
        print("\n👂 Listening for messages (Ctrl+C to stop)...")
        try:
            while True:
                await self.receive_message()
        except KeyboardInterrupt:
            print("\n✋ Stopped listening")

async def test_basic_connection():
    """Test 1: Basic connection"""
    print("\n=== Test 1: Basic Connection ===")
    client = TournamentClient()
    await client.connect()
    await client.server_hello()
    await asyncio.sleep(1)
    await client.ws.close()
    print("✅ Test passed\n")

async def test_tournament_flow():
    """Test 2: Complete tournament flow"""
    print("\n=== Test 2: Tournament Flow ===")
    client = TournamentClient()
    await client.connect()
    
    # Connect as server
    await client.server_hello()
    await asyncio.sleep(0.5)
    
    # Send player list
    players = [
        ("Player1", "76561198000000001", 1500),
        ("Player2", "76561198000000002", 1400),
        ("Player3", "76561198000000003", 1600),
        ("Player4", "76561198000000004", 1300),
    ]
    
    print("\n📝 Sending player list...")
    await client.send_players(players)
    
    # Listen for match assignments
    print("\n⏳ Waiting for match assignments...")
    for i in range(2):  # Expect 2 matches for 4 players
        await client.receive_message()
    
    await asyncio.sleep(1)
    await client.ws.close()
    print("\n✅ Test passed\n")

async def test_admin_control():
    """Test 3: Admin control"""
    print("\n=== Test 3: Admin Control ===")
    client = TournamentClient()
    await client.connect()
    
    # Connect as admin
    await client.server_hello(is_admin=True)
    await asyncio.sleep(0.5)
    
    # Start tournament
    print("\n🎮 Starting tournament...")
    await client.tournament_start()
    await asyncio.sleep(0.5)
    
    # Stop tournament
    print("\n🛑 Stopping tournament...")
    await client.tournament_stop()
    await asyncio.sleep(0.5)
    
    await client.ws.close()
    print("\n✅ Test passed\n")

async def interactive_mode():
    """Interactive mode for manual testing"""
    print("\n=== Interactive Mode ===")
    print("Commands:")
    print("  hello [admin] - Send ServerHello")
    print("  players - Send sample player list")
    print("  start - Tournament start")
    print("  stop - Tournament stop")
    print("  result <arena> - Send match result")
    print("  listen - Listen for messages")
    print("  quit - Exit")
    print()
    
    client = TournamentClient()
    await client.connect()
    
    while True:
        try:
            cmd = input(">>> ").strip().split()
            
            if not cmd:
                continue
            
            action = cmd[0].lower()
            
            if action == "hello":
                is_admin = len(cmd) > 1 and cmd[1] == "admin"
                await client.server_hello(is_admin)
            
            elif action == "players":
                players = [
                    ("Alice", "76561198000000001", 1500),
                    ("Bob", "76561198000000002", 1400),
                    ("Charlie", "76561198000000003", 1600),
                    ("Dave", "76561198000000004", 1300),
                ]
                await client.send_players(players)
            
            elif action == "start":
                await client.tournament_start()
            
            elif action == "stop":
                await client.tournament_stop()
            
            elif action == "result":
                arena = int(cmd[1]) if len(cmd) > 1 else 1
                await client.match_result(
                    "76561198000000001",
                    "76561198000000002",
                    arena
                )
            
            elif action == "listen":
                await client.listen()
            
            elif action == "quit":
                break
            
            else:
                print(f"Unknown command: {action}")
        
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"❌ Error: {e}")
    
    await client.ws.close()
    print("\n👋 Goodbye!")

async def main():
    """Main entry point"""
    if len(sys.argv) > 1:
        mode = sys.argv[1].lower()
        
        if mode == "test1":
            await test_basic_connection()
        elif mode == "test2":
            await test_tournament_flow()
        elif mode == "test3":
            await test_admin_control()
        elif mode == "all":
            await test_basic_connection()
            await test_tournament_flow()
            await test_admin_control()
        elif mode == "interactive" or mode == "i":
            await interactive_mode()
        else:
            print(f"Unknown mode: {mode}")
            print("Usage: python3 test_client.py [test1|test2|test3|all|interactive]")
    else:
        print("MGE Tournament Test Client")
        print()
        print("Usage:")
        print("  python3 test_client.py test1        - Test basic connection")
        print("  python3 test_client.py test2        - Test tournament flow")
        print("  python3 test_client.py test3        - Test admin control")
        print("  python3 test_client.py all          - Run all tests")
        print("  python3 test_client.py interactive  - Interactive mode")
        print()
        print("Interactive mode selected by default...")
        print()
        await interactive_mode()

if __name__ == "__main__":
    # Install websockets if needed: pip3 install websockets
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\n👋 Interrupted by user")