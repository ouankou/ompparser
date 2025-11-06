!$omp    metadirective when(user={condition(use_gpu)}: target teams distribute parallel do private(b) map(from:a(1:n))) otherwise(paralleldo)
!$omp    begin metadirective when(user={condition(run_parallel)}: parallel)
!$omp     metadirective when(construct={parallel}, user={condition(unbalanced)}: do schedule(guided) private(b)) when(construct={parallel}: do schedule(static))
!$omp    end metadirective
