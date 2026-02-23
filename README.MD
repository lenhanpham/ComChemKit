<p align="center">
  <a href="https://github.com/lenhanpham/ComChemKit">
    <picture>
      <img src="resources/cck-logo.svg" alt="ComChemKit"  style="width: 50%;">
    </picture>
  </a>
</p>
<p align="center">
  <a href="https://en.cppreference.com/w/cpp/17">
    <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17">
  </a>
  <a href="./LICENSE">
    <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="License">
  </a>
  <a href="https://github.com/lenhanpham/ComChemKit/actions/workflows/ci.yml">
    <img src="https://github.com/lenhanpham/ComChemKit/actions/workflows/ci.yml/badge.svg" alt="Build Status">
  </a>
</p>

**ComChemKit** is a high-performance, cluster-safe program designed to enable researchers to interact with computational chemistry programs easily and efficiently. Users can create inputs, analyze data, organize computational tasks (jobs), and calculate further properties with ComChemKit. ComChemKit runs in parallel with comprehensive safety features to prevent system overload and ensure stable operation in shared computing environments.

## Key Features

* **Extract data in Parallel**: Efficiently processes multiple quantum chemistry output files simultaneously with multi-threading
* **Check status of gaussian jobs**: Status of gaussian outputs can be checked and isolated to corresponding directories
* **Thermochemical Property Calculation**: High-performance thermochemistry calculations with parallel processing, supporting Gaussian, ORCA, GAMESS-US, NWChem, CP2K, and VASP
* **Advanced Parallel Features**: Multi-threaded batch processing, thread-safe memory monitoring, automatic load balancing, and HPC cluster integration (SLURM, PBS, SGE, LSF)
* **Automatic Sorting**: Results sorted by energy values for easy analysis
* **Extract coordinates**: Coordinates of quantum chemistry outputs can be extracted
* **Create Input Files**: Generate Gaussian input files from XYZ coordinate files with customizable calculation parameters

**Windows interactive mode**: Windows users can use all features plus popular linux commands in the interactive mode

**Important note**: **Large files (>100MB) automatically skipped by default**. Users need to set flag --max-file-size xxxxx to increase the size limit

## Full features and User Manual:

