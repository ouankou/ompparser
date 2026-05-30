#pragma omp declare target link(aint)
#pragma omp target map(from: x) map(to:y, z, aint)
#pragma omp target map (from: _ompvv_isOffloadingOn)
