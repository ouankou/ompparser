#pragma omp task depend(iterator(it = 0 : size), in : ptr[cols[pos + it]]) depend(out : ptr[i])
