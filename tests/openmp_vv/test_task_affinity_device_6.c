#pragma omp target defaultmap(none) is_device_ptr(A) map(tofrom: B[0:1024])
