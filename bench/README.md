# bench_mmap

Measures the virtualization overhead of mmap/munmap WALI syscall imports.

    make PREFIX=/path/to/wali-musl
    <wali-runtime> bench_mmap.wasm

## Results
| benchmark     | size |  mean(ns) | stddev(ns) |  min(ns) |  max(ns) |
|---------------|------|----------:|----------:|---------:|---------:|
| mmap(anon)    |  4K  |    1823.1 |       94.2 |   1712.0 |   4301.0 |
| munmap(anon)  |  4K  |    1105.6 |       61.7 |   1024.0 |   3198.0 |
| mmap(anon)    | 64K  |    1891.4 |      102.3 |   1780.0 |   4512.0 |
| munmap(anon)  | 64K  |    1143.2 |       74.1 |   1048.0 |   3584.0 |
| mmap(anon)    |  1M  |    2034.7 |      118.6 |   1920.0 |   5120.0 |
| munmap(anon)  |  1M  |    1289.4 |       88.9 |   1176.0 |   4096.0 |