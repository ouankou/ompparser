!$omp     parallel num_threads(2) default(shared)
!$omp        atomic hint(4)
!$omp        end atomic
!$omp     end parallel
!$omp     parallel num_threads(8                     ) default(shared)
!$omp        atomic hint(4132)
!$omp        end atomic
!$omp     end parallel
