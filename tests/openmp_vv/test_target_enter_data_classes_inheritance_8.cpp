#pragma omp target defaultmap(tofrom: scalar) map(from: b_array[0:n])
