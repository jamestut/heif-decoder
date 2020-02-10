#pragma once

#include <memory>
#include <stddef.h>

class ProcessSpawn {
public:
    ProcessSpawn(char* exe, char** args);
    ~ProcessSpawn();
    bool isReady();
    void stopInput();
    void stop(bool force);
    void writeData(const void* buff, size_t size);
    size_t readData(void* buff, size_t size);

private:
    class Priv;
    std::unique_ptr<Priv> p;
};