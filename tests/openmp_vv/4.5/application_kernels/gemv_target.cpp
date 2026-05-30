#pragma omp target map(to:A[:n*n], V[:n]) map(from:Vout[:n])
#pragma omp target map (from: _ompvv_isOffloadingOn)
