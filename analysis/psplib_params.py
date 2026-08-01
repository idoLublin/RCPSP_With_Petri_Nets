"""PSPLIB instance-generator parameters per parameter group.

The j30/j60/j90 sets each consist of 48 parameter groups x 10 instances.
Groups follow the standard ProGen full-factorial design, with RS varying
fastest, then RF, then NC:

    group g (1..48), idx = g - 1
    NC = {1.5, 1.8, 2.1}[idx // 16]
    RF = {0.25, 0.5, 0.75, 1.0}[(idx % 16) // 4]
    RS = set-specific 4 values [idx % 4]

j30 uses RS in {0.2, 0.5, 0.7, 1.0}; j60/j90 use {0.2, 0.5, 0.7, 1.0} as
well per the PSPLIB parameter files shipped with the benchmark.  The j30
branch is validated instance-by-instance against data/TT_ido.csv (which
records NC/RF/RS per j30 instance) by validate_against_tt_ido().
"""

from pathlib import Path

NC_LEVELS = (1.5, 1.8, 2.1)
RF_LEVELS = (0.25, 0.5, 0.75, 1.0)
RS_LEVELS = {
    "j30": (0.2, 0.5, 0.7, 1.0),
    "j60": (0.2, 0.5, 0.7, 1.0),
    "j90": (0.2, 0.5, 0.7, 1.0),
    "j120": (0.1, 0.2, 0.3, 0.4, 0.5),  # j120 uses 60 groups; see note below
}

REPO_ROOT = Path(__file__).resolve().parent.parent
TT_IDO_CSV = REPO_ROOT / "data" / "TT_ido.csv"


def group_params(set_name: str, group: int):
    """Return (NC, RF, RS) for a parameter group of j30/j60/j90.

    Raises ValueError for out-of-range groups or unsupported sets.
    """
    if set_name not in ("j30", "j60", "j90"):
        raise ValueError(f"unsupported set {set_name!r} (j30/j60/j90 only)")
    if not 1 <= group <= 48:
        raise ValueError(f"group {group} out of range 1..48 for {set_name}")
    idx = group - 1
    nc = NC_LEVELS[idx // 16]
    rf = RF_LEVELS[(idx % 16) // 4]
    rs = RS_LEVELS[set_name][idx % 4]
    return nc, rf, rs


def validate_against_tt_ido(path=TT_IDO_CSV):
    """Check the j30 factorial mapping against every row of TT_ido.csv.

    Returns the number of validated rows; raises AssertionError on the
    first mismatch.
    """
    import csv

    checked = 0
    with open(path, newline="") as fh:
        for row in csv.DictReader(fh):
            group = int(row["group"])
            nc, rf, rs = group_params("j30", group)
            got = (float(row["NC"]), float(row["RF"]), float(row["RS"]))
            assert got == (nc, rf, rs), (
                f"group {group}: TT_ido says NC/RF/RS={got}, factorial rule says {(nc, rf, rs)}"
            )
            checked += 1
    return checked


if __name__ == "__main__":
    n = validate_against_tt_ido()
    print(f"OK: factorial mapping matches data/TT_ido.csv on all {n} j30 rows")
