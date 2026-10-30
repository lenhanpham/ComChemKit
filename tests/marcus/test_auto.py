#!/usr/bin/env python3
"""Automatic Marcus workflow integration tests using a fake Gaussian executable."""

import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest

FIXTURES = Path(__file__).resolve().parent
BINARY = Path(os.environ.get("CCK_BINARY", FIXTURES.parent.parent / "build/bin/cck")).resolve()

FAKE_ENGINE = r'''#!/usr/bin/env python3
import os, re, subprocess, sys, time
from pathlib import Path

work = Path.cwd()
text = sys.stdin.read()
match = re.search(r"(?im)^%chk\s*=\s*(.*?)\s*$", text)
if not match:
    print("Error termination: missing checkpoint directive")
    sys.exit(4)
chk = Path(match.group(1))
old = re.search(r"(?im)^%oldchk\s*=\s*(.*?)\s*$", text)
if old and not Path(old.group(1)).is_file():
    print("Error termination: missing old checkpoint", old.group(1))
    sys.exit(5)
events = Path(os.environ["FAKE_EVENTS"])
existing = events.read_text().splitlines() if events.exists() else []
index = len(existing) + 1
stage = "opt" if work.name.endswith("_step_1") else ("cross" if "-from-" in chk.name else "equilibrium")
with events.open("a") as event_file:
    event_file.write(f"{index}|{stage}|{chk.name}|{os.getpid()}\n")
    event_file.flush()
print("Fake Gaussian started", flush=True)
if os.environ.get("FAKE_DELAY_INDEX") == str(index):
    if os.environ.get("FAKE_CHILD_PID_FILE"):
        child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(120)"])
        Path(os.environ["FAKE_CHILD_PID_FILE"]).write_text(str(child.pid))
    time.sleep(float(os.environ.get("FAKE_DELAY", "60")))
kind = os.environ.get("FAKE_FAILURE", "") if os.environ.get("FAKE_FAIL_INDEX") == str(index) else ""
if kind == "exit":
    print("Error termination: synthetic failure", flush=True)
    sys.exit(9)
if kind in ("truncated_link", "truncated_internal_link"):
    print("Normal termination of Gaussian 16")
    print("Entering Link 1 = another job" if kind == "truncated_link" else
          "Link1: Proceeding to internal job step number 2.")
    sys.exit(0)
if kind != "missing_checkpoint":
    chk.write_bytes(b"nonempty checkpoint")
if index == 4 and os.environ.get("FAKE_INJECT_FILE"):
    Path(os.environ["FAKE_INJECT_FILE"]).write_text("do not overwrite")
if os.environ.get("FAKE_GAS") == "1":
    energies = {
        "TEA-singlet-neutral_init": -100.0,
        "TEA-doublet-plus_final": -99.9,
        "p-terphenyl-singlet-neutral_init": -200.0,
        "p-terphenyl-doublet-minus_final": -200.15,
        "TEA-doublet-plus_final-from-TEA-singlet-neutral_init": -99.88,
        "TEA-singlet-neutral_init-from-TEA-doublet-plus_final": -99.99,
        "p-terphenyl-doublet-minus_final-from-p-terphenyl-singlet-neutral_init": -200.12,
        "p-terphenyl-singlet-neutral_init-from-p-terphenyl-doublet-minus_final": -199.98,
    }
    energy = energies[chk.stem]
    print("Gaussian 16")
    print(f" SCF Done: E(RHF) = {energy:.9f} A.U.")
    if stage == "opt":
        print(" Optimization completed.")
        print(" Temperature   298.150 Kelvin.  Pressure   1.00000 Atm.")
        print(" Frequencies -- 100.0 200.0 300.0")
        print(" Zero-point correction= 0.050000")
        print(" Thermal correction to Energy= 0.060000")
        print(" Thermal correction to Enthalpy= 0.061000")
        print(" Thermal correction to Gibbs Free Energy= 0.100000")
    print(" Normal termination of Gaussian 16")
else:
    source_dir = "oset_step_1" if stage == "opt" else "oset_step_2"
    source = Path(os.environ["FAKE_FIXTURES"]) / source_dir / (chk.stem + ".log")
    output = source.read_text(encoding="utf-8")
    if kind == "no_normal":
        output = output.replace("Normal termination of Gaussian", "Removed termination of Gaussian")
    if kind == "missing_gibbs":
        output = output.replace("Thermal correction to Gibbs Free Energy", "Removed thermal Gibbs")
    if kind == "incomplete_opt":
        output = output.replace("Optimization completed", "Incomplete optimization")
    if kind in ("negative_lambda", "asymmetric_invalid"):
        value = "-293.0" if kind == "negative_lambda" else "-290.0"
        output = re.sub(r"(After PCM corrections, the energy is\s+)[-+0-9.EeDd]+",
                        lambda match: match.group(1) + value, output)
    sys.stdout.write(output)
    if kind == "error_after_normal":
        print(" Error termination in synthetic test")
sys.stdout.flush()
'''


