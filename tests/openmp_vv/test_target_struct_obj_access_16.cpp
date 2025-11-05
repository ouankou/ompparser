#pragma omp target device(defaultDevice) map(tofrom: Errors, StrPtr) map(to: RefStr, RefId, RefAge, StrSz)
