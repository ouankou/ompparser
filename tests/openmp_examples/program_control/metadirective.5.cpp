#pragma omp metadirective when(user={condition(tasking)}: task shared(i))
#pragma omp metadirective when(user={condition(tasking)}: task shared(j))
#pragma omp metadirective when(user={condition(tasking)}: taskwait)
#pragma omp parallel
#pragma omp single
