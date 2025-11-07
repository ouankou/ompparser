#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(to: B[:N], C[:N]) map(tofrom: sum0, sum1)
#pragma omp teams num_teams(2)
#pragma omp parallel for reduction(+:sum0)
#pragma omp parallel for reduction(+:sum1)
