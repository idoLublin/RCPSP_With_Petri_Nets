import csv
import os
import time
import signal
import threading




import numpy as np
import pandas as pd
import pyomo.environ as pyo

from RCPSP_modeling.rcpsp_base import RcpspBase, divide_dicts
from RCPSP_modeling.rcpsp_petri_net import (
    RcpspTimedPlacePetriNet,
    RcpspTimedTransitionPetriNet,
)
from algorithms_for_solving_rcpsp.a_star import AStarSolver
from extract_problems.extract_problem import (
    extract_rcpsp,
    extract_opt_values,
    extract_opt_values_with_time,
)
from RCPSP_modeling.rcpsp_math_represntation import solve_file_problem_math

from algorithms_for_solving_rcpsp.solve_with_mip_solver import solve_rcpsp_optimizer


def cp_heuristic_for_timed_place(
        rcpsp_example: RcpspBase, removed_activities, started_activities, current_time
):
    if started_activities:
        expected_finsh_time = max(
            [
                rcpsp_example.find_activity_by_name(act).duration
                + started_activities[act]
                for act in started_activities
            ]
        )
    else:
        expected_finsh_time = 0
    remaining_time_of_started_jobs = expected_finsh_time - current_time

    rcpsp_example.update_problem(removed_activities=started_activities)
    return max(
        remaining_time_of_started_jobs,
        rcpsp_example.calculate_critical_path()["duration"],
    )


def sum_resource_demands(activities):
    total_demands = {}

    for activity in activities:
        for resource, demand in activity.resource_demands.items():
            if resource in total_demands:
                total_demands[resource] += demand * activity.duration
            else:
                total_demands[resource] = demand

    return total_demands


def resources_heuristic(
        rcpsp_example: RcpspBase,
        removed_activities,
        started_activities,
        current_time,
        job_finish_activity,
        alternatives,
):
    activities_left = [
        act for act in rcpsp_example.activities if act.name not in removed_activities
    ]
    if len(activities_left) < 2:
        return 0
    sum_of_resource_demands = sum_resource_demands(activities_left)
    divides_by_amount = divide_dicts(sum_of_resource_demands, rcpsp_example.resources)
    if not divides_by_amount:
        print("no")
    max_of_divdes = max(divides_by_amount.values())
    if started_activities:
        last_activity = max(removed_activities, key=removed_activities.get)
        rel_activities = [
            act
            for act in rcpsp_example.activities
            if act.name not in started_activities
        ]
        independent_activities = [
            act.name
            for act in rel_activities
            if (last_activity, act.name) not in rcpsp_example.dependencies_deep_set
        ]
        unk_time = current_time - max(started_activities.values())
    else:
        independent_activities = []
        unk_time = 0
    # rcpsp_example.get_all_critical_path_of_sub()
    # opt_1 = copy.copy(rcpsp_example)
    # opt_1.update_problem(
    #     removed_activities=list(started_activities) + independent_activities
    # )
    # opt_2 = copy.copy(rcpsp_example)
    # opt_2.update_problem(removed_activities=list(started_activities))
    return max(
        rcpsp_example.get_all_critical_path_of_sub(
            executed=list(started_activities) + independent_activities,
            job_finish_activity=job_finish_activity,
        ),
        rcpsp_example.get_all_critical_path_of_sub(
            list(started_activities),
            job_finish_activity,
        )
        - unk_time,
        max_of_divdes,
        )


def cp_heuristic(
        rcpsp_example: RcpspBase,
        removed_activities,
        started_activities,
        current_time,
        job_finish_activity,
        alternatives,
):

    if started_activities:
        last_activity = max(removed_activities, key=removed_activities.get)
        rel_activities = [
            act
            for act in rcpsp_example.activities
            if act.name not in started_activities
        ]
        independent_activities = [
            act.name
            for act in rel_activities
            if (last_activity, act.name) not in rcpsp_example.dependencies_deep_set
        ]
        unk_time = current_time - max(started_activities.values())
    else:
        independent_activities = []
        unk_time = 0
    # rcpsp_example.get_all_critical_path_of_sub()
    # opt_1 = copy.copy(rcpsp_example)
    # opt_1.update_problem(
    #     removed_activities=list(started_activities) + independent_activities
    # )
    # opt_2 = copy.copy(rcpsp_example)
    # opt_2.update_problem(removed_activities=list(started_activities))
    return max(
        rcpsp_example.get_all_critical_path_of_sub(
            executed=list(started_activities) + independent_activities,
            job_finish_activity=job_finish_activity,
            ongoing={},
        )[0],
        rcpsp_example.get_all_critical_path_of_sub(
            list(started_activities), job_finish_activity, ongoing={}
        )[0]
        - unk_time,
        )


