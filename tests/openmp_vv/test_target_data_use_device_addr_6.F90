!$omp     target data map(to: device_data) map(from: device_out) use_device_addr(device_data, device_out)
