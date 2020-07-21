#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# http
TCP_PORT = 1141

# compiled programs folder path
LSPVM_BIN_PATH = "../progs"

from socketserver import ThreadingTCPServer as TCPServer
from http.server import BaseHTTPRequestHandler

from threading import Thread, Lock, Event
import os, time, json

import pulse_bridge as pulseb

TCPServer.allow_reuse_address = True


# Mirrors d[name] to d.name
class AttrDict(dict):
    def __getattr__(self, key):
        if key in self:
            return self[key]
        raise AttributeError(key)

    def __setattr__(self, key, value):
        self[key] = value


class LSPCommandQueue:
    # This is a priority queue with a depth of one command per command type
    # Newer commands overwrite older ones with the same type

    # Cmds
    #
    # Switch the strip on/off
    #   is_on bool    the new state
    MODIFY_ON_STATE = 0
    
    # Sets the brightness
    #   brightness int    values in [0, 255]
    MODIFY_BRIGHTNESS = 1
    
    # Send an interrupt request to the LSP
    #   vector int    The interrupt vector [0-3]
    #   arg    int    Argument to pass to the irq [0, 65535]
    SEND_INTERRUPT = 2
    
    # Send a lsp binary to the board
    #   bytecode bytes    The raw lspb
    SEND_PROGRAM = 3
    
    # The lower the array index the higher the priority
    cmd_priority = [SEND_PROGRAM, SEND_INTERRUPT, MODIFY_ON_STATE, MODIFY_BRIGHTNESS]
    
    def __init__(self):
        self._lock      = Lock()
        self._added_evt = Event()
        self._commands  = {}  # key:value -> command type: AttrDict(type, cmd arguments)

    def push(self, cmd):
        with self._lock:
            self._commands[cmd.type] = cmd
        self._added_evt.set()
    
    def pop(self):
        while True:
            with self._lock:
                for cmd_type in self.cmd_priority:
                    cmd = self._commands.get(cmd_type, None)
                    if cmd:
                        self._commands[cmd_type] = None
                        return cmd
            self._added_evt.wait()
            self._added_evt.clear()


lsp_state = AttrDict(
    is_on=False,
    brightness=0
)

lsp_cmd_queue = LSPCommandQueue()

def lsp_state_handler():
    global lsp_is_on, lsp_brightness
    
    pulseb.init()
    while True:
        cmd = lsp_cmd_queue.pop()
        
        if cmd.type == LSPCommandQueue.MODIFY_ON_STATE:
            if lsp_state.is_on != cmd.is_on:
                if cmd.is_on:
                    pulseb.send_string(b"(")
                else:
                    pulseb.send_string(b")")

                lsp_state.is_on = cmd.is_on
        
        elif cmd.type == LSPCommandQueue.MODIFY_BRIGHTNESS:
            if lsp_state.brightness != cmd.brightness:
                pulseb.send_string(f"b{cmd.brightness}".encode())
                lsp_state.brightness = cmd.brightness
        
        elif cmd.type == LSPCommandQueue.SEND_INTERRUPT:
            ser_cmd = f"I{cmd.vector} {cmd.arg}".encode()
            pulseb.send_string(ser_cmd)
        
        elif cmd.type == LSPCommandQueue.SEND_PROGRAM:
            ser_bytecode = "".join(f"w{b}" for b in cmd.bytecode).encode()
            
            # stop vm
            pulseb.send_string(b"[")
            time.sleep(0.1)
            # rewind imem index
            pulseb.send_string(b"r")
            # send prog bytecode
            pulseb.send_string(ser_bytecode)
            # reset VM state, resume VM
            pulseb.send_string(b"R]")

lsp_state_thread = Thread(target=lsp_state_handler)
lsp_state_thread.start()


class LSPRequestHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "lspHTTP/1.0"
    
    def _parse_path(self):
        self.query  = ""
        self.params = {}
        
        try:
            path, query = self.path.split("?", 1)
            self.path  = path
            self.query = query
        except:
            return
        
        kv_list = query.split("&")
        for kv in kv_list:
            try:
                key, value = kv.split("=")
                
                self.params[key] = value
            except:
                pass
    
    def do_GET(self):
        self._parse_path()
        status = self.exec_endpoint()
        
        json_state = json.dumps(lsp_state)
        
        self.send_response(status)
        self.send_header("Content-Type", "text/json")
        self.send_header("Content-Length", f"{len(json_state)}")
        self.end_headers()
        
        self.wfile.write(json_state.encode("utf8"))
    
    def exec_endpoint(self):
        if self.path == "/status":
            return 200
        
        if self.path == "/on":
            lsp_cmd_queue.push(AttrDict(
                type=LSPCommandQueue.MODIFY_ON_STATE,
                is_on=True
            ))
            return 200

        if self.path == "/off":
            lsp_cmd_queue.push(AttrDict(
                type=LSPCommandQueue.MODIFY_ON_STATE,
                is_on=False
            ))
            return 200
        
        if self.path == "/brightness":
            try:
                b = int(self.query)
            except:
                return 400  # bad request
            lsp_cmd_queue.push(AttrDict(
                type=LSPCommandQueue.MODIFY_BRIGHTNESS,
                brightness=b
            ))
            return 200
        
        if self.path == "/interrupt":
            try:
                ivec = int(self.params["vector"])
                iarg = int(self.params.get("arg", 0))
            except:
                return 400  # bad req
            lsp_cmd_queue.push(AttrDict(
                type=LSPCommandQueue.SEND_INTERRUPT,
                vector=ivec,
                arg=iarg
            ))
            return 200
        
        if self.path == "/load-program":
            try:
                pgm_list = [fname[:-5] for fname in os.listdir(LSPVM_BIN_PATH) if fname.endswith(".lspb")]
            except:
                pgm_list = []

            if self.query not in pgm_list:
                return 404
            
            try:
                with open(os.path.join(LSPVM_BIN_PATH, self.query + ".lspb"), "rb") as fp:
                    data = fp.read()
            except:
                return 500
            
            lsp_cmd_queue.push(AttrDict(
                type=LSPCommandQueue.SEND_PROGRAM,
                bytecode=data
            ))
            return 200
        
        return 501  # not implemented



httpd = TCPServer(("", TCP_PORT), LSPRequestHandler)
httpd.serve_forever()
