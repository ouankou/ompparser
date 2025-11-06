#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target device(idev)
#pragma omp metadirective when( implementation={vendor(nvidia)}, device={arch("kepler")}: teams num_teams(512) thread_limit(32) ) when( implementation={vendor(amd)}, device={arch("fiji" )}: teams num_teams(512) thread_limit(64) ) otherwise( teams)
#pragma omp distribute parallel for
