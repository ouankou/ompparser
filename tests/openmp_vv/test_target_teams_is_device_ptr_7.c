#pragma omp target teams map(from: host_data[0:1024]) is_device_ptr(dev_ptr)
