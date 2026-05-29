#pragma omp target map(from: compute_array) map(asclr)
#pragma omp target map(new_scalar)
#pragma omp target map (from: _ompvv_isOffloadingOn)
