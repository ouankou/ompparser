#pragma omp target teams distribute parallel for device(dev) map(tofrom: isHost)
