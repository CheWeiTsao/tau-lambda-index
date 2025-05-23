//
// Created by Diego Diaz on 4/7/20.
//

#include "../../include/lpg/lpg_build.hpp"
#include <cmath>
#include <sdsl/select_support_mcl.hpp>
#include "../../include/cdt/parallel_string_sort.hpp"
#include "../../include/lpg/repair_algo.hpp"


//pthread_mutex_t thread_mutex=PTHREAD_MUTEX_INITIALIZER;

void lpg_build::check_plain_grammar(plain_grammar_t& p_gram, std::string& uncomp_file) {


    sdsl::int_vector<> r;
    bv_t r_lim;
    sdsl::load_from_file(r, p_gram.rules_file);
    sdsl::load_from_file(r_lim, p_gram.rules_lim_file);
    bv_t::select_1_type r_lim_ss;
    sdsl::util::init_support(r_lim_ss, &r_lim);

    bv_t is_rl;
    sdsl::load_from_file(is_rl, p_gram.is_rl_file);

    std::cout<<"  Checking the grammar produces the exact input string"<<std::endl;
    std::cout<<"    This step is optional and for debugging purposes"<<std::endl;
    std::cout<<"    Terminals:              "<<(size_t)p_gram.sigma<<std::endl;
    std::cout<<"    Number of nonterminals: "<<p_gram.r-p_gram.sigma<<std::endl;
    std::cout<<"    Compressed string:      "<<p_gram.c<<std::endl;

    std::vector<size_t> tmp_decomp;

    i_file_stream<uint8_t> if_stream(uncomp_file, BUFFER_SIZE);
    uint8_t buff_symbol;

    size_t pos=0, curr_sym, start, end;
    std::stack<size_t> stack;

    size_t f = r_lim_ss(p_gram.r - 1) + 1;
    size_t l = r_lim_ss(p_gram.r);

    for(size_t i=f; i <= l; i++){
        tmp_decomp.clear();
        stack.push(r[i]);
        assert(stack.size()<=if_stream.size());

        while(!stack.empty()){

            curr_sym = stack.top() ;
            stack.pop();
            //std::cout<<curr_sym<<std::endl;

            if(curr_sym==0){
                start = 0;
            }else{
                start = r_lim_ss(curr_sym)+1;
            }

            end = r_lim_ss(curr_sym+1);

            if(r[start] == curr_sym){
                assert((end-start+1)==1);
                assert(tmp_decomp.size()<=if_stream.size());
                tmp_decomp.push_back(curr_sym);
            }else{
                //this is a dummy way of handling rl nonterminals,
                // but I don't have enough time
                if(is_rl[curr_sym]){
                    assert(end-start+1==2);
                    for(size_t k=0;k<r[end];k++){
                        stack.push(r[start]);
                    }
                }else{
                    for(size_t j=end+1; j-->start;){
                        stack.push(r[j]);
                    }
                }
            }
        }


        size_t cont=0;
        for(auto const& tmp_sym : tmp_decomp){
            buff_symbol = if_stream.read(pos++);
            //std::cout<<(int)buff_symbol<<" "<<(int)p_gram.sym_map[tmp_sym]<<" "<<pos-1<<" "<<cont<<std::endl;
            assert(p_gram.sym_map[tmp_sym] == buff_symbol);
            cont++;
        }
    }
    std::cout<<"\tGrammar is correct!!"<<std::endl;
}

void lpg_build::compute_LPG(std::string &i_file, std::string &p_gram_file, size_t n_threads, sdsl::cache_config &config,
                            size_t hbuff_size, alpha_t &alphabet) {

    std::cout<<"  Generating the LMS-based locally consistent grammar:    "<<std::endl;

    std::string rules_file = sdsl::cache_file_name("rules", config);
    std::string rules_len_file = sdsl::cache_file_name("rules_len", config);
    std::string lvl_breaks_file = sdsl::cache_file_name("lvl_breaks", config);
    std::string is_rl_file = sdsl::cache_file_name("is_rl", config);

    plain_grammar_t p_gram(rules_file, rules_len_file, is_rl_file, lvl_breaks_file);
    p_gram.sigma = alphabet.size();

    // given an index i in symbol_desc
    //0 symbol i is in alphabet is unique
    //1 symbol i is repeated
    //>2 symbol i is sep symbol
    sdsl::int_vector<2> symbol_desc(alphabet.back().first+1,0);

    for(auto & sym : alphabet){
        p_gram.sym_map[sym.first] = sym.first;
        symbol_desc[sym.first] = sym.second > 1;
    }
    p_gram.max_tsym = alphabet.back().first;
    p_gram.r = p_gram.max_tsym + 1;
    symbol_desc[alphabet[0].first]+=2;

    ivb_t rules(p_gram.rules_file, std::ios::out, BUFFER_SIZE);
    bvb_t rules_lim(p_gram.rules_lim_file, std::ios::out);
    for(size_t i=0;i<p_gram.r; i++){
        rules.push_back(i);
        rules_lim.push_back(true);
    }
    for(auto const& pair : p_gram.sym_map){
        rules[pair.first] = pair.first;
    }

    std::string output_file = sdsl::cache_file_name("tmp_output", config);
    std::string tmp_i_file = sdsl::cache_file_name("tmp_input", config);

    size_t iter=1;
    size_t rem_phrases;

    std::cout<<"    Parsing round "<<iter++<<std::endl;
    rem_phrases = compute_LPG_int<uint8_t>(i_file, tmp_i_file,
                                           n_threads, hbuff_size,
                                           p_gram, rules, rules_lim,
                                           symbol_desc, config);

    while (rem_phrases > 0) {
        std::cout<<"    Parsing round "<<iter++<<std::endl;
        rem_phrases = compute_LPG_int<size_t>(tmp_i_file, output_file,
                                              n_threads, hbuff_size,
                                              p_gram, rules, rules_lim,
                                              symbol_desc, config);
        remove(tmp_i_file.c_str());
        rename(output_file.c_str(), tmp_i_file.c_str());
    }
    sdsl::util::clear(symbol_desc);

    {//put the compressed string at end
        std::ifstream c_vec(tmp_i_file, std::ifstream::binary);
        c_vec.seekg(0, std::ifstream::end);
        size_t tot_bytes = c_vec.tellg();
        c_vec.seekg(0, std::ifstream::beg);
        auto *buffer = reinterpret_cast<size_t*>(malloc(BUFFER_SIZE));
        size_t read_bytes =0;
        p_gram.c=0;
        while(read_bytes<tot_bytes){
            c_vec.read((char *) buffer, BUFFER_SIZE);
            read_bytes+=c_vec.gcount();
            assert((c_vec.gcount() % sizeof(size_t))==0);
            for(size_t i=0;i<c_vec.gcount()/sizeof(size_t);i++){
                rules.push_back(buffer[i]);
                rules_lim.push_back(false);
                p_gram.c++;
            }
        }
        rules_lim[rules_lim.size() - 1] = true;
        p_gram.r++;
        c_vec.close();
        free(buffer);
    }
    p_gram.g = rules.size();

    rules.close();
    rules_lim.close();

    std::cout<<"  Resulting locally consistent grammar:    "<<std::endl;
    std::cout<<"    Number of terimnals:    "<<(int)p_gram.sigma<<std::endl;
    std::cout<<"    Number of nonterminals: "<<p_gram.r-p_gram.sigma<<std::endl;
    std::cout<<"    Grammar size:           "<<p_gram.g<<std::endl;
    std::cout<<"    Compressed string:      "<<p_gram.c<<std::endl;

    run_length_compress(p_gram, config);
//    repair(p_gram, config);

    bv_t rem_nts = mark_nonterminals(p_gram);
    bv_t::rank_1_type rem_nts_rs(&rem_nts);

    simplify_grammar(p_gram, rem_nts, rem_nts_rs);

    sdsl::util::clear(rem_nts_rs);
    sdsl::util::clear(rem_nts);

    colex_nt_sort(p_gram);
    p_gram.save_to_file(p_gram_file);

    //TODO testing
//    check_plain_grammar(p_gram, i_file);
    //

    std::cout <<"  Final grammar: " << std::endl;
    std::cout <<"    Number of terminals:    " << (size_t) p_gram.sigma << std::endl;
    std::cout <<"    Number of nonterminals: " << p_gram.r - p_gram.sigma << std::endl;
    std::cout <<"    Grammar size:           " << p_gram.g - p_gram.sigma << std::endl;
    std::cout <<"    Compressed string:      " << p_gram.c << std::endl;

    if(remove(tmp_i_file.c_str())){
        std::cout<<"Error trying to delete file "<<tmp_i_file<<std::endl;
    }
}

