Dependencies  
The project requires the following dependencies to run:  
OpenGL: The core graphics library.  
GLUT (OpenGL Utility Toolkit): For managing windows, inputs, and basic rendering setup.  
  
Installing Dependencies on Ubuntu (or WSL)  
You can install the necessary libraries using the following commands:  
sudo apt update  
sudo apt install libglew-dev libglfw3-dev libglm-dev  
sudo apt install freeglut3-dev  
  
How to run:  
make  
./scop path/to/obj_file.obj path/to/texture_file.ppm  
  
Model Commands:  
Moving the model relative to the origin:  
'Z', 'W': Move the model up  
'Q', 'A': Move the model left  
'S'		: Move the model down  
'D'		: Move the model right  
'+'		: Move the model further away  
'-'		: Move the model closer  
'O'		: Increase the model speed  
'P'		: Decrease the model speed  
  
Rotating the model:  
'X'		: Toggle X axis rotation  
'Y'		: Toggle Y axis rotation  
'R'		: Stop all rotations  
'J'		: Accelerate rotations  
'K'		: Decelerate rotations  
  
Camera commands:  
'C'				: Toggle camera mode
'Z', 'W'		: Move forward  
'Q', 'A'		: Move left  
'S'				: Move back  
'D'				: Move right  
' '				: Move up  
'V'				: Move down  
'LEFT_ARROW'	: Look left  
'RIGHT_ARROW'	: Look right  
'DOWN_ARROW'	: Look down  
'UP_ARROW'		: Look up  
  
Other commands:  
0	: Reset simulation  
Esc	: Exit program  
