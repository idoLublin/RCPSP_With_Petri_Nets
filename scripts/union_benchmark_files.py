#!/usr/bin/env python3
"""
Script to union benchmark CSV files for TT (Timed Transition) methods.

Creates two output files:
1. yoni_laptop_no_dp_benchmark_tt.csv - Union of no-DP TT results (groups 1-48)
2. yoni_laptop_dp_benchmark_tt.csv - Union of DP TT results (groups 1-48)
"""

import pandas as pd
import os

# Base path for data files
DATA_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA_PATH = os.path.join(DATA_DIR, "data")

# Input files for no-DP benchmark (groups 17-48 from first file, groups 1-16 from second)
NO_DP_FILE_G17_48 = os.path.join(DATA_PATH, "2026-01-19_j30_g17-48_e1-10_tt_ no_dp.csv")
NO_DP_FILE_G1_16 = os.path.join(DATA_PATH, "test_no_dp_1-16_e1-10_12_1_2026.csv")

# Input files for DP benchmark (groups 17-48 from first file, groups 1-16 from second)
DP_FILE_G17_48 = os.path.join(DATA_PATH, "2026-01-19_j30_g17-48_e1-10_tt_g17-48_tt.csv")
DP_FILE_G1_16 = os.path.join(DATA_PATH, "2026-01-10_j30_g1-16_e1-10_all_full_test_1_1.csv")

# Output files
OUTPUT_NO_DP = os.path.join(DATA_PATH, "yoni_laptop_no_dp_benchmark_tt.csv")
OUTPUT_DP = os.path.join(DATA_PATH, "yoni_laptop_dp_benchmark_tt.csv")


def load_and_filter_tt(file_path, groups_range=None):
    """
    Load a CSV file and filter for TT (Timed Transition) PetriType only.

    Args:
        file_path: Path to the CSV file
        groups_range: Optional tuple (min_group, max_group) to filter groups

    Returns:
        DataFrame with only TT rows, optionally filtered by group range
    """
    print(f"Loading: {file_path}")
    df = pd.read_csv(file_path)

    # Filter for TT PetriType only
    df_tt = df[df['PetriType'] == 'TT'].copy()
    print(f"  Total rows: {len(df)}, TT rows: {len(df_tt)}")

    # Filter by group range if specified
    if groups_range:
        min_group, max_group = groups_range
        df_tt = df_tt[(df_tt['group'] >= min_group) & (df_tt['group'] <= max_group)]
        print(f"  After filtering groups {min_group}-{max_group}: {len(df_tt)} rows")

    return df_tt


def create_union(file1, file2, groups1_range, groups2_range, output_path, description):
    """
    Create a union of TT data from two files.

    Args:
        file1: First input file path
        file2: Second input file path
        groups1_range: Tuple (min, max) for groups from file1
        groups2_range: Tuple (min, max) for groups from file2
        output_path: Output file path
        description: Description for logging
    """
    print(f"\n{'='*60}")
    print(f"Creating: {description}")
    print(f"{'='*60}")

    # Load and filter both files
    df1 = load_and_filter_tt(file1, groups1_range)
    df2 = load_and_filter_tt(file2, groups2_range)

    # Union the dataframes
    df_union = pd.concat([df1, df2], ignore_index=True)

    # Sort by group and exam
    df_union = df_union.sort_values(['group', 'exam']).reset_index(drop=True)

    # Verify we have all groups 1-48
    unique_groups = sorted(df_union['group'].unique())
    print(f"\nUnique groups in union: {unique_groups}")
    print(f"Total groups: {len(unique_groups)}")

    # Check for expected groups
    expected_groups = list(range(1, 49))
    missing_groups = [g for g in expected_groups if g not in unique_groups]
    if missing_groups:
        print(f"WARNING: Missing groups: {missing_groups}")

    extra_groups = [g for g in unique_groups if g not in expected_groups]
    if extra_groups:
        print(f"WARNING: Extra groups (outside 1-48): {extra_groups}")

    # Save to output file
    df_union.to_csv(output_path, index=False)
    print(f"\nSaved {len(df_union)} rows to: {output_path}")

    # Summary statistics
    print(f"\nSummary:")
    print(f"  Groups: {df_union['group'].min()} - {df_union['group'].max()}")
    print(f"  Exams per group: {df_union.groupby('group').size().unique()}")
    finished_count = df_union['finished'].sum() if 'finished' in df_union.columns else 'N/A'
    print(f"  Finished problems: {finished_count}")

    return df_union


def main():
    print("Benchmark CSV Union Script")
    print("="*60)

    # Create no-DP union
    # Groups 17-48 from no_dp file, groups 1-16 from test_no_dp file
    df_no_dp = create_union(
        file1=NO_DP_FILE_G17_48,
        file2=NO_DP_FILE_G1_16,
        groups1_range=(17, 48),
        groups2_range=(1, 16),
        output_path=OUTPUT_NO_DP,
        description="No-DP Benchmark (yoni_laptop_no_dp_benchmark_tt.csv)"
    )

    # Create DP union
    # Groups 17-48 from DP file, groups 1-16 from full test file
    df_dp = create_union(
        file1=DP_FILE_G17_48,
        file2=DP_FILE_G1_16,
        groups1_range=(17, 48),
        groups2_range=(1, 16),
        output_path=OUTPUT_DP,
        description="DP Benchmark (yoni_laptop_dp_benchmark_tt.csv)"
    )

    print("\n" + "="*60)
    print("DONE!")
    print("="*60)
    print(f"\nCreated files:")
    print(f"  1. {OUTPUT_NO_DP}")
    print(f"  2. {OUTPUT_DP}")


if __name__ == "__main__":
    main()
