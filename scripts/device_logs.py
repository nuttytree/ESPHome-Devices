#!/usr/bin/env python3
# /// script
# requires-python = ">=3.12"
# dependencies = ["websockets>=14"]
# ///
"""Capture live logs from a device through the ESPHome Device Builder dashboard.

The devices live on a VLAN this machine can't reach, but the dashboard host can, and it
streams `esphome logs` output over its websocket (`/ws`, command `devices/logs`). This script
is a small client for that, so logs can be captured from the command line or by Claude.

    pipx run scripts/device_logs.py pool                    # 30 s of logs
    pipx run scripts/device_logs.py pool -t 120 -g 'pump|flow'
    pipx run scripts/device_logs.py smoke-detectors-hub --no-states -t 10

(`pipx run` reads the dependency block above; with `websockets` already installed,
`python scripts/device_logs.py ...` works too.)

Configuration, all through the environment:
  ESPHOME_MCP_BASIC       base64 of `user:password` for the dashboard (the same variable
                          .mcp.json uses for the MCP server). Required.
  ESPHOME_DASHBOARD_URL   dashboard base URL. Defaults to the host of the `esphome` server
                          in .mcp.json.

The dashboard's websocket protocol is internal to Device Builder (written against 1.25.0) and
can change between releases.
"""

import argparse
import asyncio
import base64
import contextlib
import json
import os
from pathlib import Path
import re
import sys
import time
from urllib.parse import urlsplit

import websockets

REPO_ROOT = Path(__file__).resolve().parent.parent
# The dashboard sends the escape as the four literal characters `\033`, not an ESC byte.
ANSI_RE = re.compile(r"(?:\x1b|\\033)\[[0-9;]*m")
LOGIN_ID = 1
STREAM_ID = 2


def dashboard_url():
    url = os.environ.get("ESPHOME_DASHBOARD_URL")
    if not url:
        mcp = json.loads((REPO_ROOT / ".mcp.json").read_text(encoding="utf-8"))
        url = mcp["mcpServers"]["esphome"]["url"]
    parts = urlsplit(url)
    scheme = "wss" if parts.scheme == "https" else "ws"
    return f"{scheme}://{parts.netloc}/ws"


def credentials():
    try:
        user, password = (
            base64.b64decode(os.environ["ESPHOME_MCP_BASIC"]).decode().split(":", 1)
        )
    except KeyError:
        sys.exit("ESPHOME_MCP_BASIC is not set (base64 of user:password)")
    return user, password


async def request(ws, message_id, command, args):
    """Send a command and return its result, skipping unrelated messages."""
    await ws.send(
        json.dumps({"command": command, "message_id": message_id, "args": args})
    )
    while True:
        message = json.loads(await asyncio.wait_for(ws.recv(), 15))
        if str(message.get("message_id")) != str(message_id):
            continue
        if "error_code" in message:
            sys.exit(
                f"{command} failed: {message['error_code']}: {message.get('details')}"
            )
        return message.get("result")


async def stream_logs(args):
    user, password = credentials()
    pattern = re.compile(args.grep) if args.grep else None
    configuration = (
        args.device if args.device.endswith(".yaml") else f"{args.device}.yaml"
    )
    deadline = time.monotonic() + args.seconds

    async with websockets.connect(dashboard_url(), max_size=None) as ws:
        await asyncio.wait_for(ws.recv(), 15)  # server info
        await request(
            ws, LOGIN_ID, "auth/login", {"username": user, "password": password}
        )

        stream_args = {"configuration": configuration, "port": args.port}
        if args.no_states:
            stream_args["no_states"] = True
        await ws.send(
            json.dumps(
                {
                    "command": "devices/logs",
                    "message_id": STREAM_ID,
                    "args": stream_args,
                }
            )
        )

        while (remaining := deadline - time.monotonic()) > 0:
            try:
                message = json.loads(await asyncio.wait_for(ws.recv(), remaining))
            except TimeoutError:
                break
            if str(message.get("message_id")) != str(STREAM_ID):
                continue
            if "error_code" in message:
                sys.exit(
                    f"devices/logs failed: {message['error_code']}: {message.get('details')}"
                )
            if message.get("event") == "output":
                line = message["data"]
                if not args.color:
                    line = ANSI_RE.sub("", line)
                if pattern is None or pattern.search(line):
                    print(line, flush=True)
            elif message.get("event") == "result":
                break  # esphome logs exited (device unreachable, config error, ...)

        await ws.send(
            json.dumps(
                {
                    "command": "devices/stop_stream",
                    "message_id": STREAM_ID + 1,
                    "args": {"stream_id": str(STREAM_ID)},
                }
            )
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n", maxsplit=1)[0])
    parser.add_argument(
        "device", help="device name or yaml filename, e.g. pool or pool.yaml"
    )
    parser.add_argument(
        "-t", "--seconds", type=float, default=30, help="capture duration"
    )
    parser.add_argument("-g", "--grep", help="only print lines matching this regex")
    parser.add_argument(
        "-p", "--port", default="OTA", help="'OTA' (API over the network)"
    )
    parser.add_argument(
        "--no-states", action="store_true", help="hide sensor state-change lines"
    )
    parser.add_argument("--color", action="store_true", help="keep ANSI colour codes")
    args = parser.parse_args()

    # Device logs can contain non-ASCII; the Windows console default would raise on them.
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    with contextlib.suppress(KeyboardInterrupt):
        asyncio.run(stream_logs(args))


if __name__ == "__main__":
    main()
