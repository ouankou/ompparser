#pragma omp requires reverse_offload
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map (from: _ompvv_isOffloadingOn) map(to: _ompvv_isSharedEnv)
#pragma omp target map(tofrom: errors2) map(to:A, is_shared_env)
#pragma omp target device(ancestor:1) map(always, to: A)