template<class sym_type>
size_t lpg_build::compute_LPG_int(std::string &i_file, std::string &o_file,
                                  size_t n_threads, size_t hbuff_size,
                                  plain_grammar_t &p_gram, ivb_t &rules,
                                  bvb_t &rules_lim, sdsl::int_vector<2> &phrase_desc,
                                  sdsl::cache_config &config) {

    phrase_map_t mp_table(0, "", 0.85);

    auto thread_ranges = compute_thread_ranges<sym_type>(n_threads, i_file, phrase_desc);

    std::vector<lms_info<sym_type>> threads_data;
    threads_data.reserve(thread_ranges.size());

    //how many size_t cells we can fit in the buffer
    size_t buff_cells = hbuff_size/sizeof(size_t);

    //number of bytes per thread
    size_t hb_bytes = (buff_cells / thread_ranges.size()) * sizeof(size_t);

    void *buff_addr = malloc(hbuff_size);
    auto tmp_addr = reinterpret_cast<char*>(buff_addr);

    size_t k=0;
    for(auto &range : thread_ranges) {

        std::stringstream ss;
        ss << o_file.substr(0, o_file.size() - 5) << "_range_" << range.first << "_" << range.second;
        std::string tmp_o_file = ss.str();
        threads_data.emplace_back(i_file, tmp_o_file, mp_table, range.first, range.second,
                                  p_gram.r, hb_bytes, tmp_addr + (k*hb_bytes), phrase_desc);
        k++;
    }

    std::cout<<"      Computing the LMS phrases in the text"<<std::endl;
    {
        std::vector<pthread_t> threads(threads_data.size());
        for(size_t i=0;i<threads_data.size();i++){
            int ret =  pthread_create(&threads[i],
                                      nullptr,
                                      &hash_phrases<sym_type>,
                                      (void*)&threads_data[i]);
            if(ret != 0) {
                printf("Error: pthread_create() failed\n");
                exit(EXIT_FAILURE);
            }
        }

        for(size_t i=0;i<threads_data.size();i++) {
            pthread_join(threads[i], nullptr);
        }
    }
    free(buff_addr);

    //join the different phrase files
    std::vector<std::string> phrases_files;
    for(size_t i=0;i<threads_data.size();i++){
        phrases_files.push_back(threads_data[i].thread_map.dump_file());
    }
    join_thread_phrases(mp_table, phrases_files);

    size_t psize=0;//<- for the iter stats
    if(mp_table.size()>0){

        p_gram.rules_per_level.push_back(mp_table.size());
        size_t width = sdsl::bits::hi(p_gram.r+1)+1;
        const bitstream<buff_t>& stream = mp_table.get_data();
        key_wrapper key_w{width, mp_table.description_bits(), stream};

        //temporal unload of the hash table (not the data)
        std::string st_table = sdsl::cache_file_name("ht_data", config);
        mp_table.unload_table(st_table);

        //rename phrases according to their lexicographical ranks
        std::cout<<"      Assigning identifiers to the phrases"<<std::endl;
        assign_ids(mp_table, p_gram.r-1,  key_w, rules, rules_lim, n_threads, config);

        //reload the hash table
        mp_table.load_table(st_table);
        if(remove(st_table.c_str())){
            std::cout<<"Error trying to remove temporal file"<<std::endl;
            std::cout<<"Aborting"<<std::endl;
            exit(1);
        }

        std::cout<<"      Creating the parse of the text"<<std::endl;
        {//store the phrases into a new file
            std::vector<pthread_t> threads(threads_data.size());
            for(size_t i=0;i<threads_data.size();i++){
                int ret =  pthread_create(&threads[i],
                                          nullptr,
                                          &record_phrases<sym_type>,
                                          (void*)&threads_data[i]);
                if(ret != 0) {
                    printf("Error: pthread_create() failed\n");
                    exit(EXIT_FAILURE);
                }
            }

            for(size_t i=0;i<threads_data.size();i++) {
                pthread_join(threads[i], nullptr);
            }
        }

        std::vector<std::string> chunk_files;
        for(size_t i=0;i<threads_data.size();i++){
            chunk_files.push_back(threads_data[i].ofs.file);
        }
        join_parse_chunks(o_file, chunk_files);

        {// this is just to get the size of the resulting parse
           i_file_stream<size_t> ifs(o_file, BUFFER_SIZE);
           psize = ifs.tot_cells;
        }

        {
            //keep track of the lms phrases that have to be rephrased
            phrase_desc.resize(p_gram.r+mp_table.size());
            std::cout << "      Updating symbols status" << std::endl;
            auto it = mp_table.begin();
            auto it_end = mp_table.end();
            size_t tmp_value, sym;

            while (it != it_end) {
                tmp_value = 0;
                auto val = it.value();

                //more than one occurrence of the phrase
                if (val & 1UL) {
                    tmp_value += 1;
                }

                //read the (reversed) last symbol
                sym = key_w.read(*it, 0);
                if (phrase_desc[sym] & 2U) {//phrase is suffix of some string
                    tmp_value += 2;
                }

                phrase_desc[val >> 1UL] = tmp_value;
                ++it;
            }
        }
    }else{ //just copy the input
        std::ifstream in(i_file, std::ios_base::binary);
        std::ofstream out(o_file, std::ios_base::binary);

        auto buffer = reinterpret_cast<char*>(malloc(BUFFER_SIZE));
        do {
            in.read(&buffer[0], BUFFER_SIZE);
            out.write(&buffer[0], in.gcount());
            psize+=in.gcount();
        } while (in.gcount() > 0);
        free(buffer);
        psize/=sizeof(sym_type);

        //remove remaining files
        for(size_t i=0;i<threads_data.size();i++){
            std::string tmp_file =  threads_data[i].ofs.file;
            if(remove(tmp_file.c_str())){
                std::cout<<"Error trying to delete file "<<tmp_file<<std::endl;
            }
        }
    }

    p_gram.r +=mp_table.size();
    std::cout<<"      Stats:"<<std::endl;
    std::cout<<"        Parse size:          "<<psize<<std::endl;
    std::cout<<"        New nonterminals:    "<<mp_table.size()<<std::endl;

    if(psize>1){
        return mp_table.size();
    }else{
        return 0;
    }
}

void
lpg_build::assign_ids(phrase_map_t &mp_map, size_t max_sym, key_wrapper &key_w, ivb_t &r,
                      bvb_t &r_lim, size_t n_threads, sdsl::cache_config &config) {

    std::string syms_file = sdsl::cache_file_name("syms_file", config);
    {
        sdsl::int_vector_buffer<> syms_buff(syms_file, std::ios::out);
        for (auto const &phrase : mp_map) {
            syms_buff.push_back(phrase);
        }
        syms_buff.close();
    }
    auto compare = [&](const size_t &l, const size_t &r) -> bool {
        return key_w.compare(l, r);
    };
    auto access = [&](const size_t &val, size_t idx) -> size_t {
        return key_w.read(val, key_w.size(val)-1-idx);
    };
    parallel_str_sort(syms_file, compare, access, max_sym+1, n_threads, config);

    sdsl::int_vector<> k_list;
    sdsl::load_from_file(k_list, syms_file);

    if(remove(syms_file.c_str())){
        std::cout<<"Error trying to remove file "<<syms_file<<std::endl;
    }

    for(size_t m_pos=0; m_pos < mp_map.size(); m_pos++){

        size_t len = key_w.size(k_list[m_pos]);
        for(size_t i=len; i-- > 1;){
            r.push_back(key_w.read(k_list[m_pos], i));
            r_lim.push_back(false);
        }
        r.push_back( key_w.read(k_list[m_pos], 0));
        r_lim.push_back(true);

        //modify the key value
        phrase_map_t::val_type val=0;
        mp_map.get_value_from(k_list[m_pos], val);
        val |= (max_sym+m_pos+1)<<1UL;
        mp_map.insert_value_at(k_list[m_pos], val);
        //
    }
}