def cp_heuristic_as(
        rcpsp_example: RcpspBase,
        removed_activities,
        started_activities,
        current_time,
        job_finish_activity,
        alternatives,
):

    if started_activities:
        last_activity = max(removed_activities, key=removed_activities.get)
        rel_activities = [
            act
            for act in rcpsp_example.activities
            if act.name not in started_activities
        ]
        independent_activities = [
            act.name
            for act in rel_activities
            if (last_activity, act.name) not in rcpsp_example.dependencies_deep_set
        ]
        unk_time = current_time - max(started_activities.values())
    else:
        independent_activities = []
        unk_time = 0
    # rcpsp_example.get_all_critical_path_of_sub()
    # opt_1 = copy.copy(rcpsp_example)
    # opt_1.update_problem(
    #     removed_activities=list(started_activities) + independent_activities
    # )
    # opt_2 = copy.copy(rcpsp_example)
    # opt_2.update_problem(removed_activities=list(started_activities))
    res = 9999999999
    for alternative in alternatives.values():
        # alternative_set = set(alternative)
        # not_relevant = list(set(rcpsp_example.activities_names_durations) - set(alternative))
        # not_relevant = [
        #     act
        #     for act in rcpsp_example.activities_names_durations
        #     if act not in alternative_set
        # ]
        first_res = rcpsp_example.get_all_critical_path_of_sub(
            executed=set(
                list(started_activities) + independent_activities + alternative
            ),
            job_finish_activity=job_finish_activity,
        )
        if first_res < res:
            # print("not calc 2 ")
            # continue

            res_of_alternative = max(
                first_res,
                rcpsp_example.get_all_critical_path_of_sub(
                    set(list(started_activities) + alternative),
                    job_finish_activity,
                )
                - unk_time,
                )
            if res_of_alternative < res:
                res = res_of_alternative

    return res
    # return max(
    #     rcpsp_example.get_all_critical_path_of_sub_as(
    #         executed=list(started_activities) + independent_activities,
    #         job_finish_activity=job_finish_activity,
    #         or_dependencies=or_dependencies.copy(),
    #     ),
    #     rcpsp_example.get_all_critical_path_of_sub_as(
    #         list(started_activities),
    #         job_finish_activity,
    #         or_dependencies=or_dependencies,
    #     )
    #     - unk_time,
    # )


def solver():
    petri_example, rcpsp_example = init_small_example()
    a_star_solver = AStarSolver(
        petri_example, rcpsp_example, heuristic_function=cp_heuristic
    )
    a_star_solver.solve(beam_search_size=25)


def init_real_problem(file_path, timed_transition):
    rcpsp_example = extract_rcpsp(file_path)
    if timed_transition:
        petri_example = RcpspTimedTransitionPetriNet(rcpsp_example)
    else:
        petri_example = RcpspTimedPlacePetriNet(rcpsp_example)
    return petri_example, rcpsp_example


def analyze_results(dir_path):
    true_count = 0
    false_count = 0
    total_opt = 0
    files_not_solved = []
    nodes_visited = []
    total_makespan = 0
    for filename in os.listdir(dir_path):
        filepath = os.path.join(dir_path, filename)
        with open(filepath, "r") as file:
            data = json.load(file)
            if "solved" in data:
                if data["solved"]:
                    true_count += 1
                    nodes_visited.append(data["nodes_visited"])
                    total_opt += data["opt_value"]
                    total_makespan += max(data["makespan"], data["opt_value"])
                    # if data["makespan"] > data["opt_value"]:
                    #     print(filepath)
                else:
                    files_not_solved.append(filename)
                    false_count += 1

    print(f"Files with 'solved' = True: {true_count}")
    print(f"Files with 'solved' = False: {false_count}")
    print(f"Mean nodes_visited: {np.mean(nodes_visited)}")
    print(f"Median nodes_visited: {np.median(nodes_visited)}")
    print(f"Mean ratio: {total_makespan / total_opt}")
    return files_not_solved


