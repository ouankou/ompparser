#pragma omp metadirective when( implementation={vendor(nvidia)}: teams num_teams(512) thread_limit(32) ) when( implementation={vendor(amd)}: teams num_teams(512) thread_limit(64) ) default (teams)
