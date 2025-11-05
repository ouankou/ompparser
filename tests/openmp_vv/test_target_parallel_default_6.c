#pragma omp target parallel for shared(count, IfTstPassed) default(none) map(tofrom: IfTstPassed)
