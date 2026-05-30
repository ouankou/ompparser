#pragma omp declare target local(local_var)
#pragma omp target
#pragma omp target map(tofrom: device_value)
#pragma omp target map (from: _ompvv_isOffloadingOn)