def summarize_results_to_csv(results_dir):
    opt_values = extract_opt_values_with_time(
        "/Users/iyarzaks/PycharmProjects/scheduling_with_petri_nets/extract_problems/data/j30opt.txt"
    )
    for dir in results_dir:
        for opt_value in opt_values:

            filepath = os.path.join(
                dir, opt_value["param"] + "_" + opt_value["instance"]
            )
            try:
                with open(filepath, "r") as file:
                    data = json.load(file)
                    if "solved" in data:
                        if data["solved"]:
                            opt_value[f"{dir}_solved"] = data["solved"]
                            opt_value[f"{dir}_makespan"] = data["makespan"]
                            opt_value[f"{dir}_run_time"] = data["run_time"]
                            if "nodes_expanded" in data:
                                opt_value[f"{dir}_nodes_expanded"] = data[
                                    "nodes_expanded"
                                ]
                            else:
                                opt_value[f"{dir}_nodes_expanded"] = data[
                                    "nodes_expand"
                                ]

                            opt_value[f"{dir}_nodes_generated"] = data[
                                "nodes_generated"
                            ]
                            # opt_value["timed_transition_nodes_visited"] = data[
                            #     "nodes_visited"
                            # ]
                        else:
                            opt_value[f"{dir}_solved"] = False
            except:
                pass

            # filepath_tp = os.path.join(
            #     TP_file, opt_value["param"] + "_" + opt_value["instance"]
            # )
            # with open(filepath_tp, "r") as file:
            #     data_tp = json.load(file)
            #     if "solved" in data_tp:
            #         if data_tp["solved"]:
            #             opt_value["timed_place_solved"] = data_tp["solved"]
            #             opt_value["timed_place_makespan"] = max(
            #                 data_tp["makespan"], data_tp["opt_value"]
            #             )
            #             opt_value["timed_place_nodes_visited"] = data_tp["nodes_visited"]
            #         else:
            #             opt_value["timed_place_solved"] = data_tp["solved"]

    return pd.DataFrame(opt_values)


def init_small_example(timed_transition=False):
    rcpsp_example = RcpspBase(
        activities_list=[
            {"name": "A", "duration": 8, "resource_demands": {"R1": 4}},
            {"name": "B1", "duration": 4, "resource_demands": {"R1": 4}},
            {"name": "B2", "duration": 4, "resource_demands": {"R1": 4}},
            {"name": "B3", "duration": 4, "resource_demands": {"R1": 4}},
            {"name": "B4", "duration": 4, "resource_demands": {"R1": 4}},
            {"name": "B5", "duration": 4, "resource_demands": {"R1": 4}},
            {"name": "B6", "duration": 4, "resource_demands": {"R1": 4}},
            {"name": "C", "duration": 4, "resource_demands": {"R1": 3}},
        ],
        dependencies={
            "A": ["B1", "B2", "B3", "B4", "B5", "B6"],
            "B1": ["C"],
            "B2": ["C"],
            "B3": ["C"],
            "B4": ["C"],
            "B5": ["C"],
            "B6": ["C"],
        },
        resources={"R1": 1000},
    )
    if timed_transition:
        petri_example = RcpspTimedTransitionPetriNet(rcpsp_example)
    else:
        petri_example = RcpspTimedPlacePetriNet(rcpsp_example)
    return petri_example, rcpsp_example


def init_slide_example():
    rcpsp_example = RcpspBase(
        activities_list=[
            {"name": "a", "duration": 0, "resource_demands": {}},
            {"name": "b", "duration": 2, "resource_demands": {"R1": 2}},
            {"name": "c", "duration": 2, "resource_demands": {"R1": 3}},
            {"name": "e", "duration": 0, "resource_demands": {}},
        ],
        dependencies={"a": ["b", "c"], "b": ["e"], "c": ["e"]},
        resources={"R1": 5},
    )
    petri_example = RcpspTimedPlacePetriNet(rcpsp_example)
    return petri_example, rcpsp_example


def check_opt_vs_initial_heuristic():
    opt_values = extract_opt_values(
        "/Users/iyarzaks/PycharmProjects/scheduling_with_petri_nets/extract_problems/data/j30opt.txt"
    )
    comparison = []
    for file in os.listdir("extract_problems/data/j30.sm.tgz"):
        param = re.search(r"j30(\d+)_", file).group(1)
        instance = file.split("_")[-1].split(".")[0]
        rcpsp_example = extract_rcpsp(
            f"/Users/iyarzaks/PycharmProjects/scheduling_with_petri_nets/extract_problems/data/j30.sm.tgz/{file}"
        )
        cpm_heuristic = rcpsp_example.calculate_critical_path()["duration"]
        comparison.append(
            {
                "param": param,
                "instance": instance,
                "opt_value": opt_values[(param, instance)],
                "heuristic": cpm_heuristic,
            }
        )
    comparison_df = pd.DataFrame(comparison)
    comparison_df["ratio"] = comparison_df["heuristic"] / comparison_df["opt_value"]
    print(comparison_df["ratio"].mean())
    comparison_df.to_csv("comparison_results_30.csv", index=False)


