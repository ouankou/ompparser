#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel num_threads(8)
#pragma omp single
#pragma omp task depend(iterator(it = 0 : size), in : ptr[cols[pos + it]]) depend(out : ptr[i])
#pragma omp critical
#pragma omp target map (from: _ompvv_isOffloadingOn)
