// Copyright 2022-2023 The MathWorks, Inc.
#include <mutex>
#include <condition_variable>

class binarySemp
{
    public:

    bool check_availability()
    {
        std::unique_lock<std::mutex> locker(mut);
        return state;
    }

    void acquire() // blocking call
    {
        // locker maintains the mutex - automatically freeing after "out of scope"
        std::unique_lock<std::mutex> locker(mut);
        
        // Wait suspends this thread. locker unlocks the mutex while doing so.
        //  wakes up only after notify is received from another thread
        //  sometimes spurious wake ups happen, which we check using state == true function.
        cv.wait(locker, [this](){ return state == true;});

        // Once state is available or freed somewhere, consume it off.
        state = false;
    }

    void release()
    {
        std::unique_lock<std::mutex> locker(mut);
        state = true;
        locker.unlock(); // mutex lock not needed anymore. not sure why we have to explicitly call this.
        cv.notify_one();
    }

    private:
    std::mutex mut;
    std::condition_variable cv;
    bool state = false; // 0 means it is blocked by someone
};