def solve_file_problem(
        path, beam_search_size=None, timed_transition=True, logging=False
):
    petri_example, rcpsp_example = init_real_problem(
        path, timed_transition=timed_transition
    )
    # p, u, e, c = extract_rcpsp_for_solver(path)
    # petri_example.plot("petri_example_hard.png")
    if timed_transition:
        a_star_solver = AStarSolver(
            petri_example,
            rcpsp_example,
            heuristic_function=cp_heuristic,
            timed_transition=timed_transition,
            job_finish_activity=rcpsp_example.activities[-1].name,
            # heuristic_params=(p, u, e, c),
        )
    else:
        a_star_solver = AStarSolver(
            petri_example,
            rcpsp_example,
            heuristic_function=cp_heuristic_for_timed_place,
            timed_transition=timed_transition,
        )
    result = a_star_solver.solve(beam_search_size=beam_search_size, logging=logging)
    return result


def solve_small_problem(timed_transition=False, beam_search_size=None):
    petri_example, rcpsp_example = init_small_example(timed_transition=timed_transition)
    a_star_solver = AStarSolver(
        petri_example,
        rcpsp_example,
        heuristic_function=cp_heuristic,
        timed_transition=timed_transition,
    )
    result = a_star_solver.solve(beam_search_size=beam_search_size)
    return result


def run_with_timeout(timeout, func, *args, **kwargs):
    signal.signal(signal.SIGALRM, timeout_handler)
    signal.alarm(timeout)
    start_time = time.time()
    try:
        result = func(*args, **kwargs)
        elapsed_time = time.time() - start_time
        result["run_time"] = elapsed_time
    except TimeoutException:
        result = {"solved": False, "run_time": timeout}
    finally:
        signal.alarm(0)
    return result


import os
import re
import json
import signal
import time
import platform
from concurrent.futures import ProcessPoolExecutor, as_completed, TimeoutError
from tqdm import tqdm
from functools import partial


class TimeoutException(Exception):
    pass


def timeout_handler(signum, frame):
    raise TimeoutException()


def run_with_timeout(timeout, func, *args, **kwargs):
    """
    Cross-platform timeout handler that works on both Unix and Windows.
    Falls back to threading-based timeout on Windows.
    """
    if platform.system() != "Windows":
        # Unix-based systems: use SIGALRM
        signal.signal(signal.SIGALRM, timeout_handler)
        signal.alarm(timeout)
        start_time = time.time()
        try:
            result = func(*args, **kwargs)
            elapsed_time = time.time() - start_time
            result["run_time"] = elapsed_time
        except TimeoutException:
            result = {"solved": False, "run_time": timeout}
        finally:
            signal.alarm(0)
    else:
        # Windows: use ProcessPoolExecutor with timeout
        with ProcessPoolExecutor(max_workers=1) as executor:
            future = executor.submit(func, *args, **kwargs)
            start_time = time.time()
            try:
                result = future.result(timeout=timeout)
                elapsed_time = time.time() - start_time
                result["run_time"] = elapsed_time
            except TimeoutError:
                result = {"solved": False, "run_time": timeout}

    return result


