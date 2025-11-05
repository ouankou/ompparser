!$omp     target parallel do map(pmax) shared(pmax) private(oldval, assume, newval)
