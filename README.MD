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

**ComChemKit** *can speed up your workflow thousands of times and save your 99.9% of time for input creation, data extraction, data analysis, and computational job management.*

## Key Features

* **Extract data in Parallel**: Efficiently processes multiple quantum chemistry output files simultaneously with multi-threading
* **Check status of gaussian jobs**: Status of gaussian outputs can be checked and isolated to corresponding directories
* **Thermochemical Property Calculation**: High-performance thermochemistry calculations with parallel processing, supporting Gaussian, ORCA, GAMESS-US, NWChem, CP2K, and VASP
* **Advanced Parallel Features**: Multi-threaded batch processing, thread-safe memory monitoring, automatic load balancing, and HPC cluster integration (SLURM, PBS, SGE, LSF)
* **Automatic Sorting**: Results sorted by energy values for easy analysis
* **Extract coordinates**: Coordinates of quantum chemistry outputs can be extracted
* **Create Input Files**: Generate Gaussian input files from XYZ coordinate files with customizable calculation parameters
* **Imaginary mode coordinate displacement (ivcoord)**: Displace molecular geometry along imaginary normal modes (Quick Reaction Coordinate method) to generate reactant/product guess structures for transition state searches
* **RMSD calculation (rmsd)**: Calculate root-mean-square deviation between molecular structures (XYZ/PDB) with optimal alignment, atom reordering, reflection search, and pairwise matrix mode for conformer/trajectory analysis
* **Marcus theory barrier calculations (marcus)**: Marcus barrier calculations for Outer Sphere Single Electron Transfer (OSET) and Energy Transfer (EnT) using the four-point method, with full thermochemical parameter support

**Windows interactive mode**: Windows users can use all features plus popular linux commands in the interactive mode

**Important note**: **Large files (>500MB) automatically skipped by default**. Users need to set flag --max-file-size xxxxx to increase the size limit

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

| Option                 | Description        | Values            | Default           |
| ---------------------- | ------------------ | ----------------- | ----------------- |
| `-t, --temp`           | Temperature (K)    | Positive number   | 298.15            |
| `-c, -C, --cm, --conc` | Concentration (M)  | Positive value    | 1 (Enables PCorr) |
| `-col, --column`       | Sort column        | 2-10              | 2 (ETG kJ/mol)    |
| `-e, --ext`            | File extension     | log, out          | log               |
| `-f, --format`         | Output format      | text, csv         | text              |
| `-nt, --threads`       | Thread count       | number, half, max | half              |
| `-q, --quiet`          | Quiet mode         | -                 | false             |
| `--max-file-size`      | Max file size (MB) | Positive integer  | 500               |
| `--memory-limit`       | Memory limit (MB)  | Positive integer  | Auto-calculated   |
| `--batch-size`         | Batch size         | Positive integer  | false             |
| `--resource-info`      | Show resources     | -                 | false             |
| `-h, --help`           | Show help          | -                 | false             |

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
- **Q-Chem** (.out/.log)
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

| Option                        | Description                         | Values                                                    | Default     |
| ----------------------------- | ----------------------------------- | --------------------------------------------------------- | ----------- |
| `-T <T>` or `-T <T1 T2 step>` | Temperature (K)                     | number or range                                           | 298.15      |
| `-P <P>` or `-P <P1 P2 step>` | Pressure (atm)                      | number or range                                           | 1.0         |
| `-sclZPE <factor>`            | ZPE frequency scaling               | decimal                                                   | 1.0         |
| `-sclheat <factor>`           | Thermal energy scaling              | decimal                                                   | 1.0         |
| `-sclS <factor>`              | Entropy scaling                     | decimal                                                   | 1.0         |
| `-sclCV <factor>`             | Heat capacity scaling               | decimal                                                   | 1.0         |
| `-lowvibmeth <mode>`          | Low-frequency treatment             | 0/Harmonic, 1/Truhlar, 2/Grimme, 3/Minenkov, 4/HeadGordon | 2           |
| `-ravib <value>`              | Truhlar raising threshold (cm⁻¹)    | decimal                                                   | 100.0       |
| `-intpvib <value>`            | Interpolation threshold (cm⁻¹)      | decimal                                                   | 100.0       |
| `-hgEntropy <0                | 1>`                                 | Head-Gordon entropy interpolation                         | 0=no, 1=yes |
| `-bav <preset>`               | Head-Gordon Bav preset              | grimme, qchem, auto                                       | auto        |
| `-ipmode <mode>`              | Phase mode                          | 0=gas, 1=condensed                                        | 0           |
| `-imagreal <value>`           | Imaginary freq threshold (cm⁻¹)     | decimal                                                   | 0.0         |
| `-massmod <type>`             | Mass assignment                     | 1=average, 2=isotope, 3=file                              | 3           |
| `-PGname <name>`              | Point group                         | string or "?"                                             | "?"         |
| `-conc <string>`              | Concentration (mol/L)               | decimal                                                   | 0           |
| `-prtlevel <0-3>`             | Output verbosity level              | 0=minimal, 1=default, 2=verbose, 3=full                   | 1           |
| `-prtvib <mode>`              | Vibrational contributions           | 0=no, 1=screen, -1=file                                   | 0           |
| `-outotm <mode>`              | Output .otm file                    | 0=no, 1=yes                                               | 0           |
| `-omp-threads <threads>`      | OpenMP parallelization threads      | positive integer, 0=auto                                  | 0           |
| `-nt <threads>`               | Legacy file-processing thread count | number, half, max                                         | half        |
| `-memory-limit <MB>`          | Memory limit                        | positive integer                                          | auto        |
| `-E <value>`                  | Electronic energy override (a.u.)   | decimal                                                   | from file   |

#### Calculation Methods

**Standard RRHO (Rigid-Rotor Harmonic Oscillator)**

- Traditional harmonic approximation
- Suitable for most molecular systems

