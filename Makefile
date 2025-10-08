# Windows Makefile (MinGW/MSYS2) to build ft_vox.exe
#
# Assumptions:
# - Build in MSYS2/MinGW64 or a similar MinGW environment.
# - GLFW3 and GLEW are installed via MSYS2 (or their import libs are in `lib64`).
# - OpenGL comes from Windows SDK (`-lopengl32`).
# - If using your own libs, drop the `.a`/`.dll.a` files into `lib64/` and DLLs next to the exe.

NAME            = ft_vox.exe
DEBUG_NAME      = ft_voxDebug.exe

# Compiler
CC              ?= g++

# Paths
SRC_PATH        = srcs/
OBJ_PATH        = obj_win/
DEBUG_OBJ_PATH  = debug_obj_win/

# Includes
INCLUDES        = -Iincludes -Iglm

# Flags
CFLAGS          = -Wall -Wextra -Werror -O3 -std=c++17 -g3
DEBUG_CFLAGS    = -DNDEBUG -Wall -Wextra -Werror -g3 -std=c++17

# Try to detect system-installed dependencies via pkg-config when available
PKG_CFLAGS     := $(shell pkg-config --cflags glew glfw3 2>/dev/null)
PKG_LIBS       := $(shell pkg-config --libs glew glfw3 2>/dev/null)

CFLAGS         += $(PKG_CFLAGS)
DEBUG_CFLAGS   += $(PKG_CFLAGS)

# Library search path: put your Windows libs/import libs here if not using pkg-config
LDFLAGS        = -Llib64

# Default Windows libs for OpenGL + GLEW + GLFW (MinGW)
# If linking static GLEW, set `USE_STATIC_GLEW=1` and ensure `-lglew32s` exists in lib path.
WIN_LIBS_SHARED = -lopengl32 -lglew32 -lglfw3 -lgdi32 -luser32 -lshell32 -lkernel32 -lwinmm -lws2_32
WIN_LIBS_STATIC = -DGLEW_STATIC -lopengl32 -lglew32s -lglfw3 -lgdi32 -luser32 -lshell32 -lkernel32 -lwinmm -lws2_32

ifeq ($(USE_STATIC_GLEW),1)
  CFLAGS   += -DGLEW_STATIC
  LIBS      = $(WIN_LIBS_STATIC)
else
  LIBS      = $(WIN_LIBS_SHARED)
endif

# Append pkg-config resolved libs last so they can override defaults if present
LIBS += $(PKG_LIBS)

# Source files (keep in sync with the main Makefile)
SRC_NAME	=	stb_truetype.cpp \
				main.cpp \
				StoneEngine.cpp \
				Camera.cpp \
				TextureManager.cpp \
				Chunk.cpp \
				SubChunk.cpp \
				SubChunk_faces.cpp \
				NoiseGenerator.cpp \
				Textbox.cpp \
				SplineInterpolator.cpp \
				ChunkManager.cpp \
				ChunkLoader.cpp \
				ChunkRenderer.cpp \
				Chrono.cpp \
				Shader.cpp \
				ThreadPool.cpp \
				Noise3DGenerator.cpp \
				CaveGenerator.cpp \
				Skybox.cpp \
				Raycaster.cpp \
				Player.cpp

OBJ_NAME    = $(SRC_NAME:.cpp=.o)
OBJ         = $(addprefix $(OBJ_PATH), $(OBJ_NAME))
DEBUG_OBJ   = $(addprefix $(DEBUG_OBJ_PATH), $(OBJ_NAME))

