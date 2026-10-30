#!/usr/bin/env python3
"""Integration tests for Marcus report output; set CCK_BINARY to the cck executable."""

import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

FIXTURES = Path(__file__).resolve().parent
BINARY = Path(os.environ.get("CCK_BINARY", FIXTURES.parent.parent / "build/bin/cck")).resolve()


class MarcusReportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.work = Path(self.temp.name)
        fixture = (FIXTURES / "oset.params").read_text(encoding="utf-8")
        fixture = re.sub(r"(?m)^marcus_mode\s*=.*\n", "", fixture)
        (self.work / "oset.inp").write_text(fixture, encoding="utf-8")

    def tearDown(self):
        self.temp.cleanup()

    def copy_logs(self, project="oset"):
        for fixture, step in (("oset_step_1", 1), ("oset_step_2", 2)):
            dest = self.work / f"{project}_step_{step}"
            dest.mkdir()
            for log in (FIXTURES / fixture).glob("*.log"):
                shutil.copy(log, dest / log.name)

    def run_cck(self, *args):
        return subprocess.run(
            [str(BINARY), "marcus", "oset.inp", *args],
            cwd=self.work, text=True, capture_output=True,
        )

    def run_input(self, input_file, *args):
        return subprocess.run(
            [str(BINARY), "marcus", str(input_file), *args],
            cwd=self.work, text=True, capture_output=True,
        )

    def input_without_project(self):
        return re.sub(r"(?m)^project\s*=.*\n", "", (self.work / "oset.inp").read_text(encoding="utf-8"))

    def result_values(self, report, label):
        value = r"([-\d.]+|N/A)"
        pattern = (r"^\s*" + re.escape(label) + r"\s+" + value + r" kJ/mol\s+" + value +
                   r" kcal/mol\s+" + value + r" au\s+" + value + r" eV$")
        match = re.search(pattern, report, re.MULTILINE)
        self.assertIsNotNone(match, label)
        return tuple(None if v == "N/A" else float(v) for v in match.groups())

    def assert_results_alignment(self, report, max_gap=None):
        labels = ("Reaction free energy, Delta G°", "Reorganization energy, lambda R",
                  "Reorganization energy, lambda P", "Average reorganization, lambda",
                  "Delta G‡ (symmetric)", "Delta G‡ (asymmetric)")
        unit_columns = []
        for label in labels:
            line = next(line for line in report.splitlines()
                        if line.startswith("  " + label) and " kJ/mol" in line)
            fields = list(re.finditer(r"(-?\d+\.\d+|N/A) (kJ/mol|kcal/mol|au|eV)", line))
            self.assertEqual(len(fields), 4, line)
            self.assertEqual([field.group(2) for field in fields], ["kJ/mol", "kcal/mol", "au", "eV"])
            unit_columns.append([field.start(2) for field in fields])
            for previous, following in zip(fields, fields[1:]):
                gap = following.start() - previous.end()
                self.assertGreaterEqual(gap, 3, line)
                if max_gap is not None:
                    self.assertLessEqual(gap, max_gap, line)
        self.assertTrue(all(columns == unit_columns[0] for columns in unit_columns))

    def configure_phases(self, step1="", step2=""):
        params = self.work / "oset.inp"
        params.write_text(params.read_text(encoding="utf-8") +
                          f"\nstep1_solvent = {step1}\nstep2_solvent = {step2}\n", encoding="utf-8")

    def write_gas_logs(self):
        """Four equilibrium + four cross SP logs with independently known energies."""
        step1 = self.work / "oset_step_1"
        step2 = self.work / "oset_step_2"
        step1.mkdir()
        step2.mkdir()
        states = {
            "TEA-singlet-neutral_init": (-100.0, 0.10),
            "TEA-doublet-plus_final": (-99.9, 0.11),
            "p-terphenyl-singlet-neutral_init": (-200.0, 0.20),
            "p-terphenyl-doublet-minus_final": (-200.15, 0.195),
        }
        for name, (energy, thermal) in states.items():
            (step1 / (name + ".log")).write_text(
                "Gaussian\n #P hf/sto-3g opt freq\n\n"
                f" SCF Done: E(RHF) = {energy:.9f} A.U.\n"
                " Temperature   298.150 Kelvin.  Pressure   1.00000 Atm.\n"
                " Frequencies -- 100.0 200.0 300.0\n"
                " Zero-point correction= 0.050000\n"
                " Thermal correction to Energy= 0.060000\n"
                " Thermal correction to Enthalpy= 0.061000\n"
                f" Thermal correction to Gibbs Free Energy= {thermal:.9f}\n",
                encoding="utf-8")
            (step2 / (name + ".log")).write_text(
                f" SCF Done: E(RHF) = {energy - 1:.9f} A.U.\n"
                f" SCF Done: E(RHF) = {energy:.9f} A.U.\n", encoding="utf-8")
        crosses = {
            "TEA-doublet-plus_final-from-TEA-singlet-neutral_init": -99.88,
            "TEA-singlet-neutral_init-from-TEA-doublet-plus_final": -99.99,
            "p-terphenyl-doublet-minus_final-from-p-terphenyl-singlet-neutral_init": -200.12,
            "p-terphenyl-singlet-neutral_init-from-p-terphenyl-doublet-minus_final": -199.98,
        }
        for name, energy in crosses.items():
            (step2 / (name + ".log")).write_text(
                f" SCF Done: E(RHF) = {energy - 1:.9f} A.U.\n"
                f" SCF Done: E(RHF) = {energy:.9f} A.U.\n", encoding="utf-8")
        return states, crosses

    def assert_report_header(self, report):
        version_header = (FIXTURES.parent.parent / "src/utilities/version.h").read_text(encoding="utf-8")
        def macro(name):
            match = re.search(r'^#define\s+' + name + r'\s+"([^"]+)"', version_header, re.MULTILINE)
            self.assertIsNotNone(match, name)
            return match.group(1)

        title = f'{macro("COMCHEMKIT_NAME")} {macro("COMCHEMKIT_STRING")} developed by {macro("COMCHEMKIT_AUTHOR")}'
        border = "*" * 61
        expected = "\n".join([
            " " * 61,
            border,
            f"* {title:<57} *",
            f'* {macro("COMCHEMKIT_REPOSITORY"):<57} *',
            border,
            " " * 61,
            "Marcus Theory Barrier Calculation (Four-Point Method)",
        ])
        self.assertTrue(report.read_text(encoding="utf-8").startswith(expected))

    def test_gas_phase_step2_routes_and_checkpoints(self):
        for xyz in FIXTURES.glob("*.xyz"):
            shutil.copy(xyz, self.work / xyz.name)
        params = self.work / "oset.inp"
        original = params.read_text(encoding="utf-8")
        for setting in ("blank", "omitted"):
            with self.subTest(setting=setting):
                if setting == "blank":
                    params.write_text(original + "\nstep1_solvent =\nstep2_solvent =\n", encoding="utf-8")
                else:
                    params.write_text(re.sub(r"(?m)^\s*step[12]_solvent\s*=.*$", "", original), encoding="utf-8")
                result = self.run_cck("-step", "1,2")
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                inputs = list((self.work / "oset_step_2").glob("*.gau"))
                self.assertEqual(len(inputs), 8)
                for path in inputs:
                    text = path.read_text(encoding="utf-8")
                    self.assertNotRegex(text.lower(), r"\b(scrf|noneq|externaliteration)\b")
                    self.assertIn("Guess(read)", text)
                    self.assertIn("nosymm", text)
                    if "-from-" in path.name:
                        self.assertIn("Geom=Check", text)
                        self.assertRegex(text, r"%Oldchk=[^/\\\\\n]+\.chk")
                    else:
                        self.assertIn("Geom(Check)", text)
                        self.assertIn(str(self.work / "oset_step_1"), text)
                for path in (self.work / "oset_step_1").glob("*.gau"):
                    self.assertNotRegex(path.read_text(encoding="utf-8").lower(), r"\bscrf\b")

    def test_solution_phase_step2_routes_are_unchanged(self):
        for xyz in FIXTURES.glob("*.xyz"):
            shutil.copy(xyz, self.work / xyz.name)
        result = self.run_cck("-step", "1,2")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        inputs = list((self.work / "oset_step_2").glob("*.gau"))
        self.assertEqual(len(inputs), 8)
        for path in inputs:
            text = path.read_text(encoding="utf-8").lower()
            self.assertIn("scrf=(smd,solvent=n,n-dimethylformamide", text)
            if "-from-" in path.name:
                self.assertIn("externaliteration,noneq=read", text)
            else:
                self.assertIn("noneq=write", text)

    def test_conflicting_gas_route_extras_are_rejected(self):
        params = self.work / "oset.inp"
        original = params.read_text(encoding="utf-8")
        for key, extra in (("step1_route_extra_keywords", "SCRF=(smd,solvent=water)"),
                           ("step2_route_extra_keywords", "ScRf=(smd,solvent=water)"),
                           ("step2_route_extra_keywords", "NonEq=read"),
                           ("step2_route_extra_keywords", "ExternalIteration")):
            with self.subTest(key=key, extra=extra):
                params.write_text(original + f"\nstep1_solvent =\nstep2_solvent =\n{key} = {extra}\n", encoding="utf-8")
                result = self.run_cck("-step", "1,2")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(key + " contains a solvent keyword", result.stderr)
                self.assertFalse((self.work / "oset_step_1").exists())

    def test_gas_phase_energies_gibbs_and_barriers(self):
        self.configure_phases()
        states, crosses = self.write_gas_logs()
        result = self.run_cck("-step", "3", "-v")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("Configured phase : gas phase", content)
        self.assertIn("Energy source : SCF Done (gas phase)", content)
        self.assertIn("Gas phase: no solution-phase standard-state correction", content)
        self.assertIn("Phase T / C     : N/A (gas phase", content)
        for energy, _ in states.values():
            self.assertIn(f"{energy:.9f}", content)
        for energy in crosses.values():
            self.assertIn(f"{energy:.9f}", content)
        for state in ("D-R", "D-P", "A-R", "A-P"):
            match = re.search(r"^" + state + r"\s+([-\d.]+)\s+([-\d.]+)\s+([-\d.]+)\s+([-\d.]+)$",
                              content, re.MULTILINE)
            self.assertIsNotNone(match, state)
            electronic, thermal, phase, gibbs = map(float, match.groups())
            self.assertEqual(phase, 0.0)
            self.assertAlmostEqual(gibbs, electronic + thermal, places=8)
        factor = 627.509
        dg, lr, lp = -0.045 * factor, 0.03 * factor, 0.05 * factor
        avg = (lr + lp) / 2
        symmetric = (avg + dg) ** 2 / (4 * avg)
        asymmetric = lr * ((-lp + math.sqrt(lr * lp + (lr - lp) * dg)) / (lr - lp)) ** 2
        self.assertGreater(asymmetric, 0.01)
        expected = {"Reaction free energy, Delta G°": dg,
                    "Reorganization energy, lambda R": lr,
                    "Reorganization energy, lambda P": lp,
                    "Delta G‡ (symmetric)": symmetric,
                    "Delta G‡ (asymmetric)": asymmetric}
        for label, kcal in expected.items():
            self.assertAlmostEqual(self.result_values(content, label)[1], kcal, delta=0.00006)
        for label in ("E(D-R at D-R)", "E(D-P at D-P)", "E(A-R at A-R)", "E(A-P at A-P)",
                      "E(D-P at D-R)", "E(D-R at D-P)", "E(A-P at A-R)", "E(A-R at A-P)"):
            self.assertRegex(content, re.escape(label) + r" = [-\d.]+ \(SCF Done\)")
        self.assertNotIn("PCM correction lines detected", content)
        params = self.work / "oset.inp"
        params.write_text(params.read_text(encoding="utf-8").replace("calc_type = oset", "calc_type = ent"), encoding="utf-8")
        result = self.run_cck("-step", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("Calculation type : EnT", content)
        self.assertAlmostEqual(self.result_values(content, "Reaction free energy, Delta G°")[1], dg, delta=0.00006)

    def test_gas_phase_thermo_recalculation_uses_temperature_and_pressure(self):
        self.configure_phases()
        self.copy_logs()
        # Reuse real frequency blocks, removing solvent markers and replacing
        # step-2 energies with the gas-phase test values above.
        for path in (self.work / "oset_step_1").glob("*.log"):
            text = re.sub("scrf", "nosolvent", path.read_text(encoding="utf-8"), flags=re.IGNORECASE)
            path.write_text(text, encoding="utf-8")
        energies = {
            "TEA-singlet-neutral_init.log": -100.0,
            "TEA-doublet-plus_final.log": -99.9,
            "p-terphenyl-singlet-neutral_init.log": -200.0,
            "p-terphenyl-doublet-minus_final.log": -200.15,
            "TEA-doublet-plus_final-from-TEA-singlet-neutral_init.log": -99.88,
            "TEA-singlet-neutral_init-from-TEA-doublet-plus_final.log": -99.99,
            "p-terphenyl-doublet-minus_final-from-p-terphenyl-singlet-neutral_init.log": -200.12,
            "p-terphenyl-singlet-neutral_init-from-p-terphenyl-doublet-minus_final.log": -199.98,
        }
        for name, energy in energies.items():
            path = self.work / "oset_step_2" / name
            text = re.sub("scrf", "nosolvent", path.read_text(encoding="utf-8"), flags=re.IGNORECASE)
            text = text.replace("After PCM corrections, the energy is", "Removed PCM energy")
            path.write_text(text + f"\n SCF Done: E(RHF) = {energy:.9f} A.U.\n", encoding="utf-8")
        energies_by_concentration = []
        for concentration in ("1.0", "2.0"):
            result = self.run_cck("-step", "3", "-t", "310", "--pressure", "2", "-c", concentration, "-v")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            content = (self.work / "oset.log").read_text(encoding="utf-8")
            self.assertIn("Actual source : thermo module", content)
            self.assertIn("T=310.00 K (input)", content)
            self.assertIn("Input pressure: 2.000 atm", content)
            self.assertIn("Gas phase: no solution-phase standard-state correction", content)
            self.assertNotIn("SCRF text detected in equilibrium logs", content)
            energies_by_concentration.append(self.result_values(content, "Reaction free energy, Delta G°"))
        self.assertEqual(energies_by_concentration[0], energies_by_concentration[1])

    def test_gas_phase_rejects_missing_scf_even_if_pcm_is_present(self):
        self.configure_phases()
        self.write_gas_logs()
        for name in ("TEA-singlet-neutral_init.log", "TEA-doublet-plus_final-from-TEA-singlet-neutral_init.log"):
            with self.subTest(log=name):
                path = self.work / "oset_step_2" / name
                original = path.read_text(encoding="utf-8")
                path.write_text(" After PCM corrections, the energy is -99.88 a.u.\n", encoding="utf-8")
                result = self.run_cck("-step", "3")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Could not extract SCF Done energy", result.stderr)
                self.assertIn(name, result.stderr)
                self.assertFalse((self.work / "oset.log").exists())
                path.write_text(original, encoding="utf-8")

    def test_gas_phase_warns_about_conflicting_pcm_and_scrf_logs(self):
        self.configure_phases()
        self.write_gas_logs()
        first = self.work / "oset_step_1/TEA-singlet-neutral_init.log"
        first.write_text(first.read_text(encoding="utf-8") + "\n #P SCRF=(smd,solvent=water)\n", encoding="utf-8")
        for name in ("TEA-singlet-neutral_init.log", "TEA-doublet-plus_final-from-TEA-singlet-neutral_init.log"):
            path = self.work / "oset_step_2" / name
            path.write_text(path.read_text(encoding="utf-8") +
                            " After PCM corrections, the energy is -999.0 a.u.\n", encoding="utf-8")
        other_cross = self.work / "oset_step_2/p-terphenyl-singlet-neutral_init-from-p-terphenyl-doublet-minus_final.log"
        other_cross.write_text(other_cross.read_text(encoding="utf-8") +
                               " #P SCRF=(smd,solvent=water)\n", encoding="utf-8")
        result = self.run_cck("-step", "3", "-v")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("SCRF text detected in equilibrium logs", content)
        self.assertIn("Solvent-related text detected in logs", result.stderr)
        self.assertIn("PCM correction lines detected in configured gas-phase step-2 logs", content)
        self.assertIn("SCRF text detected in configured gas-phase step-2 logs", content)
        self.assertIn(other_cross.name, content)
        self.assertIn("TEA-doublet-plus_final-from-TEA-singlet-neutral_init.log", content)
        self.assertIn("E(D-R at D-R) = -100.000000000 (SCF Done)", content)
        self.assertIn("E(D-P at D-R) = -99.880000000 (SCF Done)", content)
        self.assertEqual(self.result_values(content, "Reaction free energy, Delta G°")[2], -0.045)

    def test_solution_phase_still_prefers_pcm_in_all_energy_paths(self):
        self.configure_phases(step1="water", step2="water")
        self.write_gas_logs()
        for name, energy in (("TEA-singlet-neutral_init.log", -100.001),
                             ("TEA-doublet-plus_final-from-TEA-singlet-neutral_init.log", -99.87)):
            path = self.work / "oset_step_2" / name
            path.write_text(path.read_text(encoding="utf-8") +
                            f" After PCM corrections, the energy is {energy:.9f} a.u.\n", encoding="utf-8")
        result = self.run_cck("-step", "3", "-v")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("Configured phase : solution phase", content)
        self.assertIn("E(D-R at D-R) = -100.001000000 (After PCM)", content)
        self.assertIn("E(D-P at D-R) = -99.870000000 (After PCM)", content)
        self.assertAlmostEqual(self.result_values(content, "Reorganization energy, lambda R")[1], 0.031 * 627.509, delta=0.00006)
        self.assertAlmostEqual(self.result_values(content, "Reorganization energy, lambda P")[1], 0.06 * 627.509, delta=0.00006)

    def test_mixed_phase_preserves_existing_phase_correction(self):
        self.configure_phases(step1="water")
        self.write_gas_logs()
        first = self.work / "oset_step_1/TEA-singlet-neutral_init.log"
        first.write_text(first.read_text(encoding="utf-8") + "\n #P SCRF=(smd,solvent=water)\n", encoding="utf-8")
        result = self.run_cck("-step", "3", "-v")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("Configured phase : mixed gas/solution", content)
        self.assertIn("Step 1 and step 2 use mixed gas/solution settings", content)
        self.assertIn("Mixed gas/solution settings", result.stderr)
        self.assertIn("Energy source : SCF Done (gas phase)", content)
        line = next(line for line in content.splitlines() if line.startswith("D-R  "))
        self.assertIn("0.003018804", line)

    def test_mixed_phase_with_solution_single_points(self):
        self.configure_phases(step2="water")
        self.write_gas_logs()
        first = self.work / "oset_step_2/TEA-singlet-neutral_init.log"
        first.write_text(first.read_text(encoding="utf-8") + "\n #P SCRF=(smd,solvent=water)\n", encoding="utf-8")
        result = self.run_cck("-step", "3", "-v")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("Configured phase : mixed gas/solution", content)
        self.assertIn("Energy source : After PCM, falling back to SCF Done", content)
        self.assertIn("Mixed gas/solution settings", result.stderr)
        line = next(line for line in content.splitlines() if line.startswith("D-R  "))
        self.assertIn("0.003018804", line)

    def test_default_report_is_log_in_current_directory(self):
        self.copy_logs()
        result = self.run_cck("-step", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = self.work / "oset.log"
        self.assertTrue(report.is_file())
        self.assert_report_header(report)
        content = report.read_text(encoding="utf-8")
        self.assertIn("Delta G‡ (symmetric)", content)
        self.assertIn("Delta G‡ (asymmetric)", content)
        self.assertIn("Barrier models   : symmetric and asymmetric", content)
        self.assertIn("70.2968 kcal/mol", content)
        self.assertIn("LEVELS OF THEORY AND THERMOCHEMISTRY", content)
        self.assertIn("Basis file    : ma-def2-TZVP.gbs/N", content)
        self.assertIn("configured in the input; log routes are not verified", content)
        self.assertIn("Low-vib, scaling and other thermo settings apply only to recalculated states", content)
        self.assertIn("lambda R: initial state; lambda P: final state. Total = donor + acceptor.", content)
        self.assertNotIn("Each component is a cross energy minus", content)
        for value in ("-292.440837438", "-292.260583167", "-694.375776644", "-694.438506073",
                      "-292.194765457", "-292.386773813", "-694.397845878", "-694.335905255"):
            self.assertIn(value, content)
        self.assertNotIn("VERBOSE: THERMAL BREAKDOWN", content)
        self.assertIn("Barrier report written to: oset.log", result.stdout)
        self.assertIn("Step 1 directory: oset_step_1", report.read_text(encoding="utf-8"))
        self.assertIn("Step 2 directory: oset_step_2", report.read_text(encoding="utf-8"))
        self.assertFalse((self.work / "oset_barrier").exists())

    def test_sample_triplet_report_matches_reference_result(self):
        self.copy_logs()
        shutil.copy(FIXTURES / "oset.inp", self.work / "oset.inp")
        result = self.run_cck()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("Barrier models   : symmetric and asymmetric", content)
        self.assertIn("Calculating symmetric and asymmetric barriers", result.stdout)
        self.assertIn("18.9323 kcal/mol", content)
        self.assertIn("18.7953 kcal/mol", content)
        self.assertIn("56.1455 kcal/mol", content)
        self.assertIn("62.6829 kcal/mol", content)
        self.assertIn("multiplicity 3", content)
        self.assertNotIn("[selected]", content)
        self.assertNotIn("[reference]", content)
        self.assert_results_alignment(content, max_gap=4)

        # Independently reconstruct the barriers from the nine-decimal Hartree
        # values printed in the report, rather than only testing the labels.
        rows = {}
        for state in ("D-R", "D-P", "A-R", "A-P"):
            match = re.search(r"^" + state + r"\s+([-\d.]+)\s+[-\d.]+\s+[-\d.]+\s+([-\d.]+)$",
                              content, re.MULTILINE)
            self.assertIsNotNone(match, state)
            rows[state] = tuple(map(float, match.groups()))
        cross = {}
        for molecule, state, geometry, key in (
                ("Donor", "final (D-P)", "initial (D-R)", "D-PR"),
                ("Donor", "initial (D-R)", "final (D-P)", "D-RP"),
                ("Acceptor", "final (A-P)", "initial (A-R)", "A-PR"),
                ("Acceptor", "initial (A-R)", "final (A-P)", "A-RP")):
            pattern = (r"^" + re.escape(molecule) + r"\s+" + re.escape(state) + r"\s+" +
                       re.escape(geometry) + r"\s+([-\d.]+)$")
            match = re.search(pattern, content, re.MULTILINE)
            self.assertIsNotNone(match, key)
            cross[key] = float(match.group(1))
        factor = 627.509
        dg = (rows["D-P"][1] + rows["A-P"][1] - rows["D-R"][1] - rows["A-R"][1]) * factor
        lr = (cross["D-RP"] - rows["D-R"][0] + cross["A-RP"] - rows["A-R"][0]) * factor
        lp = (cross["D-PR"] - rows["D-P"][0] + cross["A-PR"] - rows["A-P"][0]) * factor
        symmetric = ((lr + lp) / 2 + dg) ** 2 / (2 * (lr + lp))
        asymmetric = lr * ((-lp + math.sqrt(lr * lp + (lr - lp) * dg)) / (lr - lp)) ** 2
        # Check all four columns against independently reconstructed values.
        expected_hartree = {
            "Reaction free energy, Delta G°": dg / factor,
            "Reorganization energy, lambda R": lr / factor,
            "Reorganization energy, lambda P": lp / factor,
            "Average reorganization, lambda": (lr + lp) / (2 * factor),
            "Delta G‡ (symmetric)": symmetric / factor,
            "Delta G‡ (asymmetric)": asymmetric / factor,
        }
        for label, hartree in expected_hartree.items():
            with self.subTest(quantity=label):
                values = self.result_values(content, label)
                self.assertAlmostEqual(values[0], hartree * 2625.5002, delta=0.00008)
                self.assertAlmostEqual(values[1], hartree * factor, delta=0.00006)
                self.assertAlmostEqual(values[2], hartree, delta=0.000000003)
                self.assertAlmostEqual(values[3], hartree * 27.211396641308, delta=0.00006)
        self.assertGreater(abs(symmetric - asymmetric), 0.001)
        self.assertNotIn("  Symmetric barrier:", content)
        self.assertNotIn("  Asymmetric barrier:", content)

    def test_removed_mode_options_are_rejected(self):
        params = self.work / "oset.inp"
        original = params.read_text(encoding="utf-8")
        for value in ("symmetric", "asymmetric", ""):
            with self.subTest(input_value=value):
                params.write_text(original + "\nmarcus_mode = " + value + "\n", encoding="utf-8")
                result = self.run_cck("-step", "3")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("marcus_mode is no longer supported", result.stderr)
                self.assertFalse((self.work / "oset.log").exists())
        params.write_text(original, encoding="utf-8")
        result = self.run_cck("-step", "3", "-mode", "symmetric")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unknown Marcus option: -mode", result.stderr)
        self.assertFalse((self.work / "oset.log").exists())

    def test_negative_reaction_energy_converts_with_sign(self):
        self.copy_logs()
        log = self.work / "oset_step_1/TEA-doublet-plus_final.log"
        content = log.read_text(encoding="utf-8")
        content, count = re.subn(r"(Thermal correction to Gibbs Free Energy=\s+)[-+0-9.EeDd]+",
                                 r"\g<1>-0.025388", content)
        self.assertGreater(count, 0)
        log.write_text(content, encoding="utf-8")
        result = self.run_cck("-step", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        kj, kcal, hartree, ev = self.result_values(content, "Reaction free energy, Delta G°")
        self.assertTrue(all(v < 0 for v in (kj, kcal, hartree, ev)))
        self.assert_results_alignment(content)
        self.assertAlmostEqual(kj, hartree * 2625.5002, delta=0.00006)
        self.assertAlmostEqual(kcal, hartree * 627.509, delta=0.00006)
        self.assertAlmostEqual(ev, hartree * 27.211396641308, delta=0.00006)

    def test_energy_transfer_uses_same_report_levels(self):
        self.copy_logs()
        params = self.work / "oset.inp"
        params.write_text(params.read_text(encoding="utf-8").replace("calc_type = oset", "calc_type = ent"), encoding="utf-8")
        result = self.run_cck("-step", "3", "-v")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("Calculation type : EnT", content)
        self.assertIn("VERBOSE: THERMAL BREAKDOWN BY STATE", content)
        self.assertIn("70.2968 kcal/mol", content)

    def test_verbose_report_settings_and_cli_override(self):
        self.copy_logs()
        params = self.work / "oset.inp"
        result = self.run_cck("-step", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        default_report = (self.work / "oset.log").read_text(encoding="utf-8")
        params.write_text(params.read_text(encoding="utf-8") + "\nreport_level = verbose\n", encoding="utf-8")
        result = self.run_cck("-step", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        for label in ("Report level     : verbose", "VERBOSE: THERMAL BREAKDOWN BY STATE",
                      "VERBOSE: ELECTRONIC ENERGY SOURCES", "VERBOSE: REACTION AND REORGANIZATION AUDIT",
                      "VERBOSE: CONFIGURED THEORY AND THERMO OPTIONS", "After PCM", "SCF Done",
                      "TC(E) - ZPE", "TS = TC(H)-TC(G)", "Lowest frequency", "G(raw)", "G(corrected)",
                      "Step 1 log", "Step 2 log", "lambda R acceptor", "Net phase on Delta G°"):
            self.assertIn(label, content)
        self.assertIn("70.2968 kcal/mol", content)
        for label in ("Reaction free energy, Delta G°", "Reorganization energy, lambda R",
                      "Reorganization energy, lambda P", "Average reorganization, lambda",
                      "Delta G‡ (symmetric)", "Delta G‡ (asymmetric)"):
            self.assertEqual(self.result_values(default_report, label), self.result_values(content, label))
        params.write_text(params.read_text(encoding="utf-8") + "\nreport_level = default\n", encoding="utf-8")
        (self.work / "oset.log").unlink()
        result = self.run_cck("-step", "3", "-v")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Report level     : verbose", (self.work / "oset.log").read_text(encoding="utf-8"))
        params.write_text(params.read_text(encoding="utf-8") + "\nreport_level = invalid\n", encoding="utf-8")
        result = self.run_cck("-step", "3")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("report_level must be default or verbose", result.stderr)

    def test_verbose_reports_actual_cross_energy_source(self):
        self.copy_logs()
        cross = self.work / "oset_step_2/TEA-singlet-neutral_init-from-TEA-doublet-plus_final.log"
        content = cross.read_text(encoding="utf-8")
        cross.write_text(content.replace("After PCM corrections, the energy is", "Removed PCM energy"), encoding="utf-8")
        result = self.run_cck("-step", "3", "--verbose")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("E(D-R at D-P) = -292.425283416 (SCF Done)", content)
        self.assertIn(str(cross.relative_to(self.work)), content)

    def test_asymmetric_mode_and_negative_discriminant(self):
        self.copy_logs()
        result = self.run_cck("-step", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("Delta G‡ (symmetric)", content)
        self.assertIsNotNone(self.result_values(content, "Delta G‡ (asymmetric)")[0])
        cross = self.work / "oset_step_2/TEA-doublet-plus_final-from-TEA-singlet-neutral_init.log"
        content = cross.read_text(encoding="utf-8")
        content = re.sub(r"(After PCM corrections, the energy is\s+)[-+0-9.EeDd]+", r"\g<1>-290.0", content)
        cross.write_text(content, encoding="utf-8")
        result = self.run_cck("-step", "3", "-v")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIsNotNone(self.result_values(content, "Delta G‡ (symmetric)")[0])
        self.assertEqual(self.result_values(content, "Delta G‡ (asymmetric)"), (None, None, None, None))
        self.assert_results_alignment(content)
        self.assertIn("negative discriminant", content)
        self.assertIn("Asymmetric barrier: N/A", content)
        self.assertIn("asymmetric barrier unavailable", result.stdout)

    def test_equal_reorganization_energies_use_symmetric_limit(self):
        self.copy_logs()
        cross_energies = {
            "TEA-singlet-neutral_init-from-TEA-doublet-plus_final.log": -292.430837438,
            "TEA-doublet-plus_final-from-TEA-singlet-neutral_init.log": -292.250583167,
            "p-terphenyl-singlet-neutral_init-from-p-terphenyl-doublet-minus_final.log": -694.365776644,
            "p-terphenyl-doublet-minus_final-from-p-terphenyl-singlet-neutral_init.log": -694.428506073,
        }
        for name, energy in cross_energies.items():
            log = self.work / "oset_step_2" / name
            content = log.read_text(encoding="utf-8")
            content, count = re.subn(r"(After PCM corrections, the energy is\s+)[-+0-9.EeDd]+",
                                     lambda m: m.group(1) + f"{energy:.9f}", content)
            self.assertGreater(count, 0, name)
            log.write_text(content, encoding="utf-8")
        result = self.run_cck("-step", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        symmetric = self.result_values(content, "Delta G‡ (symmetric)")
        asymmetric = self.result_values(content, "Delta G‡ (asymmetric)")
        self.assertEqual(symmetric, asymmetric)

    def test_missing_and_zero_gibbs_corrections(self):
        self.copy_logs()
        log = self.work / "oset_step_1/TEA-singlet-neutral_init.log"
        original = log.read_text(encoding="utf-8")
        log.write_text(original.replace("Thermal correction to Gibbs Free Energy", "Removed thermal Gibbs correction"), encoding="utf-8")
        result = self.run_cck("-step", "3", "-v")
        self.assertNotEqual(result.returncode, 0)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("Missing thermal Gibbs corrections", content)
        self.assertEqual(self.result_values(content, "Reaction free energy, Delta G°"), (None, None, None, None))
        self.assertEqual(self.result_values(content, "Delta G‡ (symmetric)"), (None, None, None, None))
        self.assertRegex(content, r"TC\(G\)\s+: N/A")
        self.assertIn("Reorganization energy, lambda R", content)
        log.write_text(re.sub(r"(Thermal correction to Gibbs Free Energy=\s+)[-+0-9.EeDd]+",
                              r"\g<1>0.0", original), encoding="utf-8")
        result = self.run_cck("-step", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("Missing thermal Gibbs corrections", (self.work / "oset.log").read_text(encoding="utf-8"))

    def test_actual_temperatures_and_fallback_are_reported(self):
        self.copy_logs()
        first = self.work / "oset_step_1/TEA-singlet-neutral_init.log"
        content = first.read_text(encoding="utf-8")
        first.write_text(content.replace("Temperature   298.150 Kelvin.", "Temperature   310.000 Kelvin."), encoding="utf-8")
        result = self.run_cck("-step", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("T=310.00 K (log)", content)
        self.assertIn("States use different temperatures", content)
        # Force the thermo parser to fail for one state; other states keep their frequency blocks.
        first.write_text(first.read_text(encoding="utf-8").replace("Frequencies --", "Removed frequencies --"), encoding="utf-8")
        params = self.work / "oset.inp"
        params.write_text(params.read_text(encoding="utf-8") + "\nuse_input_temp = true\n", encoding="utf-8")
        result = self.run_cck("-step", "3", "-v")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("D-R: thermo recalculation failed", content)
        self.assertIn("Actual source : mixed; see state provenance below", content)
        self.assertIn("step-1 log (thermo fallback)", content)
        self.assertIn("Thermal: thermo module; T=298.15 K (input)", content)
        self.assertIn("T=310.00 K (log)", content)

    def test_missing_log_temperature_is_not_claimed_as_thermal_temperature(self):
        self.copy_logs()
        log = self.work / "oset_step_1/TEA-singlet-neutral_init.log"
        content = log.read_text(encoding="utf-8")
        log.write_text(content.replace("Temperature   298.150 Kelvin.", "Temperature unavailable."), encoding="utf-8")
        result = self.run_cck("-step", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("log T unavailable; phase T=298.15 K (fallback if SCRF)", content)
        self.assertIn("D-R: log temperature unavailable", content)

    def test_cli_low_frequency_settings_are_reflected_in_summary(self):
        self.copy_logs()
        result = self.run_cck("-step", "3", "--lowvibmeth", "truhlar", "--ravib", "150")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("Low-vib method: truhlar; ravib=150.000 cm^-1", content)
        self.assertIn("apply only to recalculated states", content)

    def test_nonpositive_lambda_is_not_reported_as_zero_barrier(self):
        self.copy_logs()
        cross = self.work / "oset_step_2/TEA-singlet-neutral_init-from-TEA-doublet-plus_final.log"
        content = cross.read_text(encoding="utf-8")
        cross.write_text(re.sub(r"(After PCM corrections, the energy is\s+)[-+0-9.EeDd]+",
                                r"\g<1>-293.0", content), encoding="utf-8")
        result = self.run_cck("-step", "3")
        self.assertNotEqual(result.returncode, 0)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertRegex(content, r"Delta G‡ \(symmetric\)\s+N/A")
        self.assertIn("non-positive or invalid reorganization energies", content)

    def test_long_state_and_file_names_remain_separate_from_numbers(self):
        self.copy_logs()
        donor = "very-long-donor-molecule-0123456789"
        acceptor = "very-long-acceptor-molecule-0123456789"
        for directory in (self.work / "oset_step_1", self.work / "oset_step_2"):
            for log in directory.glob("*.log"):
                new_name = log.name.replace("TEA", donor).replace("p-terphenyl", acceptor)
                log.rename(directory / new_name)
        params = self.work / "oset.inp"
        params.write_text(params.read_text(encoding="utf-8").replace("donor_name        = TEA", "donor_name        = " + donor)
                          .replace("acceptor_name        = p-terphenyl", "acceptor_name        = " + acceptor), encoding="utf-8")
        result = self.run_cck("-step", "3", "-v")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        content = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("Donor initial: " + donor, content)
        self.assertIn("Acceptor initial: " + acceptor, content)
        self.assertIn("E(D-P at D-R) = -292.194765457 (After PCM)\n    oset_step_2/" + donor, content)
        self.assertIn("70.2968 kcal/mol", content)

    def test_version_options_do_not_conflict_with_marcus_verbose(self):
        for args in (("-v",), ("--version",), ("marcus", "--version")):
            result = subprocess.run([str(BINARY), *args], cwd=self.work, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("ComChemKit", result.stdout)

    def test_input_basename_drives_step3_for_arbitrary_extension_and_path(self):
        self.copy_logs(project="reaction.v1")
        (self.work / "inputs").mkdir()
        (self.work / "inputs/reaction.v1.whatever").write_text(self.input_without_project(), encoding="utf-8")
        result = self.run_input("inputs/reaction.v1.whatever", "-step", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = self.work / "reaction.v1.log"
        self.assert_report_header(report)
        self.assertIn("Project          : reaction.v1", report.read_text(encoding="utf-8"))
        self.assertIn("Step 1 directory: reaction.v1_step_1", report.read_text(encoding="utf-8"))

    def test_input_step_key_runs_without_cli_options(self):
        self.copy_logs()
        input_text = self.input_without_project().replace("step = 1,2", "step = 3")
        (self.work / "oset.inp").write_text(input_text, encoding="utf-8")
        result = self.run_input("oset.inp")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_report_header(self.work / "oset.log")

    def test_no_extension_and_blank_project_use_input_name(self):
        for xyz in FIXTURES.glob("*.xyz"):
            shutil.copy(xyz, self.work / xyz.name)
        (self.work / "reaction").write_text(self.input_without_project() + "\nproject =\n", encoding="utf-8")
        result = self.run_input("reaction")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((self.work / "reaction_step_1/.marcus_manifest_step1").exists())
        self.assertTrue((self.work / "reaction_step_2/.marcus_manifest_step2").exists())

    def test_explicit_report_path_is_preserved(self):
        self.copy_logs()
        result = self.run_cck("-step", "3", "-O", "custom.log")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((self.work / "custom.log").is_file())
        self.assert_report_header(self.work / "custom.log")
        self.assertFalse((self.work / "oset.log").exists())
        self.assertFalse((self.work / "oset_barrier").exists())

    def test_report_name_follows_project_parameter(self):
        self.copy_logs(project="electron_transfer")
        params = self.work / "oset.inp"
        params.write_text(params.read_text(encoding="utf-8").replace("project = oset", "project = electron_transfer"), encoding="utf-8")
        result = self.run_cck("-step", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_report_header(self.work / "electron_transfer.log")
        self.assertFalse((self.work / "oset.log").exists())

    def test_parameter_report_path_and_cli_override(self):
        self.copy_logs()
        params = self.work / "oset.inp"
        params.write_text(params.read_text(encoding="utf-8") + "\nout_file = from_params.log\n", encoding="utf-8")
        result = self.run_cck("-step", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((self.work / "from_params.log").is_file())
        result = self.run_cck("-step", "3", "-O", "from_cli.log")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((self.work / "from_cli.log").is_file())
        self.assertFalse((self.work / "oset_barrier").exists())

    def test_input_and_report_collision_is_rejected(self):
        self.copy_logs()
        input_path = self.work / "oset.log"
        input_text = self.input_without_project()
        input_path.write_text(input_text, encoding="utf-8")
        result = self.run_input("oset.log", "-step", "3")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("overwrite the input file", result.stderr)
        self.assertEqual(input_path.read_text(encoding="utf-8"), input_text)
        result = self.run_input("oset.log", "-step", "3", "-O", "safe.log")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_report_header(self.work / "safe.log")
        result = self.run_input("oset.log", "-step", "3", "-O", "./oset.log")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(input_path.read_text(encoding="utf-8"), input_text)

    def test_multiple_or_missing_input_files_fail(self):
        result = self.run_input("oset.inp", "another.inp", "-step", "3")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Only one Marcus input file", result.stderr)
        result = subprocess.run([str(BINARY), "marcus"], cwd=self.work, text=True, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("No input file specified", result.stderr)

    def test_removed_parameter_options_are_rejected(self):
        for option in ("-p", "--param", "--parameter", "--param-file"):
            with self.subTest(option=option):
                result = subprocess.run([str(BINARY), "marcus", option, "oset.inp", "-step", "1"],
                                        cwd=self.work, text=True, capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(f"Unknown Marcus option: {option}", result.stderr)
                self.assertFalse((self.work / "oset_step_1").exists())

    def test_removed_template_options_are_rejected(self):
        for option in ("--genoset-params", "--genent-params"):
            with self.subTest(option=option):
                result = subprocess.run([str(BINARY), "marcus", option, "-o", "old.inp"],
                                        cwd=self.work, text=True, capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(f"Unknown Marcus option: {option}", result.stderr)
                self.assertFalse((self.work / "old.inp").exists())

    def test_common_quiet_options_are_accepted(self):
        self.copy_logs()
        for option in ("-q", "--quiet"):
            with self.subTest(option=option):
                result = self.run_cck(option, "-step", "3")
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_missing_inputs_do_not_create_directories(self):
        result = self.run_cck("-step", "3")
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.work / "oset_step_1").exists())
        self.assertFalse((self.work / "oset_step_2").exists())
        self.assertFalse((self.work / "oset_barrier").exists())

    def test_report_write_failure_is_reported(self):
        self.copy_logs()
        result = self.run_cck("-step", "3", "-O", "missing/report.log")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Failed to write barrier report", result.stderr)
        self.assertNotIn("Step 3 complete", result.stdout)

    def test_help_and_template_describe_new_names(self):
        help_result = subprocess.run([str(BINARY), "marcus", "--help"], cwd=self.work, text=True, capture_output=True)
        self.assertEqual(help_result.returncode, 0, help_result.stderr)
        template_result = subprocess.run(
            [str(BINARY), "marcus", "--genoset-input", "-o", "template.inp"],
            cwd=self.work, text=True, capture_output=True,
        )
        self.assertEqual(template_result.returncode, 0, template_result.stderr)
        for content in (help_result.stdout, (self.work / "template.inp").read_text(encoding="utf-8")):
            self.assertIn("<project>_step_1/", content)
            self.assertIn("<project>_step_2/", content)
            self.assertIn("<project>.log", content)
            self.assertNotIn("--genoset-params", content)
            self.assertNotIn("--genent-params", content)
            self.assertNotIn("marcus_mode", content)
            self.assertNotIn(" -mode ", content)
            self.assertNotIn("skip_existing", content)

    def test_default_input_templates_for_both_calculation_types(self):
        for calc_type in ("oset", "ent"):
            result = subprocess.run([str(BINARY), "marcus", f"--gen{calc_type}-input"],
                                    cwd=self.work, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            template = (self.work / f"marcus_{calc_type}.inp").read_text(encoding="utf-8")
            self.assertIn("# project =", template)
            self.assertIn(f"calc_type = {calc_type}", template)
            self.assertIn(f'cck marcus "marcus_{calc_type}.inp"', template)
            self.assertNotIn("cck marcus -p", template)

    def test_generated_input_defaults_project_to_template_filename(self):
        result = subprocess.run([str(BINARY), "marcus", "--genoset-input", "-o", "chemistry.in"],
                                cwd=self.work, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        path = self.work / "chemistry.in"
        template = path.read_text(encoding="utf-8")
        template = re.sub(r"(?m)^(donor_xyz\s*=\s*).*$", r"\g<1>TEA.xyz", template)
        template = re.sub(r"(?m)^(acceptor_xyz\s*=\s*).*$", r"\g<1>p-terphenyl.xyz", template)
        path.write_text(template, encoding="utf-8")
        for xyz in FIXTURES.glob("*.xyz"):
            shutil.copy(xyz, self.work / xyz.name)
        result = self.run_input("chemistry.in", "-step", "1")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((self.work / "chemistry_step_1/.marcus_manifest_step1").exists())
        self.assertFalse((self.work / "my_marcus_project_step_1").exists())

    def test_input_generation_does_not_create_barrier_directory(self):
        for xyz in FIXTURES.glob("*.xyz"):
            shutil.copy(xyz, self.work / xyz.name)
        result = self.run_cck("-step", "1,2")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((self.work / "oset_step_1/.marcus_manifest_step1").is_file())
        self.assertTrue((self.work / "oset_step_2/.marcus_manifest_step2").is_file())
        equilibrium = (self.work / "oset_step_2/TEA-singlet-neutral_init.gau").read_text(encoding="utf-8")
        self.assertIn(str(self.work / "oset_step_1/TEA-singlet-neutral_init.chk"), equilibrium)

        # Regeneration deliberately replaces matching inputs and manifests, but
        # must not touch Gaussian results or checkpoints.
        inputs = [self.work / "oset_step_1/TEA-singlet-neutral_init.gau",
                  self.work / "oset_step_2/TEA-singlet-neutral_init.gau"]
        manifests = [self.work / "oset_step_1/.marcus_manifest_step1",
                     self.work / "oset_step_2/.marcus_manifest_step2"]
        preserved = [self.work / "oset_step_1/TEA-singlet-neutral_init.log",
                     self.work / "oset_step_1/TEA-singlet-neutral_init.chk",
                     self.work / "oset_step_2/TEA-singlet-neutral_init.log",
                     self.work / "oset_step_2/TEA-singlet-neutral_init.chk"]
        for path in inputs + manifests + preserved:
            path.write_text("existing content\n", encoding="utf-8")
        result = self.run_cck("-step", "1,2")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for path in inputs + manifests:
            self.assertNotEqual(path.read_text(encoding="utf-8"), "existing content\n")
        for path in preserved:
            self.assertEqual(path.read_text(encoding="utf-8"), "existing content\n")
        self.assertFalse((self.work / "oset_step_1_opt").exists())
        self.assertFalse((self.work / "oset_step_2_sp").exists())
        self.assertFalse((self.work / "oset_barrier").exists())


if __name__ == "__main__":
    unittest.main()