def run_over_files(results_dir, timeout, max_retries=3, retry_delay=5):
    """
    Process files with automatic retries and robust error handling.
    """
    os.makedirs(results_dir, exist_ok=True)
    opt_values = extract_opt_values("extract_problems/data/j30opt.txt")

    # Track failed files for retry
    failed_files = []

    files_to_check = [
        file
        for file in os.listdir("extract_problems/data/j30.sm.tgz")
        if not os.path.exists(
            f'{results_dir}/{file.replace("j30", "").replace(".sm", "")}'
        )
    ]

    print(f"{len(files_to_check)} files to check")

    def process_batch(file_list, attempt=1):
        progress_bar = tqdm(
            total=len(file_list), desc=f"Processing (Attempt {attempt})"
        )

        current_failed = []

        # Use fewer workers to avoid memory issues
        max_workers = min(os.cpu_count() or 1, 6)
        with ProcessPoolExecutor(max_workers=max_workers) as executor:
            solve_func = partial(
                solve_wrapper,
                results_dir=results_dir,
                timeout=timeout,
                opt_values=opt_values,
            )

            # Submit all tasks at once - the ProcessPoolExecutor will handle the queuing
            futures = {executor.submit(solve_func, file): file for file in file_list}

            # Process results as they complete (faster jobs will finish first)
            for future in as_completed(futures):
                original_file = futures[future]
                try:
                    result = future.result()
                    if isinstance(result, Exception):
                        current_failed.append(original_file)
                        print(f"Failed {original_file}: {result}")
                    else:
                        print(f"Completed {original_file}")
                except Exception as e:
                    current_failed.append(original_file)
                    print(f"Error processing {original_file}: {e}")
                progress_bar.update(1)

        progress_bar.close()
        return current_failed

    # Initial processing
    current_files = files_to_check
    attempt = 1

    while attempt <= max_retries and current_files:
        if attempt > 1:
            print(f"\nRetry attempt {attempt} for {len(current_files)} files")
            time.sleep(retry_delay)

        failed_files = process_batch(current_files, attempt)

        # Update for next iteration
        current_files = failed_files
        attempt += 1

    # Final report
    if failed_files:
        print(
            f"\nFailed to process {len(failed_files)} files after {max_retries} attempts:"
        )
        for file in failed_files:
            print(f"- {file}")
    else:
        print("\nAll files processed successfully!")


def solve_wrapper(file, results_dir, timeout, opt_values):
    """Wrapper to catch and handle exceptions, including timeouts."""
    max_attempts = 2  # Individual file retry attempts
    attempt = 0

    while attempt < max_attempts:
        try:
            solve_problem_with_time_limit(
                results_dir=results_dir,
                timeout=timeout,
                problem_file=f"extract_problems/data/j30.sm.tgz/{file}",
                opt_values=opt_values,
                beam_search_size=None,
                timed_transition=True,
            )
            return True
        except Exception as e:
            attempt += 1
            if attempt >= max_attempts:
                return e
            time.sleep(1)  # Short delay between attempts


def solve_problem_with_time_limit(
        results_dir, timeout, problem_file, opt_values, beam_search_size, timed_transition
):
    param = re.search(r"j30(\d+)_", problem_file).group(1)
    instance = problem_file.split("_")[-1].split(".")[0]

    try:
        result = run_with_timeout(
            timeout=timeout,
            func=solve_file_problem,
            path=problem_file,
        )
        result["opt_value"] = opt_values.get((param, instance), None)

        # Ensure atomic write using temporary file
        temp_file = f"{results_dir}/{param}_{instance}.tmp"
        final_file = f"{results_dir}/{param}_{instance}"

        try:
            with open(temp_file, "w") as file:
                json.dump(result, file)
            os.rename(temp_file, final_file)  # Atomic operation
        except Exception as e:
            if os.path.exists(temp_file):
                os.remove(temp_file)
            raise e

    except Exception as e:
        print(f"Error processing {problem_file}: {str(e)}")
        raise e


# def run_over_files(results_dir, timeout):
#     opt_values = extract_opt_values("extract_problems/data/j30opt.txt")
#     files_to_check = []
#     for file in os.listdir("extract_problems/data/j30.sm.tgz"):
#         if not os.path.exists(
#             f'{results_dir}/{file.replace("j30", "").replace(".sm", "")}'
#         ):
#             files_to_check.append(file)
#     print(f"{len(files_to_check)} files to check")
#     progress_bar = tqdm(total=len(files_to_check), desc="Processing")
#     for file in files_to_check:
#
#         solve_problem_with_time_limit(
#             results_dir=results_dir,
#             timeout=timeout,
#             problem_file=f"extract_problems/data/j30.sm.tgz/{file}",
#             opt_values=opt_values,
#             beam_search_size=None,
#             timed_transition=True,
#         )
#         progress_bar.update(1)


