#pragma omp unroll partial(4)
#pragma omp target map (from: _ompvv_isOffloadingOn)
