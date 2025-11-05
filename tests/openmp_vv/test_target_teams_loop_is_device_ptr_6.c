#pragma omp target teams loop is_device_ptr(a_d) map(from: a_h[0:1024])
