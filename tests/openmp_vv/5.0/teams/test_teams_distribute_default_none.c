#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp teams distribute num_teams(8) default(none) shared(a, b, c, d, num_teams) private(privatized)
#pragma omp teams distribute num_teams(8) default(none) shared(share, b, num_teams)
#pragma omp atomic update
