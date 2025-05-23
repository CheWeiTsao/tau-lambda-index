//
// Created by diego on 27-08-20.
//

#ifndef LPG_COMPRESSOR_INT_ARRAY_H
#define LPG_COMPRESSOR_INT_ARRAY_H

#include <iostream>
#include <limits>
#include <cassert>
#include <utility>
#include <sdsl/util.hpp>
#include "memory_handler.hpp"
#include "bitstream.h"

template<class word_t>
struct int_array{

    typedef size_t size_type;
    typedef word_t value_type;
    typedef bitstream<word_t> stream_t;

    size_t m_size;
    uint8_t m_width;
    stream_t bits;

    //default constructor
    int_array():  m_size(0), m_width(0) {};

    //simple constructor (cap is the capacity of the vector)
    int_array(size_t cap_, uint8_t width_) : m_size(0), m_width(width_) {
        assert(m_width <= stream_t::word_bits);
        bits.stream_size = INT_CEIL(cap_ * width_, stream_t::word_bits);
        bits.stream = LMS_allocator::allocator::allocate<word_t>(bits.stream_size, true, 0);
    }

    //initialize from a allocated memory
    int_array(word_t *ptr, size_t size_, uint8_t width_){
        m_size = size_;
        m_width = width_;
        bits.stream_size = INT_CEIL(m_size*m_width, stream_t::word_bits);
        bits.stream = ptr;
    }

    //initializer-list constructor
    int_array(std::initializer_list<size_t> list) {
        assert(list.size()>0);
        size_t max=0;
        for(auto const& sym : list) if(sym>max) max=sym;

        m_width = std::max(4UL, sizeof(unsigned int)*8 - __builtin_clz(max));
        m_size = list.size();

        bits.stream_size = n_words();
        bits.stream = LMS_allocator::allocator::allocate<word_t>(bits.stream_size, false, 0);
        size_t i=0;
        for(auto const& sym : list){
            write(i++,sym);
        }
        mask_tail();
    }

    ~int_array() {
        LMS_allocator::allocator::deallocate<word_t>(bits.stream);
    }

    //move constructor
    int_array(int_array<word_t>&& other) noexcept:  m_size(0),  m_width(0) {
        bits.stream=nullptr;
        bits.stream_size=0;
        move(std::forward<int_array<word_t>>(other));
    };

    //copy constructor
    int_array(const int_array& other) noexcept : m_size(0),  m_width(0) {
        copy(other);
    };

    void move(int_array<word_t>&& other) noexcept {
        m_size = std::exchange(other.m_size, 0);
        m_width = std::exchange(other.m_width, 0);

        bits.stream_size = std::exchange(other.bits.stream_size, 0);
        if(bits.stream!= nullptr){
            LMS_allocator::allocator::deallocate(bits.stream);
        }
        bits.stream = std::exchange(other.bits.stream, nullptr);
    }

    inline void copy(const int_array<word_t> &other) {
        m_size = other.m_size;
        m_width = other.m_width;
        bits.stream_size = n_words();

        if(bits.stream!= nullptr){
            LMS_allocator::allocator::deallocate(bits.stream);
        }

        bits.stream = LMS_allocator::allocator::allocate<word_t>(bits.stream_size, false, 0);
        memcpy(bits.stream, other.stream, bits.stream * sizeof(word_t));
    }

    inline void clear() {
        m_size=0;
    }

    //copy assignment operator
    int_array& operator=(const int_array<word_t> & other){
        if(this!=&other){
            copy(other);
        }
        return *this;
    }

    //move assignment operator
    int_array& operator=(int_array<word_t> && other) noexcept{
        if(this!=&other){
            move(std::forward<int_array<word_t>>(other));
        }
        return *this;
    }

    inline bool operator==(const int_array<word_t> &other) const {
        if(m_size!=other.m_size) return false;
        size_t tot_bits = m_size * m_width;
        return bits.compare_chunk(other.bits.stream, 0, tot_bits);
    }

    inline void* data() const{
        return bits.stream;
    }

    inline void set_data(const word_t* new_data, size_t size) {
        bits.stream = new_data;
        bits.stream_size = size;
    }

    inline size_t width() const{
        return m_width;
    }

    inline size_t n_words() const{
        return m_size==0? 0: INT_CEIL(n_bits(), stream_t::word_bits);
    }

    inline size_t n_bytes() const{//number of bytes used by the data (ceil)
        return INT_CEIL(n_bits(), 8);
    }

    inline size_t n_bits() const{//number of bits used by the data (exact)
        return m_size * m_width;
    }

    inline void mask_tail() {
        size_t tot_bits = m_size * m_width;
        size_t n_cells = tot_bits / stream_t::word_bits;//floor

        if (tot_bits > n_cells * stream_t::word_bits) {
            bits.write(tot_bits, (n_cells + 1) * stream_t::word_bits - 1, 0);
        }
    }

    inline void push_back(value_type value) {
        assert(sdsl::bits::hi(value)+1<=m_width);
        size_t i = m_size * m_width;
        size_t j = (m_size+1) * m_width - 1;
        if((j/stream_t::word_bits)>=bits.stream_size){
            reserve(m_size*2);
        }
        bits.write(i, j , value);
        m_size++;
    }

    inline void pop_back() {
        if(m_size>=1) m_size--;
    }

    void reserve(size_t new_size) {
        //reserve memory for new_size number of elements
        size_t new_buffer_size = 0;
        if(new_size>0){
            new_buffer_size = INT_CEIL(new_size * m_width, stream_t::word_bits);
        }

        if(new_buffer_size>bits.stream_size){
            bits.stream = LMS_allocator::allocator::reallocate<word_t>(bits.stream, bits.stream_size, new_buffer_size, false, 0);
            bits.stream_size = new_buffer_size;
        }
    }

    inline size_t size() const{
        return m_size;
    }

    inline bool empty() const{
        return m_size==0;
    }

    inline value_type back() const{
        assert(m_size>0);
        return bits.read((m_size-1)*m_width, m_size*m_width-1);
    }

    inline void write(size_t idx, value_type value){
        if(idx>=m_size) m_size = idx+1;
        bits.write(idx * m_width, (idx + 1) * m_width - 1, value);
    }

    inline value_type operator[](size_t idx) const{
        assert(idx<m_size);
        return bits.read(idx * m_width, (idx + 1) * m_width - 1);
    }

    size_type serialize(std::ostream &out, sdsl::structure_tree_node *v, std::string name) const{
        sdsl::structure_tree_node* child = sdsl::structure_tree::add_child(v, name, sdsl::util::class_name(*this));
        size_t written_bytes = 0;
        written_bytes +=  bits.serialize(out, "bit_stream");
        out.write((char *)&m_size, sizeof(size_t));
        written_bytes+=sizeof(size_t);
        out.write((char *)&m_width, 1);
        sdsl::structure_tree::add_size(child, written_bytes+1);
        return written_bytes + 1;
    }

    void load(std::istream &in){
        bits.load(in);
        in.read((char *)m_size, sizeof(size_t));
        in.read((char *)m_width, 1);
    }

    void set(size_t val, size_t start, size_t end){
        if(val==0){
            memset(bits.stream, 0, bits.stream_size*sizeof(word_t));
        }else{
            std::cout<<"not implemented yet"<<std::endl;
        }
        if(end>m_size) m_size = end;
    }
};
#endif //LPG_COMPRESSOR_INT_ARRAY_H
