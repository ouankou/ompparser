#pragma omp target teams loop defaultmap(alloc) map(to: device_data[0:1024])
