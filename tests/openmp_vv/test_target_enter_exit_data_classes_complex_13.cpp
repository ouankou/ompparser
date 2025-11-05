#pragma omp target defaultmap(tofrom:scalar) map(from:copy_x[0:n], copy_y[0:n])
