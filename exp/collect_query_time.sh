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

parse_result_file() {
  local file_path="$1"
  local line
  xwbt_time_us=""
  time_us=""
  occ=""

  while IFS= read -r line; do
    if [[ $line == time_xbwt\(us\):* ]]; then
      xwbt_time_us="${line#time_xbwt(us): }"
    elif [[ $line == time\(us\):* ]]; then
      time_us="${line#time(us): }"
    elif [[ $line == occ:* ]]; then
      occ="${line#occ: }"
    fi
  done < "$file_path"
}

outputFile="[summary]query_time_and_RSS_space"
touch ${outputFile}
echo -e "xbwt_time(us)\ttime(us)\tocc\ttext_length\ttype\tcorpus\twith_mf\ttau_l\ttau_u\tlambda\tmasked_ratio" > "${outputFile}"

for config in "${configs[@]}"; do
  eval set -- $config
  corpora=($1)
  types=($2)
  tau_l=($3)
  tau_u=($4)
  lambda=($5)

  for corpus in "${corpora[@]}"; do
    folder="results/${corpus}"
    input_text_path="${folder}/rawText"
    file_size=$(stat --format=%s ${input_text_path})

    for type in "${types[@]}"; do
      subfolder=$(get_subfolder_name "$type")

      idx_dir="${folder}/${subfolder}"
      check_directory "$idx_dir"

      result_folder_with="${idx_dir}/result_with"
      check_directory "$result_folder_with"

      result_folder_without="${idx_dir}/result_without"
      check_directory "$result_folder_without"

      for tau_l_value in "${tau_l[@]}"; do
        for tau_u_value in "${tau_u[@]}"; do
          for lambda_value in "${lambda[@]}"; do
            log_path="${idx_dir}/${tau_l_value}_${tau_u_value}_${lambda_value}_log"
            result_path_with="${result_folder_with}/${tau_l_value}_${tau_u_value}_${lambda_value}"
            result_path_with_bsl="${result_folder_with}/${tau_l_value}_${tau_u_value}_${lambda_value}_bsl"
            result_path_without="${result_folder_without}/${tau_l_value}_${tau_u_value}_${lambda_value}"
            result_path_without_bsl="${result_folder_without}/${tau_l_value}_${tau_u_value}_${lambda_value}_bsl"
            
            masked_ratio=""
            while IFS= read -r line; do
              if [[ $line == masked_ratio:* ]]; then
                  masked_ratio="${line#masked_ratio: }"
              fi
            done < "$log_path"
            
            xwbt_time_us=""
            time_us=""
            occ=""

            parse_result_file "$result_path_with"
            echo -e "${xwbt_time_us}\t${time_us}\t${occ}\t${file_size}\t${subfolder}\t${corpus}\twith\t${tau_l_value}\t${tau_u_value}\t${lambda_value}\t${masked_ratio}" >> "$outputFile"

            if [ ${subfolder} != "DCC" ]; then
              parse_result_file "$result_path_with_bsl"
              echo -e "${xwbt_time_us}\t${time_us}\t${occ}\t${file_size}\t${subfolder}_bsl\t${corpus}\twith\t${tau_l_value}\t${tau_u_value}\t${lambda_value}\t${masked_ratio}" >> "$outputFile"
            fi

            parse_result_file "$result_path_without"
            echo -e "${xwbt_time_us}\t${time_us}\t${occ}\t${file_size}\t${subfolder}\t${corpus}\twithout\t${tau_l_value}\t${tau_u_value}\t${lambda_value}\t${masked_ratio}" >> "$outputFile"

            if [ ${subfolder} != "DCC" ]; then
              parse_result_file "$result_path_without_bsl"
              echo -e "${xwbt_time_us}\t${time_us}\t${occ}\t${file_size}\t${subfolder}_bsl\t${corpus}\twithout\t${tau_l_value}\t${tau_u_value}\t${lambda_value}\t${masked_ratio}" >> "$outputFile"
            fi
            
          done
        done
      done
    done
  done
done