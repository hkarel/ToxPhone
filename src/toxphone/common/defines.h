#pragma once

#define CHECK_THREAD_STOP \
    if (threadStop()) { \
        log_debug_m << "Thread stop command received"; \
        break; \
    }
