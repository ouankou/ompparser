#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target is_device_ptr(mem_dev_cpy) device(t)
#pragma omp teams distribute parallel for
