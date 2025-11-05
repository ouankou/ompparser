#pragma omp target defaultmap(none) map(to: scalar, A, new_struct, ptr)
