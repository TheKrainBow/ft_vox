#pragma once

#include <chrono>
#include <string>
#include <map>
#include <mutex>

class Chrono
{
private:
    struct ChronoData {
        std::string label;
        std::chrono::_V2::system_clock::time_point start;
        std::chrono::_V2::system_clock::time_point end;
    };
    std::map<int, ChronoData> _chronos;
    enum ChronoType {
        PERLIN,
        CHUNK_LOADING,
        TOTAL_CHUNK_LOADING,
    };
	std::mutex	mic;
	std::mutex chronosMutex;
public:
    Chrono();
    ~Chrono();
    void startChrono(int index, std::string label);
    void stopChrono(int index);
    void printChronos(void);
	void printChrono(size_t index);
};
