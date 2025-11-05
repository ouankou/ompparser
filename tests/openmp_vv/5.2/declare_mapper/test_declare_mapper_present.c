#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare mapper(default:myvec_t v) map(always, present, from: v, v.len, v.data[0:v.len])
#pragma omp target enter data map(to: s.len, s.data[0:s.len])
#pragma omp target
#pragma omp target exit data map(delete: s.len, s.data[0:s.len])
#pragma omp target map (from: _ompvv_isOffloadingOn)
