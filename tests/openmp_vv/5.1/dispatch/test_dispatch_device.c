#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare variant(add_dev) match(construct={dispatch})
#pragma omp parallel for
#pragma omp dispatch device(device_num)
