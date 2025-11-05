#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp requires reverse_offload
#pragma omp target map (from: _ompvv_isOffloadingOn) map(to: _ompvv_isSharedEnv)
#pragma omp target map(tofrom: errors2) map(to:a, which_device, is_shared_env)
#pragma omp target device(ancestor: 1) map(to: a) map(to: which_device)
#pragma omp target device(device_num: first_device_num) map(tofrom: b, target_device_num)
#pragma omp target map (from: _ompvv_isOffloadingOn)
