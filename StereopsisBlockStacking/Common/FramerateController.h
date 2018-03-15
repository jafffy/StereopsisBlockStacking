#ifndef FRAMERATECONTROLLER_H_
#define FRAMERATECONTROLLER_H_

#include "Common\Singleton.h"

class FramerateController
    : public Singleton<FramerateController>
{
public:
    FramerateController();
    ~FramerateController();

    void Start();

    void Tick();
    void Wait();
    void SetFramerate(double frameratePerSecond);
    bool ShouldPassThisFrame() const { return currentFramesInSecond > wantedFramePerSecond; }
    double GetFPS() const { return wantedFramePerSecond; }

private:
    LARGE_INTEGER lastTime;
    LARGE_INTEGER frequency;
    LARGE_INTEGER qpcMaxDelta;

    double oneSecTimer = 0;

    double wantedFramePerSecond;
    int currentFramesInSecond = 0;
    double amountToSleep;
};

#endif // FRAMERATECONTROLLER_H_
