#!/bin/bash
ulimit -v $((180*1024*1024))

# config = [corpus name] [idx type] [tau_l] [tau_u] [lambda]
configs=(
  "'english dblp dna_real influenza'    '1 2 3'   '0'   '0'                       '0'            "
  "english                              '1 2 3'   '0'   '1 2 3 4'                 '20 60 100 200'"
  "dblp                                 '1 2 3'   '0'   '1 2 3 4'                 '20 60 100 200'"
  "dna_real                             '1 2 3'   '0'   '20 40 60 80'             '20 60 100 200'"
  "influenza                            '1 2 3'   '0'   '780 1561 2341 3122'      '20 60 100 200'"
  "dna_pseudoreal                       '1 2 3'   '0'   '6291 12583 18874 25166'  '20 60 100 200'"
)

create_directory() {
  local dir=$1
  if [ ! -d "$dir" ]; then
    echo "Directory $dir does not exist. Creating..."
    mkdir -p "$dir"
  fi
}

check_directory() {
  local dir=$1
  if [ ! -d "$dir" ]; then
    echo "❌ Directory $dir does not exist. Breaking out."
    exit 1
  fi
}

get_subfolder_name() {
  local type=$1
  case "$type" in
    1) echo "r-index" ;;
    2) echo "LZ77" ;;
    3) echo "LPG" ;;
    *)
      echo "Error: invalid type '$type'" >&2
      exit 1
      ;;
  esac
}

for config in "${configs[@]}"; do
  eval set -- $config
  corpora=($1)
  types=($2)
  tau_l=($3)
  tau_u=($4)
  lambda=($5)

  mfSubfolder="minimal_factors"
  patternSubfolder="patterns"
  exec_dir="../build"

  check_directory "$exec_dir"

  for corpus in "${corpora[@]}"; do
    folder="results/${corpus}"
    input_text_path="${folder}/rawText"

    for type in "${types[@]}"; do
      subfolder=$(get_subfolder_name "$type")
      mf_dir="${folder}/${mfSubfolder}"
      check_directory "$mf_dir"

      idx_dir="${folder}/${subfolder}"
      check_directory "$idx_dir"

      pattern_dir="${folder}/${patternSubfolder}"
      check_directory "$pattern_dir"

      result_folder_with="${idx_dir}/result_with"
      create_directory "$result_folder_with"

      result_folder_without="${idx_dir}/result_without"
      create_directory "$result_folder_without"

      for tau_l_value in "${tau_l[@]}"; do
        for tau_u_value in "${tau_u[@]}"; do
          for lambda_value in "${lambda[@]}"; do
            idx_path="${idx_dir}/${tau_l_value}_${tau_u_value}_${lambda_value}_idx"
            idx_path_bsl="${idx_dir}/0_0_0_idx"
            pattern_path_with="${pattern_dir}/${tau_l_value}_${tau_u_value}_${lambda_value}_patterns_with"
            pattern_path_without="${pattern_dir}/${tau_l_value}_${tau_u_value}_${lambda_value}_patterns_without"
            result_path_with="${result_folder_with}/${tau_l_value}_${tau_u_value}_${lambda_value}"
            result_path_with_bsl="${result_folder_with}/${tau_l_value}_${tau_u_value}_${lambda_value}_bsl"
            result_path_without="${result_folder_without}/${tau_l_value}_${tau_u_value}_${lambda_value}"
            result_path_without_bsl="${result_folder_without}/${tau_l_value}_${tau_u_value}_${lambda_value}_bsl"
            massif_out_with="${result_folder_with}/${tau_l_value}_${tau_u_value}_${lambda_value}_massif"

            echo "Running ./${exec_dir}/locate $idx_path $pattern_path_with $result_path_with"
            
            touch "$result_path_with"
            touch "$result_path_with_bsl"
            touch "$result_path_without"
            touch "$result_path_without_bsl"

            /usr/bin/time -v -a -o "$result_path_with" ./${exec_dir}/locate "$idx_path" "$pattern_path_with" "$result_path_with"
            if [ ${subfolder} != "DCC" ]; then
                /usr/bin/time -v -a -o "$result_path_with_bsl" ./${exec_dir}/locate "$idx_path_bsl" "$pattern_path_with" "$result_path_with_bsl"
            fi

            /usr/bin/time -v -a -o "$result_path_without" ./${exec_dir}/locate "$idx_path" "$pattern_path_without" "$result_path_without"
            if [ ${subfolder} != "DCC" ]; then
              /usr/bin/time -v -a -o "$result_path_without_bsl" ./${exec_dir}/locate "$idx_path_bsl" "$pattern_path_without" "$result_path_without_bsl"
            fi
            
          done
        done
      done
    done
  done
done