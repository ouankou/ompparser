#pragma omp target parallel for if(target: N>1000000) if(parallel: N>1000) map(to: v1[0:N], v2[:N]) map(from: p[0:N])
