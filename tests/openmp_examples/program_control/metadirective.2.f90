!$omp     target device(idev)
!$omp     begin metadirective when(implementation={vendor(nvidia)}, device={arch("kepler")}: teams num_teams(512) thread_limit(32)) when(implementation={vendor(amd)}, device={arch("fiji")}: teams num_teams(512) thread_limit(64)) otherwise(teams)
!$omp     distribute parallel do
!$omp     end metadirective
!$omp     end target