**Quasi-RRHO Treatments:**

1. **Truhlar's Method** (`lowvibmeth = 1`)
   
   - Raises frequencies below threshold (default: 100 cm⁻¹)
   - Simple and computationally efficient

2. **Grimme's Interpolation** (`lowvibmeth = 2` or `grimme`)
   
   - Entropy interpolation between RRHO and free rotor
   - Weighting: `w = 1 / (1 + (ν_threshold/ν)^4)`
   - Better treatment of low-frequency modes

3. **Minenkov's Interpolation** (`lowvibmeth = 3` or `minenkov`)
   
   - Extended Grimme's method with energy interpolation
   - Includes both entropy and thermal energy corrections
   - Most comprehensive treatment for flexible molecules

4. **Head-Gordon Interpolation** (`lowvibmeth = 4` or `headgordon`)
   
   - Smoothed damping for entropic contributions of low vibrational modes
   - Supported by Q-Chem inspired free rotor references (`-bav qchem`)
   - Fully supports interpolating entropy (`-hgEntropy 1`)

#### Advanced Examples

**High-Precision Calculation:**

```bash
cck thermo molecule.log -T 298.15 -lowvibmeth headgordon -bav qchem -prtlevel 2
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
- **modre_opt**: Modredundant optimization
- **oss_check_sp**: Openshell singlet stability check
- **high_sp**: High-level single point with larger basis set
- **irc_forward**: IRC calculation in forward direction
- **irc_reverse**: IRC calculation in reverse direction
- **irc**: IRC calculation in both directions
- **tddft**: TD-DFT excited state calculation (singlets, triplets, or 50-50 states)

#### Command-Line Options

| Option               | Description                                                   | Values                      | Default   |
| -------------------- | ------------------------------------------------------------- | --------------------------- | --------- |
| `--calc-type`        | Calculation type                                              | sp, opt_freq, ts_freq, etc. | sp        |
| `--functional`       | DFT functional                                                | UWB97XD, B3LYP, etc.        | UWB97XD   |
| `--basis`            | Basis set                                                     | def2-SVPP, 6-31G*, etc.     | def2-SVPP |
| `--large-basis`      | Large basis for high_sp only                                  | def2-TZVPP, etc.            | -         |
| `--solvent`          | Solvent name                                                  | water, ethanol, etc.        | gas phase |
| `--solvent-model`    | Solvent model                                                 | smd, cpcm, iefpcm           | smd       |
| `--charge`           | Molecular charge                                              | integer                     | 0         |
| `--mult`             | Multiplicity                                                  | integer                     | 1         |
| `--freeze-bond`      | Freeze bond for TS (legacy alias: `--freeze-atoms`, deprecated) | atom1 atom2               | -         |
| `--print-level`      | Route modifier                                                | N, P, T                     | -         |
| `--extra-keywords`   | Additional keywords                                           | any Gaussian keywords       | -         |
| `--extension`        | Output extension                                              | .gau, .com, etc.            | .gau      |
| `--tschk-path`       | TS checkpoint path                                            | directory path              | -         |
| `--solvent-extra`    | Extra SCRF keyword                                            | e.g. read                   | -         |
| `--tddft-method`     | TD-DFT method                                                 | td, tda                     | tda       |
| `--tddft-states`     | States to calculate                                           | singlets, triplets, 50-50   | both      |
| `--tddft-nstates`    | Number of excited states                                      | integer                     | 15        |
| `--tddft-extra`      | Extra keywords in td/tda()                                    | e.g. Root=5,Read,IVOGuess   | -         |
| `--fix-pcm`          | Fix PCM solvation discontinuity (SES→normal, 2-section Link1) | (flag)                      | false     |
| `--temperature`      | Temperature (K)                                               | e.g. 298.15                 | -         |
| `--genci-params`     | Generate parameter template                                   | calculation type            | -         |
| `--genci-all-params` | Generate all templates                                        | directory path              | -         |
| `--param-file`       | Load parameters from file                                     | file path                   | -         |

#### Parameter File Keys (summary)

The parameter file uses `name = value` lines (`#` = comment); keys present in the file override CLI flags. Full reference with per-type routes: `docs/usage.rst` (section 5. Create Input Files (ci)).

| Key | Default | Notes |
| --- | ------- | ----- |
| `calc_type` | `sp` | sp, opt_freq, ts_freq, oss_ts_freq, modre_ts_freq, modre_opt, oss_check_sp, high_sp, irc_forward, irc_reverse, irc, tddft (`irc` valid, no template; `ts_freq_from_chk` internal-only) |
| `functional` / `basis` | `UWB97XD` / `Def2SVPP` | Uppercased; `GEN`/`GENECP` requires `tail` |
| `large_basis` | (empty) | Route basis for `high_sp` only (template also suggests it for TS types) |
| `charge` / `mult` | `0` / `1` | Molecule spec |
| `solvent` / `solvent_model` / `solvent_extra` | (empty) / `smd` / (empty) | Adds `scrf(model,solvent=X)`; model is smd, cpcm or iefpcm |
| `print_level` | (empty) | `#`, `#P`/`#T`/`#N`, else `#<value>` |
| `route_extra_keywords` | (empty) | Extra route tokens; separators: space, vertical bar, comma, semicolon |
| `scf_maxcycle` / `scf_options` / `scf_restart` | `300` / (empty) / `false` | Param-file-only; inside `scf(...)`; `true`/`yes`/`1` appends one `restart` (deduped) |
| `opt_maxcycles` / `opt_options` / `opt_restart` | `300` / (empty) / `false` | Param-file-only; inside `opt(...)`; same restart rule |
| `opt_maxstep` | (unset; `5` in `ts_freq_from_chk` sections) | Adds `,MaxStep=N` |
| `irc_maxpoints` / `irc_recalc` / `irc_maxcycle` / `irc_stepsize` | `50` / `10` / `350` / `10` | Param-file-only; IRC types ignore `opt_*`/`scf_*` |
| `freeze_bond` (`freeze_bond_atom1/2` fallback; legacy `freeze_atoms`, `freeze_atom1/2` deprecated) / `modre` | - | One of them required for `oss_ts_freq`/`modre_ts_freq`; `modre` also valid for `modre_opt` |
| `tschk_path` | parent dir | For `high_sp`, `irc_forward`/`irc_reverse` |
| `tddft_method` / `tddft_states` / `tddft_nstates` / `tddft_extra` | `tda` / both / `15` / (empty) | `singlets` writes `-singlets.gau`, `triplets` writes `-triplets.gau`, `50-50` writes one file, empty writes both |
| `fix_pcm` / `temperature` | `false` / (unset) | Two-section Link1 SES job; not valid for high_sp, oss_ts_freq, modre_ts_freq, tddft, irc |
| `mem` / `cpu` / `nprocshared` / `gpucpu` | (empty) | Param-file-only; Link0 first-section-only, emitted before `%chk` in order `%Mem`, `%CPU`, `%NProcShared`, `%GPUCPU`; uncomment in template to use |
| `tail` / `extra_options` | (empty) | `tail` carries GEN/GENECP basis; `extra_options` appends an extra section (param-file-only) |

