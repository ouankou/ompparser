#pragma omp target map(tofrom:errors) map(to:hold_arr2) defaultmap(default:pointer)
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map (from: _ompvv_isOffloadingOn) map(to: _ompvv_isSharedEnv)
