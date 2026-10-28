#!/usr/bin/env python3
"""Drive the PowerBook through the classic-mac-hardware MCP from the shell.

    machw.py call TOOL '{"json": "args"}' [TOOL ARGS ...]   # raw MCP tool calls
    machw.py autotest tests/autotest/smoke.txt              # scripted run

`autotest` uploads the script and an options.cfg naming it, deletes old
logs/screenshots on the Mac, runs the build through LaunchAPPL (optionally
recording the Cam Link at the same time), then downloads JA2PPC-Debug.log,
JA2AUTOTEST.log, JA2PROF.csv and every screenshot the script took (as PNG)
into --out, and restores the bundle's normal options.cfg. Exit code 0 means
the script reported result=pass.

The MCP server reads the machine list (with the FTP password) from
~/.config/classic-mac-hardware/machines.json itself; this tool never sees it.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MCP_COMMAND = ["uvx", "--with", "mcp<2", "--from",
               "git+https://github.com/matthewdeaves/classic-mac-hardware-mcp",
               "classic-mac-hardware-mcp"]
REMOTE_OUTPUTS = ["/JA2PPC-Debug.log", "/JA2AUTOTEST.log", "/JA2PROF.csv"]


class McpError(RuntimeError):
    pass


class Mcp:
    def __init__(self):
        env = dict(os.environ)
        env.setdefault("MACHINES_CONFIG", os.path.expanduser(
            "~/.config/classic-mac-hardware/machines.json"))
        self.process = subprocess.Popen(
            MCP_COMMAND, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL, env=env, text=True)
        self.next_id = 0
        self.request("initialize", {"protocolVersion": "2024-11-05", "capabilities": {},
                                    "clientInfo": {"name": "machw", "version": "1"}})
        self.send({"jsonrpc": "2.0", "method": "notifications/initialized", "params": {}})

    def send(self, message):
        self.process.stdin.write(json.dumps(message) + "\n")
        self.process.stdin.flush()

    def request(self, method, params):
        self.next_id += 1
        self.send({"jsonrpc": "2.0", "id": self.next_id, "method": method, "params": params})
        while True:
            line = self.process.stdout.readline()
            if not line:
                raise McpError("MCP server exited")
            reply = json.loads(line)
            if reply.get("id") == self.next_id:
                if "error" in reply:
                    raise McpError(reply["error"].get("message", str(reply["error"])))
                return reply["result"]

    def call(self, tool, check=True, **arguments):
        result = self.request("tools/call", {"name": tool, "arguments": arguments})
        text = "\n".join(item.get("text", "") for item in result.get("content", []))
        if check and result.get("isError"):
            raise McpError(f"{tool}: {text}")
        return text

    def close(self):
        self.process.stdin.close()
        self.process.wait(timeout=10)


def run_call(args):
    mcp = Mcp()
    try:
        pairs = args.calls
        for index in range(0, len(pairs), 2):
            arguments = json.loads(pairs[index + 1]) if index + 1 < len(pairs) else {}
            print(mcp.call(pairs[index], check=False, **arguments))
    finally:
        mcp.close()


def build_options(base_path, script_name, overrides):
    lines = []
    keys = {}
    with open(base_path, encoding="latin-1") as handle:
        for line in handle:
            line = line.rstrip("\r\n")
            key = line.split("=", 1)[0].strip()
            if line and not line.startswith(("#", ";")) and "=" in line:
                keys[key] = len(lines)
            lines.append(line)
    for key, value in [("autotest", script_name)] + overrides:
        if key in keys:
            lines[keys[key]] = f"{key}={value}"
        else:
            keys[key] = len(lines)
            lines.append(f"{key}={value}")
    # Retro68's stdio reads LF files as they are (options.cfg is LF).
    return "\n".join(lines) + "\n"


def remote_shots(mcp, machine):
    listing = mcp.call("list_directory", machine=machine, path="/")
    return re.findall(r"\s(\S+\.bmp)$", listing, re.MULTILINE)


def wait_for_result(mcp, machine, staging, max_wait):
    probe = os.path.join(staging, "probe.log")
    deadline = time.time() + max_wait
    while time.time() < deadline:
        time.sleep(10)
        if os.path.exists(probe):
            os.remove(probe)
        mcp.call("download_file", check=False, machine=machine,
                 remote_path="/JA2AUTOTEST.log", local_path=probe)
        if os.path.exists(probe):
            with open(probe, encoding="latin-1") as handle:
                if "result=" in handle.read():
                    # Give the app time to write its shutdown logs and exit.
                    time.sleep(5)
                    return
    print(f"no result= after {max_wait} s; the app may still be running on the Mac")


def run_autotest(args):
    build_dir = os.path.abspath(args.build)
    binary = os.path.join(build_dir, "JA2PPC.bin")
    base_options = os.path.join(build_dir, "JA2PPC Bundle", "options.cfg")
    if not os.path.isfile(binary):
        sys.exit(f"missing {binary}; build first")
    if not os.path.isfile(base_options):
        base_options = os.path.join(ROOT, "retro68", "options.cfg")
    script_name = os.path.basename(args.script)
    if len(script_name) > 31:
        sys.exit("script name must fit HFS (31 characters)")
    overrides = [tuple(item.split("=", 1)) for item in args.set]
    os.makedirs(args.out, exist_ok=True)
    for name in os.listdir(args.out):
        if name.endswith((".png", ".log", ".csv", ".mp4")):
            os.remove(os.path.join(args.out, name))

    staging = tempfile.mkdtemp(prefix="machw-")
    options_path = os.path.join(staging, "options.cfg")
    with open(options_path, "w", encoding="latin-1", newline="") as handle:
        handle.write(build_options(base_options, script_name, overrides))
    script_path = os.path.join(staging, script_name)
    with open(args.script, encoding="latin-1") as source, \
            open(script_path, "w", encoding="latin-1", newline="") as target:
        target.write("".join(line.rstrip("\r\n") + "\n" for line in source))

    mcp = Mcp()
    capture = None
    machine = args.machine
    try:
        print(mcp.call("test_connection", machine=machine).splitlines()[0])
        for remote in REMOTE_OUTPUTS + ["/" + name for name in remote_shots(mcp, machine)]:
            mcp.call("delete_files", check=False, machine=machine, path=remote)
        mcp.call("upload_file", machine=machine, local_path=options_path, remote_path="/options.cfg")
        mcp.call("upload_file", machine=machine, local_path=script_path,
                 remote_path="/" + script_name)
        if args.capture:
            capture = subprocess.Popen(
                ["ffmpeg", "-loglevel", "error", "-y", "-f", "v4l2", "-framerate", "30",
                 "-video_size", "1024x768", "-i", args.video_device, "-t", str(args.capture),
                 "-an", "-c:v", "libx264", "-preset", "ultrafast", "-crf", "18",
                 os.path.join(args.out, "capture.mp4")])
        launched = mcp.call("execute_binary", check=False, machine=machine, platform="ppc",
                            binary_path=binary)
        print(launched.splitlines()[0])
        if "Timed out" in launched:
            # LaunchAPPL gives up after 45 s but the app keeps running on the
            # Mac; the script's own result line marks the real end.
            wait_for_result(mcp, machine, staging, args.max_wait)
        for remote in REMOTE_OUTPUTS:
            mcp.call("download_file", check=False, machine=machine, remote_path=remote,
                     local_path=os.path.join(args.out, remote.lstrip("/")))
        for shot in remote_shots(mcp, machine):
            local = os.path.join(staging, shot)
            mcp.call("download_file", machine=machine, remote_path="/" + shot, local_path=local)
            png = os.path.join(args.out, shot[:-4] + ".png")
            subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", local, png], check=True)
        mcp.call("upload_file", machine=machine, local_path=base_options,
                 remote_path="/options.cfg")
    finally:
        mcp.close()
        if capture is not None:
            capture.wait()
        shutil.rmtree(staging, ignore_errors=True)

    log_path = os.path.join(args.out, "JA2AUTOTEST.log")
    if not os.path.isfile(log_path):
        print("no JA2AUTOTEST.log: the app did not start the script (crash or old build?)")
        return 2
    with open(log_path, encoding="latin-1") as handle:
        log = handle.read().replace("\r", "\n")
    print(log, end="")
    print(f"outputs in {args.out}")
    return 0 if "result=pass" in log else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--machine", default="powerbook-g3")
    commands = parser.add_subparsers(dest="command", required=True)
    call = commands.add_parser("call", help="raw MCP tool calls")
    call.add_argument("calls", nargs="+", help="TOOL JSON [TOOL JSON ...]")
    autotest = commands.add_parser("autotest", help="run a script on the Mac")
    autotest.add_argument("script")
    autotest.add_argument("--build", default=os.path.join(ROOT, "retro68", "build-german-debug"))
    autotest.add_argument("--out", default=os.path.join(ROOT, ".hw-out"))
    autotest.add_argument("--set", action="append", default=[], metavar="KEY=VALUE",
                          help="extra options.cfg entries, e.g. skip_intros=1")
    autotest.add_argument("--capture", type=int, default=0, metavar="SECONDS",
                          help="record the Cam Link in parallel")
    autotest.add_argument("--video-device", default="/dev/video0")
    autotest.add_argument("--max-wait", type=int, default=900, metavar="SECONDS",
                          help="how long to poll for result= after LaunchAPPL times out")
    args = parser.parse_args()
    if args.command == "call":
        run_call(args)
        return 0
    return run_autotest(args)


if __name__ == "__main__":
    sys.exit(main())
