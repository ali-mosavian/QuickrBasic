"""conformance fails the gate on a failing case: it once printed 168/169 and exited 0, so make ctest passed
with nbody_fixed over its time limit."""
import sys
from pathlib import Path

sys.path.append(str(Path(__file__).parent))
import conformance
import dosrun


def main() -> None:
    dosrun.run_all = lambda batches, tmp: {k: dosrun.Result(k, "wrong", "exit", 0, 0) for b in batches for k in b["jobs"]}
    try:
        conformance.main()
    except SystemExit as e:
        assert e.code, "exit 0 on a failing case"
        print("ok")
        return
    raise AssertionError("exit 0 on a failing case")


if __name__ == "__main__":
    sys.argv = [sys.argv[0], "nbody_fixed"]
    main()
