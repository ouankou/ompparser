#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel num_threads(2) default(shared)
#pragma omp atomic hint(omp_sync_hint_uncontended)
#pragma omp parallel num_threads(8) default(shared)
#pragma omp atomic hint(omp_sync_hint_contended+omp_sync_hint_nonspeculative)
#pragma omp parallel for num_threads(8) default(shared)
#pragma omp atomic hint(omp_sync_hint_speculative)
#pragma omp atomic hint(omp_sync_hint_speculative)
