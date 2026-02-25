/**
 * @file command_registry.h
 * @brief Defines the CommandRegistry class for managing application commands.
 * @author Le Nhan Pham
 * @date 2026
 *
 * The CommandRegistry acts as a central hub where all ICommand implementations
 * are registered and resolved. This allows for dynamic and decoupled command
 * execution across the ComChemKit utility.
 */

#ifndef COMMAND_REGISTRY_H
#define COMMAND_REGISTRY_H

#include "commands/icommand.h"
#include <memory>
#include <map>
#include <string>

/**
 * @class CommandRegistry
 * @brief A singleton registry that maintains all available commands in the application.
 *
 * This class applies the Singleton design pattern to provide a centralized
 * repository for ICommand instances. It maps command names (strings) to their
 * respective ICommand implementations, allowing the application to dynamically
 * resolve and dispatch command execution.
 */
class CommandRegistry {
public:
    /**
     * @brief Retrieves the singleton instance of the CommandRegistry.
     * 
     * @return CommandRegistry& Reference to the single target instance.
     */
    static CommandRegistry& get_instance();

    /**
     * @brief Registers a new command with the registry.
     * 
     * @param command A unique pointer to the command being registered. The registry takes ownership.
     */
    void register_command(std::unique_ptr<ICommand> command);

    /**
     * @brief Retrieves a command by its name.
     * 
     * @param name The name of the command to retrieve.
     * @return ICommand* Pointer to the command if found, or nullptr if not registered.
     */
    ICommand* get_command(const std::string& name) const;

    /**
     * @brief Retrieves all registered commands.
     * 
     * @return std::map<std::string, ICommand*> A map of all registered command names and their pointers.
     */
    std::map<std::string, ICommand*> get_all_commands() const;

private:
    CommandRegistry() = default;
    ~CommandRegistry() = default;
    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;

    std::map<std::string, std::unique_ptr<ICommand>> commands;
};

#endif // COMMAND_REGISTRY_H
