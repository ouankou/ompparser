!$omp       declare target
!$omp    assume no_openmp
!$omp    end assume
!$omp    target teams loop map(to: arr) map(from: arr_bang)
!$omp       assume no_parallelism
!$omp       end assume
