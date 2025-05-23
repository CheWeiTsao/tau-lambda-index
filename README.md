# tau-lambda index

To generate the locations of all the minimal factors:
./gen_mf [input text path] [output mf path] [$\tau_\ell$] [$\tau_u$] [$\lambda$] [delimiter/terminal symbols](optional, default with '\0')
(Let $\tau_u$ = 0 means to generate the original self-index)
+ 'delimiter/terminal symbols': those symbols you use to delimit each sample or the terminal symbols (e.g. "#$" or "\\n" for some special symbol)

To gengerate the tau-lambda index of the text (need to gen the minimal factors first):
./gen_index [input mf path] [output index path] [self-index type] [log](optional)
+ 'self-index type':
    - 1: r-index
    - 2: LZ77
    - 3: LMS
    <!-- - 4: DCC version
    - 5: compact_suffix_trie
    - 6: hybrid -->

To generate query patterns: (pattern length is equal to $\lambda$)
./gen_pattern [input mf path] [output pattern path] [pattern counts]

To do the locations queries:
./locate [input index path] [input pattern path] [output results path]