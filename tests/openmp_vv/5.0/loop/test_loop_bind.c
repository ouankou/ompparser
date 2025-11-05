#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp teams num_teams(8) thread_limit(8)
#pragma omp loop bind(teams)
#pragma omp parallel if(0)
#pragma omp parallel num_threads(8)
#pragma omp loop bind(parallel)
#pragma omp teams num_teams(8) thread_limit(8)
#pragma omp loop bind(thread)
#pragma omp parallel if(0)
#pragma omp parallel shared(result) num_threads(8)
#pragma omp loop bind(thread)
