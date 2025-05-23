# tau-lambda index

The **τλ-index** is designed for efficiently locating *rare patterns* in highly repetitive corpora.

A rare pattern `P` is defined by the following two conditions:
1. The length of `P` is less than λ, i.e., |P| < λ
2. The number of occurrences of `P` in the corpus `T` falls within the interval [τₗ, τᵤ]

## Dependencies

This implementation requires the [SDSL-lite library](https://github.com/simongog/sdsl-lite).  
You can install it with:

```bash
git clone https://github.com/simongog/sdsl-lite.git
cd sdsl-lite
./install.sh
```

## Compilation

We use **CMake** to build the project:

```bash
mkdir build
cd build
cmake ..
make
```

## Run

Follow the steps below to construct and query a τλ-index:

1. Generate minimal factors
```bash
./gen_mf [input_text_path] [output_mf_path] [τₗ] [τᵤ] [λ] [delimiter_symbol] (optional)
```

2. (Optional) Generate query patterns
```bash
./gen_pattern [input_mf_path] [output_patterns_path] [number_of_patterns]
```

3. Build the τλ-index
```bash
./gen_index [input_mf_path] [output_index_path] [index_type] [output_log_path](optional)
```
+ Supported index_type values:
    - 1 for r-index
    - 2 for LZ77
    - 3 for LPG

4. Perform locate queries 
```bash
./locate [input_index_path] [input_pattern_path] [output_result_path]
```

5. (Optional) Output internal index components 
If you want to examine the partitioned XBWT and masked self-index:
```bash
./serialize_partition [input_index_path] [output_xbwt_path] [output_masked_index_path]
```

## Reproduce Paper Experiments
To reproduce the experimental results from our paper, use the scripts in the exp/ folder:  
1. Generate minimal factors and patterns
```bash
./exp/gen_mf_and_pattern.sh
```

2. Build tge index
```bash
./exp/gen_idx.sh
```

3. Run locate queries 
```bash
./exp/locate.sh
```
Note: Using τₗ = 0, τᵤ = 0, λ = 0 reproduces the baseline of the original self-index on the unmasked corpus.

4. Collecting experiment statistics
```bash
./exp/collect_serialized_space.sh
./exp/collect_mask_ratio_and_repetition_measures.sh
./exp/collect_query_time.sh
```