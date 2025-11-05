#pragma omp target data map(to: array_device[0:100]) use_device_ptr(array_device)
