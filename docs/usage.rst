Usage Guide
===========

This comprehensive guide covers all features and commands of ComChemKit with detailed examples and explanations.

Quick Start
-----------

**Basic Usage:**

.. code-block:: bash

   # Extract data from all .log files in current directory
   cck

   # Process with custom settings
   cck -t 300 -c 2 -f csv -q

   # Get help
   cck --help

Command Overview
----------------

ComChemKit supports the following commands:

+------------------+--------------------------------------------------+
| Command          | Description                                      |
+==================+==================================================+
| ``extract``      | Extract thermodynamic data (default)             |
+------------------+--------------------------------------------------+
| ``done``         | Check and organize completed jobs                |
+------------------+--------------------------------------------------+
| ``errors``       | Check and organize failed jobs                   |
+------------------+--------------------------------------------------+
| ``pcm``          | Check PCM convergence failures                   |
+------------------+--------------------------------------------------+
| ``imode``        | Check jobs with imaginary frequencies            |
+------------------+--------------------------------------------------+
| ``check``        | Run all job checks                               |
+------------------+--------------------------------------------------+
| ``thermo``       | Calculate thermochemical properties (OpenThermo) |
+------------------+--------------------------------------------------+
| ``high-kj``      | Calculate high-level energies (kJ/mol)           |
+------------------+--------------------------------------------------+
| ``high-au``      | Calculate high-level energies (atomic units)     |
+------------------+--------------------------------------------------+
| ``extract-au``   | Single-file energies, au breakdown (no Status)   |
+------------------+--------------------------------------------------+
| ``xyz``          | Extract coordinates to XYZ format                |
+------------------+--------------------------------------------------+
| ``ci``           | Create Gaussian input files from XYZ             |
+------------------+--------------------------------------------------+
| ``ivcoord``      | Displace geometry along imaginary modes (QRC)    |
+------------------+--------------------------------------------------+
| ``rmsd``         | RMSD between structures (XYZ/PDB), pair+matrix   |
+------------------+--------------------------------------------------+
| ``marcus``       | Marcus barrier calculations (OSET / EnT)         |
+------------------+--------------------------------------------------+
| ``interactive``  | Launch interactive mode (Windows)                |
+------------------+--------------------------------------------------+

Core Commands
=============

1. Extract (Default Command)
-----------------------------

**Purpose:** Extract thermodynamic data from Gaussian log files.

**Basic Usage:**

.. code-block:: bash

   # Process all .log files in current directory
   cck

   # Process .out files instead
   cck -e out

**Output Formats:**

**Text Format (Default):**

.. code-block:: bash

   cck

**CSV Format:**

.. code-block:: bash

   cck -f csv

**Sample Output:**

.. code-block::

   Found 150 .log files
   Using 4 threads (requested: half)
   Processed 75/150 files (50%)
   Processed 150/150 files (100%)

   Results written to project_directory.results
   Total execution time: 12.347 seconds
   Memory usage: 256.7 MB / 4.0 GB

**Output Columns Explanation:**

+------------------+--------------------------------------------------+
| Column           | Description                                      |
+==================+==================================================+
| Output name      | Name of the Gaussian output file                 |
+------------------+--------------------------------------------------+
| ETG kJ/mol       | Gibbs free energy with phase correction          |
+------------------+--------------------------------------------------+
| Low FC           | Lowest vibrational frequency                     |
+------------------+--------------------------------------------------+
| ETG a.u          | Gibbs free energy in atomic units                |
+------------------+--------------------------------------------------+
| Nuclear E au     | Nuclear repulsion energy                         |
+------------------+--------------------------------------------------+
| SCFE             | SCF energy in atomic units                       |
+------------------+--------------------------------------------------+
| ZPE              | Zero-point energy                                |
+------------------+--------------------------------------------------+
| Status           | Job status (DONE/UNDONE)                         |
+------------------+--------------------------------------------------+
| PCorr            | Phase correction applied (YES/NO)                |
+------------------+--------------------------------------------------+
| Round            | Number of Gaussian calculation rounds            |
+------------------+--------------------------------------------------+

**Advanced Options:**

.. code-block:: bash

   # Custom temperature and concentration
   cck -t 310.15 -c 5

   # Sort by different column (2=ETG kJ/mol, 3=Low FC, 6=SCFE)
   cck -col 6

   # Quiet mode (minimal output)
   cck -q

   # Use specific number of threads
   cck -nt 8

   # Process files larger than default 500MB limit
   cck --max-file-size 1000

**Temperature and Phase Correction:**

.. code-block:: bash

   # Use temperature from input files (default)
   cck

   # Override with custom temperature
   cck -t 298.15

   # Custom concentration for phase correction
   cck -c 2  # 2M concentration

**Understanding Phase Correction:**

Phase correction converts gas-phase energies to solution-phase:

- **Gas phase (1 atm)** → **Solution (1M standard state)**
- Applied automatically for SCRF calculations
- Critical for accurate free energy comparisons

2. Job Status Commands
-----------------------

**Purpose:** Check and organize Gaussian jobs by completion status.

**Check Completed Jobs:**

.. code-block:: bash

   # Move completed jobs to {dirname}-done/
   cck done

   # Custom directory suffix
   cck done --dir-suffix completed

**Check Failed Jobs:**

.. code-block:: bash

   # Move failed jobs to errorJobs/
   cck errors

   # Show detailed error messages
   cck errors --show-details

   # Custom target directory
   cck errors --target-dir failed_calculations

**Check PCM Failures:**

.. code-block:: bash

   # Move PCM convergence failures to PCMMkU/
   cck pcm

**Check Imaginary Frequencies:**

.. code-block:: bash

   # Move jobs with imaginary frequencies to imaginary_freqs/
   cck imode

**Run All Checks:**

.. code-block:: bash

   # Execute all job checks in sequence
   cck check

**Workflow Example:**

.. code-block:: bash

   # 1. Run calculations
   # (submit your Gaussian jobs)

   # 2. Check completion status
   cck done

   # 3. Check for failures
   cck errors

   # 4. Check PCM issues
   cck pcm

   # 5. Check vibrational analysis
   cck imode

3. High-Level Energy Calculations
----------------------------------

**Purpose:** Calculate energies using high-level electronic energies combined with low-level thermal corrections.

**Directory Structure:**

.. code-block::

   project/
   ├── low_level/          # Opt + Freq calculations
   │   ├── molecule1.log
   │   └── molecule2.log
   └── high_level/         # Single point calculations
       ├── molecule1.log
       └── molecule2.log

**Basic Usage:**

.. code-block:: bash

   # Navigate to high-level directory
   cd high_level

   # Calculate energies in kJ/mol
   cck high-kj

   # Calculate detailed energies in atomic units
   cck high-au

**Energy Combination Process:**

1. **High-level electronic energy** from current directory
2. **Thermal corrections** from parent directory (../)
3. **Combined result**: E_high + (E_low_thermal - E_low_electronic)

**Output Formats:**

**high-kj (kJ/mol):**

.. code-block::

   Name              G kJ/mol    Status
   molecule1         -1234.56    DONE
   molecule2         -2345.67    DONE

**high-au (Atomic Units - Detailed):**

.. code-block::

   Name    E high    E low     ZPE      TC       TS       H        G
   mol1    -456.78   -450.12   0.123    0.456    0.789    -455.67  -456.46

**extract-au (Atomic Units - Single-file reporting table, no Status column):**

.. code-block::

   Name    E         ZPE      TC       TS       H        G        LowFQ   PhaseCorr
   mol1    -456.78   0.123    0.012    0.045    -455.67  -456.46  25.14   YES

.. code-block:: bash

   # Single-file breakdown from current directory only (no ../ lookup)
   cck extract-au              # Text table
   cck extract-au -f csv       # CSV for reporting
   cck extract-au -col 7       # Sort by G a.u (1-9: Name,E,ZPE,TC,TS,H,G,LowFQ,PhCorr)
   cck extract-au -t 273 -c 1  # Recalculated thermochemistry via thermo module

**Default Sorting:**

Both commands order rows by Gibbs free energy (ascending, most negative first)
by default, so the same compounds appear in the same order in either table:

- ``high-kj`` sorts by ``G kJ/mol`` (column 2)
- ``high-au`` sorts by ``G a.u`` (column 8)

Use ``-col`` to sort by another column (see the column lists above).

**Advanced Options:**

