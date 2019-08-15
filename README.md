ChillerBot-ng
-------------

The no gui client maintained by ChillerDragon.

Based on DDNet ( cmake version ) which is maintained by deen and contributors


which is based on DDrace which is a teeworlds mod.

```
# non interactive client (can fix input bugs on unsupportet os)
./chillerbot-ng "cl_chiller_inp 0"
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
