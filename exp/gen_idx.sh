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

check_file() {
  local file=$1
  if [ ! -f "$file" ]; then
    echo "❌ File $file does not exist. Breaking out."
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

  mf_folder="minimal_factors"
  exec_dir="../build"

  check_directory "$exec_dir"

  for corpus in "${corpora[@]}"; do
    folder="results/${corpus}"
    input_text_path="${folder}/rawText"

    for type in "${types[@]}"; do
      subfolder=$(get_subfolder_name "$type")

      mf_dir="${folder}/${mf_folder}"
      check_directory "$mf_dir"

      idx_dir="${folder}/${subfolder}"
      create_directory "$idx_dir"

      for tau_l_value in "${tau_l[@]}"; do
        for tau_u_value in "${tau_u[@]}"; do
          for lambda_value in "${lambda[@]}"; do
            mf_path="${mf_dir}/${tau_l_value}_${tau_u_value}_${lambda_value}_mf"
            idx_path="${idx_dir}/${tau_l_value}_${tau_u_value}_${lambda_value}_idx"
            log_path="${idx_dir}/${tau_l_value}_${tau_u_value}_${lambda_value}_log"

            check_file "$mf_path"
            
            rm tmp*

            echo "Running ./${exec_dir}/gen_index $mf_path $idx_path $type $log_path"
            
            ./${exec_dir}/gen_index "$mf_path" "$idx_path" "$type" "$log_path"
            
          done
        done
      done
    done
  done
done