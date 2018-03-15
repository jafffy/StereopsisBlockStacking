#include "pch.h"
#include "FramerateController.h"


FramerateController::FramerateController()
{
}


FramerateController::~FramerateController()
{
}

void FramerateController::Start()
{
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&lastTime);
    qpcMaxDelta.QuadPart = frequency.QuadPart / 10;
}

void FramerateController::Tick()
{
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);

    uint64 timeDelta = currentTime.QuadPart - lastTime.QuadPart;
    lastTime.QuadPart = currentTime.QuadPart;

    if (timeDelta > qpcMaxDelta.QuadPart)
    {
        timeDelta = qpcMaxDelta.QuadPart;
    }

    static const unsigned TicksPerSecond = 10'000'000;
    timeDelta *= TicksPerSecond;
    timeDelta /= frequency.QuadPart;

    double dt = static_cast<double>(timeDelta) / TicksPerSecond;

    amountToSleep = 1.0 / wantedFramePerSecond - dt;
    amountToSleep = amountToSleep > 0 ? amountToSleep : 0;

    oneSecTimer += dt;

    currentFramesInSecond++;

    if (oneSecTimer > 1.0) {
        currentFramesInSecond = 0;
        oneSecTimer = 0.0;
    }
}

void FramerateController::Wait()
{
    if (amountToSleep > 0)
        Sleep(amountToSleep * 1000);
}

void FramerateController::SetFramerate(double frameratePerSecond)
{
    wantedFramePerSecond = frameratePerSecond;
}
