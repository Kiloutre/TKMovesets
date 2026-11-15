#pragma once
#include <chrono>
#include <algorithm>

// Worker-owned: the regular 200 ms status tick does not imply a process scan.
class AttachRetryBackoff {
    using Clock = std::chrono::steady_clock;
    Clock::time_point m_next = (Clock::time_point::min)();
    std::chrono::milliseconds m_delay{1000};
public:
    bool Ready(Clock::time_point now) const { return now >= m_next; }
    void Failed(Clock::time_point now) {
        m_next = now + m_delay;
        m_delay = (std::min)(m_delay * 2, std::chrono::milliseconds(10000));
    }
    void Reset() { m_next = (Clock::time_point::min)(); m_delay = std::chrono::milliseconds(1000); }
};
