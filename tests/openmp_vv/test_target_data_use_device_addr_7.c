#pragma omp target map(to:device_data) map(tofrom: errors) map(from: host_data) is_device_ptr(dev_ptr)
