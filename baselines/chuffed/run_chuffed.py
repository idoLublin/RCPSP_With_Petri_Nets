#!/usr/bin/env python3
"""
Chuffed (via MiniZinc) RCPSP baseline.
Reads PSPLIB .sm files, converts to .dzn on the fly, runs MiniZinc + Chuffed.
Output CSV columns: instance, solved, optimal, makespan, time_s
"""
import subprocess, time, re, csv, sys, os, tempfile, glob, argparse, shutil

TIMEOUT_S = 300
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MZN_FILE = os.path.join(SCRIPT_DIR, "rcpsp.mzn")
MINIZINC_EXE = shutil.which("minizinc") or r"C:\Users\idolu\AppData\Local\Programs\MiniZinc\minizinc.exe"

DATA_DIRS = {
    "j30": r"C:\Users\idolu\CLionProjects\RCPSP_With_Petri_nets\extract_problems\data\j30.sm.tgz",
    "j60": r"C:\Users\idolu\Downloads\j60.sm.tgz",
    "j90": r"C:\Users\idolu\Downloads",
}
PREFIX = {"j30": "j30", "j60": "j60", "j90": "j90"}


def sm_to_dzn(sm_path):
    with open(sm_path, encoding="utf-8") as f:
        data = f.read()

    m = re.search(r"jobs \(incl\. supersource/sink \)\s*:\s*(\d+)", data)
    n_tasks = int(m.group(1))

    prec_text = re.search(
        r"PRECEDENCE RELATIONS:(.*?)REQUESTS/DURATIONS:", data, re.DOTALL
    ).group(1)
    succs = []
    for line in prec_text.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[0].isdigit():
            job = int(parts[0])
            for s in parts[3:]:
                succs.append((job, int(s)))

    req_text = re.search(
        r"REQUESTS/DURATIONS:(.*?)RESOURCEAVAILABILITIES:", data, re.DOTALL
    ).group(1)
    durations, rreq_rows = [], []
    for line in req_text.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[0].isdigit():
            durations.append(int(parts[2]))
            rreq_rows.append(list(map(int, parts[3:])))

    res_text = re.search(r"RESOURCEAVAILABILITIES:(.*?)\*", data, re.DOTALL).group(1)
    res_lines = [l.strip() for l in res_text.splitlines() if l.strip()]
    rc = list(map(int, res_lines[-1].split()))

    suc_str = "[|" + "|".join(f"{a},{b}" for a, b in succs) + "|]" if succs else "[||]"
    rreq_str = "[|" + "|".join(",".join(map(str, row)) for row in rreq_rows) + "|]"

    return "\n".join([
        f"n_tasks = {n_tasks};",
        f"n_res = {len(rc)};",
        f"rc = {rc};",
        f"dur = {durations};",
        f"rreq = {rreq_str};",
        f"n_succs = {len(succs)};",
        f"suc = {suc_str};",
    ])


def run_instance(sm_path, inst_name):
    dzn_content = sm_to_dzn(sm_path)

    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".dzn", delete=False, encoding="utf-8"
    ) as tf:
        tf.write(dzn_content)
        dzn_path = tf.name

    try:
        t0 = time.time()
        result = subprocess.run(
            [
                MINIZINC_EXE,
                "--solver", "chuffed",
                "--time-limit", str(TIMEOUT_S * 1000),
                "-p", "1",
                MZN_FILE,
                dzn_path,
            ],
            capture_output=True,
            text=True,
            timeout=TIMEOUT_S + 30,
        )
        elapsed = time.time() - t0
        out = result.stdout

        makespans = re.findall(r"makespan\s*=\s*(\d+)", out)
        optimal = "==========" in out

        if makespans:
            return {
                "instance": inst_name,
                "solved": True,
                "optimal": optimal,
                "makespan": int(makespans[-1]),
                "time_s": round(elapsed, 3),
            }
        else:
            return {
                "instance": inst_name,
                "solved": False,
                "optimal": False,
                "makespan": -1,
                "time_s": round(elapsed, 3),
            }
    except subprocess.TimeoutExpired:
        return {
            "instance": inst_name,
            "solved": False,
            "optimal": False,
            "makespan": -1,
            "time_s": TIMEOUT_S,
        }
    finally:
        os.unlink(dzn_path)


def _natural_key(path, prefix):
    # Sort by (param, instance) numerically so order is 1..48 x 1..10,
    # not lexicographic (which puts j3010_1 before j301_1).
    name = os.path.basename(path).replace(".sm", "")
    after = name[len(prefix):]            # e.g. "10_1"
    parts = after.split("_")
    try:
        return (0, int(parts[0]), int(parts[1]))
    except (ValueError, IndexError):
        return (1, 0, name)               # push any oddly-named files to the end


def collect_instances(size, data_dir=None):
    data_dir = data_dir or DATA_DIRS[size]
    prefix = PREFIX[size]
    # Use *_* to match only instance files (e.g. j901_1.sm), not j90hrs.sm etc.
    pattern = os.path.join(data_dir, f"{prefix}*_*.sm")
    files = sorted(glob.glob(pattern), key=lambda f: _natural_key(f, prefix))
    return [(f, os.path.basename(f).replace(".sm", "")) for f in files]


def main():
    parser = argparse.ArgumentParser(description="Chuffed RCPSP baseline")
    parser.add_argument("size", choices=["j30", "j60", "j90"], help="Dataset size")
    parser.add_argument(
        "--out",
        default=None,
        help="Output CSV path (default: results_chuffed_<size>.csv in script dir)",
    )
    parser.add_argument(
        "--data-dir",
        default=None,
        help="Directory containing the .sm files (overrides built-in default)",
    )
    args = parser.parse_args()

    out_csv = args.out or os.path.join(SCRIPT_DIR, f"results_chuffed_{args.size}.csv")
    instances = collect_instances(args.size, args.data_dir)

    if not instances:
        data_dir = args.data_dir or DATA_DIRS[args.size]
        print(f"No .sm files found for {args.size} in {data_dir}")
        sys.exit(1)

    print(f"Running Chuffed on {len(instances)} {args.size} instances (timeout={TIMEOUT_S}s)")
    print(f"Output: {out_csv}")

    with open(out_csv, "w", newline="", encoding="utf-8") as csvfile:
        writer = csv.DictWriter(
            csvfile, fieldnames=["instance", "solved", "optimal", "makespan", "time_s"]
        )
        writer.writeheader()

        for i, (sm_path, inst_name) in enumerate(instances, 1):
            row = run_instance(sm_path, inst_name)
            writer.writerow(row)
            csvfile.flush()
            status = "OPT" if row["optimal"] else ("SAT" if row["solved"] else "---")
            mk = row["makespan"] if row["solved"] else "n/a"
            print(f"  [{i:3d}/{len(instances)}] {inst_name:15s}  {status}  makespan={mk}  {row['time_s']:.1f}s")

    print(f"\nDone. Results saved to {out_csv}")


if __name__ == "__main__":
    main()
