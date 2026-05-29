#pragma omp interop init(targetsync: obj) device(device) depend(inout: A[0:1024])
#pragma omp target depend(inout: A[0:1024]) nowait map(tofrom: A[0:1024]) device(device)
#pragma omp atomic
#pragma omp interop destroy(obj) depend(out: A[0:1024])
#pragma omp target map (from: _ompvv_isOffloadingOn)