.. code-block:: bash

   # Custom temperature
   cck high-kj -t 310.15

   # Custom concentration (accepts -c, -C, --cm, --conc)
   cck high-kj -c 2

   # Sort by different column
   cck high-kj -col 4
   cck high-au -col 2          # E high a.u instead of the default G a.u (column 8)

   # CSV output
   cck high-kj -f csv

4. Coordinate Extraction
-------------------------

**Purpose:** Extract final Cartesian coordinates from Gaussian log files and organize them.

**Basic Usage:**

.. code-block:: bash

   # Extract coordinates from all log files
   cck xyz

   # Extract from specific files
   cck xyz -f molecule1.log molecule2.log

**Output Organization:**

.. code-block::

   current_directory/
   ├── molecule1.log
   ├── molecule2.log
   └── current_directory_final_coord/
       ├── molecule1.xyz
       └── molecule2.xyz

**Directory Structure:**

- **Completed jobs** → ``{dirname}_final_coord/``
- **Incomplete jobs** → ``{dirname}_running_coord/``

**XYZ File Format:**

.. code-block::

   12
   molecule1.log Final coordinates
   C     0.000000    0.000000    0.000000
   H     1.089000    0.000000    0.000000
   H    -0.363000    1.032000    0.000000
   ...

**Advanced Usage:**

.. code-block:: bash

   # Process .out files
   cck xyz -e out

   # Use multiple threads
   cck xyz -nt 8

   # Quiet mode
   cck xyz -q

5. Create Input Files (ci)
---------------------------

**Purpose:** Generate Gaussian input files from XYZ coordinate files.

**Supported Calculation Types:**

+------------------+--------------------------------------------------+
| Type             | Description                                      |
+==================+==================================================+
| ``sp``           | Single point energy (default)                    |
+------------------+--------------------------------------------------+
| ``opt_freq``     | Geometry optimization + frequency                |
+------------------+--------------------------------------------------+
| ``ts_freq``      | Transition state search + frequency              |
+------------------+--------------------------------------------------+
| ``oss_ts_freq``  | Open-shell singlet TS + frequency                |
+------------------+--------------------------------------------------+
| ``modre_ts_freq``| Modredundant TS + frequency                      |
+------------------+--------------------------------------------------+
| ``modre_opt``    | Modredundant optimization                        |
+------------------+--------------------------------------------------+
| ``oss_check_sp`` | Open-shell singlet stability check               |
+------------------+--------------------------------------------------+
| ``high_sp``      | High-level single point                          |
+------------------+--------------------------------------------------+
| ``irc_forward``  | IRC calculation (forward direction)              |
+------------------+--------------------------------------------------+
| ``irc_reverse``  | IRC calculation (reverse direction)              |
+------------------+--------------------------------------------------+
| ``irc``          | IRC calculation (both directions)                |
+------------------+--------------------------------------------------+
| ``tddft``        | TD-DFT excited state calculation                 |
+------------------+--------------------------------------------------+

**Basic Examples:**

.. code-block:: bash

   # Single point energy calculation (default)
   cck ci

   # Geometry optimization + frequency
   cck ci --calc-type opt_freq

   # Transition state search
   cck ci --calc-type ts_freq

**Advanced Examples:**

.. code-block:: bash

   # Transition state with frozen bond
   cck ci --calc-type modre_ts_freq --freeze-bond 1 2

   # High-level single point with custom functional
   cck ci --calc-type high_sp --functional B3LYP --basis 6-311+G**

   # Solvent calculation
   cck ci --calc-type opt_freq --solvent water --solvent-model smd

   # Fix PCM solvation discontinuity (SES -> normal surface, two-section Link1)
   # Supported calc types: sp, opt_freq, ts_freq, oss_check_sp, modre_opt
   cck ci --calc-type opt_freq --solvent water --solvent-model smd --fix-pcm

   # PCM fix with explicit temperature for thermochemical corrections
   cck ci --calc-type opt_freq --solvent water --solvent-model smd --fix-pcm --temperature 373.15

   # IRC from transition state
   cck ci --calc-type irc --tschk-path ../ts_checkpoints

   # Custom settings
   cck ci --calc-type opt_freq --charge 1 --mult 2

**Multiple XYZ Files:**

.. code-block:: bash

   # Process multiple files (comma-separated)
   cck ci file1.xyz,file2.xyz,file3.xyz

   # Mixed separators (space and comma)
   cck ci file1.xyz file2.xyz,file3.xyz

   # With calculation type
   cck ci --calc-type opt_freq file1.xyz,file2.xyz

**Template System:**

**Generate Templates:**

.. code-block:: bash

   # Generate template for specific calculation type
   cck ci --genci-params opt_freq

   # Generate all available templates
   cck ci --genci-all-params

   # Generate in specific directory
   cck ci --genci-params opt_freq ./my_templates

**Use Templates:**

.. code-block:: bash

   # Use specific parameter file
   cck ci --param-file opt_freq.params

   # Use default parameter file
   cck ci --param-file

**Template Workflow:**

1. **Generate Template:**

   .. code-block:: bash

      cck ci --genci-params opt_freq

2. **Edit Template (opt_freq.params):**

   .. code-block::

      calc_type = opt_freq
      functional = B3LYP
      basis = 6-31G*
      solvent = chloroform
      charge = 1
      mult = 2
      route_extra_keywords = Int=UltraFine

3. **Use Template:**

   .. code-block:: bash

      cck ci --param-file opt_freq.params

Restart and extra opt/scf sub-options (param-file only):

   .. code-block::

      # Extra tokens appended inside the parentheses (comma-separated;
      # ';' and '|' are also accepted as commas)
      scf_options = restart,Conver=8
      opt_options = restart,ModRedundant
      # Restart-only shortcuts (default false; true/yes/1 activates)
      scf_restart = false   # true -> scf(maxcycle=300,xqc,restart)
      opt_restart = false   # true -> opt(maxcycles=300,restart)

   Notes: ``*_options`` are appended inside ``scf(...)``/``opt(...)``;
   ``*_restart=true`` appends a single ``restart`` (deduped if already listed
   in ``*_options``); defaults (``""``/``false``) leave current routes unchanged;
   ``opt_*`` applies to opt_freq/ts_freq/modre types, ``scf_*`` also applies to
   sp/tddft/high_sp/oss_check_sp; IRC types ignore both.

**Parameter File Keys:**

