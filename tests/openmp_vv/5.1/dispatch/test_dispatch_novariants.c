#pragma omp declare variant(add_two) match(construct={dispatch})
#pragma omp dispatch novariants(novariant_arg)
#pragma omp dispatch novariants(novariant_arg)
#pragma omp target map (from: _ompvv_isOffloadingOn)
