#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp teams num_teams(nteams_required) thread_limit(max_thrds) private(tm_id)
#pragma omp parallel
#pragma omp for
#pragma omp for simd simdlen(8)
#pragma omp parallel
#pragma omp for
#pragma omp for simd simdlen(4)
