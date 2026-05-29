#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target enter data map(to: A[:n])
#pragma omp target map(from: B[:n])
#pragma omp target exit data map(release: A[:n])
