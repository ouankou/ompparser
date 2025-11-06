!$omp  metadirective when(implementation={vendor(gnu)}: nothing) otherwise(error at(compilation) severity(fatal) message("GNU compiler required."))
!$omp     error at(execution) severity(fatal) message("3 or more procs required.")
!$omp   parallel master
!$omp    error at(compilation) severity(warning) message("Notice: masteris deprecated.")
!$omp    error at(execution) severity(warning) message("Notice: masked to be used in next release.")
!$omp   end parallel master
