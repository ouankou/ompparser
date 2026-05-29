#pragma omp target parallel for induction(step(step_var), *: induction_var) map(tofrom: arr[:8])
#pragma omp target map (from: _ompvv_isOffloadingOn)
