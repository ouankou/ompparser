#pragma omp target map(tofrom: data1, data2)
#pragma omp parallel for collapse(2)
#pragma omp target map (from: _ompvv_isOffloadingOn)
