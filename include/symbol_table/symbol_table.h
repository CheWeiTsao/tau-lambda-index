#pragma once

#include "../util/utility.h"
#include <utility>
#include <sdsl/int_vector.hpp>
#include <sdsl/rank_support.hpp>

class SymbolTable
{
public:

  SymbolTable () = default;
  SymbolTable (SymbolTable const&) = delete;
  SymbolTable (SymbolTable&&);
  SymbolTable (sdsl::int_vector<> const &byte_text);
  SymbolTable& operator= (SymbolTable const&) = delete;
  SymbolTable& operator= (SymbolTable &&);

  void Swap (SymbolTable&);
  
  uint64_t Serialize
  (
    std::ostream &out,
    std::shared_ptr<SpaceNode> parent = nullptr,
    std::string const name = ""
  );
  void Load (std::istream &in);

  inline uint8_t GetEffectiveAlphabetWidth () const
  {
    return effective_alphabet_width_;
  }

  // void SetAlphabetSize(uint64_t s) { alphabet_size = s; }
  uint64_t GetAlphabetSize() { return alphabet_size; }

  uint64_t operator[] (uint64_t const byte) const;

  uint64_t lower_bound(uint64_t int_symbol, uint64_t length, uint64_t maxLength) const;

  uint64_t max_of_this_alphabet(uint64_t rl_int_symbol, uint64_t maxLength) const;

  uint64_t symbol_to_byte(uint64_t const symbol) const { return alphabet_select_1_(symbol); }

  friend std::ostream& operator<< (std::ostream &out, SymbolTable const &symbol_table);

  uint64_t get_run_length(uint64_t c, uint64_t maxRunLength);

private:

  uint64_t effective_alphabet_width_;
  uint64_t alphabet_size;
  sdsl::bit_vector alphabet_bits_;
  sdsl::bit_vector::rank_1_type alphabet_rank_1_;
  sdsl::bit_vector::select_1_type alphabet_select_1_;

};

SymbolTable::SymbolTable (SymbolTable&& symbol_table)
{
  if (this != &symbol_table)
  {
    this->Swap(symbol_table);
  }
}

SymbolTable::SymbolTable (sdsl::int_vector<> const &byte_text)
{
  alphabet_bits_.resize(*std::max_element(std::begin(byte_text), std::end(byte_text)) + 1);
  sdsl::util::set_to_value(alphabet_bits_, 0);
  for (auto const byte : byte_text)
  {
    alphabet_bits_[byte] = 1;
  }
  //std::cout << "alphabet_bits: " << alphabet_bits_ << "\n";
  alphabet_rank_1_ = decltype(alphabet_rank_1_)(&alphabet_bits_);
  alphabet_select_1_ = decltype(alphabet_select_1_)(&alphabet_bits_);
  effective_alphabet_width_ = sdsl::bits::hi(alphabet_rank_1_(std::size(alphabet_bits_)) - 1) + 1;
  alphabet_size = alphabet_rank_1_(alphabet_bits_.size());
}

SymbolTable& SymbolTable::operator= (SymbolTable &&symbol_table)
{
  if (this != &symbol_table)
  {
    SymbolTable temp {std::move(symbol_table)};
    this->Swap(temp);
  }
  return *this;
}

void SymbolTable::Swap (SymbolTable& symbol_table)
{
  if (this != &symbol_table)
  {
    std::swap(effective_alphabet_width_, symbol_table.effective_alphabet_width_);
    alphabet_bits_.swap(symbol_table.alphabet_bits_);
    alphabet_rank_1_.swap(symbol_table.alphabet_rank_1_);
    alphabet_rank_1_.set_vector(&alphabet_bits_);
    alphabet_select_1_.swap(symbol_table.alphabet_select_1_);
    alphabet_select_1_.set_vector(&alphabet_bits_);
    symbol_table.alphabet_rank_1_.set_vector(&symbol_table.alphabet_bits_);
    symbol_table.alphabet_select_1_.set_vector(&symbol_table.alphabet_bits_);
    alphabet_size = symbol_table.GetAlphabetSize();
  }
  return;
}

