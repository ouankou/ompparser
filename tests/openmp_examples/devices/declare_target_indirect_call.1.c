#pragma omp declare target enter(fun1, fun2) indirect
#pragma omp target map(from: ret_val)
