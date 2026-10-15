#pragma once

#include <climits>
#include <cstdio>
#include <string>
#include <windows.h>

namespace KlakSpout {

// Spout frame count and texture access mutex for one sender, compatible with
// the Spout 2 SDK's spoutFrameCount (SpoutGL/SpoutFrameCount.cpp):
//
// - "<sender>_Count_Semaphore": the sender raises the count on every new frame,
//   so receivers that read it (spoutFrameCount::GetNewFrame) skip frames they
//   have already received instead of copying the same texture again.
// - "<sender>_SpoutAccessMutex": held while the shared texture is written, so
//   a receiver does not read a frame halfway through the copy.
//
// Unlike the SDK class, this does not depend on the "Framecount" registry
// setting: the sender always counts. A receiver that does not use frame
// counting is unaffected.
class FrameCount final
{
public:

    explicit FrameCount(const std::string& sender)
    {
        auto count = sender + "_Count_Semaphore";
        // Initial count 1, as in the SDK; either side may create it first.
        _semaphore = CreateSemaphoreA(nullptr, 1, LONG_MAX, count.c_str());

        auto access = sender + "_SpoutAccessMutex";
        _mutex = CreateMutexA(nullptr, FALSE, access.c_str());
    }

    ~FrameCount()
    {
        if (_semaphore) CloseHandle(_semaphore);
        if (_mutex) CloseHandle(_mutex);
    }

    FrameCount(const FrameCount&) = delete;
    FrameCount& operator=(const FrameCount&) = delete;

    // Waits for receivers to finish reading. Returns false on timeout; the
    // caller writes anyway (as the SDK does) but must not call EndAccess.
    bool BeginAccess()
    {
        if (!_mutex) return false;
        // Same timeout as the SDK (4 frames at 60 fps).
        auto result = WaitForSingleObject(_mutex, 67);
        return result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
    }

    void EndAccess()
    {
        if (_mutex) ReleaseMutex(_mutex);
    }

    // spoutFrameCount::SetNewFrame: take one count and give two back, so the
    // count goes up by one per frame.
    void SetNewFrame()
    {
        if (!_semaphore) return;
        if (WaitForSingleObject(_semaphore, 0) == WAIT_OBJECT_0)
            ReleaseSemaphore(_semaphore, 2, nullptr);
    }

private:

    HANDLE _semaphore = nullptr;
    HANDLE _mutex = nullptr;
};

} // namespace KlakSpout
