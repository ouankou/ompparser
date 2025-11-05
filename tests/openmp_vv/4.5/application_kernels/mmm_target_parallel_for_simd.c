#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map(to: a[0:DimA], b[0:DimB]) map(from: c[0:DimC])
#pragma omp teams distribute parallel for simd collapse(2) private(k)