void lpg_build::join_parse_chunks(const std::string &output_file, std::vector<std::string> &chunk_files) {

    //concatenate the files
    std::ofstream of(output_file, std::ofstream::binary);
    size_t buff_size = BUFFER_SIZE/sizeof(size_t);
    size_t len, rem, to_read, start, end;
    auto *buffer = new size_t[buff_size];

    for(auto const& file: chunk_files){

        std::ifstream i_file(file, std::ifstream::binary);

        i_file.seekg (0, std::ifstream::end);
        len = i_file.tellg()/sizeof(size_t);
        i_file.seekg (0, std::ifstream::beg);

        rem=len;
        to_read = std::min<size_t>(buff_size, len);

        while(true){

            i_file.seekg( (rem - to_read) * sizeof(size_t));
            i_file.read((char *)buffer, sizeof(size_t)*to_read);
            assert(i_file.good());

            //invert data
            start =0;end=to_read-1;
            while(start<end){
                std::swap(buffer[start++], buffer[end--]);
            }

            of.write((char *)buffer, sizeof(size_t)*to_read);
            assert(of.good());

            rem -= i_file.gcount()/sizeof(size_t);
            to_read = std::min<size_t>(buff_size, rem);
            if(to_read == 0) break;
        }
        i_file.close();

        if(remove(file.c_str())){
            std::cout<<"Error trying to remove temporal file"<<std::endl;
            std::cout<<"Aborting"<<std::endl;
            exit(1);
        }
    }
    delete[] buffer;
    of.close();
}

template<class sym_t>
void * lpg_build::hash_phrases(void * data) {

    auto lms_data = (lms_info<sym_t> *) data;

    bool s_type, prev_s_type = S_TYPE;
    sym_t curr_sym, prev_sym;

    string_t curr_lms(2, lms_data->sym_width);

    prev_sym = lms_data->ifs.read(lms_data->end);
    curr_lms.push_back(prev_sym);

    for(size_t i = lms_data->end; i-- > lms_data->start;){

        curr_sym = lms_data->ifs.read(i);

        //                                     L_TYPE   S_TYPE*
        //                                        ---- ----
        //this is a junction between two strings = ...$ $...
        if(lms_data->is_suffix(curr_sym)){
            bool full_str = curr_lms.size()==1 && lms_data->is_suffix(curr_lms[0]);
            if(!curr_lms.empty() && !full_str){
                lms_data->hash_phrase(curr_lms);
            }
            curr_lms.clear();
            s_type = S_TYPE;
        } else {
            if (curr_sym < prev_sym) {//S_TYPE type
                s_type = S_TYPE;
            } else if (curr_sym == prev_sym) {
                s_type = prev_s_type;
            } else {//L_TYPE type
                s_type = L_TYPE;

                if(prev_s_type == S_TYPE) {//Leftmost S-type suffix
                    curr_lms.pop_back();
                    if(!curr_lms.empty()){
                        lms_data->hash_phrase(curr_lms);
                        curr_lms.clear();
                    }
                    curr_lms.push_back(prev_sym);
                }
            }
        }
        curr_lms.push_back(curr_sym);
        prev_sym = curr_sym;
        prev_s_type = s_type;
    }

    assert(curr_lms[0]!=1);
    bool full_str = curr_lms.size()==1 &&
                    lms_data->is_suffix(curr_lms[0]) &&
                    (lms_data->start==0 || lms_data->is_suffix(lms_data->ifs.read(lms_data->start-1)));
    if(!curr_lms.empty() && !full_str){
        lms_data->hash_phrase(curr_lms);
    }
    lms_data->thread_map.flush();

    pthread_exit(nullptr);
}

void lpg_build::join_thread_phrases(phrase_map_t& map, std::vector<std::string> &files) {

    bool rep;

    for(auto const& file : files){

        std::ifstream text_i(file, std::ios_base::binary);

        text_i.seekg (0, std::ifstream::end);
        size_t tot_bytes = text_i.tellg();
        text_i.seekg (0, std::ifstream::beg);

        auto buffer = reinterpret_cast<char *>(malloc(tot_bytes));

        text_i.read(buffer, tot_bytes);

        bitstream<buff_t> bits;
        bits.stream = reinterpret_cast<buff_t*>(buffer);

        size_t next_bit = 32;
        size_t tot_bits = tot_bytes*8;
        size_t key_bits;
        void* key=nullptr;
        size_t max_key_bits=0;

        while(next_bit<tot_bits){

            key_bits = bits.read(next_bit-32, next_bit-1);

            size_t n_bytes = INT_CEIL(key_bits, bitstream<buff_t>::word_bits)*sizeof(buff_t);
            if(key_bits>max_key_bits){
                if(key==nullptr){
                    key = malloc(n_bytes);
                }else {
                    key = realloc(key, n_bytes);
                }
                max_key_bits = key_bits;
            }

            char *tmp = reinterpret_cast<char*>(key);
            tmp[INT_CEIL(key_bits, 8)-1] = 0;

            bits.read_chunk(key, next_bit, next_bit+key_bits-1);
            next_bit+=key_bits;
            rep = bits.read(next_bit, next_bit);
            next_bit+=33;

            auto res = map.insert(key, key_bits, rep);
            if(!res.second){
                map.insert_value_at(*res.first, 1UL);
            }
        }
        text_i.close();

        if(remove(file.c_str())){
            std::cout<<"Error trying to remove temporal file"<<std::endl;
            std::cout<<"Aborting"<<std::endl;
            exit(1);
        }

        free(key);
        free(buffer);
    }
    map.shrink_databuff();
}

template<class sym_t>
void * lpg_build::record_phrases(void *data) {

    auto lms_data = (lms_info<sym_t> *) data;

    bool s_type, prev_s_type = S_TYPE;
    sym_t curr_sym, prev_sym;

    string_t curr_lms(2, lms_data->sym_width);
    prev_sym = lms_data->ifs.read(lms_data->end);
    curr_lms.push_back(prev_sym);

    for(size_t i = lms_data->end; i-- > lms_data->start;){

        curr_sym = lms_data->ifs.read(i);

        //                                     L_TYPE   S_TYPE*
        //                                        ---- ----
        //this is a junction between two strings = ...$ $...
        if(lms_data->is_suffix(curr_sym)){
            if(!curr_lms.empty()){
                lms_data->store_phrase(curr_lms);
            }
            curr_lms.clear();
            s_type = S_TYPE;
        } else {
            if (curr_sym < prev_sym) {//S_TYPE type
                s_type = S_TYPE;
            } else if (curr_sym == prev_sym) {
                s_type = prev_s_type;
            } else {//L_TYPE type
                s_type = L_TYPE;

                if(prev_s_type == S_TYPE) {//Left-most suffix
                    curr_lms.pop_back();
                    if(!curr_lms.empty()){
                        lms_data->store_phrase(curr_lms);
                    }
                    curr_lms.clear();
                    curr_lms.push_back(prev_sym);
                }
            }
        }
        curr_lms.push_back(curr_sym);
        prev_sym = curr_sym;
        prev_s_type = s_type;
    }

    assert(curr_lms[0]!=1);
    if(!curr_lms.empty()){
        lms_data->store_phrase(curr_lms);
    }

    lms_data->ofs.close();
    lms_data->ifs.close();
    pthread_exit(nullptr);
}

