#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel shared(x) num_threads(8)
#pragma omp single
#pragma omp task shared(x) transparent
#pragma omp atomic
#pragma omp task shared(x) transparent(omp_not_impex)
#pragma omp atomic
#pragma omp task shared(x) transparent(omp_import)
#pragma omp atomic
#pragma omp task shared(x) transparent(omp_export)
#pragma omp atomic
#pragma omp task shared(x) transparent(omp_impex)
#pragma omp atomic
