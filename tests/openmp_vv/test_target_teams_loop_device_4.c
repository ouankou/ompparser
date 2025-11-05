#pragma omp target teams loop map(tofrom: a[0:1024]) device(dev_num)
