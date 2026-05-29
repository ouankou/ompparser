#pragma omp requires unified_shared_memory
#pragma omp target
#pragma omp target
#pragma omp target map (from: _ompvv_isOffloadingOn)
