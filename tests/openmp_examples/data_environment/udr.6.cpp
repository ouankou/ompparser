#pragma omp declare reduction( + : std::vector<int> ) combiner( std::transform (omp_out.begin(), omp_out.end(), omp_in.begin(), omp_in.end(),std::plus<int>()) )
#pragma omp declare reduction( merge : std::vector<int> ) combiner( omp_out.insert(omp_out.end(), omp_in.begin(), omp_in.end()) )
#pragma omp declare reduction( merge : std::list<int> ) combiner( omp_out.merge(omp_in) )
