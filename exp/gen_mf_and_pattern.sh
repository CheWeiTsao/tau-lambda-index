#!/bin/bash
ulimit -v $((150*1024*1024))

# config = [corpus name] [tau_l] [tau_u] [lambda] [pattern cnt]
configs=(
  "'english dblp dna_real influenza'         '0' '0'                 '0' '100'"
  "english         '0' '1 2 3 4'                 '20 60 100 200' '100'"
  "dblp            '0' '1 2 3 4'                 '20 60 100 200' '100'"
  "dna_real        '0' '20 40 60 80'             '20 60 100 200' '100'"
  "influenza       '0' '780 1561 2341 3122'      '20 60 100 200' '100'"
  "dna_pseudoreal  '0' '6291 12583 18874 25166'  '20 60 100 200' '100'"
)

create_rawText() {
  local dir=$1
  local file=$2
  if [ ! -d "$dir" ]; then
    echo "rawText $dir does not exist. Uncompresseing..."
    unxz -c "dataset/${file}" > "$dir"
  fi
}

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

for config in "${configs[@]}"; do
  eval set -- $config
  corpora=($1)
  tau_l=($2)
  tau_u=($3)
  lambda=($4)
  pattern_cnt=$5

  mfSubfolder="minimal_factors"
  patternSubfolder="patterns"
  delimiter_symbols=""
  exec_dir="../build"

  check_directory "$exec_dir"

  for corpus in "${corpora[@]}"; do
    folder="results/${corpus}"
    create_directory "$folder"

    input_text_path="${folder}/rawText"

    mf_output_dir="${folder}/${mfSubfolder}"
    create_directory "$mf_output_dir"

    create_rawText "$input_text_path" "$corpus"

    pattern_output_dir="${folder}/${patternSubfolder}"
    create_directory "$pattern_output_dir"

    for tau_l_value in "${tau_l[@]}"; do
      for tau_u_value in "${tau_u[@]}"; do
        for lambda_value in "${lambda[@]}"; do
          output_mf_path="${mf_output_dir}/${tau_l_value}_${tau_u_value}_${lambda_value}_mf"
          output_pattern_path="${pattern_output_dir}/${tau_l_value}_${tau_u_value}_${lambda_value}_patterns"

          echo "Running ./${exec_dir}/gen_mf with tau_ell=$tau_l_value, tau_u=$tau_u_value, lambda=$lambda_value, delimiter=$delimiter_symbols"
          ./${exec_dir}/gen_mf "$input_text_path" "$output_mf_path" "$tau_l_value" "$tau_u_value" "$lambda_value" "$delimiter_symbols"

          if [[ $tau_l_value -gt 0 || $tau_r_value -gt 0 || $lambda_value -gt 0 ]]; then
              echo "Running ./${exec_dir}/gen_pattern with this mf file, pattern cnt = $pattern_cnt"
              ./${exec_dir}/gen_pattern "$output_mf_path" "$output_pattern_path" $pattern_cnt
          fi

        done
      done
    done
  done
done