#include "ft_vox.hpp"
#include "StoneEngine.hpp"
#include <cstdlib>

bool isWSL() {
	return (std::getenv("WSL_DISTRO_NAME") != nullptr); // WSL_DISTRO_NAME is set in WSL
}

int main(int argc, char **argv)
{
    int seed = 42;
    if (argc == 2)
    {
        seed = atoi(argv[1]);
    }
    // Under sanitizers, Mesa's driver threads can trigger benign data race
    // reports. Disabling Mesa's multi-threaded GL dispatch reduces noise.
    // This environment hint is harmless if unsupported.
    setenv("MESA_GLTHREAD", "0", 1);
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
	ThreadPool pool(std::thread::hardware_concurrency());
	StoneEngine stone(seed, pool);
	stone.run();
	// Ensure all worker threads are stopped before tearing down world/GL
	pool.joinThreads();
	return 0;
}
