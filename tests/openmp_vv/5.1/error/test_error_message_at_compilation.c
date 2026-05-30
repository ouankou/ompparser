#pragma omp parallel
#pragma omp single
#pragma omp error severity(warning) at(compilation) message("error message success")
#pragma omp target map (from: _ompvv_isOffloadingOn)
