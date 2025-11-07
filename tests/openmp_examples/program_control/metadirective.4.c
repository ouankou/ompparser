#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp metadirective when( user={condition(use_gpu)}: target teams distribute parallel for private(b) map(from:a[0:n]) ) otherwise( parallel for )
#pragma omp metadirective when(user={condition(run_parallel)}: parallel)
#pragma omp metadirective when( construct={parallel}, user={condition(unbalanced)}: for schedule(guided) private(b)) when( construct={parallel} : for schedule(static))
