/**
 * @file signal_handler.h
 * @brief Global signal handling for ComChemKit
 * @author Le Nhan Pham
 * @date 2026
 *
 * This header defines execution-level utility functions that run across the
 * entire application, such as setting up graceful shutdown signal handlers
 * for long-running computational jobs.
 */

#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

/**
 * @brief Sets up OS signal handlers for graceful shutdown
 * 
 * Registers handlers for SIGINT (Ctrl+C) and SIGTERM. This allows the
 * application to gracefully terminate long-running operations and
 * safely release resources before exiting.
 */
void setup_signal_handlers();

#endif  // SIGNAL_HANDLER_H
