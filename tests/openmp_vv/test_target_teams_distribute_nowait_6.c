#pragma omp target teams distribute map(alloc: work_storage[i][0:1024], ticket[0:1]) nowait
