#pragma omp target map(tofrom: arrayReverse[:1024])
#pragma omp reverse
#pragma omp target map (from: _ompvv_isOffloadingOn)