@unittest.skipIf(os.name == "nt", "The fake engine is a POSIX executable script")
class MarcusAutomaticTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.work = Path(self.temp.name)
        self.engine = self.work / "fake gaussian"
        self.engine.write_text(FAKE_ENGINE, encoding="utf-8")
        self.engine.chmod(0o755)
        self.events = self.work / "events.txt"
        self.env = os.environ.copy()
        self.env.update(FAKE_EVENTS=str(self.events), FAKE_FIXTURES=str(FIXTURES))
        for xyz in FIXTURES.glob("*.xyz"):
            shutil.copy(xyz, self.work / xyz.name)
        source = (FIXTURES / "oset.inp").read_text(encoding="utf-8")
        source = re.sub(r"(?m)^(acceptor_init_mult\s*=\s*)\d+", r"\g<1>1", source)
        self.input = self.work / "automatic input.inp"
        self.input.write_text(source + f"\nworkflow = auto\nqc_engine = {self.engine}\n", encoding="utf-8")

    def tearDown(self):
        self.temp.cleanup()

    def run_cck(self, *args, env=None):
        return subprocess.run([str(BINARY), "marcus", self.input.name, *args], cwd=self.work,
                              env=env or self.env, text=True, capture_output=True, timeout=30)

    def read_events(self):
        if not self.events.exists(): return []
        return [line.split("|") for line in self.events.read_text(encoding="utf-8").splitlines()]

    def test_sequential_solution_and_progress_preserved(self):
        with self.input.open("a", encoding="utf-8") as f: f.write("\nout_file = ignored-by-cli.out\n")
        result = self.run_cck("-O", "progress report.out", "-v")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        events = self.read_events()
        self.assertEqual([x[1] for x in events], ["opt"] * 4 + ["equilibrium"] * 4 + ["cross"] * 4)
        report = (self.work / "progress report.out").read_text(encoding="utf-8")
        self.assertEqual(report.count("* ComChemKit v"), 1)
        self.assertIn("Input step setting is ignored", report)
        self.assertIn("[1/12]", report)
        self.assertIn("[12/12] Completed", report)
        self.assertLess(report.index("[12/12] Completed"), report.index("Marcus Theory Barrier Calculation"))
        self.assertIn("70.2968 kcal/mol", report)
        self.assertIn("VERBOSE: THERMAL BREAKDOWN BY STATE", report)
        self.assertTrue(report.rstrip().endswith("---- ComChemKit normally terminated ---"))
        footer = report[report.rfind("\nCalculation completed at:"):]
        self.assertTrue(result.stdout.endswith(footer))
        self.assertFalse((self.work / "oset.log").exists())
        self.assertFalse((self.work / "ignored-by-cli.out").exists())
        self.assertFalse((self.work / "oset.marcus.lock").exists())
        self.assertFalse((self.work / "progress report.out.marcus.lock").exists())

    def test_gas_phase_uses_scf(self):
        with self.input.open("a", encoding="utf-8") as f:
            f.write("\nstep1_solvent =\nstep2_solvent =\n")
        env = self.env.copy(); env["FAKE_GAS"] = "1"
        result = self.run_cck(env=env)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("Configured phase : gas phase", report)
        self.assertIn("Energy source : SCF Done (gas phase)", report)
        self.assertEqual(len(self.read_events()), 12)
        for file in (self.work / "oset_step_2").glob("*.gau"):
            self.assertNotRegex(file.read_text(encoding="utf-8").lower(), r"\b(scrf|noneq|externaliteration)\b")

    def test_auto_respects_thermochemistry_options(self):
        result = self.run_cck("-t", "310", "--pressure", "2")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("Actual source : thermo module", report)
        self.assertIn("T=310.00 K (input)", report)
        self.assertIn("Input pressure: 2.000 atm", report)
        self.assertEqual(len(self.read_events()), 12)

    def test_engine_can_be_resolved_from_path(self):
        with self.input.open("a", encoding="utf-8") as f: f.write("\nqc_engine = fake gaussian\nstep = ignored-in-auto\n")
        env = self.env.copy(); env["PATH"] = str(self.work) + os.pathsep + env.get("PATH", "")
        result = self.run_cck(env=env)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(len(self.read_events()), 12)
        self.assertIn(str(self.engine), (self.work / "oset.log").read_text(encoding="utf-8"))

    def test_project_defaults_to_input_stem_in_auto_mode(self):
        content = self.input.read_text(encoding="utf-8")
        content = re.sub(r"(?m)^project\s*=.*\n", "", content)
        self.input.write_text(content, encoding="utf-8")
        result = self.run_cck()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((self.work / "automatic input.log").is_file())
        self.assertTrue((self.work / "automatic input_step_1").is_dir())
        self.assertTrue((self.work / "automatic input_step_2").is_dir())
        self.assertEqual(len(self.read_events()), 12)

    def test_existing_artifacts_and_locks_are_preserved(self):
        output = self.work / "oset.log"
        output.write_text("existing output\n", encoding="utf-8")
        result = self.run_cck()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("will not overwrite output", result.stderr)
        self.assertEqual(output.read_text(encoding="utf-8"), "existing output\n")
        self.assertEqual(self.read_events(), [])
        output.unlink()
        (self.work / "oset_step_1").mkdir()
        existing = self.work / "oset_step_1" / "existing.chk"
        existing.write_text("checkpoint", encoding="utf-8")
        result = self.run_cck()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("will not overwrite existing workflow files", result.stderr)
        self.assertEqual(existing.read_text(encoding="utf-8"), "checkpoint")
        self.assertFalse(output.exists())
        existing.unlink(); (self.work / "oset_step_1").rmdir()
        (self.work / "oset.marcus.lock").mkdir()
        result = self.run_cck()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Cannot acquire lock", result.stderr)
        self.assertTrue((self.work / "oset.marcus.lock").is_dir())
        self.assertEqual(self.read_events(), [])

    def test_invalid_engine_and_step_option(self):
        result = self.run_cck("-step", "3")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("-step cannot be combined", result.stderr)
        self.assertEqual(self.read_events(), [])
        with self.input.open("a", encoding="utf-8") as f: f.write("\nqc_engine = nonexistent_gaussian_engine\n")
        result = self.run_cck()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Cannot find executable qc_engine", result.stderr)
        self.assertFalse((self.work / "oset.log").exists())
        with self.input.open("a", encoding="utf-8") as f: f.write("\nqc_engine =\n")
        result = self.run_cck()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requires qc_engine", result.stderr)
        with self.input.open("a", encoding="utf-8") as f: f.write("\nworkflow = invalid\n")
        result = self.run_cck()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("workflow must be manual or auto", result.stderr)
        self.assertEqual(self.read_events(), [])

    def test_output_parent_is_checked_before_launch(self):
        result = self.run_cck("-O", "missing/report.out")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Report parent directory does not exist", result.stderr)
        self.assertEqual(self.read_events(), [])
        self.assertFalse((self.work / "oset_step_1").exists())

    def test_output_cannot_alias_a_step_directory(self):
        step1 = self.work / "oset_step_1"
        step1.mkdir()
        (self.work / "alias").symlink_to(step1, target_is_directory=True)
        result = self.run_cck("-O", "alias/report.out")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("must be outside the step directories", result.stderr)
        self.assertFalse((step1 / "report.out").exists())
        self.assertEqual(self.read_events(), [])

    def test_failures_stop_without_normal_footer(self):
        for index, kind, expected in (("1", "exit", "status 9"),
                                       ("1", "no_normal", "did not finish normally"),
                                       ("1", "truncated_link", "did not finish normally"),
                                       ("1", "truncated_internal_link", "did not finish normally"),
                                       ("1", "missing_checkpoint", "checkpoint missing or empty"),
                                       ("1", "missing_gibbs", "Frequency/Gibbs correction is missing"),
                                       ("1", "incomplete_opt", "Geometry optimization did not complete"),
                                       ("5", "error_after_normal", "did not finish normally"),
                                       ("9", "exit", "status 9")):
            with self.subTest(index=index, kind=kind), tempfile.TemporaryDirectory() as tmp:
                work = Path(tmp)
                for name in ("TEA.xyz", "p-terphenyl.xyz"):
                    shutil.copy(FIXTURES / name, work / name)
                inp = work / "job.inp"
                inp.write_text(self.input.read_text(encoding="utf-8"), encoding="utf-8")
                env = self.env.copy(); env.update(FAKE_EVENTS=str(work / "events.txt"), FAKE_FAIL_INDEX=index, FAKE_FAILURE=kind)
                result = subprocess.run([str(BINARY), "marcus", inp.name], cwd=work, env=env,
                                        text=True, capture_output=True, timeout=30)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                report = (work / "oset.log").read_text(encoding="utf-8")
                self.assertIn(expected, report)
                self.assertIn("Automatic workflow stopped", report)
                self.assertIn(f"Jobs: {int(index) - 1} completed, 1 failed/interrupted, {12 - int(index)} pending.", report)
                self.assertNotIn("ComChemKit normally terminated", report)
                events = (work / "events.txt").read_text(encoding="utf-8").splitlines()
                self.assertEqual(len(events), int(index))
                self.assertFalse((work / "oset.marcus.lock").exists())

    def test_generated_inputs_never_overwrite_racing_files(self):
        target = self.work / "oset_step_2/TEA-singlet-neutral_init.gau"
        env = self.env.copy(); env["FAKE_INJECT_FILE"] = str(target)
        result = self.run_cck(env=env)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(target.read_text(encoding="utf-8"), "do not overwrite")
        self.assertEqual(len(self.read_events()), 4)
        report = (self.work / "oset.log").read_text(encoding="utf-8")
        self.assertIn("Step 2 input generation failed", report)
        self.assertIn(str(target.relative_to(self.work)), report)
        self.assertIn("Jobs: 4 completed, 0 failed/interrupted, 8 pending", report)
        self.assertNotIn("ComChemKit normally terminated", report)

    def test_barrier_failures_and_asymmetric_warning(self):
        for kind, successful in (("negative_lambda", False), ("asymmetric_invalid", True)):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as tmp:
                work = Path(tmp)
                for name in ("TEA.xyz", "p-terphenyl.xyz"):
                    shutil.copy(FIXTURES / name, work / name)
                inp = work / "job.inp"
                inp.write_text(self.input.read_text(encoding="utf-8"), encoding="utf-8")
                env = self.env.copy(); env.update(FAKE_EVENTS=str(work / "events.txt"), FAKE_FAIL_INDEX="9", FAKE_FAILURE=kind)
                result = subprocess.run([str(BINARY), "marcus", inp.name], cwd=work, env=env,
                                        text=True, capture_output=True, timeout=30)
                report = (work / "oset.log").read_text(encoding="utf-8")
                self.assertEqual(result.returncode == 0, successful, result.stdout + result.stderr)
                self.assertEqual(len((work / "events.txt").read_text(encoding="utf-8").splitlines()), 12)
                if successful:
                    self.assertRegex(report, r"Delta G‡ \(asymmetric\)\s+N/A")
                    self.assertIn("negative discriminant", report)
                    self.assertIn("ComChemKit normally terminated", report)
                else:
                    self.assertIn("Barrier calculation is incomplete", report)
                    self.assertIn("Jobs: 12 completed, 0 failed/interrupted, 0 pending", report)
                    self.assertNotIn("ComChemKit normally terminated", report)

    def test_running_progress_and_interruption(self):
        child_file = self.work / "grandchild.pid"
        env = self.env.copy(); env.update(FAKE_DELAY_INDEX="1", FAKE_DELAY="60", FAKE_CHILD_PID_FILE=str(child_file))
        process = subprocess.Popen([str(BINARY), "marcus", self.input.name], cwd=self.work,
                                   env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            report = self.work / "oset.log"
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                if report.exists() and "Status: running" in report.read_text(encoding="utf-8") and self.events.exists() and child_file.exists(): break
                if process.poll() is not None: self.fail("Workflow exited before progress was visible")
                time.sleep(0.05)
            else: self.fail("Running progress was not flushed")
            child_pid = int(self.read_events()[0][3])
            grandchild_pid = int(child_file.read_text(encoding="utf-8"))
            self.assertIsNone(process.poll())
            concurrent = self.run_cck()
            self.assertNotEqual(concurrent.returncode, 0)
            self.assertIn("will not overwrite output", concurrent.stderr)
            self.assertEqual(len(self.read_events()), 1)
            process.send_signal(signal.SIGTERM)
            stdout, stderr = process.communicate(timeout=10)
            self.assertEqual(process.returncode, 143, stdout + stderr)
            text = report.read_text(encoding="utf-8")
            self.assertIn("Interrupted", text)
            self.assertNotIn("ComChemKit normally terminated", text)
            self.assertEqual(len(self.read_events()), 1)
            self.assertFalse((self.work / "oset.marcus.lock").exists())
            with self.assertRaises(ProcessLookupError): os.kill(child_pid, 0)
            # Orphaned grandchildren may briefly remain zombies in containers;
            # they must no longer be running after the process group is killed.
            stat = Path(f"/proc/{grandchild_pid}/stat")
            if stat.exists():
                self.assertEqual(stat.read_text(encoding="utf-8").split()[2], "Z")
            else:
                with self.assertRaises(ProcessLookupError): os.kill(grandchild_pid, 0)
        finally:
            if process.poll() is None:
                process.kill(); process.communicate(timeout=10)

    def test_engine_is_not_executed_through_a_shell(self):
        with self.input.open("a", encoding="utf-8") as f:
            f.write("\nqc_engine = g16; touch injected-file\n")
        result = self.run_cck()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Cannot find executable qc_engine", result.stderr)
        self.assertFalse((self.work / "injected-file").exists())
        self.assertEqual(self.read_events(), [])

    def test_scheduler_memory_limit_is_respected(self):
        with self.input.open("a", encoding="utf-8") as f: f.write("\nstep1_mem = 8GB\n")
        env = self.env.copy(); env.update(SLURM_JOB_ID="123", SLURM_MEM_PER_NODE="4096", SLURM_JOB_NUM_NODES="1")
        result = self.run_cck(env=env)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("exceeds allocated memory", result.stderr)
        self.assertEqual(self.read_events(), [])

    def test_scheduler_cpu_limit_is_respected(self):
        with self.input.open("a", encoding="utf-8") as f: f.write("\nstep1_nprocshared = 4\n")
        # Two tasks allocate four CPUs in total, but one Gaussian process
        # should not exceed the per-task CPU allocation.
        env = self.env.copy(); env.update(SLURM_JOB_ID="123", SLURM_CPUS_PER_TASK="2", SLURM_NTASKS="2", SLURM_JOB_NUM_NODES="1")
        result = self.run_cck(env=env)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("exceeds the allocated CPU count", result.stderr)
        self.assertEqual(self.read_events(), [])


if __name__ == "__main__":
    unittest.main()
