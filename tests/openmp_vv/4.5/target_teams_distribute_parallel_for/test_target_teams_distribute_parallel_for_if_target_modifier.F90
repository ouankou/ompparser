!$omp          target teams distribute parallel do if(target: attempt .gt. 70) map(tofrom: a) num_threads(8)
