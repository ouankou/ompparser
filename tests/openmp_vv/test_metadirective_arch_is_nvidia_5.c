#pragma omp metadirective when( device={arch("nvptx")}: teams distribute parallel for) default( parallel for)
