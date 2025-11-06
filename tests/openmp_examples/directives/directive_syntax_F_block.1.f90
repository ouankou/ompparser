!$omp     parallel num_threads(NT)
!$omp         do schedule(static,chunks)
