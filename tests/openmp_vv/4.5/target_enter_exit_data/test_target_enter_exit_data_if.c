#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target enter data if(size > 512) map(to: size) map(to: c[0:size])
#pragma omp target if(size > 512) map(to: a[0:size], b[0:size]) map(to: c[0:size])
#pragma omp target exit data if(size > 512) map(from: c[0:size])