uint64_t SymbolTable::Serialize
(
  std::ostream &out,
  std::shared_ptr<SpaceNode> parent,
  std::string const name
)
{
  uint64_t size_in_bytes {};
  if (!parent)
  {
    sdsl::write_member(effective_alphabet_width_, out);
    sdsl::write_member(alphabet_size, out);
    // sdsl::serialize(alphabet_bits_, out);
    // sdsl::serialize(alphabet_rank_1_, out);
    // sdsl::serialize(alphabet_select_1_, out);
    alphabet_bits_.serialize(out);
    alphabet_rank_1_.serialize(out);
    alphabet_select_1_.serialize(out);
  }
  else
  {
    auto node {std::make_shared<SpaceNode>(name)};
    node->AddLeaf("effective_alphabet_width_", sdsl::write_member(effective_alphabet_width_, out));
    node->AddLeaf("alphabet_bits_", sdsl::serialize(alphabet_bits_, out));
    node->AddLeaf("alphabet_rank_1_", sdsl::serialize(alphabet_rank_1_, out));
    node->AddLeaf("alphabet_rank_1_", sdsl::serialize(alphabet_select_1_, out));
    parent->AddChild(node);
    size_in_bytes = node->GetSizeInBytes();
  }
  return size_in_bytes;
}

void SymbolTable::Load (std::istream &in)
{
  sdsl::read_member(effective_alphabet_width_, in);
  sdsl::read_member(alphabet_size, in);
  alphabet_bits_.load(in);
  alphabet_rank_1_.load(in, &alphabet_bits_);
  alphabet_select_1_.load(in, &alphabet_bits_);
  return;
}

uint64_t SymbolTable::operator[] (uint64_t const byte) const
{
  if ((byte < std::size(alphabet_bits_)) && alphabet_bits_[byte])
  {
    return alphabet_rank_1_(byte);
  }
  return 0;
}

uint64_t SymbolTable::lower_bound(uint64_t int_symbol, uint64_t length, uint64_t maxLength) const {
  // +2 for '0' and '1' (t_offset)
  // if (length > maxLength || int_symbol * maxLength + 2 > std::size(alphabet_bits_)) { return 0; }
  size_t alphabet_index = (int_symbol-1) * maxLength + length + 1; // 0-index
  if (length > maxLength || alphabet_index > std::size(alphabet_bits_)) { return 0; }
  else {
    // write ugly to minimize the times for calling 'rank' and 'select'
    size_t _1_cnt = alphabet_rank_1_(alphabet_index + 1) - 1; // how many '1' up to this pos (included), -1 for shifting to 0-index
    if (alphabet_bits_[alphabet_index]) { return _1_cnt; } // return if that pos = '1'
    else if (_1_cnt == alphabet_size) { return 0; }
    size_t next_1_idx = alphabet_select_1_(_1_cnt + 2); // +1 for shifting back to 1-index, +1 for the next
    if (next_1_idx > int_symbol * maxLength + 1) { return 0; }
    else { return _1_cnt + 1; }
  }
}

uint64_t SymbolTable::max_of_this_alphabet(uint64_t rl_int_symbol, uint64_t maxLength) const {
  // +2 for '0' and '1' (t_offset), +1 for ranking is right-most excluded
  // std::cout << "rl_int_symbol: " << rl_int_symbol << "\n";
  size_t int_symbol = (alphabet_select_1_(rl_int_symbol + 1) - 2) / maxLength + 1;
  // std::cout << "int_symbol: " << int_symbol << "\n";
  // std::cout << "offset: " << (int_symbol * maxLength + 2) << "\n";
  if (int_symbol * maxLength + 2 > std::size(alphabet_bits_)) { return alphabet_size; }
  else { return alphabet_rank_1_(int_symbol * maxLength + 2); }
}

uint64_t SymbolTable::get_run_length(uint64_t c, uint64_t maxRunLength) {
  if (c > alphabet_size) { return 0; }
  return (alphabet_select_1_(c + 1) - 2) % maxRunLength + 1; // -2 for skipping '0' and '1'; +1 for shifting to 1-index
}

std::ostream& operator<< (std::ostream &out, SymbolTable const &symbol_table)
{
  {
    out << "value:\n";
    out << "effective_alphabet_width_:\n";
    out << static_cast<uint64_t>(symbol_table.effective_alphabet_width_) << "\n";
    out << "byte alphabet:\n";
    out << 0 << ":" << 0 << "\n";
    for (uint64_t i {1}; i != std::size(symbol_table.alphabet_bits_); ++i)
    {
      auto symbol {symbol_table[i]};
      if (symbol != 0)
      {
        out << symbol << ":" << i << "\n";
      }
    }
  }
  {
    out << "space:\n";
    out << "effective_alphabet_width_: " << sizeof(symbol_table.effective_alphabet_width_) << "B\n";
    out << "alphabet_bits_: " << ProperSizeRepresentation(sdsl::size_in_bytes(symbol_table.alphabet_bits_)) << "B\n";
    out << "alphabet_rank_1_: " << ProperSizeRepresentation(sdsl::size_in_bytes(symbol_table.alphabet_rank_1_)) << "B\n";
  }
  return out;
}