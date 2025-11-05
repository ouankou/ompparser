!$omp          target teams distribute parallel do if(parallel: attempt.gt. 70) map(tofrom: a, warning) num_threads(8)
