!$omp    declare mapper(deep_copy: T :: s) map(s, s%ptr(:s%buf_size))
!$omp     begin metadirective when(user={condition(accessible)}: target)otherwise(target map(mapper(deep_copy),tofrom:s))
!$omp     end   metadirective
