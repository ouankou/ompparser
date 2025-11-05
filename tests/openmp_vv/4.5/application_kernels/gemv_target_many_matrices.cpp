#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute map(to:A[:n*n], V[:n]) map(from:Vout[:n])
#pragma omp target enter data map(to:ptr[:n])
#pragma omp target exit data map(delete:ptr[:n])
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp parallel for
#pragma omp target update from(Vout[:4096])
