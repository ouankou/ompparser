!$omp    declare target
!$omp       parallel do private(i) num_threads(nthreads)
!$omp    target device(42) map(p, v1, v2)
!$omp    end target
