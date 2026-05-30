#pragma omp declare variant(add_two) match(construct={dispatch})
#pragma omp dispatch nowait
#pragma omp barrier
#pragma omp target map (from: _ompvv_isOffloadingOn)
