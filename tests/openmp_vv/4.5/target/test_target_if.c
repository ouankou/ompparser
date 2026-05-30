#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target if(size > 512) map(to: size) map(tofrom: c[0:size]) map(to: a[0:size], b[0:size]) map(tofrom: isHost)
