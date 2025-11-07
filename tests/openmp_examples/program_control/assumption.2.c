#pragma omp assume no_openmp
#pragma omp target teams loop map(to: arr) map(from: arr_bang)
#pragma omp assume no_parallelism
