#pragma omp target map(from: first_scalar_device_addr, first_arr_device_addr) map(to: x, arr)
