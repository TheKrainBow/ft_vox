#pragma once

#include <chrono>
#include <string>
#include <map>

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
public:
    Chrono();
    ~Chrono();
    void startChrono(int index, std::string label);
    void stopChrono(int index);
    void printChronos(void);
};
