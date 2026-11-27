"""The DOS build's lib/ held gfx.qbx alone: a Quickr program importing keys, math, random or text
stopped on DOS with "no module keys". The build and its zip ship stdlib/*.bas in lib/. Every step
planned (-B): after a build, the copy was made already and the plan had none."""

import subprocess
from pathlib import Path


def shipped() -> None:
    plan = subprocess.run(["make", "-n", "-B", "dos-zip"], capture_output=True, text=True, check=True).stdout
    steps = [step.split() for line in plan.splitlines() for step in line.split("&&")]
    copies = [s for s in steps if s and s[0] == "cp"]
    for module in sorted(Path("stdlib").glob("*.bas")):
        for to in ("build/c/lib", "build/c/release/lib/"):
            assert any(str(module) in c and c[-1] == to for c in copies), f"{module} not in {to}"


if __name__ == "__main__":
    shipped()
    print("ok")