def main():
    # res_df = summarize_results_to_csv(
    #     [
    #         "results/paper_version_scip",
    #         "results/paper_version_TTPN_10_min",
    #     ]
    # )
    # res_df.to_csv("results/paper_new_scip.csv", index=False)

    # analyze_results("results/j30_time_transition_depends_heuristic")
    # analyze_results("results/j30")
    # easy_problems = pd.read_csv("results/summary_2.csv")
    # easy_problems = easy_problems[easy_problems["timed_transition_solved"]]
    # easy_problems["param"] = easy_problems["param"].apply(str)
    # easy_problems["param"] = easy_problems["param"] + "_"
    # easy_problems["instance"] = easy_problems["instance"].apply(str)
    # easy_problems["problem_instance"] = (
    #     easy_problems["param"] + easy_problems["instance"]
    # )
    # easy_problems = easy_problems["problem_instance"]
    # run_over_files(results_dir="results/paper_version_TTPN_10_min", timeout=18000)
    # # print(solve_small_problem_math())
    # print(
    #     solve_file_problem(
    #         path="extract_problems/data/j60.sm.tgz/j6040_2.sm",
    #         # timed_transition=True,
    #         logging=True,
    #     )
    # )


    class TimeoutException(Exception):
        pass

    def timeout_handler(signum, frame):
        raise TimeoutException()

    def solve_one(group,example,benchmark):
        filename = f"extract_problems/data/j{benchmark}.sm.tgz/j{benchmark}{group}_{example}.sm"
        problem_name = f"j{benchmark}{group}_{example}"

        print(f"Solving {problem_name}...")
        start_time = time.time()

        # Set 10-minute timeout using threading
        timeout_occurred = False

        def run_solver():
            nonlocal result, timeout_occurred
            try:
                result = solve_rcpsp_optimizer(filename)
            except Exception as e:
                if not timeout_occurred:
                    raise e

        result = None
        solver_thread = threading.Thread(target=run_solver)
        solver_thread.daemon = True
        solver_thread.start()
        solver_thread.join(timeout=300)  # 300 seconds = 5 minutes

        solve_time = time.time() - start_time

        if solver_thread.is_alive():
            timeout_occurred = True
            print(f"Timeout for {problem_name} after 5 minutes")
            with open("result.csv", "a", newline='') as f:
                writer = csv.writer(f)
                writer.writerow([problem_name, "TIMEOUT", False, solve_time])
        elif result is not None:
            with open("result.csv", "a", newline='') as f:
                writer = csv.writer(f)
                writer.writerow([problem_name, result.get("makespan"), result.get("solved"), solve_time])
            print(f"Saved {problem_name}: makespan={result.get('makespan')}, time={solve_time:.2f}s")
        else:
            print(f"Error with {problem_name}")
            with open("result.csv", "a", newline='') as f:
                writer = csv.writer(f)
                writer.writerow([problem_name, "ERROR", False, solve_time])





                # benchmark=60
    # solve_one(45,8,60)
    # solve_one(45,9,60)
    # solve_one(46,6,60)
    # solve_one(46,7,60)


    # benchmark=90
    # solve_one(7,3,90)
    # solve_one(7,8,90)
    # solve_one(7,9,90)

    # for i in range(6,11):
    #     solve_one(8,i,90)

    # solve_one(9,1,90)
    # solve_one(9,8,90)
    # solve_one(9,9,90)

    # solve_one(10,3,90)
    # solve_one(10,5,90)
    # solve_one(10,9,90)
    # solve_one(10,10,90)

    # solve_one(11,1,90)
    # solve_one(11,2,90)

    # solve_one(15,9,90)
    # solve_one(15,10,90)


    # for i in range(4,11):
    #     solve_one(16,i,90)

    # for i in range(2,8):
    #     solve_one(25,i,90)
    # solve_one(25,10,90)

    # solve_one(26,1,90)
    # solve_one(26,3,90)
    # solve_one(26,6,90)
    # solve_one(26,7,90)

    # solve_one(29,4,90)

    # solve_one(32,10,90)

    # solve_one(33,6,90)
    # solve_one(33,7,90)
    # solve_one(33,9,90)

    # solve_one(34,3,90)
    # solve_one(34,4,90)
    # solve_one(34,6,90)
    # solve_one(34,7,90)

    # solve_one(35,2,90)
    # solve_one(35,4,90)
    # solve_one(35,6,90)

    # for i in range(1,8):
    #     solve_one(36,i,90)

    # for i in range(1,7):
    #     solve_one(37,i,90)

    # solve_one(38,4,90)
    # solve_one(38,5,90)
    # solve_one(38,8,90)
    # solve_one(38,9,90)

    # solve_one(39,5,90)

    # for i in range(8,11):
    #     solve_one(39,i,90)

    # solve_one(40,1,90)

    # solve_one(41,2,90)
    # solve_one(41,3,90)
    # solve_one(41,6,90)

    # solve_one(42,4,90)
    # solve_one(42,5,90)

    # solve_one(43,5,90)
    # solve_one(43,6,90)

    # solve_one(47,4,90)

    # for i in range(1,6):
    #     solve_one(48,i,90)



    # if(0):
    #     print("Starting batch solve...")
    #     tocomplete=33
    #     for group in range(tocomplete, tocomplete+1):  # Groups 1 to 48
    #         for example in range(9, 11):  # Examples 1 to 10

    #             #עופר תשנה ב2 שורות מתחת את ה90 ל120 בהרצה השניה
    #             filename = f"extract_problems/data/j{benchmark}.sm.tgz/j{benchmark}{group}_{example}.sm"
    #             problem_name = f"j{benchmark}{group}_{example}"

    #             print(f"Solving {problem_name}...")
    #             start_time = time.time()

    #             # Set 10-minute timeout using threading
    #             timeout_occurred = False

    #             def run_solver():
    #                 nonlocal result, timeout_occurred
    #                 try:
    #                     result = solve_rcpsp_optimizer(filename)
    #                 except Exception as e:
    #                     if not timeout_occurred:
    #                         raise e

    #             result = None
    #             solver_thread = threading.Thread(target=run_solver)
    #             solver_thread.daemon = True
    #             solver_thread.start()
    #             solver_thread.join(timeout=300)  # 300 seconds = 5 minutes

    #             solve_time = time.time() - start_time

    #             if solver_thread.is_alive():
    #                 timeout_occurred = True
    #                 print(f"Timeout for {problem_name} after 5 minutes")
    #                 with open("result.csv", "a", newline='') as f:
    #                     writer = csv.writer(f)
    #                     writer.writerow([problem_name, "TIMEOUT", False, solve_time])
    #             elif result is not None:
    #                 with open("result.csv", "a", newline='') as f:
    #                     writer = csv.writer(f)
    #                     writer.writerow([problem_name, result.get("makespan"), result.get("solved"), solve_time])
    #                 print(f"Saved {problem_name}: makespan={result.get('makespan')}, time={solve_time:.2f}s")
    #             else:
    #                 print(f"Error with {problem_name}")
    #                 with open("result.csv", "a", newline='') as f:
    #                     writer = csv.writer(f)
    #                     writer.writerow([problem_name, "ERROR", False, solve_time])

    #     print("All problems completed!")




