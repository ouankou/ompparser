#pragma omp target map(from: host_data) is_device_ptr(dev_ptr)
