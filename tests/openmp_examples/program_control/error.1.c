#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp metadirective when(implementation={vendor(gnu)}: nothing ) otherwise(error at(compilation) severity(fatal) message("GNU compiler required."))
#pragma omp error at(execution) severity(fatal) message("3 or more procs required.")
#pragma omp parallel master
#pragma omp error at(compilation) severity(warning) message("Notice: master is deprecated.")
#pragma omp error at(execution) severity(warning) message("Notice: masked used next release.")