benchmark = 30
solver = "cbc"
# ========== MAIN BENCHMARKING LOOP ==========
benchmark = 30
solver = "cbc"

# Generate a unique random string for this specific benchmark run
run_id = uuid.uuid4().hex[:6]
csv_filename = f"result_{solver}_j{benchmark}_{run_id}.csv"
print(f"Starting benchmark. Logging to: {csv_filename}")

for group in range(1, 2):
    for example in range(1, 11):
        filename = f"extract_problems/data/j{benchmark}.sm.tgz/j{benchmark}{group}_{example}.sm"
        problem_name = f"j{benchmark}{group}_{example}"
        print(f"Solving {problem_name}...")

        try:
            result = solve_rcpsp_optimizer(filename, solver)
            solve_time = result.get("solve_time")
            status = result.get("status")
            makespan = result.get("makespan")

            # Log to the dynamically named CSV
            with open(csv_filename, "a", newline='') as f:
                writer = csv.writer(f)
                writer.writerow([problem_name, makespan, status, solve_time])

            # Format time to 2 decimal places for cleaner terminal output
            if solve_time is not None:
                print(f"Saved {problem_name}: {status}, makespan={makespan}, time={solve_time:.2f}s")
            else:
                print(f"Saved {problem_name}: {status}, makespan={makespan}, time=Unknown")

        except Exception as e:
            error_msg = str(e).lower()
            if "aborted" in error_msg or "timeout" in error_msg:
                status = "TIMEOUT"
                makespan = -1

                with open(csv_filename, "a", newline='') as f:
                    writer = csv.writer(f)
                    writer.writerow([problem_name, makespan, status, 300])
                print(f"Saved {problem_name}: {status}, makespan={makespan}, time=300")
            else:
                print(f"Error solving {problem_name}: {e}")
# for group in range(1, 2):
#     for example in range(1, 11):
#         filename = f"extract_problems/data/j{benchmark}.sm.tgz/j{benchmark}{group}_{example}.sm"
#         problem_name = f"j{benchmark}{group}_{example}"
#         print(f"Solving {problem_name}...")

#         try:
#             # The timing is now INSIDE solve_rcpsp_optimizer
#             result = solve_rcpsp_optimizer(filename)
#             solve_time = result.get("solve_time")  # Get time from result

#             status = result.get("status")
#             makespan = result.get("makespan")
#             solved = result.get("solved")

