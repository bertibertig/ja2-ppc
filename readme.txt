The included files comprise the complete source code of JA2 (as provided on the JA2:Wildfire CD) and additional files made by me to enable out-of-the-box compilation. This source code is for educational purposes only (read the included license for more info).  Please note that you'll need a copy of MS VS6.0 to build it painlessly.

How to:

1: Extract the included files
2: Copy the "ja2" and "Standard Gaming Platform" directories into c:\ 
3: Set up the include directories in VC6 to include the Build directory as well as subdirectories, and the "Standard Gaming Platform" on the list
4: Include the "Standard Gaming Platform" path in the library paths
5: Go to Build->Set Active Configurations and chose Win32 ja2.exe release
6: Build the projects (press "Build ja2.exe")
7: Wait for 5 minutes, serve when ready

Please note that it's preferable to open the source by opening the workspace in the Build directory
If something's difficult to understand, find Digicrab (me) on the www.ja-galaxy-forum.com and ask nicely for help.  Bring cookies.

Retro68 / classic Mac note
--------------------------
The original project is a Win32/DirectX VC6 build and is not source-portable
to System 9 without replacing its platform layer. A native PowerPC QuickDraw
platform shell and Retro68 build is in the retro68 directory. See
retro68/README.md for the exact build command and the remaining migration
boundary. The full game data files are not included in this source drop.
