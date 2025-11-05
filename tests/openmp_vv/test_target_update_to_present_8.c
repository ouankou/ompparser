#pragma omp target map(tofrom: errors) defaultmap(none) map(to: scalar_var, A, new_struct)