#             # Log to CSV with dynamic filename
#             csv_filename = f"result_{solver}_j{benchmark}_new.csv"
#             with open(csv_filename, "a", newline='') as f:
#                 writer = csv.writer(f)
#                 writer.writerow([problem_name, makespan, status, solve_time])

#             print(f"Saved {problem_name}: {status}, makespan={makespan}, time={solve_time:.2f}s")

#         except Exception as e:
#             error_msg = str(e).lower()
#             if "aborted" in error_msg or "timeout" in error_msg:
#                 status = "TIMEOUT"
#                 makespan = -1

#                 csv_filename = f"result_{solver}_j{benchmark}_new.csv"
#                 with open(csv_filename, "a", newline='') as f:
#                     writer = csv.writer(f)
#                     writer.writerow([problem_name, makespan, status, 300])
#                 print(f"Saved {problem_name}: {status}, makespan={makespan}, time=300")
#             else:
#                 print(f"Error solving {problem_name}: {e}")
# print("Starting solver...")
#result = solve_rcpsp_optimizer("extract_problems/data/j30.sm.tgz/j301_1.sm")
# #print("Result:", result)
# result = solve_rcpsp_optimizer("extract_problems/data/j30.sm.tgz/j301_4.sm")
# with open("result.csv", "a", newline='') as f:
#     writer = csv.writer(f)
#     writer.writerow(["j301_4", result.get("makespan"), result.get("solved")])
# print("Saved to CSV")

# print("Starting batch solve...")
# for group in range(1, 49):  # Groups 1 to 48
#     for example in range(1, 11):  # Examples 1 to 10
#         filename = f"extract_problems/data/j30.sm.tgz/j30{group}_{example}.sm"
#         problem_name = f"j30{group}_{example}"

#         print(f"Solving {problem_name}...")
#         try:
#             result = solve_rcpsp_optimizer(filename)
#             with open("result.csv", "a", newline='') as f:
#                 writer = csv.writer(f)
#                 writer.writerow([problem_name, result.get("makespan"), result.get("solved")])
#             print(f"Saved {problem_name}: makespan={result.get('makespan')}")
#         except Exception as e:
#             print(f"Error with {problem_name}: {e}")
#             with open("result.csv", "a", newline='') as f:
#                 writer = csv.writer(f)
#                 writer.writerow([problem_name, "ERROR", False])

# print("All problems completed!")


# print("Starting batch solve...")
# for group in range(1, 49):  # Groups 1 to 48
#     for example in range(1, 11):  # Examples 1 to 10
#         filename = f"extract_problems/data/j30.sm.tgz/j30{group}_{example}.sm"
#         problem_name = f"j30{group}_{example}"

#         print(f"Solving {problem_name}...")
#         start_time = time.time()
#         try:
#             result = solve_rcpsp_optimizer(filename)
#             solve_time = time.time() - start_time
#             with open("result.csv", "a", newline='') as f:
#                 writer = csv.writer(f)
#                 writer.writerow([problem_name, result.get("makespan"), result.get("solved"), solve_time])
#             print(f"Saved {problem_name}: makespan={result.get('makespan')}, time={solve_time:.2f}s")
#         except Exception as e:
#             solve_time = time.time() - start_time
#             print(f"Error with {problem_name}: {e}")
#             with open("result.csv", "a", newline='') as f:
#                 writer = csv.writer(f)
#                 writer.writerow([problem_name, "ERROR", False, solve_time])

# print("All problems completed!")


# result = solve_rcpsp_optimizer("extract_problems/data/j30.sm.tgz/j3021_4.sm")
# with open("results1111111111111111111111111111.csv", "a", newline='') as f:
#     writer = csv.writer(f)
#     writer.writerow(["j301_1", result.get("makespan"), result.get("solved")])


# print(solve_rcpsp_optimizer("extract_problems/data/j30.sm.tgz/j301_1.sm"))
#print(solve_rcpsp_optimizer("extract_problems/data/j30.sm.tgz/j301_10.sm"))
#print(solve_rcpsp_optimizer("extract_problems\data\j30.sm.tgz\j301_10.sm"))


# print(solve_file_problem(path="extract_problems\data\j30.sm.tgz\j3016_9.sm",timed_transition=True,logging=True,))


# print(solve_file_problem_math(path="extract_problems\data\j30.sm.tgz\j3023_1.sm",timed_transition=False,logging=True,))
#solve_rcpsp_optimizer(path="extract_problems\data\j30.sm.tgz\j301_1.sm")


if __name__ == "__main__":
    main()