template<class sym_type>
std::vector<std::pair<size_t, size_t>> lpg_build::compute_thread_ranges(size_t n_threads, std::string& i_file,
                                                                        sdsl::int_vector<2>& phrase_desc) {
    std::vector<std::pair<size_t, size_t>> thread_ranges;

    i_file_stream<sym_type> is(i_file, BUFFER_SIZE);
    size_t n_chars = is.tot_cells;
    assert(n_chars>0);
    size_t sym_per_thread = INT_CEIL(n_chars, n_threads);
    size_t start, end;
    size_t eff_threads = INT_CEIL(n_chars, sym_per_thread);

    for(size_t i=0;i<eff_threads;i++){
        start = (i * sym_per_thread);
        end = std::min<size_t>(((i + 1) * sym_per_thread), n_chars-1);

        start = start==0? 0 : size_t(prev_lms_sym(start, is, phrase_desc)+1);
        long long tmp_end = prev_lms_sym(end, is, phrase_desc);
        end = tmp_end<0?  0 : size_t(tmp_end);
        if(start<end){
            thread_ranges.emplace_back(start, end);
        }
    }
    is.close();
    return thread_ranges;
}

void lpg_build::decomp(size_t nt, sdsl::int_vector<> &rules, bv_t::select_1_type &rlim_ss, bv_t &rem_nt,
                       bv_t::rank_1_type &rem_nt_rs,
                       ivb_t &dec) {

    std::stack<size_t> stack;
    stack.push(nt);
    size_t start, end, tmp;
    while(!stack.empty()){
        tmp = stack.top();
        stack.pop();
        if(rem_nt[tmp]){
            start = rlim_ss(tmp)+1;
            end = rlim_ss(tmp+1);
            for(size_t j=end+1;j-->start;){
                stack.push(rules[j]);
            }
        }else{
            dec.push_back(tmp - rem_nt_rs(tmp));
        }
    }
}

void lpg_build::simplify_grammar(lpg_build::plain_grammar_t &p_gram, bv_t &rem_nts, bv_t::rank_1_type &rem_nts_rs) {

    std::cout<<"  Simplifying the grammar"<<std::endl;

    bv_t r_lim;
    sdsl::load_from_file(r_lim, p_gram.rules_lim_file);
    bv_t::select_1_type r_lim_ss(&r_lim);

    sdsl::int_vector<> rules;
    sdsl::load_from_file(rules, p_gram.rules_file);
    size_t max_tsym = p_gram.max_tsym;

    bv_t is_rl;
    sdsl::load_from_file(is_rl, p_gram.is_rl_file);
    sdsl::int_vector_buffer<1> new_is_rl(p_gram.is_rl_file, std::ios::out);

    ivb_t new_rules(p_gram.rules_file, std::ios::out);
    sdsl::int_vector_buffer<1> new_r_lim(p_gram.rules_lim_file, std::ios::out);

    //compress the alphabet
    size_t cont=0;
    uint8_t byte_sym;
    std::unordered_map<size_t, uint8_t> new_sym_map;
    for(size_t k=0;k<=p_gram.max_tsym;k++){
        auto res = p_gram.sym_map.find(k);
        if(res!=p_gram.sym_map.end()){
            byte_sym = res->second;
            new_sym_map.insert({cont, byte_sym});
            cont++;
        }
    }
    std::swap(new_sym_map, p_gram.sym_map);
    assert(p_gram.sym_map.size()==p_gram.sigma);

    for(size_t k=0;k<p_gram.sigma;k++){
        new_r_lim.push_back(true);
        new_rules.push_back(k);
    }

    size_t pos, tr_rule=p_gram.sigma;
    for(size_t i=max_tsym+1,curr_rule=max_tsym+1;i<rules.size();curr_rule++){
        assert(r_lim[i-1]);
        pos = i;
        while(!r_lim[i]) i++;
        i++;


        if(!rem_nts[curr_rule]){
            if(!is_rl[curr_rule]){//regular rule
                for(size_t j=pos;j<i;j++){
                    if(rem_nts[rules[j]]){
                        decomp(rules[j], rules, r_lim_ss, rem_nts, rem_nts_rs, new_rules);
                    }else{
                        new_rules.push_back(rules[j]-rem_nts_rs(rules[j]));
                    }
                }
            }else{//run-length rule
                assert((i-pos)==2);
                new_rules.push_back(rules[pos]-rem_nts_rs(rules[pos]));
                new_rules.push_back(rules[pos+1]);
                new_is_rl[tr_rule] = true;
            }
            new_r_lim[new_rules.size()-1]=true;
            tr_rule++;
        }
    }

    //mark the compressed string as a non-run-length rule
    new_is_rl[tr_rule-1]=false;

    size_t rm_nt =rem_nts_rs(rem_nts.size());
    float rm_per = float(rm_nt)/float(p_gram.r)*100;
    float comp_rat = float(new_rules.size())/float(rules.size());

    std::cout<<"    Stats:"<<std::endl;
    std::cout<<"      Grammar size before:  "<<p_gram.g<<std::endl;
    std::cout<<"      Grammar size after:   "<<new_rules.size()<<std::endl;
    std::cout<<"      Deleted nonterminals: "<<rm_nt<<" ("<<rm_per<<"%)"<<std::endl;
    std::cout<<"      Compression ratio:    "<<comp_rat<<std::endl;

    /*cont=0;
    for(auto && new_rule : new_rules){
        if(new_rule==0){
            cont++;
        }
    }
    std::cout<<"there are "<<cont<<" zeroes"<<std::endl;*/

    new_rules.close();
    new_r_lim.close();
    new_is_rl.close();
    p_gram.r -= rm_nt;
    p_gram.g = new_rules.size();
}

//TODO: this functions are just for debugging
void shape_int(size_t nt,  sdsl::int_vector<>& rules, sdsl::bit_vector & rem_nts,
                      sdsl::bit_vector ::select_1_type &r_lim_ss, std::string& shape){

    if(rem_nts[nt]){
        shape.push_back('(');
        size_t start = r_lim_ss(nt)+1;
        size_t end = r_lim_ss(nt+1);
        for(size_t k=start;k<=end;k++){
            shape_int(rules[k], rules, rem_nts, r_lim_ss, shape);
        }
        shape.push_back(')');
    }else{
        shape.push_back('(');
        shape.push_back(')');
    }
}
std::string shape(size_t nt,  sdsl::int_vector<>& rules, sdsl::bit_vector & rem_nts,
                  sdsl::bit_vector::select_1_type &r_lim_ss){

    std::string shape;
    size_t start = r_lim_ss(nt)+1;
    size_t end = r_lim_ss(nt+1);

    shape.push_back('(');
    for(size_t k=start;k<=end;k++){
        shape_int(rules[k], rules, rem_nts, r_lim_ss, shape);
    }
    shape.push_back(')');
    return shape;
}
//

void lpg_build::rec_dc_int(gram_wrapper_t& gram_w, size_t nt, uint8_t lev, size_t &pos, bool rm,
                           std::vector<uint8_t> &lms_breaks) {

    if(gram_w.rm_nts[nt]){
        size_t start = gram_w.r_lim_ss(nt) + 1;
        size_t end = gram_w.r_lim_ss(nt + 1);
        for(size_t k=start; k<=end; k++) {
            rec_dc_int(gram_w, gram_w.rules[k], lev - 1, pos, k == end, lms_breaks);
            if(k==end && !rm){
                lms_breaks.emplace_back(lev + 1);
                //std::cout<<pos<<" "<<(int)lev+1<<std::endl;
                pos++;

                if(nt>=gram_w.first_non_lms_nts){
                    if(gram_w.non_lms_lvl[nt-gram_w.first_non_lms_nts] == 0){
                        gram_w.non_lms_lvl[nt-gram_w.first_non_lms_nts] = lev + 1;
                    }
                }
            }
        }
    }else if(!rm){
        assert(nt<gram_w.p_gram.r-1);
        lms_breaks.emplace_back(lev + 1);
        //std::cout<<pos<<" "<<(int)lev+1<<std::endl;
        pos++;

        //TODO remove asserts afterwards
        if(nt>=gram_w.first_non_lms_nts){
            assert(gram_w.is_rl[nt]);
            if(gram_w.non_lms_lvl[nt-gram_w.first_non_lms_nts] == 0){
                gram_w.non_lms_lvl[nt-gram_w.first_non_lms_nts] = lev + 1;
            }else{
                assert(gram_w.non_lms_lvl[nt-gram_w.first_non_lms_nts] == lev + 1);
            }
        }
    }
}