**Visit**: [ComChemKit documentation](https://lenhanpham.github.io/ComChemKit/)

## Quick Start

Suppose that you have cck in a dir where you have exported it to your PATH

```bash
cd to_the_dir_where_your_outputs_are_located
```

**Basic extraction** (processes all .log files in current directory):

```bash
cck
```

**Default behavior:**

* **Temperature**: Read from files (default: 298.15 K if not found)
* **Concentration**: 1 M for phase correction
* **Threads**: Half of available CPU cores (cluster-safe)
* **Output**: Sorted by Gibbs free energy (kJ/mol)
* **Format**: A table of text

## Installation and Compilation

### Windows

- **A binary package is included in the release**. Just download this package and extract it to use the program. Users should add the program to PATH system variable and then interact with ComChemKit through Windows terminal

- Windows users can use ComChemKit (cck) in interactive mode directly: double click and type commands

- Use windows terminal (powershell) to run ComChemKit.exe in a folder where your quantum chemical log files are available.

### Linux

#### Prerequisites

**Compiler Requirements:**

* **C++17 or newer**: GCC 10+, Intel oneAPI, Intel Classic, Clang 10+, or MSVC 2019+
* **Threading Support**: pthread library (Linux/macOS) or Windows threading
* **Filesystem Support**: std::filesystem (included in modern compilers)

#### Compilation Options

**Note that I do not use MacOS computers or servers to test the program; I am not sure if it is going to work properly on MacOS. Only linux and Windows were tested well.**

##### Option 1: Automatic Build (Recommended)

**Basic Build (Auto-detects Compiler):**

```bash
# Load your preferred compiler module if on a cluster
module load intel-compiler-llvm         # For Intel compilers
module load intel-tbb                   # For intel Threading Building Blocks
# OR
module load gcc                         # For GCC

# Build with auto-detected compiler (priority: icpx > icpc > icc > g++)
make -j 10                  # Automatically selects best available compiler
```

If ComChemKit is complied with intel compilers (icpx for example), Threading Building Blocks (tbb) library should be loaded. Users might need to export this library to their LD_LIBRARY_PATH:

```
echo 'export LD_LIBRARY_PATH=/path/to/tbb/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc
```

**Build Variants:**

```bash
make cluster -j 8           # Cluster-optimized build
make debug                  # Debug build with safety checks
make release                # High-performance release
```

**Force Specific Compiler:**

```bash
# Intel oneAPI compiler
CXX=icpx make -j 8

# Intel Classic compiler
CXX=icpc make -j 8

# GNU compiler
CXX=g++ make -j 8
```

##### Option 2: CMake (Cross-platform)

**Basic CMake Build (Auto-detects Compiler):**

```bash
# Load your preferred compiler module if on a cluster
module load intel-compiler-llvm         # For Intel compilers
module load intel-tbb                   # For intel Threading Building Blocks
# OR
module load gcc                         # For GCC

# Create build directory
mkdir build && cd build

# Configure with auto-detected compiler (priority: icpx > icpc > icc > g++)
cmake ..

# Build with parallel jobs
cmake --build . -j 10
```

```bash
# System-wide installation (requires sudo)
make install

# User installation
make install-user

# Or add to PATH manually
export PATH=$PATH:/path/to/ComChemKit

# Or copy cck to your ~/bin/ . Note that if your ~/bin/ does not exist, you need to mkdir ~/bin
cp cck ~/bin/
```

## Core Modules

### 3.1 Data Extraction Module

The Data Extraction Module processes quantum chemistry output files and extracts thermochemical properties, energies, frequencies, and job status information.

***Note: currently, only outputs from Gaussian are supported. Other programs will be supported soon***

#### Basic Usage

**Default extraction** (processes all .log files in current directory):

```bash
cck
```

**Process with specific options:**

```bash
cck -t 300                  # Set temperature to 300 K
cck -c 2 -col 6             # 2M concentration, sort by SCF energy
cck -f csv -q               # Generate CSV output quietly
cck -e out                  # Process .out files instead of .log
```

#### Command-Line Options

| Option            | Description        | Values            | Default         |
| ----------------- | ------------------ | ----------------- | --------------- |
| `-t, --temp`      | Temperature (K)    | Positive number   | 298.15          |
| `-c, --cm`        | Concentration (M)  | Positive value    | 1               |
| `-col, --column`  | Sort column        | 2-10              | 2 (ETG kJ/mol)  |
| `-e, --ext`       | File extension     | log, out          | log             |
| `-f, --format`    | Output format      | text, csv         | text            |
| `-nt, --threads`  | Thread count       | number, half, max | half            |
| `-q, --quiet`     | Quiet mode         | -                 | false           |
| `--max-file-size` | Max file size (MB) | Positive integer  | 100             |
| `--memory-limit`  | Memory limit (MB)  | Positive integer  | Auto-calculated |
| `--batch-size`    | Batch size         | Positive integer  | false           |
| `--resource-info` | Show resources     | -                 | false           |
| `-h, --help`      | Show help          | -                 | false           |

#### Sorting Options

| Column | Description                                         |
| ------ | --------------------------------------------------- |
| 2      | ETG kJ/mol (Gibbs free energy) - **Default**        |
| 3      | Lowest frequency (for transition state analysis)    |
| 4      | ETG a.u (Gibbs free energy in atomic units)         |
| 5      | Nuclear repulsion energy                            |
| 6      | SCF energy                                          |
| 7      | Zero-point energy                                   |
| 10     | Round count (number of Gaussian calculation rounds) |

#### Output Formats

**Text Format (Default):**

```bash
cck                  # A table of text
```

**CSV Format:**

```bash
cck -f csv          # CSV
cck -f csv -q       # CSV with minimal console output
```

**Output Files:**

- **Text**: `base_directory_name.results`
- **CSV**: `base_directory_name.csv`

#### Understanding the Output

The output table contains the following columns:

1. **Output name**: Name of the quantum chemistry output file
2. **ETG kJ/mol**: Gibbs free energy with all corrections (entropy at corresponding temperature and phase correction from gas phase 1 atm to 1 M)
3. **Low FC**: Lowest vibrational frequency (useful for detecting imaginary modes)
4. **ETG a.u**: Same as column 2 but in hartree/atomic units
5. **Nuclear E au**: Repulsive nuclear energy (sensitive to molecular coordinates)
6. **SCFE**: SCF energy in hartree
7. **ZPE**: Zero-point energy
8. **Status**: Status of the job: DONE or UNDONE
9. **PCorr**: Phase correction status (YES for solvent calculations, NO for gas phase)
10. **Round**: Number of calculation rounds in the output (increments with each `--link1--` section)

### 3.2 Job Organization & Management Module

This module categorizes and organizes quantum chemistry job outputs based on their completion status, enabling efficient project management.

***Note: currently, only outputs from Gaussian are supported. Other programs will be supported soon***

#### Commands

**Sort completed jobs:**

```bash
cck done      # Completed jobs will be moved to base_name_dir-done
```

**Sort failed jobs:**

```bash
cck errors    # All general errors will be moved to errorJobs
```

**Sort PCM convergence errors:**

```bash
cck pcm       # Errors related to PCM convergence will be moved to PCMMkU
```

**Sort jobs with imaginary frequencies:**

```bash
cck imode     # Outputs with imaginary modes will be moved to imaginary_freqs
```

#### Functionality

These commands will:

- Check the status of all job outputs in the current directory
- Move log files (and associated .chk, .input files) to corresponding directories
- Organize jobs based on their completion status or specific error types
- Create directories with descriptive names for easy identification

#### Integration with Other Modules

After organizing jobs, you can:

```bash
# Generate inputs for failed jobs
cck errors
cck ci --calc-type opt_freq

# Run thermochemistry on completed jobs
cck done
cck thermo base_name_dir-done/*.log -T 298.15
```

### 3.3 Thermo Module (OpenThermo)

The **Thermo Module** (based on OpenThermo) provides comprehensive thermochemical property calculations with state-of-the-art parallel processing capabilities. It implements rigorous statistical mechanics methods for calculating thermodynamic properties from quantum chemistry output files.

#### Supported Quantum Chemistry Programs

- **Gaussian** (.log, .out)
- **ORCA** (.out)
- **GAMESS-US** (.log)
- **NWChem** (.out)
- **CP2K** (.out)
- **VASP** (OUTCAR)
- **OpenThermo Format** (.otm)

#### Basic Usage

**Single file thermochemistry calculation:**

```bash
cck thermo molecule.log

# Custom temperature and pressure
cck thermo molecule.log -T 298.15 -P 1.0

# Grimme's low-frequency treatment
cck thermo molecule.log -lowvibmeth Grimme
```

**Batch processing (parallel):**

```bash
# Process multiple files with auto-detected threads
cck thermo files.list

# Specify thread count
cck thermo files.list -nt 8

# Set memory limit (auto-calculated if not specified)
cck thermo files.list -memory-limit 4096
```

**Temperature/Pressure Scanning**

```bash
# Temperature scan
cck thermo molecule.log -T 200 400 25

# Pressure scan
cck thermo molecule.log -P 0.5 2.0 0.2

# Combined scan
cck thermo molecule.log -T 273 373 50 -P 0.5 2.0 0.5
# Output: molecule.UHG and molecule.SCq files
```

#### Command-Line Options

| Option                        | Description                       | Values                                      | Default   |
| ----------------------------- | --------------------------------- | ------------------------------------------- | --------- |
| `-T <T>` or `-T <T1 T2 step>` | Temperature (K)                   | number or range                             | 298.15    |
| `-P <P>` or `-P <P1 P2 step>` | Pressure (atm)                    | number or range                             | 1.0       |
| `-sclZPE <factor>`            | ZPE frequency scaling             | decimal                                     | 1.0       |
| `-sclheat <factor>`           | Thermal energy scaling            | decimal                                     | 1.0       |
| `-sclS <factor>`              | Entropy scaling                   | decimal                                     | 1.0       |
| `-sclCV <factor>`             | Heat capacity scaling             | decimal                                     | 1.0       |
| `-lowvibmeth <mode>`          | Low-frequency treatment           | 0/Harmonic, 1/Truhlar, 2/Grimme, 3/Minenkov | 2         |
| `-ravib <value>`              | Truhlar raising threshold (cm⁻¹)  | decimal                                     | 100.0     |
| `-intpvib <value>`            | Interpolation threshold (cm⁻¹)    | decimal                                     | 100.0     |
| `-ipmode <mode>`              | Phase mode                        | 0=gas, 1=condensed                          | 0         |
| `-imagreal <value>`           | Imaginary freq threshold (cm⁻¹)   | decimal                                     | 0.0       |
| `-massmod <type>`             | Mass assignment                   | 1=average, 2=isotope, 3=file                | 3         |
| `-PGname <name>`              | Point group                       | string or "?"                               | "?"       |
| `-conc <string>`              | Concentration (mol/L)             | decimal                                     | 0         |
| `-prtvib <mode>`              | Vibrational contributions         | 0=no, 1=screen, -1=file                     | 0         |
| `-outotm <mode>`              | Output .otm file                  | 0=no, 1=yes                                 | 0         |
| `-nt <threads>`               | Thread count                      | number, half, max                           | half      |
| `-memory-limit <MB>`          | Memory limit                      | positive integer                            | auto      |
| `-E <value>`                  | Electronic energy override (a.u.) | decimal                                     | from file |

#### Calculation Methods

**Standard RRHO (Rigid-Rotor Harmonic Oscillator)**

- Traditional harmonic approximation
- Suitable for most molecular systems

**Quasi-RRHO Treatments:**

1. **Truhlar's Method** (`lowvibmeth = 1`)
   
   - Raises frequencies below threshold (default: 100 cm⁻¹)
   - Simple and computationally efficient

2. **Grimme's Interpolation** (`lowvibmeth = 2`)
   
   - Entropy interpolation between RRHO and free rotor
   - Weighting: `w = 1 / (1 + (ν_threshold/ν)^4)`
   - Better treatment of low-frequency modes

3. **Minenkov's Interpolation** (`lowvibmeth = 3`)
   
   - Extended Grimme's method with energy interpolation
   - Includes both entropy and thermal energy corrections
   - Most comprehensive treatment for flexible molecules

#### Advanced Examples

**High-Precision Calculation:**

```bash
cck thermo molecule.log -T 298.15 -lowvibmeth 3 -sclZPE 0.98 -sclS 0.99 -sclCV 0.99
```

**Transition State Analysis:**

```bash
cck thermo ts.log -imagreal 100 -lowvibmeth 1 -ravib 50 -prtvib 1
```

**Condensed Phase Calculation:**

```bash
cck thermo crystal.out -ipmode 1 -conc "1.0 M"
```

**Batch Processing with Custom Settings:**

```bash
cck thermo batch.list -T 310.15 -nt 8 -memory-limit 8192 -lowvibmeth 2 -outotm 1
```

### 3.4 Input Creation Module

The Input Creation Module generates Gaussian input files from XYZ coordinate files with customizable calculation parameters, enabling efficient workflow automation.

***Note: currently, only outputs from Gaussian are supported. Other programs will be supported soon***

#### Basic Usage

**Generate single point energy inputs (default):**

```bash
cck ci
```

**Generate optimization + frequency inputs:**

```bash
cck ci --calc-type opt_freq
```

**Generate transition state search inputs:**

```bash
cck ci --calc-type ts_freq
```

#### Supported Calculation Types

- **sp**: Single point energy calculation
- **opt_freq**: Geometry optimization + frequency analysis
- **ts_freq**: Transition state search + frequency analysis
- **oss_ts_freq**: Openshell singlet TS search + frequency analysis
- **modre_ts_freq**: Modredundant TS search + frequency analysis
- **oss_check_sp**: Openshell singlet stability check
- **high_sp**: High-level single point with larger basis set
- **irc_forward**: IRC calculation in forward direction
- **irc_reverse**: IRC calculation in reverse direction
- **irc**: IRC calculation in both directions
- **tddft**: TD-DFT excited state calculation (singlets, triplets, or 50-50 states)

#### Command-Line Options

| Option               | Description                   | Values                      | Default   |
| -------------------- | ----------------------------- | --------------------------- | --------- |
| `--calc-type`        | Calculation type              | sp, opt_freq, ts_freq, etc. | sp        |
| `--functional`       | DFT functional                | UWB97XD, B3LYP, etc.        | UWB97XD   |
| `--basis`            | Basis set                     | def2-SVPP, 6-31G*, etc.     | def2-SVPP |
| `--large-basis`      | Large basis for TS/high-level | def2-TZVPP, etc.            | -         |
| `--solvent`          | Solvent name                  | water, ethanol, etc.        | gas phase |
| `--solvent-model`    | Solvent model                 | smd, cpcm, iefpcm           | smd       |
| `--charge`           | Molecular charge              | integer                     | 0         |
| `--mult`             | Multiplicity                  | integer                     | 1         |
| `--freeze-atoms`     | Freeze bond for TS            | atom1 atom2                 | -         |
| `--print-level`      | Route modifier                | N, P, T                     | -         |
| `--extra-keywords`   | Additional keywords           | any Gaussian keywords       | -         |
| `--extension`        | Output extension              | .gau, .com, etc.            | .gau      |
| `--tschk-path`       | TS checkpoint path            | directory path              | -         |
| `--solvent-extra`    | Extra SCRF keyword            | e.g. read                   | -         |
| `--tddft-method`     | TD-DFT method                 | td, tda                     | tda       |
| `--tddft-states`     | States to calculate           | singlets, triplets, 50-50   | both      |
| `--tddft-nstates`    | Number of excited states      | integer                     | 15        |
| `--tddft-extra`      | Extra keywords in td/tda()    | e.g. Root=5,Read,IVOGuess   | -         |
| `--genci-params`     | Generate parameter template   | calculation type            | -         |
| `--genci-all-params` | Generate all templates        | directory path              | -         |
| `--param-file`       | Load parameters from file     | file path                   | -         |

#### Advanced Examples

**Transition state search with frozen bond:**

```bash
cck ci --calc-type modre_ts_freq --freeze-atoms 1 2
```

**High-level single point with custom functional and basis:**

```bash
cck ci --calc-type high_sp --functional B3LYP --basis 6-311+G** --large-basis def2-TZVPP
```

**Solvent calculation:**

```bash
cck ci --calc-type opt_freq --solvent water --solvent-model smd
```

**IRC calculation from transition state:**

```bash
cck ci --calc-type irc --tschk-path ../ts_checkpoints
```

**Custom keywords and settings:**

```bash
cck ci --calc-type opt_freq --extra-keywords "Int=UltraFine SCF=Conver=8" --pound-sign P
```

#### Parameter Templates

Generate reusable parameter templates:

```bash
# Generate a template for optimization + frequency
cck ci --genci-params opt_freq

# Generate templates for all calculation types
cck ci --genci-all-params ./my_templates

# Use a saved parameter file
cck ci --param-file opt_freq.params
```

**Workflow with parameter templates:**

1. Generate template:
   
   ```bash
   cck ci --genci-params opt_freq
   # Creates opt_freq.params
   ```

2. Edit template (opt_freq.params):
   
   ```bash
   # Modify parameters as needed
   calc_type = opt_freq
   functional = B3LYP
   basis = 6-31G*
   solvent = chloroform
   charge = 1
   mult = 2
   ```

3. Use template:
   
   ```bash
   cck ci --param-file opt_freq.params
   ```

### 3.5 High-level Energy Calculation Module

This module improves Gibbs free energy accuracy by combining optimized geometries from lower-level theory with high-level single-point electronic energies.

***Note: currently, only outputs from Gaussian are supported. Other programs will be supported soon***

#### Directory Structure

- **Current directory**: Geometrical optimizations and frequency calculations (Opt Freq)
- **Subdirectory** (e.g., `higher_level`): Single point calculations at higher theory level
- **Log files must have the same names** in both directories

```
project/
├── molecule1.log  (Opt Freq)
├── molecule2.log  (Opt Freq)
└── higher_level/
    ├── molecule1.log  (High-level SP)
    └── molecule2.log  (High-level SP)
```

#### Basic Usage

```bash
cd higher_level
cck high-kj    # Improved Gibbs free energy in kJ/mol
cck high-au    # Improved Gibbs free energy in hartree
```

#### Functionality

The module:

- Reads thermochemical corrections from optimized geometry calculations (lower level)
- Reads high-level electronic energies from single-point calculations
- Combines them to produce improved Gibbs free energies
- Automatically handles phase corrections and temperature-dependent terms

### 3.6 Coordinate Extraction Module

Extract molecular coordinates from quantum chemistry output files for further analysis or conversion to other formats.

#### Basic Usage

```bash
# Extract coordinates from all output files
cck xyz

# Extract coordinates from specific files
cck xyz -f file1.log file2.log
```

#### Functionality

- Parses quantum chemistry output files for final optimized geometry
- Extracts atomic positions and element types
- Outputs in standard XYZ format
- Supports multiple file processing with parallel extraction

## Parallel Processing & Resource Management

### Thread Control

**Specify the number of threads:**

```bash
cck -nt 4        # Use exactly 4 threads
cck -nt half     # Use half cores (default, safest)
cck -nt max      # Use all cores (with safety limits)
```

### Memory Management

The program **automatically calculates optimal memory limits** based on:

- **System RAM**: Detects total available memory
- **Thread Count**: More threads = higher memory allocation
- **Environment**: Conservative limits in cluster environments

**Memory Allocation Strategy:**

- **1-4 threads**: 30% of system RAM
- **5-8 threads**: 40% of system RAM
- **9-16 threads**: 50% of system RAM
- **17+ threads**: 60% of system RAM
- **Cluster environments**: 70% of calculated amount

**Example on 32GB system:**

```bash
cck -nt 4   # Uses ~9.6GB (30% of 32GB)
cck -nt 8   # Uses ~12.8GB (40% of 32GB)
cck -nt 16  # Uses ~16GB (50% of 32GB)
```

### HPC Cluster Integration

ComChemKit automatically detects and adapts to:

- **SLURM**: Reads `SLURM_JOB_ID`, `SLURM_CPUS_PER_TASK`, `SLURM_MEM_PER_NODE`
- **PBS**: Reads `PBS_JOBID`, `PBS_NUM_PPN`, `PBS_JOBMEM`
- **SGE**: Reads `JOB_ID`, `pe_slots`, `h_vmem`
- **LSF**: Reads `LSB_JOBID`, `LSB_DJOB_NUMPROC`, `LSB_MAX_MEM_PROCESSOR`

### Cluster Environment Safety

**Cluster Safety Warning:**

- Program automatically detects cluster environments (SLURM, PBS, SGE, LSF)
- In clusters: Maximum threads = min(requested, cores/4, 8)
- On head nodes: **Never use "max"** - it can overload shared resources
- Recommended: `-nt 2` or `-nt 4` for head nodes

**Resource Monitoring:**

```bash
cck --resource-info  # Show system resources before running
```

### Parallel Performance

**Typical Speedups:**

- 4 threads: ~3-4x faster than sequential
- 8 threads: ~6-7x faster
- 16+ threads: Up to 13x faster
- Memory overhead: ~50MB per thread

### File Handle Management

- RAII-based resource management
- Prevents file descriptor exhaustion
- Configurable concurrent file limits

### Error Collection and Reporting

- Thread-safe error aggregation
- Centralized warning system
- Graceful error handling

## Advanced Usage Examples

### Production Examples of Data Extraction

```bash
# Cluster-safe processing
cck -nt 4 -q -f csv -t 298.15

# High-throughput processing
cck -nt 8 -c 5 -col 4 -f csv

# Conservative head node usage
cck -nt 2 -q --resource-info

# Process larger files (increase 100MB default limit)
cck --max-file-size 500

# Set specific memory limit for high-memory systems
cck --memory-limit 8192 -nt 16
```

### Complete Workflow Example

```bash
# 1. Create inputs for optimization and frequency
cck ci --calc-type opt_freq

# 2. Run Gaussian calculations...
# ... wait for calculations to complete ...

# 3. Organize completed jobs
cck done

# 4. Run thermochemistry on completed jobs
cck thermo base_name_dir-done/*.log -T 298.15 -lowvibmeth 2

# 5. Extract data for analysis
cck thermo base_name_dir-done/*.log -f csv -q
```

### High-level Energy Workflow

```bash
# Current directory: optimization and frequency calculations
# Directory "higher_level": single point calculations

cd higher_level

# Calculate improved Gibbs free energies
cck high-kj    # Results in kJ/mol
cck high-au    # Results in hartree
```

### Temperature-Dependent Analysis

```bash
# Single file temperature scan
cck thermo molecule.log -T 273 373 10 -lowvibmeth 2

# Batch processing with temperature scan
cck thermo batch.list -T 298.15 -nt 8 -memory-limit 4096 -lowvibmeth 2 -outotm 1
```

### Transition State Screening

```bash
# Create TS inputs with frozen bond
cck ci --calc-type ts_freq --freeze-atoms 1 2

# Analyze TS candidates
cck thermo ts_candidates.log -imagreal 100 -lowvibmeth 1 -ravib 50

# Filter and organize
cck imode
cck done
```

### Batch Processing Workflows

```bash
# Create a file list for batch processing
ls *.log > batch.list

# Process with custom settings
cck thermo batch.list -nt 8 -memory-limit 8192 -T 298.15 -lowvibmeth 2 -outotm 1

# Generate additional data files
cck -nt 8 -c 5 -col 4 -f csv
```

### Integration Between Modules

```bash
# Full computational chemistry workflow
cck ci --calc-type opt_freq
# ... submit to cluster ...

# After completion: organize jobs
cck done
cck errors
cck pcm
cck imode

# Run thermochemistry on completed jobs
cck thermo base_name_dir-done/*.log -T 298.15 -lowvibmeth 2 -outotm 1

# Extract data for analysis
cck -f csv -q

# Create inputs for failed jobs
cck ci --calc-type opt_freq --param-file custom.params
```

### Advanced Parallel Processing

```bash
# High-performance setup for 32GB+ systems
cck --memory-limit 16384 -nt 20 --max-file-size 500

# Cluster-safe batch processing
cck thermo batch.list -nt 4 -q -memory-limit 4096

# Production-level throughput
cck thermo batch.list -nt 16 -memory-limit 16384 -T 298.15 -lowvibmeth 2
```

## Safety Guidelines & Best Practices

### For Cluster Head Nodes

**RECOMMENDED: Conservative settings**

```bash
cck -nt 2 -q -f csv

# CHECK FIRST: Resource availability
cck --resource-info

# AVOID: Resource-intensive operations
cck -nt max  # DON'T USE ON HEAD NODES
```

### For Compute Nodes

**Optimal performance settings**

```bash
# Usually optimal
cck -nt half

# If node is dedicated to your job
cck -nt max

# High-performance setups (adjust based on your system)
cck -nt 20 --memory-limit 16384  # 16GB limit, 20 threads
cck -nt 32 --memory-limit 24576  # 24GB limit, 32 threads
```

### Safety-First Usage for Clusters

**Recommended usage if you run the program on your head node:**

```bash
# Set a limit number of cores which will not over use your head node
cck -nt 4 -q

# You can check resource limits before running
cck --resource-info

# Maximum safety on shared head nodes
cck -nt 2 -q -f csv
```

### Quiet Mode

**Recommended for Batch Jobs:**

```bash
cck -q              # Minimal output, perfect for scripts
```

### Thread Control Best Practices

- **More threads ≠ faster**: Optimal performance typically at half CPU cores
- **Check actual CPU usage**: Use `top` or `htop` to verify thread utilization
- **I/O bottlenecks**: Files on network storage can limit parallel speed
- **Memory bandwidth**: Saturated memory bandwidth can limit performance
- **Use auto-detection**: `-nt half` automatically selects optimal thread count

### Resource Monitoring

```bash
# Show system information before running
cck --resource-info

# Monitor during execution
# Check actual memory usage with system tools:
# Linux: free -h, vmstat 1
# macOS: vm_stat
# Windows: Task Manager
```

### File Size Management

- **Default limit**: 100MB per file
- **Larger files**: Use `--max-file-size 200` to increase limit
- **Very large files**: Consider processing sequentially (`-nt 1`)
- **Disk space**: Ensure adequate space for output files

### Memory Guidelines

- **Auto-calculation**: Most users should rely on automatic memory limits
- **Custom limits**: Use `--memory-limit 8192` for specific requirements
- **Reduce if needed**: Lower thread count reduces memory requirements
- **Monitor usage**: Check actual memory consumption during execution

### Cluster-Specific Best Practices

- **Respect allocations**: Never use more resources than allocated
- **Check job scheduler**: Verify environment variables are set correctly
- **Head node courtesy**: Always use `-nt 2` or `-nt 4` on shared head nodes
- **Use job scripts**: Submit batch jobs rather than running on head nodes
- **Monitor usage**: Track actual resource consumption

### Performance Optimization

**Checklist for optimal performance:**

- [ ] Verify adequate disk I/O (local storage preferred)
- [ ] Ensure sufficient memory for chosen thread count
- [ ] Test with `-nt half` before adjusting
- [ ] Monitor CPU utilization during execution
- [ ] Check for I/O wait states (`iostat` or `iotop`)
- [ ] Consider file size distribution (many small vs. few large files)

## Troubleshooting

### Build Errors

```bash
# Check compiler version
g++ --version                # Need GCC 10+ for C++17
icpx --version               # Intel oneAPI compiler
icpc --version               # Intel Classic compiler

# Install dependencies (Ubuntu/Debian)
sudo apt install build-essential

# Check compiler information
make compiler-info           # Shows detected compiler and flags

# Try alternative build
make debug                   # Often reveals compilation issues

# Force specific compiler
CXX=g++ make                 # Force GCC compiler
CXX=icpx make                # Force Intel oneAPI compiler
```

### Performance Issues

```bash
# Reduce thread count
cck -nt 2

# Check file sizes
ls -lh *.log  # Files >100MB are automatically skipped

# Monitor resources
cck --resource-info
```

### File Format Recognition

**Error**: `Error: Unable to identify the program that generated this file`

*Solution*:

- Verify input file contains frequency analysis output
- Check if quantum chemistry program is supported (Gaussian, ORCA, GAMESS-US, NWChem, CP2K, VASP)
- Ensure calculation completed successfully
- Try converting to OpenThermo format (.otm)

### Invalid Temperature/Pressure Arguments

**Error**: `Error: Invalid value for -T`

*Solution*:

- Check syntax: `-T 298.15` (single) or `-T 200 400 25` (range)
- Ensure numeric values are valid
- Use `--help` for detailed syntax

### Low-Frequency Treatment Issues

**Error**: Unrealistic entropy or heat capacity values

*Solution*:

- Use `-lowvibmeth 1` (Truhlar) or `-lowvibmeth 2` (Grimme) for flexible molecules
- Adjust threshold with `-ravib` (Truhlar) or `-intpvib` (Grimme/Minenkov)
- Lower threshold (e.g., 50 cm⁻¹) for more conservative treatment
- Higher threshold (e.g., 150 cm⁻¹) for more aggressive treatment

### Imaginary Frequency Warnings

**Warning**: `Note: There are X imaginary frequencies, they will be ignored`

*Solution*:

- Check if system is a transition state (expected for TS calculations)
- For minima with imaginary frequencies: geometry optimization not converged
- Use `-imagreal <value>` to treat small imaginary modes as real (e.g., `-imagreal 50`)
- Verify molecular structure and consider re-optimization

### Memory Issues with Large Batch Jobs

**Error**: `Insufficient memory to process file`

*Solution*:

- Reduce thread count: `-nt 4` or `-nt 2`
- Set explicit memory limit: `-memory-limit 2048`
- Process files in smaller batches
- Use quiet mode: `-q`
- Check available memory: `--resource-info`

### Parallel Performance Issues

**Problem**: Slow performance with many threads

*Solution*:

- Check actual CPU usage: `top` or `htop`
- Reduce thread count: More threads ≠ faster (optimal around half CPU cores)
- Check I/O bottleneck: Files on network storage can limit speed
- Verify memory bandwidth isn't saturated
- Use `-nt half` for automatic optimal selection

### Cluster-Specific Issues

**Problem**: Program using too many resources on shared cluster

*Solution*:

- Always use `-nt 2` or `-nt 4` on head nodes
- Check job scheduler environment variables are set
- Use `-nt half` in SLURM/PBS jobs to respect allocations
- Monitor actual usage: `cck --resource-info`

### Getting Help

```bash
cck --help           # Complete help
cck --resource-info  # System information
cck --help command   # Help for specific commands

# Thermo Module specific help
cck thermo --help
cck thermo --help-T
cck thermo --help-lowvibmeth
```

## Technical Appendix

### Dynamic Memory Management

The program **automatically calculates optimal memory limits** based on:

- **System RAM**: Detects total available memory
- **Thread Count**: More threads = higher memory allocation
- **Environment**: Conservative limits in cluster environments

**Memory Allocation Strategy:**

- **1-4 threads**: 30% of system RAM
- **5-8 threads**: 40% of system RAM
- **9-16 threads**: 50% of system RAM
- **17+ threads**: 60% of system RAM
- **Cluster environments**: 70% of calculated amount

**Example on 32GB system:**

```bash
cck -nt 4   # Uses ~9.6GB (30% of 32GB)
cck -nt 8   # Uses ~12.8GB (40% of 32GB)
cck -nt 16  # Uses ~16GB (50% of 32GB)
cck -nt 40  # Uses ~19.2GB (60% of 32GB)
```

### **Performance Benefits:**

- **4 threads**: ~3-4x speedup over sequential processing
- **8 threads**: ~6-7x speedup for typical workloads
- **16+ threads**: Up to 13x speedup for large datasets
- **Memory efficiency**: ~50MB overhead per thread
- **Automatic optimization**: Adapts to system resources and workload

**Safety Features:**

- Automatic memory limit detection and enforcement
- File size validation (500MB limit per file)
- Graceful degradation under resource constraints
- Thread-safe error handling and reporting
- Resource cleanup on completion or interruption

### Thermo Module Parallel Processing Details

The thermo module implements advanced parallel processing specifically designed for thermochemical calculations:

**Atomic File Distribution:**

- Uses `std::atomic<size_t>` for thread-safe file indexing
- Each worker thread atomically fetches next file index
- No race conditions or duplicate processing
- Load balancing automatically handled

**Thread-Safe Infrastructure:**

1. **MemoryMonitor Class**
   
   - Tracks memory allocation across threads
   - Prevents system memory exhaustion
   - Peak usage monitoring and reporting
   - RAII-based memory guards

2. **FileHandleManager Class**
   
   - Limits concurrent file operations
   - Uses `std::counting_semaphore` (C++20) or condition_variable
   - Prevents file descriptor exhaustion
   - Automatic resource cleanup

3. **ThreadSafeErrorCollector Class**
   
   - Centralized error/warning aggregation
   - Mutex-protected vector operations
   - Preserves error order and context
   - Non-blocking error collection

## Version History

- **v0.000.2** Experimental version

## Contributing and Issues

For bug reports, feature requests, or safety concerns, please create an issue at the project repository. When reporting issues, include:

- System specifications (OS, compiler, cluster environment)
- Command used and error messages
- Sample files (if possible)
- Resource usage information (`--resource-info` output)

---
