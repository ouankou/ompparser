#pragma omp target map(from:second_scalar_device_addr, second_arr_device_addr) has_device_addr(x, arr)
