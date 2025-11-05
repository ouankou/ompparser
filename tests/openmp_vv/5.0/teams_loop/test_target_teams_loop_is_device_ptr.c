#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams loop is_device_ptr(a_d) map(from: a_h[0:1024])
#pragma omp target teams loop is_device_ptr(a_d)
#pragma omp target teams loop is_device_ptr(a_d) map(from: a_h[0:1024])
#pragma omp target teams loop is_device_ptr(a_d1, a_d2)
#pragma omp target teams loop is_device_ptr(a_d1, a_d2) map(from: a_h[0:1024])
