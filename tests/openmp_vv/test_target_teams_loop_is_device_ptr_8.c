#pragma omp target teams loop is_device_ptr(a_d1, a_d2) map(from: a_h[0:1024])
