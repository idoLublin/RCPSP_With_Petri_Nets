include "cumulative.mzn";

int: n_tasks;
int: n_res;
array[1..n_res] of int: rc;
array[1..n_tasks] of int: dur;
array[1..n_tasks, 1..n_res] of int: rreq;
int: n_succs;
array[1..n_succs, 1..2] of int: suc;

int: horizon = sum(dur);

array[1..n_tasks] of var 0..horizon: s;

constraint s[1] = 0;

constraint forall(i in 1..n_succs)(
  s[suc[i,1]] + dur[suc[i,1]] <= s[suc[i,2]]
);

constraint forall(r in 1..n_res)(
  cumulative(s, dur, [rreq[i,r] | i in 1..n_tasks], rc[r])
);

solve minimize s[n_tasks];

output ["makespan = \(s[n_tasks]);\n"];
