#pragma omp target teams distribute is_device_ptr(c) map(tofrom: a[0:1024]) map(to: b[0:1024])