Restart example (param file):

```bash
scf_options = restart,Conver=8
opt_options = restart,ModRedundant
scf_restart = false   # true gives scf(maxcycle=300,xqc,restart)
opt_restart = false   # true gives opt(maxcycles=300,restart)
```

#### Advanced Examples

**Transition state search with frozen bond:**

```bash
cck ci --calc-type modre_ts_freq --freeze-bond 1 2
```

**High-level single point with custom functional and basis:**

```bash
cck ci --calc-type high_sp --functional B3LYP --basis 6-311+G** --large-basis def2-TZVPP
```

**Solvent calculation:**

```bash
cck ci --calc-type opt_freq --solvent water --solvent-model smd
```

**Fix PCM solvation discontinuity (SES surface → normal surface):**

```bash
# Generates a two-section Link1 input: section 1 relaxes geometry on SES surface,
# section 2 performs the full calculation reading from the SES checkpoint.
# Supported calc types: sp, opt_freq, ts_freq, oss_check_sp, modre_opt
cck ci --calc-type opt_freq --solvent water --solvent-model smd --fix-pcm
```

**IRC calculation from transition state:**

```bash
cck ci --calc-type irc --tschk-path ../ts_checkpoints
```

**Custom keywords and settings:**

```bash
cck ci --calc-type opt_freq --extra-keywords "Int=UltraFine SCF=Conver=8" --print-level P
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

Restart and extra opt/scf sub-options (param-file only):

   ```bash
   # Extra tokens appended inside the parentheses (comma-separated;
   # ';' and '|' are also accepted as commas)
   scf_options = restart,Conver=8
   opt_options = restart,ModRedundant
   # Restart-only shortcuts (default false; true/yes/1 activates)
   scf_restart = false   # true -> scf(maxcycle=300,xqc,restart)
   opt_restart = false   # true -> opt(maxcycles=300,restart)
   ```

   Notes: `*_options` are appended inside `scf(...)`/`opt(...)`;
   `*_restart=true` appends a single `restart` (deduped if already listed
   in `*_options`); defaults (`""`/`false`) leave current routes unchanged;
   `opt_*` applies to opt_freq/ts_freq/modre types, `scf_*` also applies to
   sp/tddft/high_sp/oss_check_sp; IRC types ignore both.

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
cck extract-au # Extract energy and breakdown in hartree (E/ZPE/TC/TS/H/G, no Status column)

# Custom temperature, pressure and concentration (All auto-enable phase correction)
cck high-kj -T 310.15
cck high-au -P 2
cck high-au -conc 2       # -conc is the thermo spelling; -c, -C, --cm also work
cck high-au -T 300 -conc 2
```

By default both commands sort the table by Gibbs free energy (ascending), so the
same compounds appear in the same order: `high-kj` uses column 2 (`G kJ/mol`) and
`high-au` uses column 8 (`G a.u`). Override with `-col`.

#### Functionality

The module:

- Reads thermochemical corrections from optimized geometry calculations (lower level)
- Reads high-level electronic energies from single-point calculations
- Combines them to produce improved Gibbs free energies
- Automatically handles phase corrections and temperature-dependent terms

### 3.6 Marcus Theory Barrier Calculations (marcus)

The **Marcus module** implements Marcus theory barrier calculations for **Outer Sphere Single Electron Transfer (OSET)** and **Energy Transfer (EnT)** using the **four-point method**. It follows a 3-step workflow:

1. **Step 1** — Generate geometry optimization + frequency inputs (4 `.gau` files: donor-init, donor-final, acceptor-init, acceptor-final)
2. **Step 2** — Generate single point inputs reading step-1 checkpoints (8 `.gau` files: 4 equilibrium + 4 cross/internal)
3. **Step 3** — Calculate barriers from collected outputs (ΔG°r, reorganization energies λ_R/λ_P, symmetric & asymmetric barriers)

**Directory Structure:**

```
project/
├── Donor.xyz                  # Donor coordinates
├── Acceptor.xyz               # Acceptor coordinates
├── marcus_project.inp         # Marcus input file
├── marcus_project_step_1/     # Step 1: optimization inputs (.gau, .chk)
├── marcus_project_step_2/     # Step 2: single point inputs (.gau, .chk)
└── marcus_project.log  # Step 3: barrier report
```

In manual mode, steps 1 and 2 overwrite matching generated `.gau` files and manifests. Existing Gaussian `.log` and `.chk` files are not overwritten by input generation. Automatic mode refuses to overwrite an existing report or nonempty step directory.

**Basic Usage:**

