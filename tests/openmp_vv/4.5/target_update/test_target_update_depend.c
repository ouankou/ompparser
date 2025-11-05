#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(alloc: h_array[0:1000], in_1[0:1000], in_2[0:1000]) depend(out: h_array, in_1, in_2)
#pragma omp task depend(out: in_1) shared(in_1)
#pragma omp task depend(out: in_2) shared(in_2)
#pragma omp target update depend(in: in_1, in_2) depend(out: in_1, in_2) to(in_1[0:1000], in_2[0:1000])
#pragma omp task shared (isHost, h_array, in_1, in_2) depend(inout: h_array) depend(in: in_1) depend(in: in_2)
#pragma omp target map(tofrom: isHost) map(alloc: in_1[0:1000]) map(alloc: in_2[0:1000]) map(alloc: h_array[0:1000])
#pragma omp target update depend(inout: h_array) from(h_array[0:1000])
#pragma omp task depend(in: h_array) shared(sum, h_array)
#pragma omp taskwait
#pragma omp target exit data map(delete: h_array[0:1000], in_1[0:1000], in_2[0:1000])
#pragma omp target enter data map(alloc: h_array[0:1000], in_1[0:1000], in_2[0:1000]) depend(out: h_array, in_1, in_2)
#pragma omp task depend(out: in_1) shared(in_1)
#pragma omp task depend(out: in_2) shared(in_2)
#pragma omp target update depend(inout: in_1, in_2) to(in_1[0:1000], in_2[0:1000])
#pragma omp target depend(inout: h_array) depend(in: in_1) depend(in: in_2) map(tofrom: isHost) map(alloc: in_1[0:1000]) map(alloc: in_2[0:1000]) map(alloc: h_array[0:1000])
#pragma omp target update depend(inout: h_array) from(h_array[0:1000])
#pragma omp task depend(in: h_array) shared(sum, h_array)
#pragma omp taskwait
#pragma omp target exit data map(delete: h_array[0:1000], in_1[0:1000], in_2[0:1000])
#pragma omp target map (from: _ompvv_isOffloadingOn)
