#include "PipeWriter.h"

#ifdef _WIN32
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>

namespace ff
{
static std::wstring widen (const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar (CP_UTF8, 0, s.c_str(), (int) s.size(), nullptr, 0);
    std::wstring w ((size_t) n, L'\0');
    MultiByteToWideChar (CP_UTF8, 0, s.c_str(), (int) s.size(), &w[0], n);
    return w;
}

bool PipeWriter::open (const std::string& exe, const std::vector<std::string>& args, const std::string& logFile)
{
    close();
    SECURITY_ATTRIBUTES sa { sizeof (sa), nullptr, TRUE };
    HANDLE rd = nullptr, wr = nullptr;
    if (! CreatePipe (&rd, &wr, &sa, 1 << 22)) return false;
    SetHandleInformation (wr, HANDLE_FLAG_INHERIT, 0);

    HANDLE log = CreateFileW (widen (logFile).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE)
        log = CreateFileW (L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

    std::wstring cmd = L"\"" + widen (exe) + L"\"";
    for (auto& a : args) cmd += L" \"" + widen (a) + L"\"";

    STARTUPINFOW si {};
    si.cb = sizeof (si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = rd;
    si.hStdOutput = log;
    si.hStdError = log;
    PROCESS_INFORMATION pi {};
    std::vector<wchar_t> buf (cmd.begin(), cmd.end());
    buf.push_back (0);
    const BOOL ok = CreateProcessW (nullptr, buf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle (rd);
    if (log != INVALID_HANDLE_VALUE) CloseHandle (log);
    if (! ok) { CloseHandle (wr); return false; }
    CloseHandle (pi.hThread);
    handle = wr;
    process = pi.hProcess;
    return true;
}

bool PipeWriter::write (const void* data, size_t bytes)
{
    if (handle == nullptr) return false;
    auto* p = static_cast<const char*> (data);
    while (bytes > 0)
    {
        DWORD done = 0;
        if (! WriteFile ((HANDLE) handle, p, (DWORD) (bytes > (1u << 20) ? (1u << 20) : bytes), &done, nullptr) || done == 0) return false;
        p += done; bytes -= done;
    }
    return true;
}

int PipeWriter::close()
{
    int code = -1;
    if (handle != nullptr) { CloseHandle ((HANDLE) handle); handle = nullptr; }
    if (process != nullptr)
    {
        WaitForSingleObject ((HANDLE) process, INFINITE);
        DWORD c = 0; GetExitCodeProcess ((HANDLE) process, &c); code = (int) c;
        CloseHandle ((HANDLE) process); process = nullptr;
    }
    return code;
}
}
#else
 #include <cstdio>
 #include <csignal>
 #include <sys/wait.h>

namespace ff
{
static std::string shq (const std::string& s)
{
    std::string r = "'";
    for (char c : s) { if (c == '\'') r += "'\\''"; else r += c; }
    return r + "'";
}

bool PipeWriter::open (const std::string& exe, const std::vector<std::string>& args, const std::string& logFile)
{
    close();
    signal (SIGPIPE, SIG_IGN);
    std::string cmd = shq (exe);
    for (auto& a : args) cmd += " " + shq (a);
    cmd += " > " + shq (logFile) + " 2>&1";
    handle = popen (cmd.c_str(), "w");
    return handle != nullptr;
}

bool PipeWriter::write (const void* data, size_t bytes)
{
    return handle != nullptr && fwrite (data, 1, bytes, (FILE*) handle) == bytes;
}

int PipeWriter::close()
{
    if (handle == nullptr) return -1;
    int st = pclose ((FILE*) handle);
    handle = nullptr;
    return WIFEXITED (st) ? WEXITSTATUS (st) : -1;
}
}
#endif
