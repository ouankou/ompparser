#pragma omp target map(tofrom: errors, called_add) is_device_ptr(arr)
