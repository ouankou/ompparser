#pragma omp target defaultmap(none) map(tofrom: scalar, A, new_struct, ptr)
