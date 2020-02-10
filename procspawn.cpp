#include "procspawn.h"

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <cstring>
#include <algorithm>
#include <atomic>

#define PIPE_READ 0
#define PIPE_WRITE 1

using namespace std;

class ProcessSpawn::Priv {
public:
    atomic<bool> ready;
    atomic<bool> inputStopped;
    atomic<bool> stopped;
    pid_t childPid;
    struct {
        int childStdin[2];
        int childStdout[2];
    } pipes;
};

ProcessSpawn::ProcessSpawn(char* exe, char** args) {
    p = make_unique<Priv>();

    p->ready = false;
    p->inputStopped = false;
    p->stopped = false;
    
    // create pipe for child's stdin
    if(pipe2(p->pipes.childStdin, 0) < 0) {
        perror("Failed to allocate pipe for child's stdin.");
        return;
    }
    //we'd like for nonblocking read
    if(pipe2(p->pipes.childStdout, 0)) {
        perror("Failed to allocate pipe for child's stdout.");
        close(p->pipes.childStdin[0]);
        close(p->pipes.childStdin[1]);
        return;
    }

    // spawn a new process
    if((p->childPid = fork())) {
        // I am the parent
        // close unused pipes
        close(p->pipes.childStdin[PIPE_READ]);
        close(p->pipes.childStdout[PIPE_WRITE]);
        // continue business
        p->ready = true;
    }
    else {
        // I am the child
        // "connect" stdin ...
        if(dup2(p->pipes.childStdin[PIPE_READ], STDIN_FILENO) < 0) {
            perror("Child: error connecting stdin");
            exit(errno);
        }
        // ... and stdout
        if(dup2(p->pipes.childStdout[PIPE_WRITE], STDOUT_FILENO) < 0) {
            perror("Child: error connecting stdout");
            exit(errno);
        }

        // since we've replaced the stdin and stdout, we as a child don't
        // need to keep all pipes in the "pipes" variable, so discard all of them.
        for(int i=0; i<4; ++i) {
            close(((int*)&p->pipes)[i]);
        }

        // OK bye!
        if(execvpe(exe, args, nullptr) < 0) {
            perror("Child: exec error");
            exit(errno);
        }
    }
}

ProcessSpawn::~ProcessSpawn() {
    // force stop by default. we expect caller to call stop manually.
    this->stop(true);
}

bool ProcessSpawn::isReady() {
    if(p->ready) {
        // check if child doesn't return error
        int childRet = -2;
        if(waitpid(p->childPid, &childRet, WNOHANG) == 0)
            return true;
        printf("Child process returned error %d\n", childRet);
        p->ready = false;
        return false;
    }
    return false;
}

void ProcessSpawn::stopInput() {
    if(p->stopped || p->inputStopped || !p->ready)
        return;
    // equiv to EOF
    p->inputStopped = true;
    close(p->pipes.childStdin[PIPE_WRITE]);
}

void ProcessSpawn::stop(bool force) {
    if(p->stopped)
        return;
    p->ready = false;
    p->stopped = true;

    // close the pipes (equiv to EOF)
    close(p->pipes.childStdin[PIPE_WRITE]);
    close(p->pipes.childStdout[PIPE_READ]);
    
    int childRet;
    // we don't care about child's return this time!
    if(force)
        waitpid(p->childPid, &childRet, 0);
}

void ProcessSpawn::writeData(const void* buff, size_t size) {
    if(!p->ready || p->inputStopped)
        return;
    ssize_t wr = write(p->pipes.childStdin[PIPE_WRITE], buff, size);
    if(wr < size) {
        perror("Pipe write error");
        p->ready = false;
    }
}

size_t ProcessSpawn::readData(void* buff, size_t size) {
    if(!p->ready)
        return 0;
    
    // SIGNED size_t
    ssize_t ret;
    size_t numRead = 0;
    uint8_t* cBuff = (uint8_t*)buff;

    fd_set rdSet;

    while(size-numRead) {
        ret = read(p->pipes.childStdout[PIPE_READ], cBuff + numRead, min(size,size-numRead));
        if(ret < 0) {
            perror("Pipe read error");
            p->ready = false;
            return numRead;
        }
        if(!ret) {
            // EOF, no more data to read :(
            return numRead;
        }
        numRead += ret;
    }
    
    // number of bytes read
    return numRead;
}