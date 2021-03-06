/* MIT License
 *
 * Copyright (c) 2018 Sam Kovaka <skovaka@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _INCL_BWAFMI
#define _INCL_BWAFMI

#include <string>
#include <climits>
#include <utility>
#include <cstring>
#include <bwa/bwa.h>
#include <bwa/utils.h>
#include <pdqsort.h>
#include "util.hpp"
#include "bp.hpp"
#include "range.hpp"

#ifdef PYBIND
#include <pybind11/pybind11.h>
#endif

template <KmerLen KLEN>
class SubSeq {
    public:
    SubSeq(const u8 *pacseq, u64 st, u64 en) :
        pacseq_(pacseq),
        st_(st),
        en_(en),
        size_(en_-st_-KLEN) {}

    u16 operator[](u64 i) {
        i += st_;
        u64 pst = i >> 2;
        u32 comb = *((u32 *) &pacseq_[pst]);
        u8 shift = i & 3;
        return (u16) ( (comb >> ((16-KLEN)<<1)) & KMASK(KLEN) );
    }

    u64 size() {
        return size_;
    }

    std::string to_str() {
        std::string str(size_, 'N');
        u64 pst = st_ >> 2,
            pen = ((en_) >> 2)+1;
        u8 bst = (st_&3), ben;
        u64 i = 0;
        for (u64 j = pst; j < pen; j++) {
            ben = j == pen-1 ? (en_&3) : 4;
            for (u8 k = bst; k < ben; k++) {
                str[i++] = BASE_CHARS[(pacseq_[j] >> ((k^3) << 1) ) & 3];
            }
            bst = 0;
        }
        return str;
    }

    private:
    const u8 *pacseq_;
    u64 st_, en_, pst_, pen_, bst_, ben_,
        size_;
};

//From submods/bwa/bwtindex.c
#define BWA_BLOCK_SIZE 10000000

template <KmerLen KLEN>
class BwaIndex {
    public:

    static void create(const std::string &fasta_fname, 
                      const std::string &prefix = "") {

        std::string prefix_auto = prefix.empty() ? fasta_fname : prefix;

        bwa_idx_build(fasta_fname.c_str(), 
                      prefix.c_str(), 
                      BWTALGO_AUTO,
                      BWA_BLOCK_SIZE);
    }

    BwaIndex() :
        index_(NULL),
        bns_(NULL),
        pacseq_(NULL),
        klen_(KLEN),
        kmer_ranges_(kmer_count<KLEN>()),
        loaded_(false) {}

    BwaIndex(const std::string &prefix, bool pacseq=false) : BwaIndex() {
        if (!prefix.empty()) load_index(prefix);
        if (pacseq) load_pacseq();
    }

    void load_index(const std::string &prefix) {
        std::string bwt_fname = prefix + ".bwt",
                    sa_fname = prefix + ".sa";

        index_ = bwt_restore_bwt(bwt_fname.c_str());
        bwt_restore_sa(sa_fname.c_str(), index_);
        bns_ = bns_restore(prefix.c_str());

        for (u16 k = 0; k < kmer_ranges_.size(); k++) {

            Range r = get_base_range(kmer_head<KLEN>(k));
            for (u8 i = 1; i < KLEN; i++) {
                r = get_neighbor(r, kmer_base<KLEN>(k, i));
            }

            kmer_ranges_[k] = r;
        }

        loaded_ = true;
    }

    bool is_loaded() {
        return loaded_;
    }

    void load_pacseq() {
        if (!pacseq_loaded()) {
            //Copied from bwa/bwase.c
            pacseq_ = (u8*) calloc(bns_->l_pac/4+1, 1);
            err_fread_noeof(pacseq_, 1, bns_->l_pac/4+1, bns_->fp_pac);
        }   
    }

    void destroy() {
        if (index_ != NULL) { 
            bwt_destroy(index_);
        }
        if (bns_ != NULL) { 
            bns_destroy(bns_);
        }
    }

    Range get_neighbor(Range r1, u8 base) const {
        u64 os, oe;
        bwt_2occ(index_, r1.start_ - 1, r1.end_, base, &os, &oe);
        return Range(index_->L2[base] + os + 1, index_->L2[base] + oe);
    }

    Range get_kmer_range(u16 kmer) const {
        return kmer_ranges_[kmer];
    }

    u64 get_kmer_count(u16 kmer) const {
        return kmer_ranges_[kmer].length();
    }

    Range get_base_range(u8 base) const {
        return Range(index_->L2[base], index_->L2[base+1]);
    }

    u64 sa(u64 i) const {
        return bwt_sa(index_, i);
    }

    u64 size() const {
        return index_->seq_len;
    }

    int get_rid(u64 sa_loc) {
        return bns_pos2rid(bns_, sa_loc);
    }

    std::pair<int, u32> get_ref_coord(u64 sa_loc) {
        int rid = get_rid(sa_loc);
        return {
            rid, 
            sa_loc - bns_->anns[rid].offset
        };
    }

    std::string get_ref_name(int rid) {
        return std::string(bns_->anns[rid].name);
    }

    u64 get_ref_len(int rid) {
        return bns_->anns[rid].len;
    }

    u64 get_sa_loc(const std::string &name, u64 coord) {
        for (int i = 0; i < bns_->n_seqs; i++) {
            if (strcmp(bns_->anns[i].name, name.c_str()) == 0) {
                return bns_->anns[i].offset + coord;
            }
        }
        return 0;
    }

    u64 translate_loc(u64 sa_loc, std::string &ref_name, u64 &ref_loc) const {
        i32 rid = bns_pos2rid(bns_, sa_loc);
        if (rid < 0) return 0;

        ref_name = std::string(bns_->anns[rid].name);
        ref_loc = sa_loc - bns_->anns[rid].offset;
        return bns_->anns[rid].len;
    }

    std::vector< std::pair<std::string, u64> > get_seqs() const {
        std::vector< std::pair<std::string, u64> > seqs;

        for (i32 i = 0; i < bns_->n_seqs; i++) {
            bntann1_t ann = bns_->anns[i];
            std::string name = std::string(ann.name);
            seqs.push_back( std::pair<std::string, u64>(name, ann.len) );
        }

        return seqs;
    }

    u64 coord_to_pacseq(std::string name, u64 coord) {
        i32 i;
        for (i = 0; i < bns_->n_seqs; i++) {
            if (strcmp(name.c_str(), bns_->anns[i].name) == 0)
                return bns_->anns[i].offset + coord;
        }
        return INT_MAX;
    } 

    bool pacseq_loaded() const {
        return pacseq_ != NULL;
    }

    std::vector<u16> get_kmers(std::string nm, u64 st, u64 en) {
        u64 sti = coord_to_pacseq(nm, st),
            eni = coord_to_pacseq(nm, en);
        return get_kmers(sti, eni);
    }

    std::vector<u16> get_kmers(u64 st, u64 en) {
        return seq_to_kmers<KLEN>(pacseq_, st, en);
    }

    u8 get_base(u64 i) {
        return (pacseq_[i>>2] >> ( ((3^i)&3) << 1 )) & 3;
    }

    using FwdRevCoords = std::pair< std::vector<u64>, std::vector<u64> >;

    //Returns all FM index coordinates which translate into reference 
    //coordinates that overlap the specified range
    FwdRevCoords range_to_fms(std::string ref_name, u64 start, u64 end) {

        std::vector<u64> fwd_fms, rev_fms;

        auto ref_len = size() / 2;

        u32 slop = static_cast<int>( ceil(log(ref_len) / log(4)) );

        u64 pac_min = coord_to_pacseq(ref_name, start),
            pac_max = pac_min + (end - start) - 1;

        u64 fwd_st;
        if (ref_len - pac_max > slop) {
            fwd_st = pac_max + slop;
        } else {
            fwd_st = ref_len - 1;
        }

        Range r = get_base_range(get_base(fwd_st));
        for (u64 i = fwd_st-1; i >= pac_max && i <= fwd_st; i--) {
            r = get_neighbor(r, get_base(i));
        }

        u64 sa_loc = 0;
        for (u64 f = r.start_; f <= r.end_; f++) {
            sa_loc = sa(f);
            if (sa_loc == pac_max) {
                r = Range(f,f);
                break;
            }
        }

        fwd_fms.push_back(r.start_);
        for (u64 i = pac_max-1; i >= pac_min && i < pac_max; i--) {
            r = get_neighbor(r, get_base(i));
            fwd_fms.push_back(r.start_);
        }

        u64 rev_st;
        if (pac_min > slop) {
            rev_st = pac_min - slop;
        } else {
            rev_st = 0;
        }

        r = get_base_range(BASE_COMP_B[get_base(rev_st)]);
        for (u64 i = rev_st+1; i <= pac_min; i++) {
            r = get_neighbor(r, BASE_COMP_B[get_base(i)]);
        }

        for (u64 f = r.start_; f <= r.end_; f++) {
            sa_loc = size() - sa(f);
            if (sa_loc == pac_min) {
                r = Range(f,f);
                break;
            }
        }

        rev_fms.push_back(r.start_);
        for (u64 i = pac_min+1; i <= pac_max; i++) {
            r = get_neighbor(r, BASE_COMP_B[get_base(i)]);
            rev_fms.push_back(r.start_);
        }

        //pdqsort(fwd_fms.begin(), fwd_fms.end());
        //pdqsort(rev_fms.begin(), rev_fms.end());

        return FwdRevCoords(rev_fms, fwd_fms);
    }

    #ifdef PYBIND

    #define PY_BWA_INDEX_METH(P) c.def(#P, &BwaIndex<KLEN>::P);

    static void pybind_defs(pybind11::class_<BwaIndex<KLEN>> &c) {
        c.def(pybind11::init<>());
        c.def(pybind11::init<const std::string &, bool>());
        PY_BWA_INDEX_METH(create);
        PY_BWA_INDEX_METH(load_index);
        PY_BWA_INDEX_METH(is_loaded);
        PY_BWA_INDEX_METH(load_pacseq);
        PY_BWA_INDEX_METH(destroy);
        PY_BWA_INDEX_METH(get_neighbor);
        PY_BWA_INDEX_METH(get_kmer_range);
        PY_BWA_INDEX_METH(get_kmer_count);
        PY_BWA_INDEX_METH(get_base_range);
        PY_BWA_INDEX_METH(sa);
        PY_BWA_INDEX_METH(size);
        PY_BWA_INDEX_METH(translate_loc);
        PY_BWA_INDEX_METH(get_seqs);
        PY_BWA_INDEX_METH(coord_to_pacseq);
        PY_BWA_INDEX_METH(pacseq_loaded);
        PY_BWA_INDEX_METH(get_base);
        PY_BWA_INDEX_METH(get_rid);
        PY_BWA_INDEX_METH(get_sa_loc);
        PY_BWA_INDEX_METH(get_ref_coord);
        PY_BWA_INDEX_METH(get_ref_name);
        PY_BWA_INDEX_METH(get_ref_len);
        PY_BWA_INDEX_METH(range_to_fms);
        c.def("get_kmers", static_cast< std::vector<u16> (BwaIndex::*)(u64, u64)> (&BwaIndex::get_kmers) );
        c.def("get_kmers", static_cast< std::vector<u16> (BwaIndex::*)(std::string, u64, u64)> (&BwaIndex::get_kmers) );
    }

    #endif

    private:
    bwt_t *index_;
    bntseq_t *bns_;
    u8 *pacseq_;
    KmerLen klen_;
    std::vector<Range> kmer_ranges_;
    bool loaded_;
};



#endif
