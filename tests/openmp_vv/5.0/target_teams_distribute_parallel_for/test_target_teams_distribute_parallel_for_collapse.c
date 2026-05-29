#pragma omp target teams distribute parallel for collapse(4) map(tofrom: a) private(i,j,k,l)
#pragma omp target map (from: _ompvv_isOffloadingOn)
