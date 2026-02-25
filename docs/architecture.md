# ComChemKit Architecture & Developer Guide

This document provides a comprehensive overview of the **ComChemKit** (CCK) architecture. It explains how the modules are structured, how the application flow works, and serves as a vital resource for any developer looking to understand or extend the codebase.

## 1. High-Level Architecture

ComChemKit follows a modular architecture centered around a **Command Pattern**. The application is designed to be highly scalable, allowing new computational chemistry tools (commands) to be plugged in trivially without modifying the core execution engine.

### Core Architecture Components:

1.  **Entry Point (`main.cpp`)**: Bootstraps the application, parses the initial configuration, initializes the `CommandRegistry`, and routes execution either to the Interactive CLI or the terminal command dispatcher.
2.  **Command System (`src/commands/`)**: The beating heart of the application. It maps string inputs from the terminal (e.g., `"extract"`, `"high-kj"`) to specific execution classes via the `CommandRegistry`.
3.  **Feature Modules (`src/extraction/`, `src/thermo/`, etc.)**: The actual "business logic". These modules perform the heavy lifting (parsing Gaussian files, calculating thermodynamic corrections, extracting XYZ coordinates, etc.) but are cleanly decoupled from the CLI argument parsing.
4.  **Utilities & Core Subsystems (`src/utilities/`)**: Provides foundational resources shared across all modules, including:
    -   `ConfigManager`: Centralized file-based configuration (`.qc_extractor.conf`).
    -   Metadata parsing and version control.
    -   Shared filesystem utilities and error logging.

---

## 2. The Command Pattern Workflow

The most critical aspect of the CCK architecture is how it handles actions through the **Command Pattern**.

### Key Interfaces

*   **`ICommand` (`src/commands/icommand.h`)**: The base interface that every actionable command must implement.
    ```cpp
    class ICommand {
    public:
        virtual ~ICommand() = default;
        virtual std::string get_name() const = 0;
        virtual std::string get_description() const = 0;
        
        // Custom argument parser for this specific command's flags
        virtual void parse_args(int argc, char* argv[], int& i, CommandContext& context) = 0;
        
        // The actual execution logic for this command
        virtual int execute(const CommandContext& context) = 0;
    };
    ```

*   **`CommandRegistry` (`src/commands/command_registry.h`)**: A singleton that holds all active `ICommand` instances.

*   **`CommandContext` (`src/commands/command_system.h`)**: A structured context passed around containing *common* parsed arguments (e.g., `--quiet`, `--threads`).

### Execution Flow (Terminal to Output)

1.  **Bootstrapping**: Upon startup, `main.cpp` registers every known command into the `CommandRegistry`:
    ```cpp
    auto& registry = CommandRegistry::get_instance();
    registry.register_command(std::make_unique<ExtractCommand>());
    registry.register_command(std::make_unique<ThermoCommand>());
    // ...
    ```
2.  **Parsing Routing (`CommandParser::parse`)**:
    *   The `CommandParser` inspects `argv[1]` to identify the target command (e.g., `cck thermo`).
    *   It extracts all *common* options (like `-nt 4` or `-q`) and stores them in the `CommandContext`.
    *   It then dynamically retrieves the registered `ICommand` from the registry and delegates string parsing to the specific command's `cmd->parse_args(...)` method for specialized flags (like `--format csv`).
3.  **Execution (`cmd->execute`)**:
    *   The `CommandParser` hands the fully populated `CommandContext` back to `main.cpp`.
    *   `main.cpp` calls `cmd->execute(context)` on the dynamically resolved command object.
    *   The specific command (e.g., `ExtractCommand`) extracts its specific state and invokes the heavy-lifting logic in the Feature Modules (e.g., `QcExtractor`).

---

## 3. Directory & Module Structure Map

Here is the functional breakdown of the `src/` directory.

### `src/commands/` (Command Routing)
*The routing layer interconnecting the UI/CLI with the backend modules.*

*   `command_registry.*`, `command_system.*`, `icommand.h`: The Command Pattern infrastructure.
*   `signal_handler.*`: OS-level interrupt hooking (Ctrl+C graceful shutdowns).
*   **Concrete Commands**:
    *   `extract_command.cpp`: Dispatches to `qc_extractor`.
    *   `thermo_command.cpp`: Dispatches to the `thermo` advanced analysis module.
    *   `create_input_command.cpp`: Dispatches to `input_gen`.
    *   `checker_command.cpp`: Dispatches to `job_checker`.
    *   `high_level_command.cpp`: Dispatches to `high_level_energy`.
    *   `extract_coords_command.cpp`: Dispatches to `coord_extractor`.

