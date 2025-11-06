#pragma omp target map(to:v1,v2) map(from:v3) device(0)
#pragma omp metadirective when( device={arch("nvptx")}: teams loop) otherwise( parallel loop)