std::vector<uint8_t>
lpg_build::rec_dc(gram_wrapper_t& gram_w, size_t nt, uint8_t lev) {
    //std::cout<<"lev: "<<(int)lev<<" nt"<<nt<<" : "<<shape(nt, rules, rem_nts, r_lim_ss)<<std::endl;
    std::vector<uint8_t> lms_breaks;
    size_t start = gram_w.r_lim_ss(nt) + 1;
    size_t end = gram_w.r_lim_ss(nt + 1);
    size_t pos =0;
    for(size_t k=start; k<=end; k++) {
        rec_dc_int(gram_w, gram_w.rules[k], lev - 1, pos, k == end, lms_breaks);
    }
    //std::cout<<""<<std::endl;
    return lms_breaks;
}

void lpg_build::create_lvl_breaks(plain_grammar_t &p_gram, bv_t &rem_nts, bv_t::rank_1_type &rem_nts_rs) {

    std::cout<<"  Computing the grid level of every nonterminal cut"<<std::endl;
    ivb_t breaks_buff(p_gram.lvl_breaks_file, std::ios::out);

    sdsl::int_vector<> rules;
    sdsl::load_from_file(rules, p_gram.rules_file);

    bv_t r_lim;
    sdsl::load_from_file(r_lim, p_gram.rules_lim_file);
    bv_t::select_1_type r_lim_ss(&r_lim);

    bv_t is_rl;
    sdsl::load_from_file(is_rl, p_gram.is_rl_file);

    size_t curr_rule = p_gram.max_tsym+1;
    size_t lev=1;

    //a simple wrapper for the grammar
    size_t first_non_lms_nt=p_gram.max_tsym+1;
    for(unsigned long lev_nter : p_gram.rules_per_level){
        first_non_lms_nt+=lev_nter;
    }
    gram_wrapper_t gram_w(p_gram, rules, r_lim_ss, is_rl, rem_nts, first_non_lms_nt);
    //

    for(unsigned long lev_nter : p_gram.rules_per_level){
        for(size_t j=0;j<lev_nter;j++){
            if(!rem_nts[curr_rule]){
                auto breaks = rec_dc(gram_w, curr_rule, lev);
                breaks_buff.push_back(curr_rule-rem_nts_rs(curr_rule));
                breaks_buff.push_back(breaks.size());
                assert(!breaks.empty());
                for(auto const& lvl : breaks){
                    breaks_buff.push_back(lvl);
                }
            }
            curr_rule++;
        }
        lev++;
    }

    while(curr_rule<p_gram.r-1){
        if(!rem_nts[curr_rule]){
            //TODO generalize this idea for any type of rule. For the moment,
            // this is only for run-length compressed rules
            breaks_buff.push_back(curr_rule-rem_nts_rs(curr_rule));
            breaks_buff.push_back(1);
            assert(gram_w.non_lms_lvl[curr_rule-first_non_lms_nt]>0);
            breaks_buff.push_back(gram_w.non_lms_lvl[curr_rule-first_non_lms_nt]);
        }
        curr_rule++;
    }
}

lpg_build::bv_t lpg_build::mark_nonterminals(lpg_build::plain_grammar_t &p_gram) {

    size_t max_tsym = p_gram.max_tsym;

    bv_t r_lim;
    sdsl::load_from_file(r_lim, p_gram.rules_lim_file);
    bv_t::select_1_type r_lim_ss(&r_lim);

    sdsl::int_vector<> rules;
    sdsl::load_from_file(rules, p_gram.rules_file);

    sdsl::int_vector_buffer<1> is_rl(p_gram.is_rl_file, std::ios::in);
    size_t rl_rule=max_tsym+1;
    for(unsigned long freq : p_gram.rules_per_level){
        rl_rule+=freq;
    }
    assert(!is_rl[rl_rule-1]);

    bv_t rem_nts(p_gram.r, false);

    //compute which nonterminals are repeated and
    // which have a rule of length 1
    sdsl::int_vector<2> rep_nts(p_gram.r + 1, 0);

    size_t r_len=1, cont=0, curr_rule=max_tsym+1,k=max_tsym+1;
    while(k<rules.size()){
        if(curr_rule>=rl_rule && curr_rule<p_gram.r-1){//run-length compressed rules
            rep_nts[rules[k++]] = 2;
            assert(r_lim[k] && r_len==1);
            r_len=0;
            curr_rule++;
        }else{
            //get the frequency of every symbol
            if(rep_nts[rules[k]]<2){
                rep_nts[rules[k]]++;
            }

            if(r_lim[k]){
                //mark the rules with a right-hand side of length 1
                if(r_len==1 && rules[k]>=max_tsym){
                    rem_nts[curr_rule] = true;
                    cont++;
                    assert(r_lim[k-1]);
                }
                r_len=0;
                curr_rule++;
            }
        }
        r_len++;
        k++;
    }

    //mark the rules to remove
    //1) rules whose left-hand side has length one
    //2) terminal symbols between [min_sym..max_sym] with
    // frequency zero: to compress the alphabet
    for(size_t i=0;i<p_gram.r;i++){
        //mark the rules with frequency one
        if(!rem_nts[i]){
            rem_nts[i] = rep_nts[i]==0 || (rep_nts[i]==1 && i > max_tsym);
        }
    }

    //unmark unique nonterminals that
    // appear in the compressed string
    for(size_t i=r_lim_ss(p_gram.r-1)+1; i<rules.size();i++){
        rem_nts[rules[i]] = false;
    }

    //unmark the run-length rules
    while(rl_rule<p_gram.r-1){
        if(is_rl[rl_rule]){
            rem_nts[rl_rule] = false;
        }
        rl_rule++;
    }

    rem_nts[p_gram.r-1] = false;//unmark the compressed string

    return rem_nts;
}

