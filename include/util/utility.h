#pragma once

#include <sys/wait.h>
#include <sdsl/int_vector.hpp>
#include <sdsl/construct.hpp>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <memory>
#include <random>

template <typename File, typename Container>
void Print
(
  Container const& container,
  File& file,
  int8_t const step = 1,
  std::string const& separator = " ",
  std::string const& endmarker = "\n"
);

template <typename File, typename Iterator>
void Print
(
  Iterator first,
  Iterator last,
  File& file,
  int8_t const step = 1,
  std::string const& separator = " ",
  std::string const& endmarker = "\n"
);

void GeneratePrefix
(
  std::filesystem::path const& byte_text_path,
  uint64_t const size_in_megabytes
);

template <typename Size>
std::string ProperSizeRepresentation (Size const size);

class SpaceNode
{
public:

  SpaceNode () = default;
  SpaceNode (std::string const& name, uint64_t const size_in_bytes = 0);

  void AccumalateSizeInBytes (uint64_t const size_in_bytes);
  void AddChild (std::shared_ptr<SpaceNode> child);
  void AddLeaf (std::string const& name, uint64_t const size_in_bytes);

  inline uint64_t GetSizeInBytes () const
  {
    return size_in_bytes_;
  }

  friend std::ostream& operator<<
  (
    std::ostream& out,
    std::pair<std::shared_ptr<SpaceNode>, bool> pair
  );

private:

  std::string name_;
  uint64_t size_in_bytes_;
  std::deque<std::shared_ptr<SpaceNode>> children_;

};

void DecompressCompressedCorpus
(
  std::filesystem::path const& compressed_corpus_path,
  std::filesystem::path const& corpus_path
);

void GenerateGenerationalDnaSequence
(
  uint64_t const size,
  uint64_t const amount_copies,
  uint64_t const mutative_rate,
  std::filesystem::path const& path
);

void PrintTextParameters (std::filesystem::path const& byte_text_path);