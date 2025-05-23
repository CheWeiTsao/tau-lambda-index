cmake_minimum_required(VERSION 3.10)
project(tau-lambda-index)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -w -DNDEBUG")

# Try to locate SDSL and divsufsorts
find_path(SDSL_INCLUDE_DIR sdsl/suffix_arrays.hpp
  HINTS /opt/homebrew/include /usr/local/include /usr/include
  PATH_SUFFIXES sdsl
  NO_DEFAULT_PATH
)

find_library(SDSL_LIB NAMES sdsl
  HINTS /opt/homebrew/lib /usr/local/lib /usr/lib
  NO_DEFAULT_PATH
)

find_library(DIVSUFSORT_LIB NAMES divsufsort
  HINTS /opt/homebrew/lib /usr/local/lib /usr/lib
  NO_DEFAULT_PATH
)

find_library(DIVSUFSORT64_LIB NAMES divsufsort64
  HINTS /opt/homebrew/lib /usr/local/lib /usr/lib
  NO_DEFAULT_PATH
)

if(NOT SDSL_INCLUDE_DIR OR NOT SDSL_LIB OR NOT DIVSUFSORT_LIB OR NOT DIVSUFSORT64_LIB)
  message(STATUS "SDSL_INCLUDE_DIR: ${SDSL_INCLUDE_DIR}")
  message(STATUS "SDSL_LIB: ${SDSL_LIB}")
  message(STATUS "DIVSUFSORT_LIB: ${DIVSUFSORT_LIB}")
  message(STATUS "DIVSUFSORT64_LIB: ${DIVSUFSORT64_LIB}")
  message(FATAL_ERROR "❌ Required libraries not found. Ple   ase install with: brew install sdsl-lite divsufsort")
endif()

# Include paths
include_directories(${SDSL_INCLUDE_DIR})
include_directories(include/self_indexes/LMS-based-self-index-LPG_grid/include/lpg/sdsl-files)
include_directories(include)

# Define source lists
set(LZ77_Ref
  include/self_indexes/LZ/src/utils.cpp
  include/self_indexes/LZ/src/mapper.cpp
  include/self_indexes/LZ/src/static_selfindex.cpp
  include/self_indexes/LZ/src/static_selfindex_none.cpp
  include/self_indexes/LZ/src/static_selfindex_lz77.cpp
  include/self_indexes/LZ/src/LZparser.cpp
  include/self_indexes/LZ/src/LZ77.cpp
  include/self_indexes/LZ/src/LZend.cpp
  include/self_indexes/LZ/src/treelist.cpp
  include/self_indexes/LZ/src/static_bitsequence.cpp
  include/self_indexes/LZ/src/static_bitsequence_brw32.cpp
  include/self_indexes/LZ/src/static_bitsequence_builder_brw32.cpp
  include/self_indexes/LZ/src/static_permutation.cpp
  include/self_indexes/LZ/src/static_permutation_mrrr.cpp
  include/self_indexes/LZ/src/RMQ.cpp
  include/self_indexes/LZ/src/RMQ_succinct.cpp
  include/self_indexes/LZ/src/perm.cpp
  include/self_indexes/LZ/src/deltacodes.cpp
  include/self_indexes/LZ/src/patricia.cpp
  include/self_indexes/LZ/src/patricia_node.cpp
  include/self_indexes/LZ/src/bp_hb.cpp
  include/self_indexes/LZ/src/dfuds.cpp
  include/self_indexes/LZ/src/static_doublebitmap.cpp
  include/self_indexes/LZ/src/static_doublebitmap_s.cpp
  include/self_indexes/LZ/src/hash.cpp
  include/self_indexes/LZ/src/utils_index.cpp
  include/self_indexes/LZ/src/range.cpp
  include/self_indexes/LZ/src/static_range.cpp
  include/self_indexes/LZ/src/static_range_none.cpp
  include/self_indexes/LZ/src/static_range_chazelle.cpp
  include/self_indexes/LZ/src/static_sequence_wt.cpp
  include/self_indexes/LZ/src/wt_node.cpp
  include/self_indexes/LZ/src/perm2.cpp
  include/self_indexes/LZ/src/static_permutation_mrrr2.cpp
  include/self_indexes/LZ/src/static_selfindex_lzend.cpp
  include/self_indexes/LZ/src/directcodes.cpp
  include/self_indexes/LZ/src/bitrankw32int.cpp
)

set(LMS_Ref
  include/self_indexes/LMS-based-self-index-LPG_grid/lib/lpg/lpg_build.cpp
  include/self_indexes/LMS-based-self-index-LPG_grid/third-party/xxHash-dev/xxhash.c
)

set(UTIL_SRC include/util/utility.cc)

# Build targets
add_executable(gen_mf src/gen_mf.cc)
add_executable(add_delimiter src/add_delimiter.cc)

add_executable(gen_index src/gen_index.cc ${UTIL_SRC} ${LZ77_Ref} ${LMS_Ref})
add_executable(gen_pattern src/gen_pattern.cc ${UTIL_SRC} ${LZ77_Ref} ${LMS_Ref})
add_executable(locate src/locate.cc ${UTIL_SRC} ${LZ77_Ref} ${LMS_Ref})
add_executable(serialize_partition src/serialize_partition.cc ${UTIL_SRC} ${LZ77_Ref} ${LMS_Ref})

# Link libraries
set(COMMON_LIBS ${SDSL_LIB} ${DIVSUFSORT_LIB} ${DIVSUFSORT64_LIB})
target_link_libraries(gen_index ${COMMON_LIBS})
target_link_libraries(gen_pattern ${COMMON_LIBS})
target_link_libraries(locate ${COMMON_LIBS})
target_link_libraries(serialize_partition ${COMMON_LIBS})