```bash
# Generate an input template
cck marcus --genoset-input -o marcus_project.inp
# or for Energy Transfer:
cck marcus --genent-input -o marcus_project.inp

# Edit marcus_project.inp with your molecule names, XYZ paths, charges, multiplicities, etc.

# Step 1: Generate optimization inputs
cck marcus marcus_project.inp -step 1
# Submit the .gau files to Gaussian, wait for completion

# Step 2: Generate single point inputs (reads step-1 checkpoints)
cck marcus marcus_project.inp -step 2
# Submit equilibrium jobs first, then cross jobs; wait for completion

# Step 3: Calculate barriers
cck marcus marcus_project.inp -step 3   # reports both barriers
```

**Automatic workflow:** Set `workflow = auto` and `qc_engine = g16` (or an executable path) in the input, then submit job script and call the marcus module `cck marcus marcus_project.inp`. 

For example, the input can contain:

```ini
workflow = auto
qc_engine = g16
step1_nprocshared = 16
step2_nprocshared = 16
step1_mem = 24GB
step2_mem = 24GB
out_file = marcus_project.out
```

Example SLURM script (adjust module and resource settings to your cluster):

```bash
#!/bin/bash
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=16
#SBATCH --mem=32G
#SBATCH --time=24:00:00
#SBATCH --output=marcus-%j.scheduler.log

module load gaussian
cd "$SLURM_SUBMIT_DIR"
cck marcus marcus_project.inp
```

**Command-Line Options:**

| Option | Description | Values | Default |
|--------|-------------|--------|---------|
| `<input_file>` | Input file (positional) | file path | (required) |
| `-step` | Workflow step | 1, 2, 3 | 1,2 |
| `-O, --out` | Output report file | file path | `<project>.log` |
| `-v, --verbose` | Verbose report and console diagnostics | (flag) | false |
| `-T, --temp` | Temperature (K) | decimal | 298.15 |
| `-conc, -c, -C, --cm` | Concentration (mol/L) | decimal | 1.0 |
| `-P, --pressure` | Pressure (atm) | decimal | 1.0 |
| `-lowvibmeth` | Low-frequency treatment | 0/harmonic, 1/truhlar, 2/grimme, 3/minenkov, 4/headgordon | grimme |
| `-ravib` | Truhlar threshold (cm⁻¹) | decimal | 100.0 |
| `--genoset-input` | Generate OSET input template | (flag) | - |
| `--genent-input` | Generate EnT input template | (flag) | - |
| `-o, --output-template` | Template output path | file path | `marcus_oset.inp` / `marcus_ent.inp` |
| `-h, --help` | Show help | (flag) | - |

**Input File (`marcus_project.inp`) Keys:**

The input file uses `key = value` format (`#` = comment) and can have any extension. Unless `project` is set, its filename without the last extension determines the project name (e.g. `reaction.inp` → `reaction`). Relative paths are resolved from the current working directory. All step 3 thermo options are supported:

**Project Identification:**

| Key | Description | Default |
|-----|-------------|---------|
| `project` | Project name (used for directory names) | input filename stem |
| `calc_type` | Calculation type | `oset` \| `ent` |
| `workflow` | `manual` or sequential `auto` | `manual` |
| `qc_engine` | Gaussian executable name or path; required in auto mode | (empty) |

**Donor / Acceptor Definitions:**

| Key | Description | Default |
|-----|-------------|---------|
| `donor_name` | Donor molecule name | (required) |
| `donor_xyz` | Donor XYZ file path | (required) |
| `donor_init_charge` | Donor initial state charge | 0 |
| `donor_init_mult` | Donor initial state multiplicity | 1 |
| `donor_final_charge` | Donor final state charge | 0 |
| `donor_final_mult` | Donor final state multiplicity | 1 |
| `acceptor_name` | Acceptor molecule name | (required) |
| `acceptor_xyz` | Acceptor XYZ file path | (required) |
| `acceptor_init_charge` | Acceptor initial state charge | 0 |
| `acceptor_init_mult` | Acceptor initial state multiplicity | 1 |
| `acceptor_final_charge` | Acceptor final state charge | 0 |
| `acceptor_final_mult` | Acceptor final state multiplicity | 1 |

`donor_name` and `acceptor_name` are labels used as the base of every generated `.gau`/`.chk`/`.log` filename; the charge/multiplicity tag (for example `-singlet-neutral` or `-doublet-plus`) and `_init`/`_final` are appended automatically. They do not affect the route, coordinates, or job title.

**Method Selection (normal DFT or TD-DFT for excited states):**

