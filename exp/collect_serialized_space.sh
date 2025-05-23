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

outputFile="[summary]serialize_space"
touch ${outputFile}
echo -e "type\tcorpus\ttau_l\ttau_u\tlambda\txbwt_size_bytes\timasked_index_size_bytes" > "${outputFile}"

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

    for type in "${types[@]}"; do
      subfolder=$(get_subfolder_name "$type")
      idx_dir="${folder}/${subfolder}"
      check_directory "$idx_dir"
      for tau_l_value in "${tau_l[@]}"; do
        for tau_u_value in "${tau_u[@]}"; do
          for lambda_value in "${lambda[@]}"; do
            idx_path="${idx_dir}/${tau_l_value}_${tau_u_value}_${lambda_value}_idx"
            echo "Recording serialize_partition $idx_path"
            
            if [ "${subfolder}" = "LZ77" ]; then
              par_1="${idx_path}"
              par_2="${idx_path}_lz77"
              
            else
              par_1="${idx_path}_xbwt"
              par_2="${idx_path}_Imasked"

              ./${exec_dir}/serialize_partition "$idx_path" "$par_1" "$par_2"
            fi

            size1=$(stat -c%s "$par_1")
            size2=$(stat -c%s "$par_2")

            echo -e "${subfolder}\t${corpus}\t${tau_l_value}\t${tau_u_value}\t${lambda_value}\t${size1}\t${size2}" >> "${outputFile}"
            
            if [ "${subfolder}" != "LZ77" ]; then
              par_1="${idx_path}_xbwt"
              par_2="${idx_path}_Imasked"

              rm "${par_1}"
              rm "${par_2}"
            fi
          
          done
        done
      done
    done
  done
done