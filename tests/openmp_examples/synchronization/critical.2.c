#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel shared(x, y) private(ix_next, iy_next)
#pragma omp critical (xaxis) hint(omp_sync_hint_contended)
#pragma omp critical (yaxis) hint(omp_sync_hint_contended)
