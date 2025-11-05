#pragma omp target thread_limit(8) map(to : count1, count2) map(a) map(to : b) map(tofrom : num_threads)
