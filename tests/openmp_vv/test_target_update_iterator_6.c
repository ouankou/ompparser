#pragma omp target update to(iterator(it = 0:1024): new_struct.data[it])
