#pragma omp target device(defaultDevice) map(tofrom: Errors) map(to: RefStr, RefId, RefAge, StrSz)