Set the method per species; per-state keys override the species default. A state that cannot be reached by normal DFT (S1, T2, or any Sn/Tn) must use `tddft`. For a TD-DFT state, the `*_charge`/`*_mult` values are the SCF reference, and the excited state is selected by `tddft_states` plus `tddft_extra`/`tddft_root`. The global / per-step / per-state TD-DFT options, their precedence and worked examples are in [TD-DFT (excited-state) barriers](#361-td-dft-excited-state-barriers) below.

| Key | Description | Values | Default |
|-----|-------------|--------|---------|
| `donor_method` | Donor method, applied to both donor states | `dft` \| `tddft` | `dft` |
| `acceptor_method` | Acceptor method, applied to both acceptor states | `dft` \| `tddft` | `dft` |
| `donor_init_method` | Donor initial-state method override | `dft` \| `tddft` | `donor_method` |
| `donor_final_method` | Donor final-state method override | `dft` \| `tddft` | `donor_method` |
| `acceptor_init_method` | Acceptor initial-state method override | `dft` \| `tddft` | `acceptor_method` |
| `acceptor_final_method` | Acceptor final-state method override | `dft` \| `tddft` | `acceptor_method` |
| `tddft_method` | TD method (TDA or full TD) | `tda` \| `td` | `tda` |
| `tddft_states` | Excited-state manifold | `singlets` \| `triplets` \| `50-50` \| blank | `singlets` |
| `tddft_nstates` | Number of excited states | integer > 0 | 15 |
| `tddft_extra` | Extra keywords inside `td(...)`/`tda(...)`, e.g. `Root=1,Read` | text | (empty) |
| `tddft_root` | Convenience for `Root=N` (ignored if `tddft_extra` already sets a root) | integer > 0 | (unset) |
| `step1_tddft_*`, `step2_tddft_*` | Per-step overrides of the TD-DFT keys above | same values | inherit global |
| `<state>_tddft_*` | Per-state overrides, e.g. `donor_init_tddft_states` | same values | inherit step/global |

Resolved route fragment example: `tda(singlets,nstates=3,Root=1)`. TD-DFT is inserted between `opt(...)` and `freq` in step 1, and before `scf(...)` in step 2.

**Step 1 Theory (Geometry Optimization + Freq):**

| Key | Description | Default |
|-----|-------------|---------|
| `step1_functional` | DFT functional | `uwb97xd` |
| `step1_basis` | Basis set | `def2svpp` |
| `step1_basis_path` | Custom basis path | (empty) |
| `step1_solvent_model` | Solvent model | `smd` |
| `step1_solvent` | Solvent name; blank for gas phase | (empty) |
| `step1_solvent_extra` | Extra SCRF keywords | (empty) |
| `step1_scf_maxcycle` | SCF max cycles | 300 |
| `step1_opt_maxcycles` | Opt max cycles | 300 |
| `step1_opt_options` | Extra opt keywords | (empty) |
| `step1_scf_options` | Extra SCF keywords | (empty) |
| `step1_mem` | Memory | (empty) |
| `step1_nprocshared` | NProcShared | (empty) |
| `step1_cpu` | CPU | (empty) |
| `step1_gpucpu` | GPUCPU | (empty) |
| `step1_print_level` | Print level | (empty) |
| `step1_tail` | Basis tail block | (empty) |
| `step1_route_extra_keywords` | Extra route keywords | (empty) |
| `step1_tddft_method` | Step-1 TD method override | (empty) |
| `step1_tddft_states` | Step-1 TD states override | (empty) |
| `step1_tddft_nstates` | Step-1 number of excited states override | (empty) |
| `step1_tddft_extra` | Step-1 extra TD keywords override | (empty) |

**Step 2 Theory (Single Point):**

| Key | Description | Default |
|-----|-------------|---------|
| `step2_functional` | DFT functional | `uwb97xd` |
| `step2_basis` | Basis set | `def2tzvp` |
| `step2_basis_path` | Custom basis path | (empty) |
| `step2_solvent_model` | Solvent model | `smd` |
| `step2_solvent` | Solvent name; blank for gas-phase single points | (empty) |
| `step2_solvent_extra` | Extra SCRF keywords; ignored in gas phase | `ExternalIteration,NonEq=read` |
| `step2_scf_maxcycle` | SCF max cycles | 500 |
| `step2_mem` | Memory | (empty) |
| `step2_nprocshared` | NProcShared | (empty) |
| `step2_cpu` | CPU | (empty) |
| `step2_gpucpu` | GPUCPU | (empty) |
| `step2_print_level` | Print level | (empty) |
| `step2_tail` | Basis tail block | (empty) |
| `step2_route_extra_keywords` | Extra route keywords | `Geom=Check Guess(Read)` |
| `step2_tddft_method` | Step-2 TD method override | (empty) |
| `step2_tddft_states` | Step-2 TD states override | (empty) |
| `step2_tddft_nstates` | Step-2 number of excited states override | (empty) |
| `step2_tddft_extra` | Step-2 extra TD keywords override | (empty) |

**Step 3 — Barrier Calculation & Thermo Options:**

| Key | Description | Default |
|-----|-------------|---------|
| `temperature` | Temperature (K) | 298.15 |
| `concentration` | Concentration (mol/L) | 1.0 |
| `pressure` | Thermo-recalculation pressure (atm) | 1.0 |
| `low_vib_method` | Low-frequency treatment | `grimme` |
| `ravib` | Truhlar threshold (cm⁻¹) | 100.0 |
| `scale_zpe` | ZPE scaling factor | 1.0 |
| `scale_heat` | Heat capacity scaling factor | 1.0 |
| `scale_entropy` | Entropy scaling factor | 1.0 |
| `scale_cv` | CV scaling factor | 1.0 |
| `interp_vib` | Vibrational interpolation (cm⁻¹) | 100.0 |
| `imag_real` | Imaginary frequency threshold | 0.0 |
| `external_energy` | External electronic energy (Hartree) | 0.0 |
| `mass_mode` | Mass assignment mode | 3 |
| `prt_level` | Thermo-module output verbosity (0-3), independent of report level | 1 |
| `print_vib` | Print vibration contributions | 0 |
| `ip_mode` | Phase mode (0=gas, 1=condensed) | 0 |
| `point_group` | Point group label | (empty) |
| `hg_entropy` | Head-Gordon entropy interpolation | `false` |
| `bav_preset` | Bav preset (qchem\|grimme) | (empty) |
| `output_otm` | Output .otm file | `false` |
| `use_input_temp` | Activate thermo module T pathway | `false` |
| `use_input_concentration` | Activate thermo module C pathway | `false` |
| `use_input_pressure` | Activate pressure pathway | `false` |
| `report_level` | Marcus report detail: `default` or `verbose` (`-v` enables verbose) | `default` |
| `out_file` | Output report path | (empty) |

**Workflow Example:**

```bash
# 1. Generate template
cck marcus --genoset-input -o my_marcus.inp

# 2. Edit my_marcus.inp — set donor_name, donor_xyz, acceptor_name, acceptor_xyz,
#    charges, multiplicities, and any thermo options (uncomment to activate)

# 3. Step 1: Generate inputs
cck marcus my_marcus.inp -step 1
# Submit .gau files to Gaussian

# 4. Step 2: Generate single point inputs
cck marcus my_marcus.inp -step 2
# Submit .gau files to Gaussian

# 5. Step 3: Calculate barriers with thermo options
cck marcus my_marcus.inp -step 3
```

If step-2 electronic outputs are missing or unusable, step 3 lists the status and path of all eight expected logs, states the required energy type, and stops without writing a report. An energy being present does not confirm normal Gaussian termination; check the jobs before rerunning.

### 3.6.1 TD-DFT (excited-state) barriers

The four-point workflow is unchanged when a state uses TD-DFT: step 1 generates `opt freq` inputs with the TD-DFT keyword, and step 2 generates single-point inputs with the same keyword. Donor and acceptor are independent, and so are their states; for example, a donor S1 state (TD-DFT) can be combined with a ground-state acceptor (DFT), or both sides can use TD-DFT. A state that normal DFT cannot reach (S1, T2, any Sn/Tn) must use `tddft`; a triplet such as T1 that *can* be reached by normal DFT only needs `*_mult = 3` and stays `dft`.

#### The five TD-DFT options

These names mirror the `calc_type = tddft` options of the `create-input` module:

| Option | Meaning | Values | Default |
|--------|---------|--------|---------|
| `tddft_method` | Excited-state method | `tda` \| `td` | `tda` |
| `tddft_states` | Excited-state manifold | `singlets` \| `triplets` \| `50-50` \| blank | `singlets` |
| `tddft_nstates` | Number of excited states | integer > 0 | 15 |
| `tddft_extra` | Extra keywords inside `td(...)` / `tda(...)`, e.g. `Read,Root=1,IVOGuess` | text | (empty) |
| `tddft_root` | Convenience for `Root=N`; ignored when `tddft_extra` already contains a root | integer > 0 | (unset) |

A blank `tddft_states` is normalised to `singlets`. For a TD-DFT state, the state's `*_charge`/`*_mult` are the **SCF reference** (for example `0 1` for a neutral singlet reference), and the target excited state is selected by `tddft_states` + `tddft_root`/`tddft_extra`. **Rule:** for a given state the same `tddft_states`/`tddft_root` must be used in step 1 and step 2 — step 1 optimizes that excited state and step 2 evaluates the same state at the optimized geometry. Do not change the root between steps.

#### Three levels: global, per-step, per-state

Each of the five options can be given at three levels. The levels are merged **per option**, so you can set shared defaults and override only the values that differ.

| Level | Key pattern | Example key | Applies to |
|-------|-------------|-------------|------------|
| Global | `tddft_<option>` | `tddft_states` | every TD-DFT state, both steps |
| Per-step | `step1_tddft_<option>` / `step2_tddft_<option>` | `step2_tddft_extra` | both states of both species for one step |
| Per-state | `<state>_tddft_<option>` | `donor_init_tddft_root` | one state and all its jobs (equilibrium + cross, step 1 + step 2) |

`<state>` is one of `donor_init`, `donor_final`, `acceptor_init`, `acceptor_final`. In the generated template these groups are written as `step1_tddft_*`, `step2_tddft_*`, and `<state>_tddft_*`; the `*` is only a wildcard for the five option names and is **not** a literal key. The `<option>` part is always one of `method`, `states`, `nstates`, `extra`, `root`.

Precedence, highest first:

```text
per-state  >  per-step  >  global  >  built-in defaults
```

Built-in defaults are `tda`, `singlets`, `15`, empty extra and no root. The full expansion of the two template lines is therefore:

```text
# option names: method | states | nstates | extra | root

# Global
tddft_method  tddft_states  tddft_nstates  tddft_extra  tddft_root

# Per-step (step1 = opt+freq, step2 = single point)
step1_tddft_method  step1_tddft_states  step1_tddft_nstates  step1_tddft_extra  step1_tddft_root
step2_tddft_method  step2_tddft_states  step2_tddft_nstates  step2_tddft_extra  step2_tddft_root

# Per-state
donor_init_tddft_method      donor_init_tddft_states      donor_init_tddft_nstates      donor_init_tddft_extra      donor_init_tddft_root
donor_final_tddft_method     donor_final_tddft_states     donor_final_tddft_nstates     donor_final_tddft_extra     donor_final_tddft_root
acceptor_init_tddft_method   acceptor_init_tddft_states   acceptor_init_tddft_nstates   acceptor_init_tddft_extra   acceptor_init_tddft_root
acceptor_final_tddft_method  acceptor_final_tddft_states  acceptor_final_tddft_nstates  acceptor_final_tddft_extra  acceptor_final_tddft_root
```

Because the merge is per option, this input:

```ini
tddft_states        = singlets
tddft_nstates       = 5
tddft_root          = 2
step2_tddft_nstates = 10
```

resolves for the donor initial state in step 2 to `method = tda` (default), `states = singlets` (global), `nstates = 10` (step 2), `root = 2` (global), i.e. `tda(singlets,nstates=10,Root=2)`. Step 1 inherits `nstates = 5` and the same `root = 2`, i.e. `tda(singlets,nstates=5,Root=2)`. The root is never changed between steps: step 1 optimizes that excited state and step 2 evaluates the same state.

#### Example 1 — donor S1 with TD-DFT, donor product and acceptor with DFT

This is the common mixed case: the donor starts in an excited singlet, so only the donor initial state needs TD-DFT.

```ini
donor_init_method  = tddft
donor_final_method = dft
acceptor_method    = dft

tddft_method  = tda
tddft_states  = singlets
tddft_nstates = 3
tddft_root    = 1
```

Generated step-1 route for the TD-DFT state:

```
#P opt(maxcycles=300) tda(singlets,nstates=3,Root=1) freq scf(maxcycle=300,xqc) uwb97xd/def2svpp scrf=(smd,solvent=acetonitrile)
```

Generated step-2 equilibrium and cross routes:

```
#P tda(singlets,nstates=3,Root=1) scf(maxcycle=500,xqc) uwb97xd/def2tzvp scrf=(smd,solvent=acetonitrile,NonEq=write) Geom(Check) Guess(read) nosymm
#  tda(singlets,nstates=3,Root=1) scf(maxcycle=500,xqc) uwb97xd/def2tzvp scrf=(smd,solvent=acetonitrile,ExternalIteration,NonEq=read) Geom=Check Guess(read) nosymm
```

For a TD-DFT state the step-2 electronic energy is taken from the excited-state line:

- `tddft_method = tda` → `Total Energy, E(CIS/TDA)`
- `tddft_method = td` → `Total Energy, E(TD-HF/TD-DFT)`

In solution phase, `After PCM corrections, the energy is` is still preferred when present (it is the state-specific corrected excited-state energy); otherwise the TD-DFT line is used, then `SCF Done` as the last fallback. All thermochemistry options and settings apply to TD-DFT exactly as for DFT, because the thermal corrections are computed from the step-1 excited-state `opt freq` geometry and frequencies.

#### Example 2 — per-step overrides with the same root

Use per-step overrides when the optimization and the single point need different TD settings, but keep `tddft_root` identical: step 1 optimizes that excited state and step 2 evaluates the same state at the optimized geometry.

```ini
donor_method      = tddft
acceptor_method   = dft

tddft_method  = tda
tddft_states  = singlets
tddft_root    = 2            # S2 is optimized and single-pointed

step1_tddft_nstates = 5      # smaller state count during optimization
step2_tddft_nstates = 10     # larger state count for the single point
```

Step-1 TD-DFT keyword: `tda(singlets,nstates=5,Root=2)`. Step-2 TD-DFT keyword: `tda(singlets,nstates=10,Root=2)`. The root must be the same in both steps; only settings that do not change the state being computed (such as `nstates` or extra keywords) should differ between steps.

#### Example 3 — both species TD-DFT with different states

Use per-state overrides when both sides are TD-DFT but target different manifolds/roots:

```ini
donor_method    = tddft
acceptor_method = tddft

tddft_method  = tda
tddft_nstates = 5

# Donor initial state = S1
donor_init_tddft_states = singlets
donor_init_tddft_root   = 1

# Acceptor initial state = T2
acceptor_init_tddft_states = triplets
acceptor_init_tddft_root   = 2
```

Per-state keys accept every one of the five options (`method`, `states`, `nstates`, `extra`, `root`). They are consulted only when that state's `*_method` is `tddft`; for DFT states they are ignored.

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

### 3.7 IVCoord Module — Imaginary Mode Coordinate Displacement

Displace molecular geometries along imaginary vibrational modes to generate reactant and product guess structures from a transition state. This implements the Quick Reaction Coordinate (QRC) approach.

***Note: currently, only outputs from Gaussian are supported. Other quantum chemistry programs will be supported soon.***

#### Basic Usage

```bash
# Displace all TS log files along the imaginary mode (+direction by default)
cck ivcoord *.log

# Specify files explicitly
cck ivcoord ts1.log ts2.log

# Both + and − directions
cck ivcoord *.log --idirection 0

# Custom displacement amplitude
cck ivcoord *.log --iamp 2.0

# Generate parameter template
cck ivcoord --gen-ivcoord-params

# Use a parameter file
cck ivcoord *.log --param-file ivcoord_parameters.params
```

#### Options

| Option                 | Description                      | Values                   | Default |
| ---------------------- | -------------------------------- | ------------------------ | ------- |
| `--iamp <value>`       | Displacement amplitude           | positive decimal         | 1.0     |
| `--idirection <value>` | Direction(s) to displace         | 1=plus, -1=minus, 0=both | 1       |
| `--param-file <path>`  | Load parameters from file        | file path                | -       |
| `--gen-ivcoord-params` | Generate parameter template file | (flag)                   | -       |

#### Output

For each input file `<name>.log`, output(s) are written under `<parent_basename>_ivcoord/`:

- `<name>_p.xyz` — geometry displaced in the positive direction (`--idirection 1` or `0`)
- `<name>_m.xyz` — geometry displaced in the negative direction (`--idirection -1` or `0`)

Example: `ts_calcs/mol.log` → `ts_calcs_ivcoord/mol_p.xyz` (and/or `mol_m.xyz`)

#### Parameter File

Generate and edit a reusable template:

```bash
cck ivcoord --gen-ivcoord-params
# Creates: ivcoord_parameters.params
```

Template contents:

```
# IVCoord parameters
# iamp: displacement amplitude (default: 1.0)
# idirection: 1=plus only, -1=minus only, 0=both
iamp = 1.0
idirection = 1
```

#### Workflow to remove imaginary mode with IVCoord

```bash
# 1. Run geometrical optimization with frequency calculation
cck ci --calc-type opt_freq
# ... submit and wait ...

# 2. Identify outputs containing imaginary modes
cck imode

# 3. Displace the geometry along imaginary mode
cck ivcoord *_imode/*.log --idirection 0

# 4. Create optimization inputs from displaced geometries
cck ci --calc-type opt_freq 

# 5. Analyze thermochemistry of products
cck thermo *.log -T 298.15 -lowvibmeth 2
```

### 3.8 RMSD Module — Structure Comparison

Calculate the root-mean-square deviation (RMSD) between two molecular geometries read from XYZ or PDB files (`.gz` transparent). Both structures are centered, optimally rotated (Kabsch or quaternion method), optionally reordered when atoms are listed in different orders, and optionally searched over reflections and axis swaps. Works on single pairs as well as whole structure lists, producing a pairwise RMSD matrix for heatmap visualisation and clustering.

#### Basic Usage

**Pair mode** (print one RMSD):

```bash
cck rmsd a.xyz b.xyz

# Ignore hydrogens, find best atom correspondence first
cck rmsd -n --reorder --reorder-method hungarian a.xyz b.xyz

# Scan reflections + axis swaps (alters stereochemistry)
cck rmsd --use-reflections conf1.xyz conf2.xyz

# Print rotated structure B instead of a number
cck rmsd -p a.xyz b.xyz
```

**Matrix mode** (pairwise lower-triangle CSV beside the list file):

```bash
cck rmsd --from-list list.txt
cck rmsd list.txt   # single positional = list file

# Split rows across queue jobs, then merge (SLURM array example)
cck rmsd --from-list big.txt --rows 0:250 --out part-0.csv
cck rmsd --merge-csv full.csv part-0.csv part-1.csv

# Thread count uses the global flag, like every other module
cck rmsd --from-list big.txt -nt 8
```

List files accept one filename per line, whitespace-separated, or comma-separated entries. Relative entries resolve against the list file's directory. Interrupted runs resume from complete rows in the existing CSV (torn tail lines are discarded); `--overwrite` restarts fresh.

#### Options

| Option | Description | Values | Default |
| ------ | ----------- | ------ | ------- |
| `-r, --rotation` | Rotation/alignment method | kabsch, quaternion, none | kabsch |
| `--reorder` | Enable atom reordering (no `-e` short flag: `-e` means `--ext` globally) | (flag) | false |
| `--reorder-method` | Reorder method | none, hungarian, inertia-hungarian, distance, brute | inertia-hungarian |
| `--use-reflections` | Scan 6 axis perms x 8 sign reflections (48 trials) | (flag) | false |
| `--use-reflections-keep-stereo` | Same, skipping enantiomeric trials | (flag) | false |
| `--only-alpha-carbons` | PDB alpha carbons only | (flag, PDB-only) | false |
| `-n, --ignore-hydrogen` | Ignore hydrogen atoms | (flag) | false |
| `--remove-idx` | Drop atom indices (space/comma separated) | indices | - |
| `--add-idx` | Keep only atom indices (space/comma separated) | indices | - |
| `--format` | Input format | xyz, pdb (auto-detected) | auto |
| `-p, --print` | Print rotated structure B as XYZ (pair mode only) | (flag) | false |
| `--print-only-rmsd-atoms` | With filtering, print only RMSD atoms | (flag) | false |
| `--from-list` | Structure list for matrix mode | file path | - |
| `--rows` | Row range chunk `START:END` (0-based, END-exclusive) | range | - |
| `--out` | Output CSV path | file path | beside list |
| `--overwrite` | Discard existing output, recompute | (flag) | false |
| `--max-mem` | Matrix memory budget, e.g. `4GB` | size | auto |
| `--memory-limit` | Same budget in MB (`0` = auto) | integer | 0 (auto) |
| `--merge-csv` | Merge partial matrix CSVs into OUT (needs >= 2) | OUT PART... | - |

Reorder methods (`--reorder` enables, `--reorder-method` selects; `--reorder` with `none` errors): `none` (atoms must already match), `hungarian` (optimal assignment, Munkres), `inertia-hungarian` (default; inertia-tensor pre-alignment + Hungarian over 8 sign reflections, best for unaligned + shuffled inputs), `distance` (cheap centroid-distance heuristic), `brute` (exhaustive per-element permutations, tiny systems only as reference).

#### Resource Management

Threads and memory follow the same extract scheme: `-nt half` default with headnode caps, memory auto (30-60% of system RAM by thread count, 1-32 GB clamp, 5% job-allocation reserve). Matrix rows compute in parallel while output streams in blocks, so peak RAM stays at molecules + one block.

```bash
cck rmsd --from-list big.txt -nt 8        # 8 threads (global -nt/--threads flag)
cck rmsd --from-list big.txt -nt 2 -q     # headnode-safe
```

#### Notes

- As a rule of thumb, values below ~0.5 A indicate nearly identical structures, 1-2 A a related but relaxed geometry, larger values a different conformation or bad atom correspondence.
- On symmetric tops with degenerate inertia moments (e.g. ethane), `inertia-hungarian`/`distance` results depend on eigensolver/tie-breaking details: they are deterministic but not guaranteed to match other implementations exactly.
- Mismatched atom order without `--reorder` (or reflection search carrying a reorder method) errors instead of silently scoring; `-p` cannot be combined with atom filtering + reordering.
- Golden-value verification: `./test_rmsd.sh` (fixtures in `tests/rmsd/`).

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

# Process larger files (increase 500MB default limit)
cck --max-file-size 1000

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

# Or report single-file components from the current directory only (no ../ lookup)
cck extract-au          # Text table: Output name | E | ZPE | TC | TS | H | G | LowFQ | PhaseCorr
cck extract-au -f csv   # CSV version for reporting
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
cck ci --calc-type ts_freq --freeze-bond 1 2

# Analyze TS candidates
cck thermo ts_candidates.log -imagreal 100 -lowvibmeth 1 -ravib 50

# Filter and organize
cck imode
cck done

# Displace TS geometry along imaginary mode to generate reactant/product guesses
cck ivcoord *_imode/*.log --idirection 0

# Create optimization inputs from displaced structures
cck ci --calc-type opt_freq
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

- **Default limit**: 500MB per file
- **Larger files**: Use `--max-file-size 1000` to increase limit
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
ls -lh *.log  # Files >500MB are automatically skipped

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

## Current version
- **v0.1.6** Release version

## Contributing and Issues

For bug reports, feature requests, or safety concerns, please create an issue at the project repository. When reporting issues, include:

- System specifications (OS, compiler, cluster environment)
- Command used and error messages
- Sample files (if possible)
- Resource usage information (`--resource-info` output)

---
