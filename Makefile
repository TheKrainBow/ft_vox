NAME		=	ft_vox
DEBUG_NAME	=	ft_voxDebug

LDFLAGS =	-lGL -lGLU -Llib64 -lGLEW -lglfw

CFLAGS	=	-Wall -Wextra -Werror -O3 -std=c++17 -g3 #-fsanitize=address
DEBUG_CFLAGS	=	-DNDEBUG -Wall -Wextra -Werror -g3

OBJ_PATH		=	obj/
DEBUG_OBJ_PATH		=	debug_obj/

CC			=	g++
SRC_PATH	=	srcs/
INCLUDES	=	-Iincludes -Iglm

# Try to detect system-installed dependencies via pkg-config
PKG_CFLAGS	:= $(shell pkg-config --cflags glew glfw3 2>/dev/null)
PKG_LIBS	:= $(shell pkg-config --libs glew glfw3 2>/dev/null)

# Append detected flags (no-op if pkg-config not available)
CFLAGS		+= $(PKG_CFLAGS)
DEBUG_CFLAGS	+= $(PKG_CFLAGS)
LDFLAGS		+= $(PKG_LIBS)

# Tools for fetching dependencies
CURL		?=	curl -L --fail --silent --show-error
GIT			?=	git

# Third-party headers we vendor locally if missing
STB_IMAGE		=	includes/stb_image.h
STB_TRUETYPE	=	includes/stb_truetype.hpp
GLM_DIR		=	glm
# Provide GLEW header locally if the system one is not present
GLEW_HDR		=	includes/GL/glew.h
GLEW_VER		=	2.2.0
GLEW_SRC_TGZ	=	glew-$(GLEW_VER).tgz
GLEW_SRC_URL	=	https://github.com/nigels-com/glew/releases/download/glew-$(GLEW_VER)/$(GLEW_SRC_TGZ)
GLEW_BUILD_DIR	=	third_party/glew-$(GLEW_VER)
GLEW_LIB		=	lib64/libGLEW.a
	
SRC_NAME	=	stb_truetype.cpp		\
				main.cpp				\
				StoneEngine.cpp			\
				Camera.cpp				\
				TextureManager.cpp		\
				Chunk.cpp				\
				SubChunk.cpp			\
				SubChunk_faces.cpp		\
				NoiseGenerator.cpp		\
				Textbox.cpp				\
				SplineInterpolator.cpp	\
				ChunkManager.cpp		\
				ChunkLoader.cpp			\
				ChunkRenderer.cpp		\
				Chrono.cpp				\
				Shader.cpp				\
				ThreadPool.cpp			\
				Noise3DGenerator.cpp	\
				CaveGenerator.cpp		\
				Skybox.cpp				\
				Raycaster.cpp			\
				Player.cpp

OBJ_NAME	=	$(SRC_NAME:.cpp=.o)
OBJ		=	$(addprefix $(OBJ_PATH), $(OBJ_NAME))
DEBUG_OBJ	=	$(addprefix $(DEBUG_OBJ_PATH), $(OBJ_NAME))

#----------colors---------#
BLACK		=	\033[1;30m
RED			=	\033[1;31m
GREEN		=	\033[1;32m
BLUE		=	\033[1;34m
PURPLE		=	\033[1;35m
CYAN		=	\033[1;36m
WHITE		=	\033[1;37m
EOC			=	\033[0;0m

all: deps $(NAME)

# Fetch local header-only dependencies if they are missing
.PHONY: deps
deps:
		@echo "$(BLUE)Checking dependencies...$(WHITE)"
		@if [ ! -f $(STB_IMAGE) ]; then \
			echo "$(CYAN)Fetching stb_image.h$(WHITE)"; \
			$(CURL) https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -o $(STB_IMAGE); \
		fi
		@if [ ! -f $(STB_TRUETYPE) ]; then \
			echo "$(CYAN)Fetching stb_truetype as .hpp$(WHITE)"; \
			$(CURL) https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h -o $(STB_TRUETYPE); \
		fi
		@if [ ! -d $(GLM_DIR)/glm ]; then \
			echo "$(CYAN)Cloning GLM headers$(WHITE)"; \
			$(GIT) clone --depth 1 https://github.com/g-truc/glm.git $(GLM_DIR); \
		fi
		@if [ ! -f $(GLEW_HDR) ]; then \
			echo "$(CYAN)Fetching GLEW header (includes/GL/glew.h)$(WHITE)"; \
			mkdir -p includes/GL; \
			$(CURL) https://raw.githubusercontent.com/Perlmint/glew-cmake/master/include/GL/glew.h -o $(GLEW_HDR); \
		fi
		@# Only build a local static GLEW if system one isn't available
		@if ! pkg-config --exists glew; then \
			if [ ! -f $(GLEW_LIB) ]; then \
				echo "$(CYAN)Building static GLEW locally (for linking)$(WHITE)"; \
				set -e; \
				mkdir -p third_party; \
				$(CURL) $(GLEW_SRC_URL) -o third_party/$(GLEW_SRC_TGZ); \
				tar -C third_party -xf third_party/$(GLEW_SRC_TGZ); \
				$(MAKE) -C $(GLEW_BUILD_DIR) -j; \
				mkdir -p lib64; \
				cp $(GLEW_BUILD_DIR)/lib/libGLEW.a $(GLEW_LIB); \
			fi; \
		fi

$(NAME): $(OBJ)
	@echo "$(RED)=====>Compiling ft_vox Test<===== $(WHITE)"
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJ) -o $(NAME) $(LDFLAGS)
	@echo "$(GREEN)Done ! ✅ $(EOC)"


$(OBJ_PATH)%.o: $(SRC_PATH)%.cpp | deps
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -c $< -o $@

-include $(OBJ:%.o=%.d)

debug: $(DEBUG_NAME)

$(DEBUG_NAME): $(DEBUG_OBJ)
	@echo "$(RED)=====>Compiling ft_vox DEBUG<===== $(WHITE)"
	$(CC) $(DEBUG_CFLAGS) $(INCLUDES) $(DEBUG_OBJ) -o $(DEBUG_NAME) $(LDFLAGS)
	@echo "$(GREEN)Done ! ✅ $(EOC)"

$(DEBUG_OBJ_PATH)%.o: $(SRC_PATH)%.cpp | deps
	mkdir -p $(@D)
	$(CC) $(DEBUG_CFLAGS) $(INCLUDES) -MMD -c $< -o $@

-include $(DEBUG_OBJ:%.o=%.d)

clean:
	@echo "$(CYAN)♻  Cleaning obj files ♻ $(WHITE)"
	rm -rf $(OBJ_PATH)
	rm -rf $(DEBUG_OBJ_PATH)
	@echo "$(GREEN)Done !✅ $(EOC)"

fclean: clean
		@echo "$(CYAN)♻  Cleaning executable ♻ $(WHITE)"
		rm -rf $(NAME)
		rm -rf $(DEBUG_NAME)
		@echo "$(CYAN)♻  Removing fetched headers/libs ♻ $(WHITE)"
		rm -rf $(STB_IMAGE) $(STB_TRUETYPE) $(GLEW_HDR) $(GLEW_LIB) third_party
		rm -rf $(GLM_DIR)
		@echo "$(GREEN)Done !✅ $(EOC)"

re: fclean all
re_debug: fclean debug

.PHONY: all debug clean fclean re re_debug
