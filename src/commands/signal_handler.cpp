/**
 * @file signal_handler.cpp
 * @brief Implementation of global signal handlers
 * @author Le Nhan Pham
 * @date 2026
 */

#include "commands/signal_handler.h"
#include <atomic>
#include <csignal>
#include <iostream>

// External global variable for shutdown handling
extern std::atomic<bool> g_shutdown_requested;
extern volatile std::sig_atomic_t g_shutdown_signal;

/**
 * @brief Internal callback function for OS signals
 * @param signal The received signal number (e.g., SIGINT)
 */
void signal_handler_func(int signal)
{
    g_shutdown_signal = signal;
    g_shutdown_requested.store(true);
}

void setup_signal_handlers()
{
    std::signal(SIGINT, signal_handler_func);
    std::signal(SIGTERM, signal_handler_func);
}