void lpg_build::colex_nt_sort(plain_grammar_t &p_gram) {

    //that this point, the grammar is supposed to be collapsed
    std::cout<<"  Reordering nonterimnals in Colex"<<std::endl;

    //sort the nonterminals in reverse lexicographical order
    bv_t r_lim;
    sdsl::load_from_file(r_lim, p_gram.rules_lim_file);
    bv_t::select_1_type rlim_ss(&r_lim);

    sdsl::int_vector<> rules;
    sdsl::load_from_file(rules, p_gram.rules_file);

    bv_t is_rl;
    sdsl::load_from_file(is_rl, p_gram.is_rl_file);

    std::stack<size_t> stack;
    std::vector<uint8_t> tmp_buff;
    std::vector<std::pair<size_t, std::vector<uint8_t>>> nt_pairs;
    size_t start, end, tmp_sym, curr_rule=0;

    //decompress all the nonterinals
    while(curr_rule<p_gram.r-1){

        stack.push(curr_rule);

        while(!stack.empty()){
            tmp_sym = stack.top();
            stack.pop();

            if(tmp_sym>=p_gram.sigma){
                start = rlim_ss(tmp_sym)+1;
                if(!is_rl[tmp_sym]){
                    end = rlim_ss(tmp_sym+1);
                    for(size_t j=end+1;j-->start;){
                        stack.push(rules[j]);
                    }
                }else{
                    assert(r_lim[start+1]);
                    for(size_t j=0;j<rules[start+1];j++){
                        stack.push(rules[start]);
                    }
                }
            }else{//we reach a terminal
                tmp_buff.push_back((uint8_t)tmp_sym);
            }
        }

        /*if(curr_rule==1182963) {
            std::cout << curr_rule << " : ";
            for (char sym : tmp_buff) {
                std::cout << (size_t) sym << " ";
            }
            std::cout << "" << std::endl;
        }*/

        nt_pairs.emplace_back(curr_rule, std::move(tmp_buff));
        curr_rule++;
    }

    //sort them in reverse lexicographical order
    std::sort(nt_pairs.begin(), nt_pairs.end(), [](auto& left, auto& right){
        size_t l1=left.second.size()-1;
        size_t l2=right.second.size()-1;
        for(size_t len=std::min(l1,l2)+1; len-->0;l1--,l2--){
            if(left.second[l1]!=right.second[l2]){
                return left.second[l1]<right.second[l2];
            }
        }
        return left.second.size()<right.second.size();
    });

    //TODO testing
    /*std::cout<<"the string: "<<nt_pairs[0].first<<std::endl;
    for(size_t i=0;i<nt_pairs[0].second.size();i++){
        std::cout<<(size_t)nt_pairs[0].second[i]<<std::endl;
    }*/
    //

    ivb_t new_rules(p_gram.rules_file, std::ios::out);
    sdsl::int_vector_buffer<1> new_rlim(p_gram.rules_lim_file, std::ios::out);
    bv_t new_is_rl(is_rl.size(), false);

    //set terminal symbols
    std::vector<size_t> renames(p_gram.r, 0);

    //reorder nonterminal symbols
    size_t pos, rank=0;
    std::unordered_map<size_t, uint8_t> new_map;
    for(auto const &nt_data : nt_pairs){

        //update the bit vector with the rl-rules
        if(is_rl[nt_data.first]) new_is_rl[rank] = true;

        //store the new nonterimnal value
        // for terminal symbols
        if(nt_data.first<p_gram.sigma){
            uint8_t byte_sym = p_gram.sym_map[nt_data.first];
            new_map[rank] = byte_sym;
        }

        //rename the nonterminal
        renames[nt_data.first] = rank++;

        pos = nt_data.first==0?  0 : rlim_ss(nt_data.first)+1;

        while(!r_lim[pos]){
            new_rules.push_back(rules[pos++]);
        }
        new_rules.push_back(rules[pos]);
        assert(r_lim[pos]);
        new_rlim[new_rules.size()-1] = true;

        /*std::cout<<nt_data.first<<" = "<<rank-1<<" -> ";
        for(size_t j=nt_data.second.size();j-->0;){
            std::cout<<(int)nt_data.second[j]<<" ";
        }
        std::cout<<""<<std::endl;*/
    }
    std::swap(p_gram.sym_map, new_map);
    assert(p_gram.sym_map.size()==p_gram.sigma);

    //insert compressed string
    for(size_t i=rlim_ss(p_gram.r-1)+1;i<rules.size();i++){
        new_rules.push_back(rules[i]);
    }
    new_rlim[new_rules.size()-1] = true;

    assert(new_rules.size()==rules.size());

    //rename nonterminal references
    curr_rule=0;
    size_t k=0;
    while(k<new_rules.size()){
        /*if(renames[new_rules[k]]==0){
            std::cout<<new_rules[k]<<" "<<p_gram.r<<" "<<rlim_ss(new_rules[k])+1<<" "<<rlim_ss(new_rules[k]+1)<<std::endl;
        }*/
        new_rules[k] = renames[new_rules[k]];
        if(new_is_rl[curr_rule]){
            assert(!new_rlim[k] && new_rlim[k+1]);
            k++;
        }
        if(new_rlim[k]){
            curr_rule++;
        }
        k++;
    }

    //TODO testing
    /*size_t cont=0;
    for(auto && new_rule : new_rules){
        if(new_rule==0){
            cont++;
        }
    }
    std::cout<<"there are "<<cont<<" zeroes"<<std::endl;*/
    //

    //renaming the rules in the lvl_breaks
    /*ivb_t lvl_breaks(p_gram.lvl_breaks_file, std::ios::in | std::ios::out);
    k=0;
    while(k<lvl_breaks.size()){
        lvl_breaks[k] = renames[lvl_breaks[k]];
        k++;
        k+=lvl_breaks[k]+1;
    }
    assert(k==lvl_breaks.size());
    lvl_breaks.close();*/

    sdsl::store_to_file(new_is_rl, p_gram.is_rl_file);
    new_rules.close();
    new_rlim.close();
}

void lpg_build::run_length_compress(lpg_build::plain_grammar_t &p_gram, sdsl::cache_config& config) {

    std::cout<<"  Run-length compressing the grammar"<<std::endl;

    sdsl::int_vector_buffer<> rules(p_gram.rules_file, std::ios::in);
    sdsl::int_vector_buffer<1> r_lim(p_gram.rules_lim_file, std::ios::in);
    sdsl::int_vector_buffer<1> is_rl_bv(p_gram.is_rl_file, std::ios::out);

    std::string rl_rules_file = sdsl::cache_file_name("tmp_rl_file", config);
    sdsl::int_vector_buffer<> rl_rules(rl_rules_file, std::ios::out);
    std::string rl_r_lim_file = sdsl::cache_file_name("tmp_rl_lim_file", config);
    sdsl::int_vector_buffer<1> rl_r_lim(rl_r_lim_file, std::ios::out);

    size_t run_len=1;
    size_t new_id = p_gram.r-1;
    size_t tmp_sym;
    bit_hash_table<size_t, 44> ht;
    string_t pair(2,sdsl::bits::hi(rules.size())+1);

    for(size_t i=0;i<p_gram.sigma;i++){
        rl_rules.push_back(i);
        rl_r_lim.push_back(true);
    }

    size_t i=p_gram.sigma+1;
    while(i<rules.size()-p_gram.c){
        //std::cout<<rules[i-1]<<" ";
        if(rules[i]!=rules[i-1] || r_lim[i-1]){
            if(run_len>1){
                pair.write(0, rules[i-1]);
                pair.write(1, run_len);
                auto res = ht.insert(pair.data(), pair.n_bits(), 0);
                if(res.second){
                    tmp_sym = new_id;
                    ht.insert_value_at(*res.first, tmp_sym);
                    new_id++;
                }else{
                    tmp_sym = res.first.value();
                }
                //std::cout<<"("<<pair[0]<<" "<<pair[1]<<")"<<" "<<tmp_sym<<std::endl;
            }else{
                tmp_sym = rules[i-1];
            }
            run_len=0;

            rl_rules.push_back(tmp_sym);
            if(r_lim[i-1]){
                rl_r_lim[rl_rules.size()-1] = true;
            }
        }
        /*if(r_lim[i-1]){
            std::cout<<""<<std::endl;
        }*/
        run_len++;
        i++;
    }

    assert(r_lim[i-1]);
    if(run_len>1){
        pair.write(0, rules[i-1]);
        pair.write(1, run_len);
        auto res = ht.insert(pair.data(), pair.n_bits(), 0);
        if(res.second){
            tmp_sym = new_id;
            ht.insert_value_at(*res.first, tmp_sym);
        }else{
            tmp_sym = res.first.value();
        }
    }else{
        tmp_sym = rules[i-1];
    }
    rl_rules.push_back(tmp_sym);
    rl_r_lim[rl_rules.size()-1] = true;

    const bitstream<buff_t>& stream = ht.get_data();
    key_wrapper key_w{pair.width(), ht.description_bits(), stream};
    new_id = p_gram.r-1;
    for(auto const& phrase : ht){
        rl_rules.push_back(key_w.read(phrase, 0));
        rl_rules.push_back(key_w.read(phrase, 1));
        rl_r_lim[rl_rules.size()-1] = true;
        is_rl_bv[new_id++]=true;
    }
    is_rl_bv[new_id]=false;

    for(size_t k=rules.size()-p_gram.c;k<rules.size();k++){
        rl_rules.push_back(rules[k]);
    }
    rl_r_lim[rl_rules.size()-1] = true;

    p_gram.r+=ht.size();
    p_gram.g = rl_rules.size();

    std::cout<<"    Stats:"<<std::endl;
    std::cout<<"      Grammar size before:        "<<rules.size()<<std::endl;
    std::cout<<"      Grammar size after:         "<<rl_rules.size()<<std::endl;
    std::cout<<"      Number of new nonterminals: "<<ht.size()<<std::endl;
    std::cout<<"      Compression ratio:          "<<float(rl_rules.size())/float(rules.size())<<std::endl;

    rules.close();
    r_lim.close();
    rl_rules.close();
    rl_r_lim.close();
    is_rl_bv.close();

    rename(rl_rules_file.c_str(), p_gram.rules_file.c_str());
    rename(rl_r_lim_file.c_str(), p_gram.rules_lim_file.c_str());
}

