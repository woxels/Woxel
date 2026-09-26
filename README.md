![woxel header](https://raw.githubusercontent.com/woxels/woxels.github.io/main/woxelbanner.png)

![screenshot](https://raw.githubusercontent.com/woxels/woxels.github.io/main/Screenshot_2023-09-02_07-06-18.png)

Woxel uses intuitive controls akin to Minetest and Minecraft, while providing a design experience similar to Goxel and Magicalvoxel. Woxel paves the way to transitioning Minecraft/Minetest players to 3D asset creators for games.

💬 Join our discord! https://discord.gg/AH23bGNE2h<br>
📱 Follow us on Twitter! https://twitter.com/Woxels

## Download
* 🔗 **Flatpak:** https://flathub.org/apps/xyz.woxel.Woxel
* 🔗 **Snapcraft:** https://snapcraft.io/woxel
* 🔗 **Ubuntu:** https://github.com/woxels/Woxel/releases
* 🔗 **WebGL:** https://woxel.xyz / https://woxels.github.io

## Input Mapping

### 🏃 Movement
* **W,A,S,D** = Move around based on relative orientation to X and Y.
* **SPACE** + **L-SHIFT** = Move up and down relative Z.
* **F** = Toggle player fast speed on and off.
* **1-7** = Change move speed for selected fast state.
* **P** = Toggle pitch lock.

### 🏗️ Interaction
* **Left Click** / **R-SHIFT** = Place node.
* **Right Click** / **R-CTRL** = Delete node.
* **Q** / **Z** / **Middle Click** / **Mouse4** = Clone color of pointed node.
* **E** / **Mouse5** = Replace color of pointed node.
* **R** = Toggle mirror brush.
* **V** = Places voxel at current position.
* **Middle Scroll** = Change selected color.
* **X** + **C** / **Slash** + **Quote** = Scroll color of pointed node.

### 🛠️ Settings
* **F1** = Resets environment state back to default.
* **F2** = Toggle HUD visibility.
* **F3** = Save. (auto saves on exit, backup made if idle for 3 mins.)
* **F8** = Load. (will erase what you have done since the last save)
* **ESCAPE / TAB** = Toggle menu.

### 🖱️ Mouse locks when you click on the window, press ESCAPE / TAB to unlock the mouse.
  
#### 😲 *Arrow Keys can be used to move the view around.* 🤯

#### ✔️ *Your state is automatically saved on exit.*

## Console Arguments
### 📂🖱️🎨 Create or load a project, change project mouse sensitivity, or update a projects color palette
* `./wox <project_name> <[OPTIONAL]mouse_sensitivity> <[OPTIONAL]path to color palette>`
* *e.g;* `./wox Untitled 0.003 /tmp/colors.txt`
* 1st, "Untitled", Name of project to open or create.
* 2nd, "0.003", Mouse sensitivity.
* 3rd, "/tmp/colors.txt", path to a color palette file, the file must contain a hex color on each new line, 32 colors maximum. e.g; "#00FFFF".
* Find color palettes at; https://lospec.com/palette-list
* You can use any palette up to 32 colors. `#000000` (Black) is a valid color and does not truncate the list.

### 📂 Load Base64 from file
* `./wox loadb64 <file_path>`
* *e.g;* `./wox loadb64 /home/user/file.b64`
* The file is adopted as a project named after the basename (so `file.b64` becomes project `file`). F3 / exit then save `file.wox.gz` in the Woxel project folder.

### 📂 Load `*.wox.gz` from file
* `./wox loadgz <file_path>`
* *e.g;* `./wox loadgz /home/user/file.wox.gz`
* Same adoption: you can save and export it as a normal project afterwards.

### 🪟 Wayland custom window decorations
* Auto-detected when the SDL video driver is Wayland. Force it on or off from the command line (flag can appear anywhere):
* `./wox --wayland` / `./wox wayland` — borderless window + in-app title bar (drag, min, max, close)
* `./wox --no-wayland` / `./wox --x11` — native window decorations
* *e.g;* `./wox --wayland Untitled`
* *e.g;* `./wox export Untitled ply ./out.ply --wayland` (the flag is ignored for export-only jobs)

### 📂 Export as mesh or voxels
* `./wox export <project_or_file> <[OPTIONAL]format> <[OPTIONAL]ply_mode> <export_path>`
* Formats: `wox`, `txt`, `vv`, `ply`, `b64`
* PLY modes: `greedy` (merged same-color quads, default), `quads` (one quad per cube face), `tris` (two triangles per cube face). Greedy is always quads.
* *e.g;* `./wox export untitled ply /home/user/file.ply`
* *e.g;* `./wox export untitled ply greedy /home/user/file.ply`
* *e.g;* `./wox export untitled ply quads /home/user/file.ply`
* *e.g;* `./wox export untitled ply tris /home/user/file.ply`
* *e.g;* `./wox export ./file.b64 greedy ./file.ply`
* *e.g;* `./wox export ~/file.wox.gz txt ./file.txt`
* Source can be a saved project name, a `.b64` file, or a `.wox.gz` file. Format can be omitted when the output path ends in `.ply`, `.txt`, `.vv`, `.b64`, or `.wox.gz`.

## Compile
Run `make` or `make test` or `cc main.c -Ofast -lm -lz -lSDL2 -lGLESv2 -lEGL -o wox`
```
cc main.c -Ofast -lm -lz -lSDL2 -lGLESv2 -lEGL -o wox
./wox
```

## Similar Software
* 🖌️ https://github.com/mrbid/VoxelPaint
* 💼 https://github.com/mrbid/VoxelPaintPro

## Info
* 🎨 https://lospec.com/palette-list/resurrect-32
