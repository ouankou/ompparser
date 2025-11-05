#pragma omp target update if (change_flag) to(b[:100])
