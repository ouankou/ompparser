!$omp     target map(from: cptr_scalar2, cptr_arr2) map(to: x, arr) has_device_addr(first_scalar_device_addr, first_arr_device_addr) map(errors)
