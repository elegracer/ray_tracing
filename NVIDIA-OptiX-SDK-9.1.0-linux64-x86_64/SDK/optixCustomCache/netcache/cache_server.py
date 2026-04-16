#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: Copyright (c) 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

"""
ZeroMQ-based in-memory cache server.

This server provides a simple key-value cache accessible via ZeroMQ.
Multiple clients can connect simultaneously and perform GET/SET operations.
"""

import zmq
import binascii
import json
import threading
import time
import logging
from typing import Dict, Any


class CacheServer:
    """In-memory cache server using ZeroMQ REP/REQ pattern."""
    
    def __init__(self, port:int):
        """Initialize the cache server.
        
        Args:
            port: Port to listen on
        """
        self.port = port
        self.cache: Dict[str, bytes] = {}
        self.lock = threading.RLock()  # Thread-safe access to cache
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REP)
        self.running = False
        self.request_count = 0
        
        # Configure logging
        logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)s - %(levelname)s - %(message)s',
            datefmt='%Y-%m-%d %H:%M:%S'
        )
        self.logger = logging.getLogger(__name__)
        
    def start(self):
        """Start the cache server."""
        try:
            self.socket.bind(f"tcp://*:{self.port}")
            self.running = True
            self.logger.info(f"Cache server started on port {self.port}")
            print(f"Cache server started on port {self.port}")
            print("Supported operations: GET <key>, SIZE <key>, SET <key> <value>, LIST, CLEAR, STATS")
            print("Press Ctrl+C to stop the server")
            
            while self.running:
                try:
                    # Receive request with timeout
                    if self.socket.poll(1000):  # 1 second timeout
                        message = self.socket.recv(zmq.NOBLOCK)
                        self.request_count += 1
                        # Log first part as string if possible, otherwise as hex
                        try:
                            msg_preview = message[:64].decode('utf-8', errors='replace')
                        except:
                            msg_preview = message[:64].hex()
                        self.logger.info(f"Request #{self.request_count}: {msg_preview}{'...' if len(message) > 64 else ''}")
                        response = self.process_request(message)
                        # Log response preview
                        if isinstance(response, bytes):
                            try:
                                # Decode and remove control characters
                                resp_preview = response[:64].decode('utf-8', errors='replace')
                                # Remove control characters (keep only printable + space/tab/newline)
                                resp_preview = ''.join(c if c.isprintable() or c in '\n\r\t' else f'\\x{ord(c):02x}' for c in resp_preview)
                            except:
                                resp_preview = response[:64].hex()
                        else:
                            resp_preview = str(response)[:64]
                        self.logger.info(f"Response #{self.request_count}: {resp_preview}{'...' if len(response) > 64 else ''}")
                        self.socket.send(response)
                except zmq.Again:
                    # No message received within timeout, continue
                    continue
                except KeyboardInterrupt:
                    self.logger.info("Received interrupt signal, shutting down...")
                    print("\nShutting down server...")
                    break
                    
        except Exception as e:
            self.logger.error(f"Error starting server: {e}")
            print(f"Error starting server: {e}")
        finally:
            self.stop()
    
    def stop(self):
        """Stop the cache server."""
        self.running = False
        self.socket.close()
        self.context.term()
        self.logger.info(f"Server stopped after processing {self.request_count} requests")
        print("Server stopped")
    
    def process_request(self, message: bytes) -> bytes:
        """Process incoming cache requests.
        
        Args:
            message: Request message as bytes
            
        Returns:
            Response as bytes
        """
        try:
            # Parse command from beginning of message
            # Commands are always ASCII/UTF-8, so decode just enough to get the command
            msg_str = message.decode('utf-8', errors='ignore')
            parts = msg_str.split(' ', 2)
            command = parts[0].upper()
            
            with self.lock:
                if command == "GET":
                    if len(parts) < 2:
                        self.logger.warning("GET command missing key parameter")
                        return b"ERROR: GET requires a key"
                    key = parts[1]
                    value = self.cache.get(key)
                    if value is None:
                        self.logger.debug(f"GET '{key}': Key not found")
                        return b"NOT_FOUND"
                    self.logger.debug(f"GET '{key}': Retrieved value (length: {len(value)})")
                    return b"OK " + value
                
                elif command == "SET":
                    if len(parts) < 3:
                        self.logger.warning("SET command missing key or value parameter")
                        return b"ERROR: SET requires key and value"
                    key = parts[1]
                    
                    # Find the binary value in the original message
                    # Format: "SET key value" - find position after second space
                    msg_str_bytes = message
                    try:
                        # Find "SET " and then the key
                        set_prefix = b"SET "
                        after_set = msg_str_bytes[len(set_prefix):]
                        key_bytes = key.encode('utf-8')
                        space_pos = after_set.find(b" ")
                        if space_pos == -1:
                            return b"ERROR: SET requires key and value"
                        value = after_set[space_pos + 1:]  # Everything after key and space
                    except Exception as e:
                        self.logger.error(f"Error parsing SET command: {e}")
                        return b"ERROR: Failed to parse SET command"
                    
                    is_update = key in self.cache
                    self.cache[key] = value
                    operation = "Updated" if is_update else "Created"
                    self.logger.debug(f"SET '{key}': {operation} (value length: {len(value)})")
                    return b"OK"
                
                elif command == "DELETE" or command == "DEL":
                    if len(parts) < 2:
                        self.logger.warning("DELETE command missing key parameter")
                        return b"ERROR: DELETE requires a key"
                    key = parts[1]
                    if key in self.cache:
                        del self.cache[key]
                        self.logger.debug(f"DELETE '{key}': Key deleted successfully")
                        return b"OK"
                    self.logger.debug(f"DELETE '{key}': Key not found")
                    return b"NOT_FOUND"
                
                elif command == "LIST":
                    keys = list(self.cache.keys())
                    self.logger.debug(f"LIST: Returned {len(keys)} keys")
                    return f"OK {json.dumps(keys)}".encode('utf-8')
                
                elif command == "CLEAR":
                    keys_count = len(self.cache)
                    self.cache.clear()
                    self.logger.info(f"CLEAR: Removed {keys_count} keys from cache")
                    return b"OK"
                
                elif command == "STATS":
                    stats = {
                        "keys": len(self.cache),
                        "port": self.port,
                        "requests_processed": self.request_count
                    }
                    self.logger.debug(f"STATS: Cache contains {len(self.cache)} keys")
                    return f"OK {json.dumps(stats)}".encode('utf-8')
                
                elif command == "SIZE":
                    if len(parts) < 2:
                        self.logger.warning("SIZE command missing key parameter")
                        return b"ERROR: SIZE requires a key"
                    key = parts[1]
                    value = self.cache.get(key)
                    if value is None:
                        self.logger.debug(f"SIZE '{key}': Key not found")
                        return b"NOT_FOUND"
                    size = len(value)
                    self.logger.debug(f"SIZE '{key}': Size is {size}")
                    return f"OK {size}".encode('utf-8')
                
                elif command == "PING":
                    self.logger.debug("PING: Health check requested")
                    return b"PONG"
                
                else:
                    self.logger.warning(f"Unknown command received: '{command}'")
                    return f"ERROR: Unknown command '{command}'".encode('utf-8')
                    
        except Exception as e:
            self.logger.error(f"Error processing request: {str(e)}")
            return f"ERROR: {str(e)}".encode('utf-8')
    
    def set_log_level(self, level: str):
        """Set the logging level.
        
        Args:
            level: Logging level ('DEBUG', 'INFO', 'WARNING', 'ERROR')
        """
        numeric_level = getattr(logging, level.upper(), None)
        if not isinstance(numeric_level, int):
            raise ValueError(f'Invalid log level: {level}')
        self.logger.setLevel(numeric_level)
        logging.getLogger().setLevel(numeric_level)
        self.logger.info(f"Log level set to {level.upper()}")


def main():
    """Main function to start the cache server."""
    # Use port 5555 by default (accessible to non-admin users)
    server = CacheServer(port=5555)
    
    # Enable DEBUG logging to see detailed cache operations
    # Change to 'INFO' for less verbose output, 'WARNING' for minimal output
    server.set_log_level('INFO')
    
    try:
        server.start()
    except KeyboardInterrupt:
        print("\nReceived interrupt signal")
    finally:
        server.stop()


if __name__ == "__main__":
    main() 