# Colors (no-op on Windows cmd; visible in MSYS terminals)
BLUE  = \033[1;34m
CYAN  = \033[1;36m
GREEN = \033[1;32m
RED   = \033[1;31m
WHITE = \033[1;37m
EOC   = \033[0;0m

.PHONY: all deps debug clean fclean re re_debug run bootstrap package package-zip

all: deps $(NAME)

# Fetch header-only deps if missing (optional; requires git + curl)
deps:
	@echo "$(BLUE)Checking dependencies...$(WHITE)"
	@if [ ! -f includes/stb_image.h ]; then \
		echo "$(CYAN)Fetching stb_image.h$(WHITE)"; \
		curl -L --fail --silent --show-error https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -o includes/stb_image.h; \
	fi
	@if [ ! -f includes/stb_truetype.hpp ]; then \
		echo "$(CYAN)Fetching stb_truetype as .hpp$(WHITE)"; \
		curl -L --fail --silent --show-error https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h -o includes/stb_truetype.hpp; \
	fi
	@if [ ! -d glm/glm ]; then \
		echo "$(CYAN)Cloning GLM headers$(WHITE)"; \
		git clone --depth 1 https://github.com/g-truc/glm.git glm; \
	fi
	@if [ ! -f includes/GL/glew.h ]; then \
		echo "$(CYAN)Fetching GLEW header (includes/GL/glew.h)$(WHITE)"; \
		mkdir -p includes/GL; \
		curl -L --fail --silent --show-error https://raw.githubusercontent.com/Perlmint/glew-cmake/master/include/GL/glew.h -o includes/GL/glew.h; \
	fi

$(NAME): $(OBJ)
	@echo "$(RED)=====> Linking ft_vox.exe <===== $(WHITE)"
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJ) -o $(NAME) $(LDFLAGS) $(LIBS)
	@echo "$(GREEN)Done ! ✅ $(EOC)"

$(OBJ_PATH)%.o: $(SRC_PATH)%.cpp | deps
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -c $< -o $@

-include $(OBJ:%.o=%.d)

debug: $(DEBUG_NAME)

$(DEBUG_NAME): $(DEBUG_OBJ)
	@echo "$(RED)=====> Linking ft_voxDebug.exe <===== $(WHITE)"
	$(CC) $(DEBUG_CFLAGS) $(INCLUDES) $(DEBUG_OBJ) -o $(DEBUG_NAME) $(LDFLAGS) $(LIBS)
	@echo "$(GREEN)Done ! ✅ $(EOC)"

$(DEBUG_OBJ_PATH)%.o: $(SRC_PATH)%.cpp | deps
	@mkdir -p $(@D)
	$(CC) $(DEBUG_CFLAGS) $(INCLUDES) -MMD -c $< -o $@

-include $(DEBUG_OBJ:%.o=%.d)

clean:
	@echo "$(CYAN)♻  Cleaning obj files ♻ $(WHITE)"
	rm -rf $(OBJ_PATH)
	rm -rf $(DEBUG_OBJ_PATH)
	@echo "$(GREEN)Done !✅ $(EOC)"

fclean: clean
	@echo "$(CYAN)♻  Cleaning executables ♻ $(WHITE)"
	rm -f $(NAME)
	rm -f $(DEBUG_NAME)
	@echo "$(GREEN)Done !✅ $(EOC)"

re: fclean all
re_debug: fclean debug

# Convenience: run the built exe (works in MSYS terminals)
run: $(NAME)
	./$(NAME)

# --- Bootstrapping for MSYS2 (installs toolchain and required packages) ---
# Usage: make -f Makefile.win bootstrap
bootstrap:
	@if command -v pacman >/dev/null 2>&1; then \
		echo "$(BLUE)Installing MSYS2 MinGW packages...$(WHITE)"; \
		pacman -Sy --noconfirm; \
		pacman -S --needed --noconfirm \
		  base-devel \
		  git curl zip \
		  mingw-w64-x86_64-toolchain \
		  mingw-w64-x86_64-glfw \
		  mingw-w64-x86_64-glew; \
		echo "$(GREEN)Bootstrap complete. You can now run: make -f Makefile.win$(EOC)"; \
	else \
		echo "$(RED)pacman not found. Please build inside MSYS2 MinGW64 shell.$(EOC)"; \
		echo "Download MSYS2: https://www.msys2.org/"; \
		exit 1; \
	fi

# --- Packaging: bundle EXE + required DLLs + assets into dist/ ---
MINGW_PREFIX ?= /mingw64
MINGW_BINDIR  = $(MINGW_PREFIX)/bin
DIST_DIR      = dist/ft_vox_win64
DLLS          = glew32.dll glfw3.dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll

package: $(NAME)
	@echo "$(BLUE)Packaging distribution...$(WHITE)"
	@rm -rf $(DIST_DIR)
	@mkdir -p $(DIST_DIR)
	@cp -r shaders $(DIST_DIR)/
	@cp -r textures $(DIST_DIR)/
	@cp $(NAME) $(DIST_DIR)/
	@for d in $(DLLS); do \
		if [ -f $(MINGW_BINDIR)/$$d ]; then \
			cp $(MINGW_BINDIR)/$$d $(DIST_DIR)/; \
		else \
			echo "$(CYAN)Warning: $$d not found in $(MINGW_BINDIR)$(WHITE)"; \
		fi; \
	done
	@printf "@echo off\r\n" > $(DIST_DIR)/run.bat
	@printf "rem Ensure working directory is the script's folder\r\n" >> $(DIST_DIR)/run.bat
	@printf "cd /d \"%%~dp0\"\r\n" >> $(DIST_DIR)/run.bat
	@printf "echo Launching ft_vox...\r\n" >> $(DIST_DIR)/run.bat
	@printf ".\\ft_vox.exe %%*\r\n" >> $(DIST_DIR)/run.bat
	@printf "ft_vox — Windows build (MinGW)\n\nRun: run.bat or ft_vox.exe\n\nControls:\n- WASD to move\n- Mouse to look\n\n" > $(DIST_DIR)/README.txt
	@echo "$(GREEN)Package ready at $(DIST_DIR) $(EOC)"

package-zip: package
	@cd dist && zip -r ft_vox_win64.zip ft_vox_win64 >/dev/null && cd - >/dev/null
	@echo "$(GREEN)Created dist/ft_vox_win64.zip $(EOC)"
