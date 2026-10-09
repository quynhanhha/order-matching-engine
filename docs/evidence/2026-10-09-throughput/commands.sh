git rev-parse HEAD 
git status --porcelain=v1 
git diff --binary HEAD 
git -C /Users/quynhanhha/projects/c++/order-matching-engine/build-release/_deps/benchmark-src rev-parse HEAD 
git -C /Users/quynhanhha/projects/c++/order-matching-engine/build-release/_deps/benchmark-src describe --tags --always 
git -C /Users/quynhanhha/projects/c++/order-matching-engine/build-release/_deps/benchmark-src status --porcelain=v1 
git -C /Users/quynhanhha/projects/c++/order-matching-engine/build/_deps/googletest-src rev-parse HEAD 
git -C /Users/quynhanhha/projects/c++/order-matching-engine/build/_deps/googletest-src describe --tags --always 
git -C /Users/quynhanhha/projects/c++/order-matching-engine/build/_deps/googletest-src status --porcelain=v1 
python3 - /Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired 
date -u +%Y-%m-%dT%H:%M:%SZ 
uname -a 
/usr/bin/clang++ --version 
cmake --version 
sw_vers 
sysctl -n hw.model 
sysctl -n machdep.cpu.brand_string 
sysctl -n hw.memsize 
sysctl -n hw.ncpu 
sysctl -n hw.physicalcpu 
sysctl -n hw.logicalcpu 
pmset -g batt 
pmset -g custom 
pmset -g therm 
cmake -S /Users/quynhanhha/projects/c++/order-matching-engine -B /Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired/build-sanitizers -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DBUILD_TESTS=ON -DBUILD_BENCHMARKS=OFF -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/Users/quynhanhha/projects/c++/order-matching-engine/build/_deps/googletest-src 
cmake --build /Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired/build-sanitizers --parallel 2 
env ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir /Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired/build-sanitizers --output-on-failure 
env PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p test_throughput_summary.py 
cmake -S /Users/quynhanhha/projects/c++/order-matching-engine -B /Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired/build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DBUILD_TESTS=OFF -DBUILD_BENCHMARKS=ON -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DFETCHCONTENT_SOURCE_DIR_BENCHMARK=/Users/quynhanhha/projects/c++/order-matching-engine/build-release/_deps/benchmark-src 
cmake --build /Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired/build-release --target order_book_bench --parallel 2 
cp /Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired/build-release/compile_commands.json /Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired/compile_commands.json 
python3 - /Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired/compile_commands.json 
shasum -a 256 /Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired/build-release/order_book_bench 
/Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired/build-release/order_book_bench --benchmark_list_tests=true 
/Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired/build-release/order_book_bench --benchmark_filter=\^\(BM_AddOnly_Resting\|BM_MatchOneToOne\)/\[0-9\]+/real_time\$ --benchmark_min_time=1s --benchmark_min_warmup_time=1 --benchmark_repetitions=5 --benchmark_enable_random_interleaving=true --benchmark_out=/Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired/session-1.json --benchmark_out_format=json 
/Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired/build-release/order_book_bench --benchmark_filter=\^\(BM_AddOnly_Resting\|BM_MatchOneToOne\)/\[0-9\]+/real_time\$ --benchmark_min_time=1s --benchmark_min_warmup_time=1 --benchmark_repetitions=5 --benchmark_enable_random_interleaving=true --benchmark_out=/Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired/session-2.json --benchmark_out_format=json 
date -u +%Y-%m-%dT%H:%M:%SZ 
uname -a 
/usr/bin/clang++ --version 
cmake --version 
sw_vers 
sysctl -n hw.model 
sysctl -n machdep.cpu.brand_string 
sysctl -n hw.memsize 
sysctl -n hw.ncpu 
sysctl -n hw.physicalcpu 
sysctl -n hw.logicalcpu 
pmset -g batt 
pmset -g custom 
pmset -g therm 
git status --porcelain=v1 
python3 - /Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired 
python3 scripts/summarize_throughput.py /Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired 
