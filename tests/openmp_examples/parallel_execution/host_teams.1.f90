!$omp    teams num_teams(nteams_required) thread_limit(max_thrds) private(tm_id)
!$omp          parallel
!$omp             do
!$omp             do simd simdlen(8)
!$omp          end parallel
!$omp          parallel
!$omp             do
!$omp             do simd simdlen(4)
!$omp          end parallel
!$omp    end teams