//lpg_build::plain_grammar_t lpg_build::repair_compress(lpg_build::plain_grammar_t &p_gram, sdsl::cache_config &config) {
//    //build sequence of rules to apply repair
//    sdsl::int_vector<> rules_buff;
//    std::ifstream _buf(p_gram.rules_file,std::ios::in);
//    sdsl::load(rules_buff,_buf);
//    bvb_t rules_lim_buff(p_gram.rules_lim_file);
//    bvb_t is_rules_len(p_gram.is_rl_file);
//    size_t num_rules = p_gram.r;
//    size_t initial_rule = p_gram.r - 1;
//    size_t id = 0;
//
//    std::string repair_input_buffer_file = sdsl::cache_file_name("repair_input_buffer", config);
//    std::vector<size_t> repair_input_buffer;
//
//    std::vector<size_t> right_hand;
//    for (size_t i = 0; i < rules_lim_buff.size(); ++i) {
//            right_hand.push_back(rules_buff[i]);
//
//            if(rules_lim_buff[i] == 1 && !is_rules_len[id] && !p_gram.isTerminal(id)){
//                //write to the buffer
//                repair_input_buffer.resize(repair_input_buffer.size() + right_hand.size() + 1,0);
//                std::copy(right_hand.begin(),right_hand.end(),repair_input_buffer.end() - right_hand.size() - 1);
//                repair_input_buffer[repair_input_buffer.size() - 1 ] = num_rules++;
//                right_hand.clear();
//                ++id;
//            }else{
//                if(rules_lim_buff[i] == 1){
//                    right_hand.clear();
//                    ++id;
//                }
//
//            }
//    }
//    right_hand.clear();
//    //write file RULE_1$1RULE_2$2.....
//    std::cout<<"REPAIR INPUT("<<repair_input_buffer_file<<")"<<std::endl;
//
//    std::fstream fout(repair_input_buffer_file, std::ios::out);
//    if(!fout.good()) return p_gram;
//    for (const size_t &c : repair_input_buffer) {
//        uint32_t  cr = c;
//        fout.write((const char *) &cr,sizeof (uint32_t));
//    }
//    fout.close();
//    repair_input_buffer.clear();
//    //apply repair
//    std::string command_compressing = REPAIR_DIR + repair_input_buffer_file ;
//    if(system(command_compressing.c_str()) < 0) return p_gram;
//    //rebuild the grammar...
//
//    std::fstream inR(repair_input_buffer_file + ".R", std::ios::in);
//    std::fstream inC(repair_input_buffer_file + ".C", std::ios::in);
//
//    // read Repair rules...
//    uint32_t maxsigma = 0;
//    inR.read((char*)&maxsigma,sizeof(int));
//    uint32_t X,Y,c = 0;
//    uint32_t delta = num_rules - p_gram.r;
//
//    std::unordered_map<size_t,std::pair<size_t,size_t>> RPrules;
//    while(!inR.eof() && inR.read((char*)&X,sizeof(uint32_t)) && inR.read((char*)&Y,sizeof(uint32_t)))
//    {
//        if(X > p_gram.r) X -= delta;
//        if(Y > p_gram.r) Y -= delta;
//
//        RPrules[c+p_gram.r] = std::make_pair(X,Y);
//        ++c;
//    }
//    // read Repair first rule ...
//    X = 0;
//    std::vector<std::vector<size_t>> S;
//    while(!inC.eof() && inC.read((char*)&X,sizeof(uint32_t)))
//    {
//        if(X >= p_gram.r && X < num_rules){
//            //delimitador de regla...
//            S.push_back(right_hand);
//            right_hand.clear();
//        }else{
//            if(X >= num_rules)
//                X -= delta;
//            right_hand.push_back(X);
//        }
//    }
//    right_hand.clear();
//    //merge rules.....
//    id = 0;
//    size_t r_id = 0;
//    size_t totalSize = 0;
//    std::map<size_t,std::vector<size_t>> FinalGrammar;
//    for (size_t i = 0; i < rules_lim_buff.size(); ++i) {
//        right_hand.push_back(rules_buff[i]);
//        if(rules_lim_buff[i] == 1){
//            if(is_rules_len[id] || p_gram.isTerminal(id) ){
//                FinalGrammar[id] = right_hand;
//            }
//            else{
//                FinalGrammar[id] = S[r_id];
//                ++r_id;
//            }
//            totalSize += right_hand.size();
//            ++id;
//            right_hand.clear();
//        }
//    }
//    std::cout<<initial_rule<<std::endl;
//    S.clear();
//    for (const auto &r : RPrules) {
//        std::vector<size_t> rh;
//        rh.push_back(r.second.first);
//        rh.push_back(r.second.second);
//        FinalGrammar[r.first] = rh;
//    }
//
//    totalSize += RPrules.size()*2;
//    RPrules.clear();
//
//
//
//    //swap initial rule with last rule...
//    auto tt = FinalGrammar[initial_rule];
//    FinalGrammar[initial_rule]= FinalGrammar[FinalGrammar.size()-1];
//    FinalGrammar[FinalGrammar.size()-1] = tt;
//    std::map<size_t,size_t> renumerate_rule;
//    renumerate_rule[initial_rule] = FinalGrammar.size()-1;
//    renumerate_rule[FinalGrammar.size()-1] =initial_rule;
//    initial_rule = FinalGrammar.size()-1;
//    for ( auto &item : FinalGrammar) {
//        for (size_t i = 0; i < item.second.size(); ++i) {
//            if(item.second[i] == FinalGrammar.size()-1)
//                item.second[i] = initial_rule;
//        }
//    }
//
//
//    // compute runs and rename the rules to keep the original format rules.....runs....S
//    sdsl::bit_vector run(initial_rule + 1 ,0);
//    size_t nzeros = 0;
//    for (size_t i = 0 ; i < initial_rule; ++i) {
//        if(i < is_rules_len.size())
//            run[i] = is_rules_len[i];
//        if(!run[i])
//            ++nzeros;
//    }
//    std::cout<<"nzeros-"<<nzeros<<std::endl;
//    size_t cont = 0;
//    std::map<size_t,size_t> rrename;
//    // do not rename the initial rule
//    for (size_t i = 0 ; i < initial_rule; ++i) {
//        if(run[i] == 1){
//            rrename[i] = nzeros++;
//        }
//        else{
//            rrename[i] = cont++;
//        }
//    }
//    rrename[initial_rule] = initial_rule;
//    std::cout<<"Rename"<<std::endl;
//    for (const auto &item : rrename) {
//        std::cout<<item.first<<"-"<<item.second<<std::endl;
//    }
//    std::map<size_t,std::vector<size_t>> NewFinalGrammar;
//    for ( auto &item : FinalGrammar) {
//        for (size_t i = 0; i < item.second.size(); ++i) {
//                item.second[i] = rrename[item.second[i]];
//        }
//        NewFinalGrammar[rrename[item.first]] = item.second;
//    }
//    FinalGrammar.clear();
//    std::function<void (const size_t&, const uint32_t&)> dfs;
//    sdsl::bit_vector mark(NewFinalGrammar.size(),0);
//    std::map<uint32_t,size_t> level;
//    uint32_t max_level = 0;
//
//    dfs = [&mark,&dfs,&p_gram,&level,&max_level,&run,&NewFinalGrammar](const size_t &X,const uint32_t &_l){
//
//        if(mark[X]) return ;
//
//        mark[X] = 1;
//
//        if(_l > max_level) max_level = _l;
//
//        auto it = level.find(_l);
//        if(!run[X] && !p_gram.isTerminal(X)){
//            if(it == level.end())
//                level[_l] = 1;
//            else it->second++;
//        }
//
//        if(p_gram.isTerminal(X)) return;
//
//        if(run[X]){
//            for (size_t i = 0; i < NewFinalGrammar[X][1]; ++i) {
//                dfs(NewFinalGrammar[X][0],_l+1);
//            }
//        }
//        else
//            for (size_t i = 0; i < NewFinalGrammar[X].size(); ++i)
//                    dfs(NewFinalGrammar[X][i],_l+1);
//
//    };
//
//    dfs(initial_rule,0);
//    std::vector<size_t> _l(level.size(),0);
//    for (const auto &l : level) _l[max_level - l.first] = l.second;
//    level.clear();
//
//
//#ifdef CHECK_OCC
////    std::function<void (const size_t&, std::ofstream&)> dec;
////    dec = [&p_gram,&FinalGrammar,&run, &dec](const size_t &X,std::ofstream& out){
////        if(p_gram.isTerminal(X)){
//////            std::cout<<p_gram.sym_map[X];
////            uint8_t c = p_gram.sym_map[X];
////            out.write((const char*)&c,sizeof(uint8_t));
////        }
////        else if(run[X]){
////            for (size_t i = 0; i < FinalGrammar[X][1]; ++i) {
////                dec(FinalGrammar[X][0],out);
////            }
////        }
////        else
////            for (size_t i = 0; i < FinalGrammar[X].size(); ++i)
////                    dec(FinalGrammar[X][i],out);
////    };
////    std::string decompresion_repair = sdsl::cache_file_name("decompresion_repair2", config);
////    std::ofstream out_dec(decompresion_repair,std::ios::out);
//////    std::cout<<"Text"<<std::endl;
////    dec(1,out_dec);
//////    std::cout<<std::endl;
//#endif
//    std::cout<<"Size before re-pair compression:"<<p_gram.g<<"\n";
//    std::cout<<"num-rules before re-pair compression:"<<p_gram.r<<"\n";
//    std::cout<<"Size after re-pair compression:"<<totalSize<<"\n";
//    std::cout<<"num-rules after re-pair compression:"<<NewFinalGrammar.size()<<"\n";
//
//
//    lpg_build::plain_grammar_t p_gram_rp;
//
//    p_gram_rp.sigma = p_gram.sigma;
//    p_gram_rp.r = NewFinalGrammar.size();
//    p_gram_rp.g = totalSize;
//    p_gram_rp.c = NewFinalGrammar[NewFinalGrammar.size()-1].size();
//    p_gram_rp.max_tsym = p_gram.max_tsym;
//    p_gram_rp.sym_map = p_gram.sym_map;
//    sdsl::int_vector<> rules(totalSize,0);
//    sdsl::bit_vector limit(totalSize,0);
//    size_t j = 0;
//    std::cout<<"New grammar"<<std::endl;
//    for (const auto &item : NewFinalGrammar) {
//        std::cout<<item.first<<"->";
//        for (size_t i = 0; i <item.second.size() ; ++i) {
//            std::cout<<item.second[i]<<" ";
//            rules[j++] = item.second[i];
//        }
//        std::cout<<std::endl;
//        limit[j-1] = true;
//    }
//
////    p_gram.print_grammar();
//
//    std::ofstream rules_out(p_gram.rules_file,std::ios::out|std::ios::binary);
//    sdsl::serialize(rules,rules_out);
//    rules_out.close();
//    p_gram_rp.rules_file = p_gram.rules_file;
//    std::ofstream limit_out(p_gram.rules_lim_file,std::ios::out|std::ios::binary);
//    sdsl::serialize(limit,limit_out);
//    limit_out.close();
//    p_gram_rp.rules_lim_file = p_gram.rules_lim_file;
//
//    //todo verificar como arreglar la consistencia de esto
//    p_gram_rp.rules_per_level = _l;//p_gram.rules_per_level;
//
//    std::ofstream rl_out(p_gram.is_rl_file,std::ios::out|std::ios::binary);
//    sdsl::serialize(run,rl_out);
//    rl_out.close();
//    p_gram_rp.is_rl_file = p_gram.is_rl_file;
//
//    p_gram_rp.lvl_breaks_file = p_gram.lvl_breaks_file;
//    p_gram_rp.lms_rounds = p_gram.lms_rounds;
//
//
//
//    p_gram_rp.print_grammar();
//
//    return p_gram_rp;
//}

