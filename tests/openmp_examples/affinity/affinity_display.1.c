#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel num_threads(omp_get_num_procs())
#pragma omp parallel num_threads( omp_get_num_procs() )
#pragma omp parallel num_threads( omp_get_num_procs()/2 )
