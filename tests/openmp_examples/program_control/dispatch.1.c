#pragma omp declare variant(foo_variant1) match(user={condition(foo_sub)})
#pragma omp declare variant(foo_variant2) match(construct={dispatch},user={condition(foo_sub)})
#pragma omp dispatch
#pragma omp dispatch
#pragma omp dispatch novariants(1)
#pragma omp dispatch nocontext(1)
