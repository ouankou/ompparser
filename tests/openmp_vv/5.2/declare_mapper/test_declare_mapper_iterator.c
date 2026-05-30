#pragma omp declare mapper(myvec_t v) map(iterator(it = 0:v.len), tofrom: v.data[it])
#pragma omp target
#pragma omp target map (from: _ompvv_isOffloadingOn)
