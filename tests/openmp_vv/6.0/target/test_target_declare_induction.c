#pragma omp target parallel for induction(step(step_var), user_def_induc : induction_var) map(tofrom: arr[:8])
#pragma omp target map (from: _ompvv_isOffloadingOn)