Every key is written as ``name = value`` (one per line; ``#`` starts a comment).
CLI flags set starting values; any key present in the parameter file overrides them.
Keys marked *param-file-only* have no CLI flag.

Computing-resource (Link0) keys (param-file-only):

- ``mem`` / ``cpu`` / ``nprocshared`` / ``gpucpu`` (string, default empty = omit that line) —
  Link0 directives emitted once at the top of the generated input, before ``%chk``,
  in canonical order ``%Mem``, ``%CPU``, ``%NProcShared``, ``%GPUCPU``.
  Only the first (``--link0--``) section carries them; later ``--Link1--`` sections inherit them.
  Values are emitted verbatim, e.g. ``mem = 16GB``, ``cpu = 0-31``,
  ``nprocshared = 32``, ``gpucpu = 0,1,2,3,4,5=0,1,16,17,18,19``
  (cf. ``tests/create-input/22_468.gau``).
  Templates carry these keys commented out; uncomment the lines you need.

Basic keys:

- ``calc_type`` (string, default ``sp``) — calculation type; see the table above.
  ``irc`` is valid but has no template file; ``ts_freq_from_chk`` is internal-only
  (it is generated as the second section of ``oss_ts_freq`` / ``modre_ts_freq``).
- ``functional`` (string, default ``UWB97XD``, uppercased) — DFT functional, e.g. ``B3LYP``.
- ``basis`` (string, default ``Def2SVPP``, uppercased) — basis set. ``GEN`` / ``GENECP``
  requires a ``tail`` block carrying the custom basis.
- ``large_basis`` (string, default empty) — basis of the ``high_sp`` single point
  (the template also recommends setting it for TS/high-level types, but only the
  ``high_sp`` route consumes it; all other routes use ``basis``).
- ``charge`` (integer, default ``0``), ``mult`` (integer, default ``1``) — molecule spec.
- ``extension`` (string, default ``.gau``) — output file extension.

Solvent keys:

- ``solvent`` (string, default empty = gas phase), e.g. ``water`` — adds ``scrf(model,solvent=X)``.
- ``solvent_model`` (string, default ``smd``) — ``smd``, ``cpcm`` or ``iefpcm``.
- ``solvent_extra`` (string, default empty) — appended after ``solvent=``, e.g. ``read``.
  Ignored in section 1 of ``fix_pcm`` inputs, which always use ``,read``.

Route-control keys:

- ``print_level`` (string, default empty) — route prefix: ``#``, ``#P`` / ``#T`` / ``#N``,
  anything else gives ``#<value> ``.
- ``route_extra_keywords`` (string, default empty) — extra route tokens; separators may be
  spaces, ``|``, ``,`` or ``;``. Example: ``route_extra_keywords = Int=UltraFine SCF=Conver=8``.

SCF / OPT tuning keys (param-file-only):

- ``scf_maxcycle`` (integer, default ``300``) — ``scf(maxcycle=N,xqc ...)``.
- ``scf_options`` (string, default empty) — extra tokens inside ``scf(...)``,
  e.g. ``scf_options = restart,Conver=8``.
- ``scf_restart`` (boolean, default ``false``) — ``true`` / ``yes`` / ``1`` appends a single
  ``restart``: ``scf(maxcycle=300,xqc,restart)``.
- ``opt_maxcycles`` (integer, default ``300``) — ``opt(maxcycles=N ...)``.
- ``opt_options`` (string, default empty) — extra tokens inside ``opt(...)``.
- ``opt_restart`` (boolean, default ``false``) — appends a single ``restart``:
  ``opt(maxcycles=300,restart)``.
- ``opt_maxstep`` (integer, default unset; ``ts_freq_from_chk`` sections default to ``5``) —
  adds ``,MaxStep=N``.
- ``opt_*`` keys apply to types emitting ``opt(...)``; ``scf_*`` keys additionally apply to
  ``sp``, ``tddft``, ``high_sp`` and ``oss_check_sp``; ``irc`` types ignore both groups.
  ``*_restart`` is merged with ``*_options`` and deduped case-insensitively.

IRC keys (param-file-only):

- ``irc_maxpoints`` (default ``50``), ``irc_recalc`` (default ``10``),
  ``irc_maxcycle`` (default ``350``), ``irc_stepsize`` (default ``10``).

TS-constraint keys:

- ``freeze_bond = 1,2`` (comma- or space-separated; ``freeze_bond_atom1`` / ``freeze_bond_atom2``
  are the fallback) — required for ``oss_ts_freq`` / ``modre_ts_freq`` unless ``modre`` is given.
  The former names ``freeze_atoms`` / ``freeze_atom1`` / ``freeze_atom2`` are still accepted
  but deprecated.
- ``modre`` (multiline) — ModRedundant section; wrap lines in ``(`` ... ``)`` to preserve
  blank lines.

Checkpoint and TDDFT keys:

- ``tschk_path`` (string, default: parent directory) — TS checkpoint location for ``high_sp``
  and ``irc_forward`` / ``irc_reverse``.
- ``tddft_method`` (default ``tda``) — ``td`` or ``tda``.
- ``tddft_states`` (default empty = both files) — ``singlets``, ``triplets``, ``50-50`` or empty.
  ``singlets`` / ``triplets`` write ``stem-singlets.gau`` / ``stem-triplets.gau``.
- ``tddft_nstates`` (integer, default ``15``) — ``nstates=N``.
- ``tddft_extra`` (string, default empty) — tokens inside ``tda(...)``,
  e.g. ``tddft_extra = Root=5,Read,IVOGuess``.

PCM-fix and custom-content keys:

- ``fix_pcm`` (boolean, default ``false``) — two-section Link1 SES input for solvation
  discontinuity correction. Not valid for ``high_sp``, ``oss_ts_freq``, ``modre_ts_freq``,
  ``tddft`` or ``irc`` types.
- ``temperature`` (number, unset) — ``Temperature=K`` added to the section-2 route.
- ``tail`` (multiline, default empty) — appended at the end of the file (custom basis / ECP).
- ``extra_options`` (multiline, param-file-only) — extra section appended after the
  route/molecule content.

**Calculation Types in Detail:**

Each type below shows a minimal parameter file and the exact route line it generates
(all routes verified against the implementation).

- ``sp`` — single point. ``calc_type = sp`` gives::

     # scf(maxcycle=300,xqc) UWB97XD/DEF2SVPP

- ``opt_freq`` — optimization + frequency. Minimal file::

     calc_type = opt_freq
     functional = UWB97XD
     basis = def2SVPP
     charge = 0
     mult = 1

  gives::

     # opt(maxcycles=300) freq scf(maxcycle=300,xqc) UWB97XD/DEF2SVPP

- ``ts_freq`` — TS search + frequency. The template suggests ``large_basis`` for TS/high-level types, but
  the route itself uses ``basis``::

     calc_type = ts_freq
     functional = UWB97XD
     basis = def2SVPP
     large_basis = def2TZVPP

  gives::

     # opt(maxcycles=300,ts,noeigen,calcfc) freq scf(maxcycle=300,xqc) UWB97XD/DEF2SVPP

- ``modre_opt`` — optimization with ModRedundant constraints. Requires ``freeze_bond``
  (or ``modre``)::

     calc_type = modre_opt
     freeze_bond = 1,2

  gives::

     # opt(maxcycles=300,modredundant) scf(maxcycle=300,xqc) UWB97XD/DEF2SVPP

- ``oss_check_sp`` — open-shell singlet stability check::

     # Stable=Opt scf(maxcycle=300,xqc) UWB97XD/DEF2SVPP

- ``high_sp`` — high-level single point reusing the TS checkpoint (``Guess(Read)
  Geom(AllCheck)``). ``large_basis`` selects the route basis::

     calc_type = high_sp
     large_basis = def2TZVPP

  gives::

     # scf(maxcycle=300,xqc) UWB97XD/DEF2TZVPP Guess(Read) Geom(AllCheck)

- ``irc_forward`` / ``irc_reverse`` — needs the TS ``.chk`` (via ``tschk_path`` or the
  parent directory); writes ``stemR.gau`` / ``stemF.gau``::

     # irc=(Forward,RCFC,MaxPoints=50,Recalc=10,MaxCycle=350,StepSize=10,loose,LQA,nogradstop) UWB97XD/DEF2SVPP Guess(Read) Geom(AllCheck)

- ``irc`` — writes both ``stemF.gau`` and ``stemR.gau`` (Forward + Reverse sections,
  ``MaxPoints=50`` each).
- ``tddft`` — ``tddft_states = singlets`` writes ``stem-singlets.gau``::

     # tda(singlets,nstates=15) scf(maxcycle=300,xqc) UWB97XD/DEF2SVPP

  ``tddft_states = 50-50`` writes a single ``stem.gau`` with ``tda(50-50,nstates=15)``;
  empty ``tddft_states`` writes both files.
- ``oss_ts_freq`` / ``modre_ts_freq`` — multi-section Link1 jobs (stability check, then
  constrained optimization, then TS-from-checkpoint with ``MaxStep=5``). Example
  ``modre_ts_freq`` routes::

     # opt(maxcycles=300,modredundant) scf(maxcycle=300,xqc) UWB97XD/DEF2SVPP
     # opt(maxcycles=300,ts,noeigen,calcfc,NoFreeze,MaxStep=5) freq scf(maxcycle=300,xqc) UWB97XD/DEF2SVPP Guess(Read) Geom(AllCheck)

- ``fix_pcm = true`` (with ``solvent``) — wraps ``sp`` / ``opt_freq`` / ``ts_freq`` /
  ``oss_check_sp`` / ``modre_opt`` in a two-section Link1 job; section 1 uses
  ``scrf(...,read)``, section 2 adds ``Guess(Read) Geom(Allcheck)`` and ``Temperature=``::

     # opt(maxcycles=300) scf(maxcycle=300,xqc) UWB97XD/DEF2SVPP scrf(smd,solvent=water,read)
     # opt(maxcycles=300) freq scf(maxcycle=300,xqc) UWB97XD/DEF2SVPP scrf(smd,solvent=water) Guess(Read) Geom(Allcheck) Temperature=298.15

- ``GEN`` / ``GENECP`` basis — ``basis = GENECP`` plus a ``tail`` block::

     # scf(maxcycle=300,xqc) UWB97XD/GENECP

  with the ``tail`` content (e.g. ``C H 0`` / ``6-31G*`` / ``****``) appended at file end.

**CLI Flags vs Parameter Keys:**

Each CLI flag seeds a default that the parameter file overrides. These map one-to-one:
``--calc-type`` / ``--functional`` / ``--basis`` / ``--large-basis`` / ``--solvent`` /
``--solvent-model`` / ``--solvent-extra`` / ``--charge`` / ``--mult`` / ``--print-level`` /
``--extra-keywords`` (→ ``route_extra_keywords``) / ``--tail`` / ``--extension`` /
``--tschk-path`` / ``--freeze-bond`` / ``--tddft-method`` / ``--tddft-states`` /
``--tddft-nstates`` / ``--tddft-extra`` / ``--fix-pcm`` / ``--temperature``.

Param-file-only keys (no CLI flag): ``scf_maxcycle``, ``scf_options``, ``scf_restart``,
``opt_maxcycles``, ``opt_options``, ``opt_restart``, ``opt_maxstep``, ``irc_maxpoints``,
``irc_recalc``, ``irc_maxcycle``, ``irc_stepsize``, ``freeze_bond_atom1`` / ``freeze_bond_atom2``,
``modre``, ``extra_options``.

**Troubleshooting:**

- ``GEN/GENECP basis requires tail`` — add a ``tail`` block with the custom basis/ECP.
- ``freeze_bond or modre required`` for ``oss_ts_freq`` / ``modre_ts_freq`` — set one of them.
- ``--fix-pcm is not yet implemented`` for ``high_sp`` / ``oss_ts_freq`` / ``modre_ts_freq``
  (also not valid for ``tddft`` / ``irc``) — run without ``fix_pcm``.
- ``TS checkpoint file not found`` for ``high_sp`` / ``irc`` — set ``tschk_path`` or place the
  ``.chk`` in the parent directory.
- ``opt_*`` / ``scf_*`` keys silently do nothing on ``irc`` types (no ``opt(...)`` / ``scf(...)``
  in IRC routes).

**Generated Input File Example:**

.. code-block::

   %chk=molecule1.chk
   # B3LYP/6-31G* Opt Freq

   molecule1 B3LYP/6-31G* Opt Freq

   1 2
   C     0.000000    0.000000    0.000000
   H     1.089000    0.000000    0.000000
   ...

6. Thermo Module (OpenThermo)
-----------------------------

**Purpose:** Comprehensive thermochemical property calculations using statistical mechanics methods from quantum chemistry output files.

**Supported Quantum Chemistry Programs:**

- Gaussian (.log, .out)
- ORCA (.out)
- GAMESS-US (.log)
- NWChem (.out)
- CP2K (.out)
- VASP (OUTCAR)
- Q-Chem (.out/.log)
- OpenThermo Format (.otm)

**Basic Usage:**

.. code-block:: bash

   # Single file thermochemistry calculation
   cck thermo molecule.log

   # Custom temperature and pressure
   cck thermo molecule.log -T 298.15 -P 1.0

   # Head-Gordon's low-frequency treatment with Q-Chem preset
   cck thermo molecule.log -lowvibmeth headgordon -bav qchem -prtlevel 2

**Batch Processing (Parallel):**

.. code-block:: bash

   # Process multiple files with auto-detected threads
   cck thermo files.list

   # Specify thread count
   cck thermo files.list -nt 8

   # OpenMP parallelization for scan loops
   cck thermo molecule.log -T 200 400 25 -omp-threads 4

**Temperature/Pressure Scanning:**

.. code-block:: bash

   # Temperature scan
   cck thermo molecule.log -T 200 400 25

   # Combined scan (creates .UHG and .SCq files)
   cck thermo molecule.log -T 273 373 50 -P 0.5 2.0 0.5


**Command-Line Options:**

+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| Option                        | Description                       | Values                                    | Default |
+===============================+===================================+===========================================+=========+
| ``-T <T>`` or ``-T <...>``    | Temperature (K)                   | number or range                           | 298.15  |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-P <P>`` or ``-P <...>``    | Pressure (atm)                    | number or range                           | 1.0     |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-sclZPE <factor>``          | ZPE frequency scaling             | decimal                                   | 1.0     |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-sclheat <factor>``         | Thermal energy scaling            | decimal                                   | 1.0     |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-sclS <factor>``            | Entropy scaling                   | decimal                                   | 1.0     |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-sclCV <factor>``           | Heat capacity scaling             | decimal                                   | 1.0     |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-lowvibmeth <mode>``        | Low-frequency treatment           | 0/Harmonic, 1/Truhlar, ... 4/HeadGordon   | 2       |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-ravib <value>``            | Truhlar raising threshold (cm^-1) | decimal                                   | 100.0   |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-intpvib <value>``          | Interpolation threshold (cm^-1)   | decimal                                   | 100.0   |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-hgEntropy <0|1>``          | Head-Gordon entropy interpolation | 0=no, 1=yes                               | 1       |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-bav <preset>``             | Head-Gordon Bav preset            | grimme, qchem, auto                       | auto    |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-ipmode <mode>``            | Phase mode                        | 0=gas, 1=condensed                        | 0       |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-imagreal <value>``         | Imaginary freq threshold (cm^-1)  | decimal                                   | 0.0     |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-massmod <type>``           | Mass assignment                   | 1=average, 2=isotope, 3=file              | 3       |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-PGname <name>``            | Point group                       | string or "?"                             | "?"     |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-conc <string>``            | Concentration (mol/L)             | decimal                                   | 0       |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-prtlevel <0-3>``           | Output verbosity level            | 0=minimal, 1=default, 2=verbose, 3=full   | 1       |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-prtvib <mode>``            | Vibrational contributions         | 0=no, 1=screen, -1=file                   | 0       |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-outotm <mode>``            | Output .otm file                  | 0=no, 1=yes                               | 0       |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-omp-threads <threads>``    | OpenMP parallelization threads    | positive integer, 0=auto                  | 0       |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-nt <threads>``             | File processing thread count      | number, half, max                         | half    |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-memory-limit <MB>``        | Memory limit                      | positive integer                          | auto    |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+
| ``-E <value>``                | Electronic energy override (a.u.) | decimal                                   | from file |
+-------------------------------+-----------------------------------+-------------------------------------------+---------+

**Calculation Methods:**

**Standard RRHO (Rigid-Rotor Harmonic Oscillator)**

- Traditional harmonic approximation
- Suitable for most molecular systems

**Quasi-RRHO Treatments:**

1. **Truhlar's Method** (``lowvibmeth = 1``)
   - Raises frequencies below threshold (default: 100 cm^-1)
   - Simple and computationally efficient

2. **Grimme's Interpolation** (``lowvibmeth = 2`` or ``grimme``)
   - Entropy interpolation between RRHO and free rotor
   - Weighting: ``w = 1 / (1 + (ν_threshold/ν)^4)``
   - Better treatment of low-frequency modes

3. **Minenkov's Interpolation** (``lowvibmeth = 3`` or ``minenkov``)
   - Extended Grimme's method with energy interpolation
   - Includes both entropy and thermal energy corrections
   - Most comprehensive treatment for flexible molecules

4. **Head-Gordon Interpolation** (``lowvibmeth = 4`` or ``headgordon``)
   - Smoothed damping for entropic contributions of low vibrational modes
   - Supported by Q-Chem inspired free rotor references (``-bav qchem``)
   - Fully supports interpolating entropy (``-hgEntropy 1``)

7. IVCoord — Imaginary Mode Coordinate Displacement
-----------------------------------------------------

**Purpose:** Displace molecular geometries along the imaginary vibrational mode of a transition state to generate reactant and product guess structures. This implements the Quick Reaction Coordinate (QRC) method.

.. note::
   Currently only Gaussian output files are supported. Support for additional quantum chemistry programs is planned.

**Basic Usage:**

.. code-block:: bash

   # Displace all TS log files (positive direction by default)
   cck ivcoord *.log

   # Both + and − directions
   cck ivcoord *.log --idirection 0

   # Custom displacement amplitude
   cck ivcoord *.log --iamp 2.0

   # Specify files explicitly
   cck ivcoord ts1.log ts2.log

**Options:**

.. table:: IVCoord Options

   +---------------------------+--------------------------------------------+---------------------------+---------+
   | Option                    | Description                                | Values                    | Default |
   +===========================+============================================+===========================+=========+
   | ``--iamp <value>``        | Displacement amplitude                     | positive decimal          | 1.0     |
   +---------------------------+--------------------------------------------+---------------------------+---------+
   | ``--idirection <value>``  | Direction(s) to generate                   | 1=plus, -1=minus, 0=both  | 1       |
   +---------------------------+--------------------------------------------+---------------------------+---------+
   | ``--param-file <path>``   | Load parameters from file                  | file path                 | -       |
   +---------------------------+--------------------------------------------+---------------------------+---------+
   | ``--gen-ivcoord-params``  | Generate parameter template file           | (flag)                    | -       |
   +---------------------------+--------------------------------------------+---------------------------+---------+

**Output Files:**

For each input file ``<name>.log``, outputs are written to a new ``<parent_basename>_ivcoord/`` directory:

- ``<name>_p.xyz`` — geometry displaced in the positive direction (``--idirection 1`` or ``0``)
- ``<name>_m.xyz`` — geometry displaced in the negative direction (``--idirection -1`` or ``0``)

**Example:** ``ts_calcs/mol.log`` → ``ts_calcs_ivcoord/mol_p.xyz`` (and/or ``mol_m.xyz``)

**Parameter File:**

Generate a reusable parameter template:

.. code-block:: bash

   cck ivcoord --gen-ivcoord-params
   # Creates: ivcoord_parameters.params

Template contents:

.. code-block::

   # IVCoord parameters
   # iamp: displacement amplitude (default: 1.0)
   # idirection: 1=plus only, -1=minus only, 0=both
   iamp = 1.0
   idirection = 1

Use the template:

.. code-block:: bash

   cck ivcoord *.log --param-file ivcoord_parameters.params

**Workflow to remove imaginary mode with IVCoord:**

.. code-block:: bash

   # 1. Run geometrical optimization with frequency calculation
   cck ci --calc-type opt_freq

   # ... submit and wait for jobs to complete ...

   # 2. Identify outputs containing imaginary modes
   cck imode

   # 3. Displace TS geometry along imaginary mode (both directions or one direction)
   cck ivcoord *_imode/*.log --idirection 0

   # 4. Create optimization inputs from displaced structures
   cck ci --calc-type opt_freq

   # 5. Analyze thermochemistry of products
   cck thermo *.log -T 298.15 -lowvibmeth 2

8. RMSD — Structure Comparison
--------------------------------

**Purpose:** Calculate the root-mean-square deviation (RMSD) between two molecular geometries read from XYZ or PDB files (``.gz`` transparent). Both structures are centered, optimally rotated (Kabsch or quaternion method), optionally reordered when atoms are listed in different orders, and optionally searched over reflections and axis swaps. Works on single pairs as well as whole structure lists, producing a pairwise RMSD matrix for heatmap visualisation and clustering.

**Basic Usage:**

.. code-block:: bash

   # Pair mode: print one RMSD
   cck rmsd a.xyz b.xyz

   # Ignore hydrogens, find best atom correspondence first
   cck rmsd -n --reorder --reorder-method hungarian a.xyz b.xyz

   # Scan reflections + axis swaps (alters stereochemistry)
   cck rmsd --use-reflections conf1.xyz conf2.xyz

   # Print rotated structure B instead of a number
   cck rmsd -p a.xyz b.xyz

**Matrix Mode:**

.. code-block:: bash

   cck rmsd --from-list list.txt
   cck rmsd list.txt   # single positional = list file

   # Split rows across queue jobs, then merge (SLURM array example)
   cck rmsd --from-list big.txt --rows 0:250 --out part-0.csv
   cck rmsd --merge-csv full.csv part-0.csv part-1.csv

   # Thread count uses the global flag, like every other module
   cck rmsd --from-list big.txt -nt 8

List files accept one filename per line, whitespace-separated, or comma-separated entries. Relative entries resolve against the list file's directory. Interrupted runs resume from complete rows in the existing CSV (torn tail lines are discarded); ``--overwrite`` restarts fresh.

**Options:**

.. table:: RMSD Options

   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | Option                            | Description                                       | Values                 | Default           |
   +===================================+===================================================+========================+===================+
   | ``-r, --rotation``                | Rotation/alignment method                         | kabsch|quaternion|none | kabsch            |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``--reorder``                     | Enable reordering (no ``-e``: it means ``--ext``) | (flag)                 | false             |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``--reorder-method``              | Reorder method (see below)                        | none|hungarian|...     | inertia-hungarian |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``--use-reflections``             | Scan 48 swap x reflection trials                  | (flag)                 | false             |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``--use-reflections-keep-stereo`` | Same, skipping enantiomeric trials                | (flag)                 | false             |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``--only-alpha-carbons``          | PDB alpha carbons only                            | (flag, PDB-only)       | false             |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``-n, --ignore-hydrogen``         | Ignore hydrogen atoms                             | (flag)                 | false             |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``--remove-idx``                  | Drop atom indices (space/comma separated)         | indices                | -                 |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``--add-idx``                     | Keep only atom indices (space/comma separated)    | indices                | -                 |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``--format``                      | Input format                                      | xyz|pdb (auto)         | auto              |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``-p, --print``                   | Print rotated structure B (pair mode only)        | (flag)                 | false             |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``--print-only-rmsd-atoms``       | With filtering, print only RMSD atoms             | (flag)                 | false             |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``--from-list``                   | Structure list for matrix mode                    | file path              | -                 |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``--rows``                        | Row range chunk ``START:END``                     | range                  | -                 |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``--out``                         | Output CSV path                                   | file path              | beside list       |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``--overwrite``                   | Discard existing output, recompute                | (flag)                 | false             |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``--max-mem``                     | Matrix memory budget, e.g. ``4GB``                | size                   | auto              |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``--memory-limit``                | Same budget in MB (``0`` = auto)                  | integer                | 0                 |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+
   | ``--merge-csv``                   | Merge partial CSVs into OUT (>= 2)                | OUT PART...            | -                 |
   +-----------------------------------+---------------------------------------------------+------------------------+-------------------+

Reorder methods (``--reorder`` enables, ``--reorder-method`` selects; ``--reorder`` with ``none`` errors): ``none`` (atoms must already match), ``hungarian`` (optimal assignment, Munkres), ``inertia-hungarian`` (default; inertia-tensor pre-alignment + Hungarian over 8 sign reflections), ``distance`` (cheap centroid-distance heuristic), ``brute`` (exhaustive per-element permutations, tiny systems only).

**Resource Management:**

Threads and memory follow the same extract scheme: ``-nt half`` default with headnode caps, memory auto (30-60% of system RAM by thread count, 1-32 GB clamp, 5% job-allocation reserve). Matrix rows compute in parallel while output streams in blocks, so peak RAM stays at molecules + one block.

.. code-block:: bash

   cck rmsd --from-list big.txt -nt 8        # 8 threads (global -nt/--threads flag)
   cck rmsd --from-list big.txt -nt 2 -q     # headnode-safe

.. note::
   As a rule of thumb, values below ~0.5 A indicate nearly identical structures, 1-2 A a related but relaxed geometry, larger values a different conformation or bad atom correspondence. On symmetric tops with degenerate inertia moments (e.g. ethane), ``inertia-hungarian``/``distance`` results depend on eigensolver/tie-breaking details: they are deterministic but not guaranteed to match other implementations exactly.

Golden-value verification: ``./test_rmsd.sh`` (fixtures in ``tests/rmsd/``).

9. Marcus Theory Barrier Calculations (marcus)
----------------------------------------------

**Purpose:** Calculate Marcus theory barriers for Outer Sphere Single Electron Transfer (OSET) and Energy Transfer (EnT) using the four-point method. The workflow has three steps:

1. **Step 1**: Generate geometry optimization + frequency inputs (4 `.gau` files)
2. **Step 2**: Generate single point inputs reading step-1 checkpoints (8 `.gau` files)
3. **Step 3**: Calculate barriers from collected outputs (ΔG°r, λ_R, λ_P, symmetric & asymmetric barriers)

**Directory Structure:**

.. code-block::

   project/
   ├── PC.xyz                    # Donor coordinates
   ├── D1.xyz                    # Acceptor coordinates
   ├── marcus_project.inp        # Marcus input file
   ├── marcus_project_step_1/    # Step 1: optimization inputs
   ├── marcus_project_step_2/     # Step 2: single point inputs
   └── marcus_project.log  # Step 3: barrier report

Steps 1 and 2 overwrite matching generated ``.gau`` files and manifests. Existing
Gaussian ``.log`` and ``.chk`` files are not overwritten by input generation.

Existing projects using ``<project>_step_1_opt/`` and ``<project>_step_2_sp/``
need to rename them to ``<project>_step_1/`` and ``<project>_step_2/``.
Regenerate step-2 inputs if needed: their ``%OldChk`` paths refer to the
original step-1 directory.

**Basic Usage:**

.. code-block:: bash

   # Generate an input template (OSET)
   cck marcus --genoset-input -o marcus_project.inp

   # Generate an input template (EnT)
   cck marcus --genent-input -o marcus_project.inp

   # Edit marcus_project.inp with your molecule names, XYZ paths, charges, multiplicities, etc.

   # Step 1: Generate optimization inputs
   cck marcus marcus_project.inp -step 1
   # Submit the .gau files to Gaussian, wait for completion

   # Step 2: Generate single point inputs (reads step-1 checkpoints)
   cck marcus marcus_project.inp -step 2
   # Submit the .gau files to Gaussian, wait for completion

   # Step 3: Calculate barriers
   cck marcus marcus_project.inp -step 3   # reports both barriers


**Command-Line Options:**

.. table:: Marcus Options

   +------------------+--------------------------------------------------+------------------+-----------+
   | Option           | Description                                      | Values           | Default   |
   +==================+==================================================+==================+===========+
   | ``<input_file>`` | Input file (positional)                          | file path        | required  |
   +------------------+--------------------------------------------------+------------------+-----------+
   | ``-step``        | Workflow step                                    | 1, 2, 3          | 1,2       |
   +------------------+--------------------------------------------------+------------------+-----------+
   | ``-O, --out``    | Output report file                               | file path        | -         |
   +------------------+--------------------------------------------------+------------------+-----------+
   | ``-v, --verbose``| Verbose report and console diagnostics           | (flag)           | false     |
   +------------------+--------------------------------------------------+------------------+-----------+
   | ``-t, --temp``   | Temperature (K)                                  | decimal          | 298.15    |
   +------------------+--------------------------------------------------+------------------+-----------+
   | ``-c, --conc``   | Concentration (mol/L)                            | decimal          | 1.0       |
   +------------------+--------------------------------------------------+------------------+-----------+
   | ``--pressure``   | Pressure (atm)                                   | decimal          | 1.0       |
   +------------------+--------------------------------------------------+------------------+-----------+
   | ``--lowvibmeth`` | Low-frequency treatment                          | harmonic|truhlar|grimme|minenkov|headgordon | grimme   |
   +------------------+--------------------------------------------------+------------------+-----------+
   | ``--ravib``      | Truhlar threshold (cm^-1)                        | decimal          | 100.0     |
   +------------------+--------------------------------------------------+------------------+-----------+
   | ``--genoset-input``| Generate OSET input template                 | (flag)           | -         |
   +------------------+--------------------------------------------------+------------------+-----------+
   | ``--genent-input`` | Generate EnT input template                  | (flag)           | -         |
   +------------------+--------------------------------------------------+------------------+-----------+
   | ``-o, --output-template`` | Template output path                      | file path        | -         |
   +------------------+--------------------------------------------------+------------------+-----------+
   | ``-h, --help``   | Show help                                        | (flag)           | -         |
   +------------------+--------------------------------------------------+------------------+-----------+

**Input File (`marcus_project.inp`) Keys:**

The input file uses ``key = value`` format (``#`` = comment) and can have any
extension. Unless ``project`` is set, the filename without its last extension
determines the project name; for example, ``reaction.inp`` gives ``reaction``.
Relative paths are resolved from the current working directory. All step 3
thermo options are supported:

**Project Identification:**

.. table:: Project Identification Keys

   +------------------+--------------------------------------------------+-----------+
   | Key              | Description                                      | Default   |
   +==================+==================================================+===========+
   | ``project``      | Project name (used for directory names)          | filename stem |
   +------------------+--------------------------------------------------+-----------+
   | ``calc_type``    | Calculation type (oset \| ent)                   | oset      |
   +------------------+--------------------------------------------------+-----------+

**Donor / Acceptor Definitions:**

.. table:: Donor/Acceptor Keys

   +---------------------------+--------------------------------------------------+-----------+
   | Key                       | Description                                      | Default   |
   +===========================+==================================================+===========+
   | ``donor_name``            | Donor molecule name                              | (required)|
   +---------------------------+--------------------------------------------------+-----------+
   | ``donor_xyz``             | Donor XYZ file path                              | (required)|
   +---------------------------+--------------------------------------------------+-----------+
   | ``donor_init_charge``     | Donor initial state charge                       | 0         |
   +---------------------------+--------------------------------------------------+-----------+
   | ``donor_init_mult``       | Donor initial state multiplicity                 | 1         |
   +---------------------------+--------------------------------------------------+-----------+
   | ``donor_final_charge``    | Donor final state charge                         | 0         |
   +---------------------------+--------------------------------------------------+-----------+
   | ``donor_final_mult``      | Donor final state multiplicity                   | 1         |
   +---------------------------+--------------------------------------------------+-----------+
   | ``acceptor_name``         | Acceptor molecule name                           | (required)|
   +---------------------------+--------------------------------------------------+-----------+
   | ``acceptor_xyz``          | Acceptor XYZ file path                           | (required)|
   +---------------------------+--------------------------------------------------+-----------+
   | ``acceptor_init_charge``  | Acceptor initial state charge                    | 0         |
   +---------------------------+--------------------------------------------------+-----------+
   | ``acceptor_init_mult``    | Acceptor initial state multiplicity              | 1         |
   +---------------------------+--------------------------------------------------+-----------+
   | ``acceptor_final_charge`` | Acceptor final state charge                      | 0         |
   +---------------------------+--------------------------------------------------+-----------+
   | ``acceptor_final_mult``   | Acceptor final state multiplicity                | 1         |
   +---------------------------+--------------------------------------------------+-----------+

**Step 1 Theory (Geometry Optimization + Freq):**

.. table:: Step 1 Theory Keys

   +---------------------------+--------------------------------------------------+-----------+
   | Key                       | Description                                      | Default   |
   +===========================+==================================================+===========+
   | ``step1_functional``      | DFT functional                                   | uwb97xd   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step1_basis``           | Basis set                                        | def2svpp  |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step1_basis_path``      | Custom basis path                                | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step1_solvent_model``   | Solvent model                                    | smd       |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step1_solvent``         | Solvent name; blank for gas phase                | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step1_solvent_extra``   | Extra SCRF keywords                              | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step1_scf_maxcycle``    | SCF max cycles                                   | 300       |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step1_opt_maxcycles``   | Opt max cycles                                   | 300       |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step1_opt_options``     | Extra opt keywords                               | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step1_scf_options``     | Extra SCF keywords                               | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step1_mem``             | Memory                                           | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step1_nprocshared``     | NProcShared                                      | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step1_cpu``             | CPU                                              | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step1_gpucpu``          | GPUCPU                                           | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step1_print_level``     | Print level                                      | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step1_tail``            | Basis tail block                                 | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step1_route_extra_keywords`` | Extra route keywords                        | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+

**Step 2 Theory (Single Point):**

.. table:: Step 2 Theory Keys

   +---------------------------+--------------------------------------------------+-----------+
   | Key                       | Description                                      | Default   |
   +===========================+==================================================+===========+
   | ``step2_functional``      | DFT functional                                   | uwb97xd   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step2_basis``           | Basis set                                        | def2tzvp  |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step2_basis_path``      | Custom basis path                                | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step2_solvent_model``   | Solvent model                                    | smd       |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step2_solvent``         | Solvent name; blank for gas phase                | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step2_solvent_extra``   | Extra SCRF keywords; ignored in gas phase        | ExternalIteration,NonEq=read |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step2_scf_maxcycle``    | SCF max cycles                                   | 500       |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step2_mem``             | Memory                                           | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step2_nprocshared``     | NProcShared                                      | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step2_cpu``             | CPU                                              | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step2_gpucpu``          | GPUCPU                                           | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step2_print_level``     | Print level                                      | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step2_tail``            | Basis tail block                                 | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``step2_route_extra_keywords`` | Extra route keywords                        | Geom=Check Guess(Read) |
   +---------------------------+--------------------------------------------------+-----------+

**Step 3 — Barrier Calculation & Thermo Options:**

.. table:: Step 3 Thermo Keys

   +---------------------------+--------------------------------------------------+-----------+
   | Key                       | Description                                      | Default   |
   +===========================+==================================================+===========+
   | ``temperature``           | Temperature (K)                                  | 298.15    |
   +---------------------------+--------------------------------------------------+-----------+
   | ``concentration``         | Concentration (mol/L)                            | 1.0       |
   +---------------------------+--------------------------------------------------+-----------+
   | ``pressure``              | Thermo-recalculation pressure (atm)              | 1.0       |
   +---------------------------+--------------------------------------------------+-----------+
   | ``low_vib_method``        | Low-frequency treatment                          | grimme    |
   +---------------------------+--------------------------------------------------+-----------+
   | ``ravib``                 | Truhlar threshold (cm^-1)                        | 100.0     |
   +---------------------------+--------------------------------------------------+-----------+
   | ``scale_zpe``             | ZPE scaling factor                               | 1.0       |
   +---------------------------+--------------------------------------------------+-----------+
   | ``scale_heat``            | Heat capacity scaling factor                     | 1.0       |
   +---------------------------+--------------------------------------------------+-----------+
   | ``scale_entropy``         | Entropy scaling factor                           | 1.0       |
   +---------------------------+--------------------------------------------------+-----------+
   | ``scale_cv``              | CV scaling factor                                | 1.0       |
   +---------------------------+--------------------------------------------------+-----------+
   | ``interp_vib``            | Vibrational interpolation (cm^-1)                | 100.0     |
   +---------------------------+--------------------------------------------------+-----------+
   | ``imag_real``             | Imaginary frequency threshold                    | 0.0       |
   +---------------------------+--------------------------------------------------+-----------+
   | ``external_energy``       | External electronic energy (Hartree)             | 0.0       |
   +---------------------------+--------------------------------------------------+-----------+
   | ``mass_mode``             | Mass assignment mode                             | 3         |
   +---------------------------+--------------------------------------------------+-----------+
   | ``prt_level``             | Thermo-module output verbosity (0-3)              | 1         |
   +---------------------------+--------------------------------------------------+-----------+
   | ``print_vib``             | Print vibration contributions                    | 0         |
   +---------------------------+--------------------------------------------------+-----------+
   | ``ip_mode``               | Phase mode (0=gas, 1=condensed)                  | 0         |
   +---------------------------+--------------------------------------------------+-----------+
   | ``point_group``           | Point group label                                | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``hg_entropy``            | Head-Gordon entropy interpolation                | false     |
   +---------------------------+--------------------------------------------------+-----------+
   | ``bav_preset``            | Bav preset (qchem\|grimme)                       | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``output_otm``            | Output .otm file                                 | false     |
   +---------------------------+--------------------------------------------------+-----------+
   | ``use_input_temp``        | Activate thermo module T pathway                 | false     |
   +---------------------------+--------------------------------------------------+-----------+
   | ``use_input_concentration``| Activate thermo module C pathway                | false     |
   +---------------------------+--------------------------------------------------+-----------+
   | ``use_input_pressure``    | Activate pressure pathway                        | false     |
   +---------------------------+--------------------------------------------------+-----------+
   | ``report_level``          | Marcus report: default or verbose                | default   |
   +---------------------------+--------------------------------------------------+-----------+
   | ``out_file``              | Output report path                               | (empty)   |
   +---------------------------+--------------------------------------------------+-----------+

**Workflow Example:**

.. code-block:: bash

   # 1. Generate template
   cck marcus --genoset-input -o my_marcus.inp

   # 2. Edit my_marcus.inp — set donor_name, donor_xyz, acceptor_name,
   #    acceptor_xyz, charges, multiplicities, and any thermo options

   # 3. Step 1: Generate inputs
   cck marcus my_marcus.inp -step 1
   # Submit .gau files to Gaussian

   # 4. Step 2: Generate single point inputs
   cck marcus my_marcus.inp -step 2
   # Submit .gau files to Gaussian

   # 5. Step 3: Calculate barriers with thermo options
   cck marcus my_marcus.inp -step 3

**Output:**

Step 3 writes ``<project>.log`` in the current directory (or to the explicit
``-O`` / ``out_file`` path). The report cannot overwrite the input file; if
they would have the same name, use a different ``-O`` path. It contains:

The default report includes configured levels of theory and thermochemistry,
per-state provenance, equilibrium and cross electronic energies, Gibbs
components, reorganization components, reaction energy and barriers. Theory
settings come from the input file and are not verified against Gaussian logs.
The reaction energy, reorganization energies and both barriers are reported in
kJ/mol, kcal/mol, atomic units (Hartree) and eV.

Use ``-v`` / ``--verbose`` or ``report_level = verbose`` for thermal breakdowns,
electronic-energy sources and paths, numerical audits and complete configured
settings. The CLI flag enables verbose reporting even if the input requests
``default``; ``prt_level`` controls the thermo module independently. For Marcus,
use ``--version`` to show the program version.

Thermo recalculation is activated by ``-t``, ``-c``, ``--pressure`` or the
corresponding ``use_input_*`` setting. Otherwise thermal corrections are read
from step-1 logs; low-frequency and scaling settings do not recalculate them.
The report records per-state fallback and temperatures. Solution-phase
corrections use a fixed 1 atm standard pressure; fully gas-phase calculations
do not apply the solution-phase correction.

Missing Gibbs corrections or an invalid symmetric barrier are shown as ``N/A``;
the report is written and step 3 returns a nonzero status. If the asymmetric
expression is undefined but the symmetric result is valid, the asymmetric value
is ``N/A`` with a warning and step 3 succeeds. Missing electronic energies stop
the calculation before a report can be written.

Configuration and Customization
===============================

Configuration File
-------------------

**Create Default Configuration:**

.. code-block:: bash

   cck --create-config

**Configuration File Location:**

- **Linux/macOS:** ``~/.cck.conf``
- **Windows:** ``%USERPROFILE%\.cck.conf``

**Sample Configuration:**

.. code-block::

   # Default temperature for calculations
   default_temperature = 298.15

   # Default concentration for phase correction
   default_concentration = 1.0

   # Default output format
   output_format = text

   # Default thread count (half, max, or number)
   default_threads = half

   # File extensions to process
   output_extensions = .log,.out
   input_extensions = .com,.gjf,.gau

**Configuration Options:**

.. table:: Configuration Options

  +-------------------+--------------------------------------------------+
  | Option            | Description                                      |
  +===================+==================================================+
  | default_temperature | Default temperature (K)                        |
  +-------------------+--------------------------------------------------+
  | default_concentration | Default concentration (M)                    |
  +-------------------+--------------------------------------------------+
  | output_format     | Default output format (text/csv)                 |
  +-------------------+--------------------------------------------------+
  | default_threads   | Default thread count (half/max/number)           |
  +-------------------+--------------------------------------------------+
  | output_extensions | File extensions to process                       |
  +-------------------+--------------------------------------------------+
  | input_extensions  | Input file extensions                            |
  +-------------------+--------------------------------------------------+
  | max_file_size     | Maximum file size (MB)                           |
  +-------------------+--------------------------------------------------+
  | memory_limit      | Memory usage limit (MB)                          |
  +-------------------+--------------------------------------------------+

Performance and Resource Management
====================================

Thread Management
-----------------

**Automatic Thread Detection:**

.. code-block:: bash

   # Use half of available cores (recommended)
   cck -nt half

   # Use all available cores
   cck -nt max

   # Use specific number
   cck -nt 8

**Cluster Safety:**

.. code-block:: bash

   # Conservative settings for head nodes
   cck -nt 2 -q

   # Optimal for compute nodes
   cck -nt half

Memory Management
-----------------

**Automatic Memory Limits:**

.. code-block:: bash

   # Check current resource usage
   cck --resource-info

   # Set custom memory limit
   cck --memory-limit 8192

**Memory Allocation Strategy:**

- **1-4 threads:** 30% of system RAM
- **5-8 threads:** 40% of system RAM
- **9-16 threads:** 50% of system RAM
- **17+ threads:** 60% of system RAM

File Size Handling
------------------

**Large File Processing:**

.. code-block:: bash

   # Increase file size limit (default: 500MB)
   cck --max-file-size 1000

   # Process very large files
   cck --max-file-size 1000

Batch Processing
----------------

**Large Directory Handling:**

.. code-block:: bash

   # Enable batch processing
   cck --batch-size 50

   # Auto batch size (default)
   cck --batch-size 0

Safety Features
===============

Cluster Environment Detection
------------------------------

ComChemKit automatically detects cluster environments:

- **SLURM:** ``sbatch``, ``srun`` detection
- **PBS/Torque:** ``qsub``, ``qstat`` detection
- **SGE:** ``qsub`` detection
- **LSF:** ``bsub``, ``bjobs`` detection

**Cluster-Specific Behavior:**

- Conservative thread limits on head nodes
- Automatic resource detection
- Safe memory allocation

Graceful Shutdown
-----------------

**Signal Handling:**

.. code-block:: bash

   # Program responds to SIGINT (Ctrl+C) and SIGTERM
   # Press Ctrl+C to gracefully stop processing

**Shutdown Process:**

1. Signal received
2. Current file processing completes
3. Results written to disk
4. Clean exit with proper resource cleanup

Error Handling
--------------

**File Processing Errors:**

- Corrupted log files are skipped with warnings
- Large files (>500MB) automatically skipped by default
- Memory limits prevent system overload
- Thread-safe error reporting

**Common Error Scenarios:**

.. code-block:: bash

   # Handle large files
   cck --max-file-size 500

   # Reduce memory usage
   cck --memory-limit 4096 -nt 2

   # Check system resources
   cck --resource-info

Advanced Workflows
==================

Complete Computational Chemistry Workflow
------------------------------------------

**Step 1: Generate Input Files**

.. code-block:: bash

   # Create optimization inputs
   cck ci --calc-type opt_freq

   # Submit jobs to queue
   # (use your cluster's job submission system)

**Step 2: Check Job Status**

.. code-block:: bash

   # Check completed jobs
   cck done

   # Check for failures
   cck errors

   # Check vibrational analysis
   cck imode

**Step 3: Extract Results**

.. code-block:: bash

   # Extract thermodynamic data
   cck -t 298.15 -c 1

   # Extract coordinates for next step
   cck xyz

**Step 4: High-Level Calculations**

.. code-block:: bash

   # Navigate to high-level directory
   cd high_level

   # Calculate refined energies
   cck high-kj

High-Throughput Processing
---------------------------

**Batch Processing Setup:**

.. code-block:: bash

   # Process large datasets
   cck -nt 16 --max-file-size 500 --memory-limit 16384

   # Quiet mode for scripts
   cck -q -f csv

   # Resource monitoring
   cck --resource-info

**Script Integration:**

.. code-block:: bash

   #!/bin/bash
   # Process multiple directories

   for dir in dataset1 dataset2 dataset3; do
       cd $dir
       cck -q -f csv
       cd ..
   done

Template-Based Automation
-------------------------

**Create Reusable Templates:**

.. code-block:: bash

   # Generate comprehensive template library
   cck ci --generate-all-templates ./templates

   # Customize templates for different methods
   # Edit template files with your preferred settings

**Automated Processing:**

.. code-block:: bash

   # Process different molecule types
   cck ci --param-file ./templates/opt_freq.params molecule1.xyz
   cck ci --param-file ./templates/ts_freq.params molecule2.xyz
   cck ci --param-file ./templates/high_sp.params molecule3.xyz

Troubleshooting
===============

Common Issues and Solutions
---------------------------

**Memory Issues:**

.. code-block:: bash

   # Reduce thread count
   cck -nt 2

   # Set memory limit
   cck --memory-limit 4096

   # Check system resources
   cck --resource-info

**File Processing Issues:**

.. code-block:: bash

   # Handle large files
   cck --max-file-size 500

   # Process different file types
   cck -e out

   # Check file permissions
   ls -la *.log

**Performance Issues:**

.. code-block:: bash

   # Optimize thread usage
   cck -nt half

   # Use batch processing
   cck --batch-size 25

   # Monitor progress
   cck  # (remove -q for progress display)

**Configuration Issues:**

.. code-block:: bash

   # Reset configuration
   cck --create-config

   # Check configuration
   cck --show-config

   # Validate setup
   cck --resource-info

Best Practices
==============

**For Interactive Use:**

.. code-block:: bash

   # Start with resource check
   cck --resource-info

   # Use conservative settings
   cck -nt 4 -q

   # Monitor progress
   cck

**For Batch Processing:**

.. code-block:: bash

   # Optimize for throughput
   cck -nt half --max-file-size 500 -q -f csv

   # Use templates for consistency
   cck ci --param-file template.params

**For Cluster Environments:**

.. code-block:: bash

   # Head node safety
   cck -nt 2 -q

   # Compute node optimization
   cck -nt max --memory-limit 16384

**Data Management:**

- Use descriptive filenames
- Organize by calculation type
- Keep raw log files for reference
- Use CSV format for data analysis
- Backup important results

Command Reference
=================

**Global Options:**

+---------------------+----------------------------------+
| Option              | Description                      |
+=====================+==================================+
| ``-h, --help``      | Show help message                |
+---------------------+----------------------------------+
| ``-v, --version``   | Show version information         |
+---------------------+----------------------------------+
| ``-q, --quiet``     | Quiet mode                       |
+---------------------+----------------------------------+
| ``-nt, --threads``  | Thread count (number/half/max)   |
+---------------------+----------------------------------+
| ``-e, --ext``       | File extension (.log/.out)       |
+---------------------+----------------------------------+
| ``--max-file-size`` | Maximum file size (MB)           |
+---------------------+----------------------------------+
| ``--memory-limit``  | Memory limit (MB)                |
+---------------------+----------------------------------+
| ``--resource-info`` | Show system resource information |
+---------------------+----------------------------------+

**Extract Command Options:**

+---------------------+----------------------------------+
| Option              | Description                      |
+=====================+==================================+
| ``-t, --temp``      | Temperature (K)                  |
+---------------------+----------------------------------+
| ``-c, --cm``        | Concentration (M)                |
+---------------------+----------------------------------+
| ``-col, --column``  | Sort column (2-10)               |
+---------------------+----------------------------------+
| ``-f, --format``    | Output format (text/csv)         |
+---------------------+----------------------------------+
| ``--use-input-temp``| Use temperature from files       |
+---------------------+----------------------------------+

**Job Checker Options:**

+---------------------+----------------------------------+
| Option              | Description                      |
+=====================+==================================+
| ``--dir-suffix``    | Directory suffix for done jobs   |
+---------------------+----------------------------------+
| ``--target-dir``    | Custom target directory          |
+---------------------+----------------------------------+
| ``--show-details``  | Show detailed error messages     |
+---------------------+----------------------------------+

**Create Input Options:**

+---------------------+----------------------------------+
| Option              | Description                      |
+=====================+==================================+
| ``--calc-type``     | Calculation type                 |
+---------------------+----------------------------------+
| ``--functional``    | DFT functional                   |
+---------------------+----------------------------------+
| ``--basis``         | Basis set                        |
+---------------------+----------------------------------+
| ``--solvent``       | Solvent name                     |
+---------------------+----------------------------------+
| ``--charge``        | Molecular charge                 |
+---------------------+----------------------------------+
| ``--mult``          | Multiplicity                     |
+---------------------+----------------------------------+
| ``--freeze-bond``   | Bond atoms to freeze for TS      |
+---------------------+----------------------------------+
| ``--genci-params``  | Generate parameter template      |
+---------------------+----------------------------------+
| ``--param-file``    | Use parameter file               |
+---------------------+----------------------------------+
| ``--fix-pcm``       | Fix PCM solvation discontinuity  |
+---------------------+----------------------------------+
| ``--temperature``   | Temperature (K) for section 2    |
+---------------------+----------------------------------+


Getting Help
============

**Built-in Help:**

.. code-block:: bash

   # General help
   cck --help

   # Command-specific help
   cck extract --help
   cck ci --help

   # Configuration help
   cck --config-help

**Resource Information:**

.. code-block:: bash

   # System resource check
   cck --resource-info

   # Configuration status
   cck --show-config

**Version Information:**

.. code-block:: bash

   cck --version

This guide covers all major features and usage patterns of ComChemKit. For the most up-to-date information, always refer to the built-in help system.