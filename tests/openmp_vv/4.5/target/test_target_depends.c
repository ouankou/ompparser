#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target depend(out: dep_1) map(tofrom: dep_1[0:1000])
#pragma omp target depend(out: dep_2) map(tofrom: dep_2[0:1000])
#pragma omp task depend(inout: dep_1) depend(inout: dep_2) shared(dep_1, dep_2)
#pragma omp target depend(inout: dep_1) depend(inout: dep_2) map(tofrom: dep_1[0:1000]) map(tofrom: dep_2[0:1000])
#pragma omp target depend(in: dep_1) depend(in: dep_2) map(tofrom: dep_1[0:1000]) map(tofrom: dep_2[0:1000])
#pragma omp taskwait
#pragma omp target map (from: _ompvv_isOffloadingOn)
