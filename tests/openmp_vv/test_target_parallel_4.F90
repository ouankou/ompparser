!$omp          target parallel num_threads(8                       ) map(from: summation, thread_id)
