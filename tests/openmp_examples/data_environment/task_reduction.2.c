#pragma omp parallel num_threads(M) reduction(task,+:x)
#pragma omp single
#pragma omp task in_reduction(+:x)
#pragma omp parallel for num_threads(M) reduction(task,+:x)
#pragma omp task in_reduction(+:x)
