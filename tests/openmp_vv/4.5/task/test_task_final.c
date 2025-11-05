#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel
#pragma omp task final(isFinal)
#pragma omp parallel
#pragma omp task final(1)
#pragma omp task shared(id)
#pragma omp parallel
#pragma omp task final(1)
#pragma omp task shared(first_1stchld_task_thrdid, root_id)
#pragma omp task shared(root_id)
#pragma omp task shared(first_2ndchld_task_thrdid, root_id)
#pragma omp task shared(first_3rdchld_task_thrdid, root_id)
