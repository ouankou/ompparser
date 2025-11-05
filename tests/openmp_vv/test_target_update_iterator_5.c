#pragma omp target map(to: new_struct, new_struct.data[:1024]) map(tofrom: A[:1024])
