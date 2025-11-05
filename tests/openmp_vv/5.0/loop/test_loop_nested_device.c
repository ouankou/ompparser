#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams num_teams(8) thread_limit(8) map(tofrom: x, y, z, num_teams)
#pragma omp loop
#pragma omp parallel if(0)
#pragma omp target parallel num_threads(8) map(tofrom: x, y, z, num_threads)
#pragma omp loop
