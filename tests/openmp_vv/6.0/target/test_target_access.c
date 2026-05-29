#pragma omp target map(array[0:16]) device(first_dev)
#pragma omp target map(array[0:16]) device(second_dev)
#pragma omp target map (from: _ompvv_isOffloadingOn)