> [!TIP]
> **To add a new feature to the CLI**:
> 1. Create a logic module in a new `src/<feature>/` folder.
> 2. Create a `MyFeatureCommand` class in `src/commands/` inheriting from `ICommand`.
> 3. Register your class in `main.cpp`. Done! The CLI instantly supports your command.

### `src/thermo/` (Advanced Thermodynamics)
*A deeply featured module dedicated to computing high-accuracy thermodynamic properties.*

*   **Standalone Capability**: This module is notably dense, encompassing symmetry detection, point group analysis, moments of inertia computation, and partition functions for molecules. 
*   **Structs**: It uses decoupled structs like `ThermoSettings` rather than relying on the CLI's `CommandContext`, meaning this module can be unit-tested seamlessly or integrated as a library in other pure C++ projects.

### `src/extraction/` (Data Mining)
*High-performance file parsing.*

*   **`qc_extractor.*`**: Multi-threaded core specifically tuned to tear down massive Gaussian `.log`/`.out` files, caching thermodynamic blocks using parallel atomic parsing.
*   **`coord_extractor.*`**: Specifically mines and outputs the terminal `.xyz` coordinate blocks from quantum chemistry jobs.

### `src/job_management/` (Cluster Operations)
*Tools to ensure job stability on massive parallel compute clusters.*

*   **`job_checker.*`**: Scans output files to automatically classify jobs into bins (e.g., moving `Normal termination` jobs into a `/done` folder or flagging `PCMMkU` errors).
*   **`job_scheduler.*`**: Integrates with SLURM/PBS/SGE batch managers to intelligently restrict thread usage so ComChemKit doesn't oversubscribe compute nodes assigned to users.

### `src/input_gen/` (Input Generation)
*Templating engine for computational chemistry.*

*   **`create_input.*`**: Consumes `.xyz` templates and orchestrates batch Gaussian `.com`/`.gau` file generation based on predefined method/basis-set parameters.

### `src/high_level/`
*Specialized energy calculators combining calculations.*

*   **`high_level_energy.*`**: Implements composite logic (like deriving absolute zero point energies combined from external high-level electronic energy sp calculations).

### `src/ui/` (Interactive Interfaces)
*User-facing presentation logic.*

*   **`interactive_mode.*`**: An interactive loop (a customized shell environment within ComChemKit) providing a prompt (`cck>`) instead of single-shot terminal executions. Heavily reliant on string parsing.
*   **`help_utils.*`**: Centralized console print blocks for user `-h` or `--help` menus.

### `src/utilities/` (Foundational Library)

*   **`config_manager.*`**: Reads `~/.qc_extractor.conf` and injects default environmental states (like default thread limits) globally.
*   **`utils.*`**: String trimming, file system traversal wrapping (finding logs recursively), and general standard library helpers.

---

## 4. Concurrency and Resource Management

Performance is a primary pillar of ComChemKit. Processing thousands of multi-gigabyte Gaussian log files necessitates strict concurrency management.

1. **Multi-threading Engine**: Core batch iterations natively use OpenMP or Threading Building Blocks (TBB) parallel `for` loops. The `CommandContext` actively governs `requested_threads`.
2. **Resource Constraints**:
   * The `JobScheduler` subsystem checks environmental limits (e.g., `SLURM_CPUS_PER_TASK`) and will throttle `requested_threads` downward if the user requests more cores than the scheduler physically allocated them.
   * `ProcessingContext` dynamically governs maximum file load limits in memory to prevent `std::bad_alloc` crashes on massive directories.

## 5. Graceful Shutdown

Because operations (like extracting 50,000 files via `ExtractCommand`) take time, CCK incorporates a global `signal_handler`. 

Catching `SIGINT` (Ctrl+C) raises a global atomic boolean `g_shutdown_requested`. Loops inside `qc_extractor` and `thermo` periodically check this boolean to safely release memory, cleanly close output I/O streams (`.csv` files), and terminate safely, rather than corrupting files in-flight via a hard kill.
