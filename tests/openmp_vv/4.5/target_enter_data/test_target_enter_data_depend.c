#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp task depend(out: in_1) shared(in_1)
#pragma omp task depend(out: in_2) shared(in_2)
#pragma omp target enter data map(alloc: h_array[0:1000]) map(to: in_1[0:1000]) map(to: in_2[0:1000]) depend(out: h_array) depend(in: in_1) depend(in: in_2)
#pragma omp task shared (isHost, h_array, in_1, in_2) depend(inout: h_array) depend(in: in_1) depend(in: in_2)
#pragma omp target map(tofrom: isHost) map(alloc: in_1[0:1000]) map(alloc: in_2[0:1000]) map(alloc: h_array[0:1000])
#pragma omp task shared (h_array, h_array_copy) depend(in: h_array) depend(out: h_array_copy)
#pragma omp target map(alloc: h_array[0:1000]) map(from: h_array_copy[0:1000])
#pragma omp task depend(in: h_array_copy) shared(sum, h_array_copy)
#pragma omp taskwait
#pragma omp target exit data map(delete: h_array[0:1000], in_1[0:1000], in_2[0:1000])
#pragma omp target enter data map(alloc: h_array[0:1000]) depend(out: h_array)
#pragma omp target enter data map(to: val) depend(out: val)
#pragma omp target map(tofrom: isHost) map(alloc: h_array[0:1000]) depend(inout: h_array) depend(in: val)
#pragma omp target map(alloc: h_array[0:1000]) map(from: h_array_copy[0:1000]) depend(in: h_array) depend(out: h_array_copy)
#pragma omp taskwait
#pragma omp target exit data map(delete: h_array[0:1000], val)
#pragma omp target map (from: _ompvv_isOffloadingOn)
