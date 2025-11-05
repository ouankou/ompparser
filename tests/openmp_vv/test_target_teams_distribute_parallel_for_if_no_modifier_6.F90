!$omp          target teams distribute parallel do if(attempt .gt. 70) map(tofrom: a, warning) num_threads(8)
