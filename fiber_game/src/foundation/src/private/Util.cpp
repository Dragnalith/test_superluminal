#pragma once

#include <fnd/Util.h>

namespace engine
{

void RandomWorkload(int microsecond, int random_percent) {
    auto start = Clock::now();
    while (true) {
        auto now = Clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
        if (us >= microsecond) {
            return;
        }
    }
}

}