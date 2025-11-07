#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel num_threads(n_procs) proc_bind(close)
#pragma omp parallel num_threads(n_sockets) private(socket_num) proc_bind(spread)
