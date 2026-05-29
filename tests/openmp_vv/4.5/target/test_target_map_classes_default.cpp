#pragma omp target map(theArray[0:1000]) map(theSize)
#pragma omp declare target
#pragma omp end declare target
#pragma omp target map(tofrom: res)
#pragma omp target map(tofrom: value)
#pragma omp target map (from: _ompvv_isOffloadingOn)