void lpg_build::plain_grammar_t::save_to_file(std::string& output_file){

    size_t buffer[255];

    std::ofstream of_stream(output_file, std::ofstream::binary);

    //write number of rules and alphabet size
    buffer[0] = sigma;
    buffer[1] = r;
    buffer[2] = c;
    buffer[3] = g;
    buffer[4] = max_tsym;
    of_stream.write((char *) buffer, sizeof(size_t)*5);

    assert(sym_map.size()==sigma);

    //write symbol mappings
    size_t left, right;
    for(auto const pair : sym_map){
        left = pair.first;
        right = pair.second;
        of_stream.write((char *) &left, sizeof(size_t));
        of_stream.write((char *) &right, sizeof(size_t));
    }

    buffer[0] = rules_file.size();
    of_stream.write((char *) buffer, sizeof(size_t));
    of_stream.write(rules_file.c_str(), rules_file.size());

    buffer[0] = rules_lim_file.size();
    of_stream.write((char *) buffer, sizeof(size_t));
    of_stream.write(rules_lim_file.c_str(), rules_lim_file.size());

    buffer[0] = is_rl_file.size();
    of_stream.write((char *) buffer, sizeof(size_t));
    of_stream.write(is_rl_file.c_str(), is_rl_file.size());

    buffer[0] = lvl_breaks_file.size();
    of_stream.write((char *) buffer, sizeof(size_t));
    of_stream.write(lvl_breaks_file.c_str(), lvl_breaks_file.size());

    size_t len = rules_per_level.size();
    of_stream.write((char *) &len, sizeof(size_t));
    of_stream.write((char *)rules_per_level.data(), sizeof(size_t)*rules_per_level.size());

    of_stream.close();
}

void lpg_build::plain_grammar_t::load_from_file(std::string &g_file){

    size_t buffer[255];
    std::ifstream fp(g_file, std::ifstream::binary);

    fp.read((char *)buffer, sizeof(size_t)*5);

    sigma = buffer[0];
    r = buffer[1];
    c = buffer[2];
    g = buffer[3];
    max_tsym = buffer[4];

    for(size_t i=0;i<sigma;i++){
        fp.read((char *)&buffer[0], sizeof(size_t));
        fp.read((char *)&buffer[1], sizeof(size_t));
        sym_map.insert({buffer[0], buffer[1]});
    }

    fp.read((char *)buffer, sizeof(size_t));
    auto tmp_file = reinterpret_cast<char *>(malloc(buffer[0]+1));
    fp.read(tmp_file, buffer[0]);
    tmp_file[buffer[0]]='\0';
    rules_file = std::string(tmp_file);

    fp.read((char *)buffer, sizeof(size_t));
    tmp_file = reinterpret_cast<char *>(realloc(tmp_file, buffer[0]+1));
    fp.read(tmp_file, buffer[0]);
    tmp_file[buffer[0]] = '\0';
    rules_lim_file = std::string(tmp_file);

    fp.read((char *)buffer, sizeof(size_t));
    tmp_file = reinterpret_cast<char *>(realloc(tmp_file, buffer[0]+1));
    fp.read(tmp_file, buffer[0]);
    tmp_file[buffer[0]] = '\0';
    is_rl_file = std::string(tmp_file);

    fp.read((char *)buffer, sizeof(size_t));
    tmp_file = reinterpret_cast<char *>(realloc(tmp_file, buffer[0]+1));
    fp.read(tmp_file, buffer[0]);
    tmp_file[buffer[0]] = '\0';
    lvl_breaks_file = std::string(tmp_file);

    size_t len=0;
    fp.read((char *)&len, sizeof(size_t));
    auto tmp_arr = reinterpret_cast<size_t*>(malloc(sizeof(size_t)*len));
    fp.read((char*)tmp_arr, sizeof(size_t)*len);

    for(size_t i=0;i<len;i++){
        rules_per_level.push_back(tmp_arr[i]);
    }
    fp.close();
    free(tmp_file);
}
