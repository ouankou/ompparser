#pragma omp metadirective when( target_device={device_num(dev)}: target defaultmap(none) map(always,tofrom: A)) default( target defaultmap(none) map(to: A))
