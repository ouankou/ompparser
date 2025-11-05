!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     parallel num_threads(2) default(shared)
!$omp        atomic hint(omp_sync_hint_uncontended)
!$omp        end atomic
!$omp     end parallel
!$omp     parallel num_threads(8                     ) default(shared)
!$omp        atomic hint(omp_sync_hint_contended+omp_sync_hint_nonspeculative)
!$omp        end atomic
!$omp     end parallel
!$omp     parallel do num_threads(8                     ) default(shared)
!$omp           atomic hint(omp_sync_hint_speculative)
!$omp           end atomic
!$omp        atomic hint(omp_sync_hint_speculative)
!$omp        end atomic
!$omp     end parallel do
