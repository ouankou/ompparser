#pragma omp declare variant(fx1) match(construct={target})
#pragma omp declare variant(fx2) match(construct={teams,parallel,for})
#pragma omp declare variant(fx3) match(device={kind(gpu),isa(sm_70)})
#pragma omp declare variant(fx4) match(device={arch(nvptx),isa(sm_70)})
#pragma omp target teams distribute parallel for map(a[:4])
#pragma omp task
