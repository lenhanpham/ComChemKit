/**
 * @file command_registry.cpp
 * @brief Implementation of the CommandRegistry class for managing application commands.
 * @author Le Nhan Pham
 * @date 2026
 * 
 * Contains the Singleton logic for maintaining the map of commands and routing
 * them throughout the application based on CLI invocations.
 */
#include "commands/command_registry.h"

CommandRegistry& CommandRegistry::get_instance() {
    static CommandRegistry instance;
    return instance;
}

void CommandRegistry::register_command(std::unique_ptr<ICommand> command) {
    if (command) {
        commands[command->get_name()] = std::move(command);
    }
}

ICommand* CommandRegistry::get_command(const std::string& name) const {
    auto it = commands.find(name);
    if (it != commands.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::map<std::string, ICommand*> CommandRegistry::get_all_commands() const {
    std::map<std::string, ICommand*> result;
    for (const auto& pair : commands) {
        result[pair.first] = pair.second.get();
    }
    return result;
}
