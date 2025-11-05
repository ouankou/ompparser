#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel shared(x) num_threads(8)
#pragma omp single
#pragma omp task shared(x, errors) transparent(omp_export)
#pragma omp task shared(x, errors) depend(out: x)
#pragma omp task shared(x, errors) depend(in: x)
