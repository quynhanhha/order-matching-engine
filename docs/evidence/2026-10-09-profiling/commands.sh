bash samples/run_sample.sh <out> BM_AddOnly_Resting/10000/real_time add10000
bash samples/run_sample.sh <out> BM_MatchOneToOne/10000/real_time match10000 
bash samples/run_sample.sh <out> BM_MatchOneToOne/1000/real_time match1000 
bash samples/run_sample.sh <out> BM_AddOnly_Resting/1000/real_time add1000 
bash harness/build.sh
bash samples/run_sample.sh <out> {BM_MatchOneToOne/10000,BM_MatchOneToOne/1000,BM_AddOnly_Resting/1000}/real_time
python3 -I samples/summarize_samples.py ... (see samples/*.summary.txt)
harness/prof_alloc add 100 1 2 > alloc/add-100.txt
harness/prof_alloc add 1000 1 2 > alloc/add-1000.txt
harness/prof_alloc add 10000 1 2 > alloc/add-10000.txt
harness/prof_alloc match 100 1 2 > alloc/match-100.txt
harness/prof_alloc match 1000 1 2 > alloc/match-1000.txt
harness/prof_alloc match 10000 1 2 > alloc/match-10000.txt
harness/prof_timing add 100 2 5 > timing/add-100.txt
harness/prof_timing add 1000 2 5 > timing/add-1000.txt
harness/prof_timing add 10000 2 5 > timing/add-10000.txt
harness/prof_timing match 100 2 5 > timing/match-100.txt
harness/prof_timing match 1000 2 5 > timing/match-1000.txt
harness/prof_timing match 10000 2 5 > timing/match-10000.txt
harness/prof_rusage control-empty 1 2 5 > rusage/control-empty-1.txt
harness/prof_rusage control-chain 1000 2 5 > rusage/control-chain-1000.txt
harness/prof_rusage control-chain 100000 2 5 > rusage/control-chain-100000.txt
harness/prof_rusage control-chain 10000000 2 5 > rusage/control-chain-10000000.txt
harness/prof_rusage add 1000 2 5 > rusage/add-1000.txt
harness/prof_rusage add 10000 2 5 > rusage/add-10000.txt
harness/prof_rusage match 1000 2 5 > rusage/match-1000.txt
harness/prof_rusage match 10000 2 5 > rusage/match-10000.txt
python3 -I rusage/analyze.py . > rusage/analysis.txt
harness/prof_timing iso-new32 10000 2 5 > timing/iso-new32-10000.txt; harness/prof_rusage iso-new32 10000 2 5 > rusage/iso-new32-10000.txt
harness/prof_timing iso-delete32 10000 2 5 > timing/iso-delete32-10000.txt; harness/prof_rusage iso-delete32 10000 2 5 > rusage/iso-delete32-10000.txt
harness/prof_timing iso-umap-insert 10000 2 5 > timing/iso-umap-insert-10000.txt; harness/prof_rusage iso-umap-insert 10000 2 5 > rusage/iso-umap-insert-10000.txt
harness/prof_timing iso-umap-erase 10000 2 5 > timing/iso-umap-erase-10000.txt; harness/prof_rusage iso-umap-erase 10000 2 5 > rusage/iso-umap-erase-10000.txt
harness/prof_{timing,rusage} iso-delete32|iso-umap-insert 10000 2 5 > *.run2.txt
harness/prof_pcsamp add 10000 4 5 pcsamp/add-10000.txt > pcsamp/add-10000.run.txt
harness/prof_pcsamp match 10000 4 5 pcsamp/match-10000.txt > pcsamp/match-10000.run.txt
python3 -I pcsamp/regions.py pcsamp/add-10000.txt pcsamp/match-10000.txt > pcsamp/regions.txt
harness/prof_timing levels1 10000 2 5 > timing/levels1-10000.txt; harness/prof_rusage levels1 10000 2 5 > rusage/levels1-10000.txt
harness/prof_timing levels64 10000 2 5 > timing/levels64-10000.txt; harness/prof_rusage levels64 10000 2 5 > rusage/levels64-10000.txt
harness/prof_timing levels256 10000 2 5 > timing/levels256-10000.txt; harness/prof_rusage levels256 10000 2 5 > rusage/levels256-10000.txt
harness/prof_timing levels2048 10000 2 5 > timing/levels2048-10000.txt; harness/prof_rusage levels2048 10000 2 5 > rusage/levels2048-10000.txt
# harness rebuilt: levelsK now always uses the generic K-level generator (named add-glevelsK)
harness/prof_rusage levels1 10000 1 3 > rusage/scan-levels1.txt
harness/prof_rusage levels2 10000 1 3 > rusage/scan-levels2.txt
harness/prof_rusage levels3 10000 1 3 > rusage/scan-levels3.txt
harness/prof_rusage levels4 10000 1 3 > rusage/scan-levels4.txt
harness/prof_rusage levels6 10000 1 3 > rusage/scan-levels6.txt
harness/prof_rusage levels8 10000 1 3 > rusage/scan-levels8.txt
harness/prof_rusage levels10 10000 1 3 > rusage/scan-levels10.txt
harness/prof_rusage levels12 10000 1 3 > rusage/scan-levels12.txt
harness/prof_rusage levels16 10000 1 3 > rusage/scan-levels16.txt
harness/prof_rusage levels24 10000 1 3 > rusage/scan-levels24.txt
harness/prof_rusage levels32 10000 1 3 > rusage/scan-levels32.txt
harness/prof_rusage levels48 10000 1 3 > rusage/scan-levels48.txt
harness/prof_rusage levels64 10000 1 3 > rusage/scan-levels64.txt
harness/prof_rusage levels96 10000 1 3 > rusage/scan-levels96.txt
harness/prof_rusage levels128 10000 1 3 > rusage/scan-levels128.txt
harness/prof_rusage levels256 10000 1 3 > rusage/scan-levels256.txt
harness/prof_rusage levels512 10000 1 3 > rusage/scan-levels512.txt
harness/prof_rusage levels1024 10000 1 3 > rusage/scan-levels1024.txt
# harness rebuilt: GLEVELS_SEED env var selects generator seed for levelsK
GLEVELS_SEED=42 harness/prof_rusage levels4 10000 1 3 > rusage/seed-levels4-s42.txt
GLEVELS_SEED=7 harness/prof_rusage levels4 10000 1 3 > rusage/seed-levels4-s7.txt
GLEVELS_SEED=1234 harness/prof_rusage levels4 10000 1 3 > rusage/seed-levels4-s1234.txt
GLEVELS_SEED=99 harness/prof_rusage levels4 10000 1 3 > rusage/seed-levels4-s99.txt
GLEVELS_SEED=42 harness/prof_rusage levels6 10000 1 3 > rusage/seed-levels6-s42.txt
GLEVELS_SEED=7 harness/prof_rusage levels6 10000 1 3 > rusage/seed-levels6-s7.txt
GLEVELS_SEED=1234 harness/prof_rusage levels6 10000 1 3 > rusage/seed-levels6-s1234.txt
GLEVELS_SEED=99 harness/prof_rusage levels6 10000 1 3 > rusage/seed-levels6-s99.txt
GLEVELS_SEED=42 harness/prof_rusage levels10 10000 1 3 > rusage/seed-levels10-s42.txt
GLEVELS_SEED=7 harness/prof_rusage levels10 10000 1 3 > rusage/seed-levels10-s7.txt
GLEVELS_SEED=1234 harness/prof_rusage levels10 10000 1 3 > rusage/seed-levels10-s1234.txt
GLEVELS_SEED=99 harness/prof_rusage levels10 10000 1 3 > rusage/seed-levels10-s99.txt
GLEVELS_SEED=42 harness/prof_rusage levels16 10000 1 3 > rusage/seed-levels16-s42.txt
GLEVELS_SEED=7 harness/prof_rusage levels16 10000 1 3 > rusage/seed-levels16-s7.txt
GLEVELS_SEED=1234 harness/prof_rusage levels16 10000 1 3 > rusage/seed-levels16-s1234.txt
GLEVELS_SEED=99 harness/prof_rusage levels16 10000 1 3 > rusage/seed-levels16-s99.txt
GLEVELS_SEED=42 harness/prof_rusage levels48 10000 1 3 > rusage/seed-levels48-s42.txt
GLEVELS_SEED=7 harness/prof_rusage levels48 10000 1 3 > rusage/seed-levels48-s7.txt
GLEVELS_SEED=1234 harness/prof_rusage levels48 10000 1 3 > rusage/seed-levels48-s1234.txt
GLEVELS_SEED=99 harness/prof_rusage levels48 10000 1 3 > rusage/seed-levels48-s99.txt
GLEVELS_SEED=42 harness/prof_rusage levels64 10000 1 3 > rusage/seed-levels64-s42.txt
GLEVELS_SEED=7 harness/prof_rusage levels64 10000 1 3 > rusage/seed-levels64-s7.txt
GLEVELS_SEED=1234 harness/prof_rusage levels64 10000 1 3 > rusage/seed-levels64-s1234.txt
GLEVELS_SEED=99 harness/prof_rusage levels64 10000 1 3 > rusage/seed-levels64-s99.txt
harness/prof_timing cancel-fifo 10000 2 5 > timing/cancel-fifo-10000.txt; harness/prof_rusage cancel-fifo 10000 2 5 > rusage/cancel-fifo-10000.txt; harness/prof_alloc cancel-fifo 10000 1 2 > alloc/cancel-fifo-10000.txt
harness/prof_timing cancel-shuffled 10000 2 5 > timing/cancel-shuffled-10000.txt; harness/prof_rusage cancel-shuffled 10000 2 5 > rusage/cancel-shuffled-10000.txt; harness/prof_alloc cancel-shuffled 10000 1 2 > alloc/cancel-shuffled-10000.txt
harness/prof_timing partial 10000 2 5 > timing/partial-10000.txt; harness/prof_rusage partial 10000 2 5 > rusage/partial-10000.txt; harness/prof_alloc partial 10000 1 2 > alloc/partial-10000.txt
harness/prof_timing sweep1 2000 2 5 > timing/sweep1-2000.txt; harness/prof_rusage sweep1 2000 2 5 > rusage/sweep1-2000.txt; harness/prof_alloc sweep1 2000 1 2 > alloc/sweep1-2000.txt
harness/prof_timing sweep10 2000 2 5 > timing/sweep10-2000.txt; harness/prof_rusage sweep10 2000 2 5 > rusage/sweep10-2000.txt; harness/prof_alloc sweep10 2000 1 2 > alloc/sweep10-2000.txt
harness/prof_timing sweep100 2000 2 5 > timing/sweep100-2000.txt; harness/prof_rusage sweep100 2000 2 5 > rusage/sweep100-2000.txt; harness/prof_alloc sweep100 2000 1 2 > alloc/sweep100-2000.txt
harness/prof_pcsamp cancel-fifo 10000 4 5 pcsamp/cancel-fifo-10000.txt > pcsamp/cancel-fifo-10000.run.txt
harness/prof_pcsamp cancel-shuffled 10000 4 5 pcsamp/cancel-shuffled-10000.txt > pcsamp/cancel-shuffled-10000.run.txt
clang++ -std=c++20 -O1 -g -fsanitize=address,undefined (asserts ON) ... -o harness/prof_asan
harness/prof_asan <each workload> 0.2 1 > asan/*.txt (see asan/summary.txt)
harness/prof_timing add 100000 3 3 > timing/add-100000.txt; harness/prof_rusage add 100000 3 3 > rusage/add-100000.txt
harness/prof_timing add 1000000 3 3 > timing/add-1000000.txt; harness/prof_rusage add 1000000 3 3 > rusage/add-1000000.txt
harness/prof_timing cancel-fifo 100000 3 3 > timing/cancel-fifo-100000.txt; harness/prof_rusage cancel-fifo 100000 3 3 > rusage/cancel-fifo-100000.txt
harness/prof_timing cancel-shuffled 100000 3 3 > timing/cancel-shuffled-100000.txt; harness/prof_rusage cancel-shuffled 100000 3 3 > rusage/cancel-shuffled-100000.txt
harness/prof_timing cancel-fifo 1000000 3 3 > timing/cancel-fifo-1000000.txt; harness/prof_rusage cancel-fifo 1000000 3 3 > rusage/cancel-fifo-1000000.txt
harness/prof_timing cancel-shuffled 1000000 3 3 > timing/cancel-shuffled-1000000.txt; harness/prof_rusage cancel-shuffled 1000000 3 3 > rusage/cancel-shuffled-1000000.txt
harness/prof_timing match 100000 3 3 > timing/match-100000.txt; harness/prof_rusage match 100000 3 3 > rusage/match-100000.txt
harness/prof_timing match 1000000 3 3 > timing/match-1000000.txt; harness/prof_rusage match 1000000 3 3 > rusage/match-1000000.txt
clang++ -O0 -g -fsanitize=address,undefined dupid/dup_id_repro.cpp -> dupid/dup_id_asan; run > dupid/output-asan-debug.txt
