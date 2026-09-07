# Real PowerBook G3 Test Workflow

This repository is wired to
[classic-mac-hardware-mcp](https://github.com/matthewdeaves/classic-mac-hardware-mcp).
The MCP server runs on the modern development machine. It talks to a real
Classic Mac over TCP/IP using:

- RumpusFTP for file transfer and downloading `PT_Log`.
- Retro68 `LaunchAPPLServer` for transferring and launching a MacBinary `.bin`.

This is a remote deploy, run, and log workflow. It is not a source-level
debugger: the MCP server cannot set breakpoints, inspect PPC registers, or
attach to a running process. Use the debug window and `JA2PPC-Debug.log` for
the diagnostics currently implemented by this port. MacsBug or another
classic debugger is a separate optional tool if breakpoint debugging is
required.

## Modern Machine

Install or verify the following on the build machine:

- A working Retro68 `powerpc-apple-macos` toolchain, including the host
  `LaunchAPPL` executable and the PPC `LaunchAPPLServer` build.
- CMake, GNU Make, Python 3, and FFmpeg. The last three are used by the
  existing Retro68 build; FFmpeg is needed when intro video conversion is
  enabled.
- `uv` (recommended) or Python 3.10+ with the MCP project's dependencies.
- An MCP-compatible client. The checked-in `.mcp.json` is suitable for Claude
  Code and other clients that support project MCP configuration.
- A legally owned JA2 installation containing `Ja2.ini` and `Data/`.
- A wired or wireless network path shared with the PowerBook. Prefer a
  private, trusted LAN or an isolated network segment.

The MCP server uses plain FTP and an unauthenticated LaunchAPPL TCP service.
Do not expose either port to the Internet. The server's FTP implementation
uses passive mode, so configure the LAN/firewall to permit the FTP data
connections as well as TCP port 21.

### Configure the MCP machine entry

Copy the example to the MCP configuration directory and edit it. The example
is intentionally not a live credential file:

```sh
mkdir -p "$HOME/.config/classic-mac-hardware"
cp machines.classic-mac.example.json \
  "$HOME/.config/classic-mac-hardware/machines.json"
export CLASSIC_MAC_FTP_PASSWORD='the-password-used-by-rumpus'
```

Set `host` to the PowerBook's fixed/reserved IP address. The MCP project
expands `${CLASSIC_MAC_FTP_PASSWORD}` when it loads the JSON. Do not commit
`machines.json` or put a real password in this repository.

The `.mcp.json` starts the current upstream server through `uvx`:

```text
uvx --from git+https://github.com/matthewdeaves/classic-mac-hardware-mcp classic-mac-hardware-mcp
```

The first start downloads the server and its Python dependencies. For an
offline or reproducible setup, clone that MCP repository, run `uv sync`, and
replace the `.mcp.json` command/arguments with the local `uv run` command.

## PowerBook G3

The G3 does not run Python, MCP, or the build tools. It needs Mac OS 9 with
TCP/IP and two classic Mac applications.

### 1. Establish TCP/IP

1. Install/boot a supported Mac OS 9 installation, preferably 9.2.2.
2. Enable Open Transport TCP/IP in the TCP/IP control panel. MacTCP is also
   supported by LaunchAPPLServer, but Open Transport is the recommended G3
   configuration.
3. Give the PowerBook a stable address, either with a DHCP reservation or a
   manual address, and record it in `machines.json`.
4. From the modern machine, verify that the address is reachable before
   troubleshooting either application.

### 2. Install and run RumpusFTP

Obtain a System 9-compatible RumpusFTP installer from its vendor or an
authorized archive. This project does not redistribute RumpusFTP.

1. Install RumpusFTP on the G3 and configure it to listen on TCP port 21.
2. Create an FTP account matching the `ftp.username` and password in the
   machine configuration.
3. Give that account access to the folder containing the JA2 application and
   its `Data` folder. The account must also be able to create/replace the
   uploaded application and read `PT_Log`.
4. Enable passive FTP and configure its passive port range if Rumpus exposes
   that setting. Permit that range between the two machines.
5. Start the RumpusFTP server and leave it running while testing.

### 3. Install and run LaunchAPPLServer

Build or obtain the PPC `LaunchAPPLServer` from Retro68. A local Retro68
build normally produces:

```text
$RETRO68_BUILD/build-target-ppc/LaunchAPPL/Server/LaunchAPPLServer.bin
```

Transfer that MacBinary file to the G3 using an HFS/resource-fork-preserving
method, or use a disk image. Do not treat the raw `.APPL` data fork as a safe
standalone transfer. Copy `LaunchAPPLServer.bin` into a suitable folder on the
G3, open it, select the TCP/IP connection mode, and leave it running. Its
default TCP service is port 1984, which must match `launchappl.port`.

The LaunchAPPL service is deliberately unsecured and executes applications
received over the network. Run it only on a trusted/isolated LAN and stop it
when the test session ends.

## Build and Run

Build the debug application on the modern machine:

```sh
JA2_DATA_DIR="/path/to/legally-owned/Jagged Alliance 2" \
RETRO68_TOOLCHAIN_FILE="/path/to/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake" \
./tools/build-german-debug.sh all
```

The artifact to execute is:

```text
retro68/build-german-debug/JA2PPC.bin
```

In the MCP client, perform these checks in order:

1. Call `list_machines` and confirm the G3 is listed as PPC/Open Transport.
2. Call `test_connection` with `test_launchappl=true`.
3. Call `execute_binary` with `machine="powerbook-g3"`, `platform="ppc"`,
   and the absolute local path to `JA2PPC.bin`.
4. Call the `mac://powerbook-g3/logs/latest` resource, or use
   `download_file` for `PT_Log` and `JA2PPC-Debug.log`.

`execute_binary` transfers the `.bin` and launches it in one operation. Use
`upload_file` when you need to transfer a complete `JA2PPC Bundle` or other
files first. The bundle contains the application, `Ja2.ini`, and `Data/`;
the G3 must have enough free disk space and RAM for the selected data set.

## Troubleshooting

- FTP connects but uploads fail: check Rumpus permissions, passive FTP, and
  the account's free disk space.
- LaunchAPPL port is closed: make sure `LaunchAPPLServer.bin` is running in
  TCP/IP mode and that TCP 1984 is not blocked.
- The app launches but data is missing: run from a folder containing `Ja2.ini`
  and `Data/`, and preserve the archive names and case expected by the port.
- The app exits or hangs: retrieve `PT_Log` and `JA2PPC-Debug.log`; remember
  that the MCP timeout does not necessarily terminate an application already
  running on the G3.
- A breakpoint or register inspection is needed: install/use MacsBug or a
  compatible classic debugger separately; this MCP integration only provides
  remote execution and file/log operations.

## References

- MCP server: <https://github.com/matthewdeaves/classic-mac-hardware-mcp>
- Retro68 LaunchAPPL TCP requirements:
  `Retro68/LaunchAPPL/LaunchAPPL.cfg.example`
- Repository build instructions: `README.md` and `retro68/README.md`
