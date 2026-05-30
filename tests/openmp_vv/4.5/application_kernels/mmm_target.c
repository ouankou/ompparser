#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map(to: a[0:DimA],b[0:DimB]) map(from: c[0:DimC])
