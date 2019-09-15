ChillerBot-ng
-------------

The no gui client maintained by ChillerDragon.

Based on DDNet ( cmake version ) which is maintained by deen and contributors


which is based on DDrace which is a teeworlds mod.

```
# non interactive client (can fix input bugs on unsupportet os)
./chillerbot-ng "cl_chiller_inp 0"
```

### Penetration testing

This client is able to test the stability of servers by acting as a active player.
Create a file called ``pentest.txt`` in the same directory as the client or set a own path ``cl_pentest_file mypentest.txt``.
If the client is in pentest mode it will send random lines from the file and replace all ``?`` with a random character.

A sample ``pentest.txt`` might look like this:
```
testing chat!
/register foo bar baz
/login ??? ???
```

The client sends a chat message every x seconds which has a 50% chance to be one provided in pentest.txt otherwise it is a parsed command from cmdlist with a random argument.

```
# start pentest client and connect to localhost
./chillerbot-ng "cl_chiller_inp 0;connect localhost:8303;cl_pentest 1"
```

Penetration testing can also be automated. Only on linux tho. Dependencys are nohup,bash and vi.
Use the ``./start_pentest.sh`` and ``./stop_pentest.sh`` script to automatically start a bunch of bots and attack a specified port (defualt 8303).

Examples:

```
# start 5 pentest bots and attack default port 8303
# using the default chatfile pentest.txt
./start_pentest.sh 5

# start 22 pentest bots on port 8306
# using a custom chatfile foo.txt
./start_pentest.sh 22 8306 foo.txt

# stop all started pentest bots on all ports
./stop_pentest.sh
```

### Interactive mode keybindings
- 'A' walk left
- 'D' walk right
- 'SPACE' jump
- 'K' selfkill
- 'L' load server browser (only loading)
- 'B' show server browser
- 'TAB' scoreboard
- 'X' say 'xd' in chat
- 'V' toggle ascii map rendering
- 'T' open chat
- 'C' local console
- 'R' remote console
- ':' open chiller console (:help) for more info

[sample video](https://www.youtube.com/watch?v=LMTg2sL5pD4)

Build
-----

Linux / MacOS:

```
git clone --recursive https://github.com/chillerbot/chillerbot-ng
cd chillerbot-ng
mkdir build
cd build
cmake ..
make
```

Windows:

```
git clone --recursive https://github.com/chillerbot/chillerbot-ng
cd chillerbot-ng
mkdir build
cd build
cmake ..
cmake --build .
```

for more information and dependencys on building visit:
https://github.com/ddnet/ddnet/blob/master/README.md
