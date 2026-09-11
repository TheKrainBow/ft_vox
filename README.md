ft_vox — Voxel World Renderer 
 
ft_vox is a real‑time voxel world renderer focused on smooth performance and large‑scale terrain streaming. It generates terrain procedurally, builds chunk meshes on worker threads, and renders with modern OpenGL techniques to keep frame times steady while you explore. 
 
Highlights 
- Procedural terrain and biome shading with day/night cycle 
- Chunked world streaming with LRU caching and prioritized loading 
- Greedy meshing and solid/alpha split passes for efficiency 
- Water with planar reflections and animated normals 
- Skybox support, dynamic sun, shadows, fog and light “god rays” 
- Post‑process pipeline with MSAA resolve and UI overlay 
 
Dependencies 
- OpenGL + GLU 
- GLEW 
- GLFW 
- GLM (headers) 
 
On Ubuntu/WSL 
- sudo apt update 
- sudo apt install build-essential libgl1-mesa-dev libglu1-mesa-dev libglew-dev libglfw3-dev libglm-dev 
 
Build 
- make          # optimized build → ft_vox 
- make debug    # debug build → ft_voxDebug 
 
Run 
- ./ft_vox [seed] 
  - Optional numeric seed customizes world generation (default: 42). See srcs/main.cpp:12. 
 
Basic Controls 
- Move: W / A / S / D 
- Look: Mouse (toggle capture with M or ;) 
- Jump: Space (with gravity on) 
- Fly up/down: Space / Left Shift (with gravity off) 
- Sprint: Left Ctrl (toggle when gravity on; hold for fast fly when off) 
- Break / Place / Pick block: Mouse Left / Right / Middle 
- Zoom FOV: Mouse wheel 
- Fullscreen: F11 
 
Toggles & Tools 
- H: Help panel (keybinds) 
- F1: UI overlay (crosshair, HUD) 
- F3: Debug overlay 
- F4: Triangle mesh (wireframe) view 
- F5: Invert camera 
- L: Toggle dynamic lighting (forces daytime when off) 
- G: Toggle gravity 
- C: Toggle world generation/streaming 
- P: Pause day/night time update 
- Numpad + / −: Accelerate or rewind time 
- Esc: Quit 
 
Notes 
- The project links against OpenGL, GLU, GLEW and GLFW (see Makefile:4). GLM is header‑only. 
- A static GLEW is included in lib64 for convenience; system packages also work.
- On Linux/X11, mouse capture stays eight pixels inside the window to prevent
  desktop docks from appearing during mouse look. M releases capture; Alt-Tab
  releases it while unfocused. The game pauses rendering and simulation while
  unfocused or minimized.
- `make test-mouse-capture` checks confinement, raw input, clicks, fullscreen
  transitions and focus recovery on a virtual display (requires `xvfb` and
  `libxtst-dev`). Linux builds also require the X11 development library (`libx11-dev`).
- First launch shows a short loading splash while initial chunks stream in. 
## Screenshots

A quick look at terrain generation, rendering passes, and water/atmosphere effects.

### World generation and shaders

<p>
	<a href="screenshots/spawn.png"><img src="screenshots/spawn.png" width="49%" alt="Spawn area"></a>
	<a href="screenshots/mountains.png"><img src="screenshots/mountains.png" width="49%" alt="Mountains and terrain shaping"></a>
</p>

<p>
	<a href="screenshots/caves.png"><img src="screenshots/caves.png" width="49%" alt="Cave generation"></a>
	<a href="screenshots/render_distance_LOD.png"><img src="screenshots/render_distance_LOD.png" width="49%" alt="Render distance and LOD"></a>
</p>

### Meshing & rendering

<p>
	<a href="screenshots/greedy_meshing.png"><img src="screenshots/greedy_meshing.png" width="70%" alt="Greedy meshing output"></a>
</p>

### Water & underwater

<p>
	<a href="screenshots/water_reflects.png"><img src="screenshots/water_reflects.png" width="49%" alt="Water with planar reflections"></a>
	<a href="screenshots/under_water.png"><img src="screenshots/under_water.png" width="49%" alt="Underwater view"></a>
</p>


### Water simulation

Generated water and water placed from the block picker are sources. Breaking or
placing a block wakes nearby water. Streams update on every fixed game tick (20 Hz), fall down
to bedrock, and spread through six horizontal levels. Falling water starts a new
six-block reach at its landing point. Two horizontal source neighbors create a
source when the target has solid ground or another source underneath it. Streams
recede when their supply is removed, and water replaces decorative plants.

Flow surfaces share corner heights, dropping steeply near sources and flattening
toward the end of a stream. Swimming and underwater effects sample that same
sloped surface, so shallow flow lowers the swimming level. Holding Space smoothly
raises the feet slightly above the surface with a gentle 1.2-second bob to clear
source-block banks; releasing
Space gently sinks the player. A small exit margin prevents waterline jitter.
Flat source surfaces
still use greedy meshing.
The fixed-step game clock catches up after slow frames; simulation does not wait
for mesh builds or staged render snapshots. Simulation processes at most 512
cells per tick and waits at unloaded or coarse
LOD boundaries until full-resolution terrain is available. Water changes are kept
in the same in-memory modified-chunk cache as player edits.

Run `make test-water` for propagation regressions and shader validation (requires
`glslangValidator`), and `make -j4` to build the game. In-game checks: dig a bank
beside water; place a source on a flat platform and above a drop; block an existing
stream; and open a supported gap between two sources. Repeat across chunk edges.
