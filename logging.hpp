#ifndef LOGGING_H

#define LOGGING_H

#include <mutex>
#include <iostream>

std::mutex io_mutex;

void log(std::string message)
{
    std::lock_guard<std::mutex> lock(io_mutex);
    std::cout << message << std::endl;
}

#endif // LOGGING_H
