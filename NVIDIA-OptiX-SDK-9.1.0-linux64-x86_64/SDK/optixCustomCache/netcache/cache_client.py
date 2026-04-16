#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: Copyright (c) 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

"""
Simple client for testing the ZeroMQ cache server.
"""

import zmq
import sys
import time


class CacheClient:
    """Simple client for the ZeroMQ cache server."""
    
    def __init__(self, server_address: str = "tcp://localhost:5555"):
        """Initialize the cache client.
        
        Args:
            server_address: Address of the cache server
        """
        self.server_address = server_address
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REQ)
        
    def connect(self):
        """Connect to the cache server."""
        self.socket.connect(self.server_address)
        print(f"Connected to cache server at {self.server_address}")
    
    def disconnect(self):
        """Disconnect from the cache server."""
        self.socket.close()
        self.context.term()
        print("Disconnected from server")
    
    def send_request(self, message: str) -> bytes:
        """Send a request to the server and return the response.
        
        Args:
            message: Request message
            
        Returns:
            Server response as bytes
        """
        self.socket.send_string(message)
        return self.socket.recv()  # Receive as bytes to handle binary data
    
    def get(self, key: str):
        """Get a value from the cache.
        
        Args:
            key: Cache key
            
        Returns:
            Cached value as bytes or error message as string
        """
        response = self.send_request(f"GET {key}")
        if response.startswith(b"OK "):
            return response[3:]  # Remove "OK " prefix, return bytes
        # Decode error messages
        try:
            return response.decode('utf-8')
        except:
            return response
    
    def set(self, key: str, value: str) -> str:
        """Set a value in the cache.
        
        Args:
            key: Cache key
            value: Value to store
            
        Returns:
            Server response
        """
        return self.send_request(f"SET {key} {value}").decode('utf-8', errors='replace')
    
    def delete(self, key: str) -> str:
        """Delete a key from the cache.
        
        Args:
            key: Cache key to delete
            
        Returns:
            Server response
        """
        return self.send_request(f"DELETE {key}").decode('utf-8', errors='replace')
    
    def list_keys(self) -> str:
        """List all keys in the cache.
        
        Returns:
            JSON string of all keys
        """
        response = self.send_request("LIST").decode('utf-8', errors='replace')
        if response.startswith("OK "):
            return response[3:]  # Remove "OK " prefix
        return response
    
    def clear(self) -> str:
        """Clear all entries from the cache.
        
        Returns:
            Server response
        """
        return self.send_request("CLEAR").decode('utf-8', errors='replace')
    
    def stats(self) -> str:
        """Get cache statistics.
        
        Returns:
            JSON string with cache stats
        """
        response = self.send_request("STATS").decode('utf-8', errors='replace')
        if response.startswith("OK "):
            return response[3:]  # Remove "OK " prefix
        return response
    
    def ping(self) -> str:
        """Ping the server to test connectivity.
        
        Returns:
            Server response
        """
        return self.send_request("PING").decode('utf-8', errors='replace')


def interactive_mode():
    """Run the client in interactive mode."""
    client = CacheClient()
    
    try:
        client.connect()
        
        print("\nCache Client Interactive Mode")
        print("Commands: get <key>, set <key> <value>, delete <key>, list, clear, stats, ping, quit")
        print("Type 'help' for more information\n")
        
        while True:
            try:
                command = input("cache> ").strip()
                
                if not command:
                    continue
                
                if command.lower() in ['quit', 'exit', 'q']:
                    break
                
                if command.lower() == 'help':
                    print("Available commands:")
                    print("  get <key>         - Get value for key")
                    print("  set <key> <value> - Set key to value")
                    print("  delete <key>      - Delete key")
                    print("  list              - List all keys")
                    print("  clear             - Clear all entries")
                    print("  stats             - Show cache statistics")
                    print("  ping              - Test server connectivity")
                    print("  quit              - Exit client")
                    continue
                
                parts = command.split(' ', 2)
                cmd = parts[0].lower()
                
                if cmd == 'get' and len(parts) >= 2:
                    result = client.get(parts[1])
                    if isinstance(result, bytes):
                        # Display binary data as hex or length
                        print(f"Result: <binary data, {len(result)} bytes>")
                        print(f"Hex preview: {result[:64].hex()}{'...' if len(result) > 64 else ''}")
                    else:
                        print(f"Result: {result}")
                
                elif cmd == 'set' and len(parts) >= 3:
                    result = client.set(parts[1], parts[2])
                    print(f"Result: {result}")
                
                elif cmd == 'delete' and len(parts) >= 2:
                    result = client.delete(parts[1])
                    print(f"Result: {result}")
                
                elif cmd == 'list':
                    result = client.list_keys()
                    print(f"Keys: {result}")
                
                elif cmd == 'clear':
                    result = client.clear()
                    print(f"Result: {result}")
                
                elif cmd == 'stats':
                    result = client.stats()
                    print(f"Stats: {result}")
                
                elif cmd == 'ping':
                    result = client.ping()
                    print(f"Result: {result}")
                
                else:
                    print("Invalid command. Type 'help' for available commands.")
                
            except KeyboardInterrupt:
                print("\nUse 'quit' to exit")
            except Exception as e:
                print(f"Error: {e}")
    
    finally:
        client.disconnect()


def main():
    """Main function."""
    if len(sys.argv) > 1:
        # Command line mode
        client = CacheClient()
        try:
            client.connect()
            command = ' '.join(sys.argv[1:])
            response = client.send_request(command)
            # Handle binary responses
            if isinstance(response, bytes):
                try:
                    print(response.decode('utf-8'))
                except:
                    print(f"<binary data, {len(response)} bytes>")
                    print(f"Hex: {response.hex()}")
            else:
                print(response)
        finally:
            client.disconnect()
    else:
        # Interactive mode
        interactive_mode()


if __name__ == "__main__":
    main() 