#pragma once
#include <string>
#include <vector>

namespace ff
{
/** Starts a child process (ffmpeg) and writes raw bytes to its stdin. No JUCE / windows.h in this header. */
class PipeWriter
{
public:
    ~PipeWriter() { close(); }
    bool open (const std::string& exeUtf8, const std::vector<std::string>& argsUtf8, const std::string& logFileUtf8);
    bool write (const void* data, size_t bytes);
    int close();     // waits for the process, returns exit code (-1 if never started)
    bool isOpen() const { return handle != nullptr; }
private:
    void* handle = nullptr;
    void* process = nullptr;
};
}
