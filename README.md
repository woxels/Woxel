# Woxel

![screenshot](https://raw.githubusercontent.com/woxels/woxels.github.io/main/Screenshot_2023-09-02_07-06-18.png)

### Mouse locks when you click on the game window, press ESCAPE / TAB to unlock the mouse.

## Input Mapping

### Movement
* **W,A,S,D** = Move around based on relative orientation to X and Y.
* **SPACE** + **L-SHIFT** = Move up and down relative Z.
* **F** = Toggle player fast speed on and off.
* **1-7** = Change move speed for selected fast state.
* **P** = Toggle pitch lock.

### Interaction
* **Left Click** / **R-SHIFT** = Place node.
* **Right Click** / **R-CTRL** = Delete node.
* **Q** / **Z** / **Middle Click** / **Mouse4** = Clone color of pointed node.
* **E** / **Mouse5** = Replace color of pointed node.
* **R** = Toggle mirror brush.
* **V** = Places voxel at current position.
* **Middle Scroll** = Change selected color.
* **X** + **C** / **Slash** + **Quote** = Scroll color of pointed node.

### Settings
* **F1** = Resets environment state back to default.
* **F2** = Toggle HUD visibility.
* **F3** = Save. (auto saves on exit, backup made if idle for 3 mins.)
* **F8** = Load. (will erase what you have done since the last save)
* **ESCAPE / TAB** = Toggle menu.
  
*Arrow Keys can be used to move the view around.*

*Your state is automatically saved on exit.*

## Console Arguments
### To create or load a project, change project mouse sensitivity, or update a projects color palette
* `./wox <project_name> <[OPTIONAL]mouse_sensitivity> <[OPTIONAL]path to color palette>`
* e.g; `./wox Untitled 0.003 /tmp/colors.txt`
* 1st, "Untitled", Name of project to open or create.
* 2nd, "0.003", Mouse sensitivity.
* 3rd, "/tmp/colors.txt", path to a color palette file, the file must contain a hex
color on each new line, 32 colors maximum. e.g; "#00FFFF".

### To `*.wox.gz` load from file
* `./wox loadgz <file_path>`
* e.g; `./wox loadgz /home/user/file.wox.gz`

### To export mesh or voxels
* `./wox export <project_name> <option: wox,txt,vv,ply> <export_path>`
* e.g; `./wox export txt /home/user/file.txt`
* e.g; `./wox export ply /home/user/file.ply`

*When exporting as `ply` you will want to merge vertices by distance in [Blender](https://www.blender.org/)
or `Cleaning and Repairing > Merge Close Vertices` in [MeshLab](https://www.meshlab.net/).*

## Info
* Find more color palettes at; https://lospec.com/palette-list
* *(You can use any palette upto 32 colors. But don't use #000000 (Black) in your color palette as it will terminate at that color.)*
* Default 32 Color Palette: https://lospec.com/palette-list/resurrect-32
