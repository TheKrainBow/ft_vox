#include "Chrono.hpp"
#include <iomanip>
#include <iostream>

Chrono::Chrono() {}
Chrono::~Chrono() {}

void Chrono::startChrono(int index, std::string label)
{
	Chrono::ChronoData newChrono;
	newChrono.start = std::chrono::high_resolution_clock::now(); 
	newChrono.label = label;
	chronosMutex.lock();
	_chronos[index] = newChrono;
	chronosMutex.unlock();
}

void Chrono::stopChrono(int index)
{
	_chronos[index].end = std::chrono::high_resolution_clock::now();
}

void Chrono::printChronos(void)
{
	std::lock_guard<std::mutex> lock(mic);
	for (auto &chrono : _chronos)
	{
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(chrono.second.end - chrono.second.start);
		long long totalMilliseconds = duration.count();
		long long seconds = totalMilliseconds / 1000;
		long long milliseconds = totalMilliseconds % 1000;

		std::stringstream ss;
		ss << seconds << ',' << std::setw(3) << std::setfill('0') << milliseconds << "s";
		std::cout << chrono.second.label << ": " << ss.str() << std::endl;
	}
}

void Chrono::printChrono(size_t index)
{
	std::lock_guard<std::mutex> lock(mic);
	const auto &chrono = _chronos[index];
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(chrono.end - chrono.start);
	long long totalMilliseconds = duration.count();
	long long seconds = totalMilliseconds / 1000;
	long long milliseconds = totalMilliseconds % 1000;

	std::stringstream ss;
	ss << seconds << ',' << std::setw(3) << std::setfill('0') << milliseconds << "s";
	std::cout << chrono.label << ": " << ss.str() << std::endl;
}
