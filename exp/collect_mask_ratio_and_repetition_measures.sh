#!/bin/bash
ulimit -v $((180*1024*1024))

# config = [corpus name] [idx type] [tau_l] [tau_u] [lambda]
configs=(
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

read_data() {
  local dir=$1

  while IFS= read -r line; do
    if [[ $line == *grammar_size:* ]]; then
      grammar_size="${line#*grammar_size: }"
    elif [[ $line == *number_of_phrases:* ]]; then
      number_of_phrases="${line#*number_of_phrases: }"
    elif [[ $line == *bwt_runs:* ]]; then
      bwt_runs="${line#*bwt_runs: }"
    elif [[ $line == *masked_ratio:* ]]; then
      masked_ratio="${line#*masked_ratio: }"
    fi
  done < "$dir"
}

reset_read_data() {
  grammar_size=""
  number_of_phrases=""
  bwt_runs=""
  masked_ratio=""
}

outputFile="[summary]masked_ratio_and_repetition_measures"
touch ${outputFile}
echo -e "corpus\ttau_l\ttau_u\tlambda\tg\tz\tr\tmasked_ratio" > "${outputFile}"

for config in "${configs[@]}"; do
  eval set -- $config
  corpora=($1)
  types=($2)
  tau_l=($3)
  tau_u=($4)
  lambda=($5)

  mfSubfolder="minimal_factors"

  for corpus in "${corpora[@]}"; do
    folder="results/${corpus}"

    for tau_l_value in "${tau_l[@]}"; do
      for tau_u_value in "${tau_u[@]}"; do
        for lambda_value in "${lambda[@]}"; do

          reset_read_data

          for type in "${types[@]}"; do
            subfolder=$(get_subfolder_name "$type")
            mf_dir="${folder}/${mfSubfolder}"
            check_directory "$mf_dir"

            idx_dir="${folder}/${subfolder}"
            check_directory "$idx_dir"

            log_path="${idx_dir}/${tau_l_value}_${tau_u_value}_${lambda_value}_log"
            
            read_data "$log_path"
          done

          echo -e "${corpus}\t${tau_l_value}\t${tau_u_value}\t${lambda_value}\t${grammar_size}\t${number_of_phrases}\t${bwt_runs}\t${masked_ratio}" >>  "$outputFile"
        
        done
      done
    done

    reset_read_data

    for type in "${types[@]}"; do
      subfolder=$(get_subfolder_name "$type")
      idx_dir="${folder}/${subfolder}"
      log_path="${idx_dir}/0_0_0_log"
      read_data "$log_path"
    done

    echo -e "${corpus}\t0\t0\t0\t${grammar_size}\t${number_of_phrases}\t${bwt_runs}\t${masked_ratio}" >>  "$outputFile"
  
  done
done