!$omp      declare variant(foo_variant1) match(user={condition(foo_sub)})
!$omp      declare variant(foo_variant2) match(construct={dispatch},user={condition(foo_sub)})
!$omp   dispatch
!$omp   dispatch
!$omp   dispatch novariants(.true.)
!$omp   dispatch nocontext(.true.)
