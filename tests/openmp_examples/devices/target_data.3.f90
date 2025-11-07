!$omp       target data map(Q)
!$omp         target map(tofrom: tmp)
!$omp            parallel do reduction(+:tmp)
!$omp         end target
!$omp         target
!$omp            parallel do
!$omp         end target
!$omp       end target data
