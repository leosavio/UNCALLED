#ifndef SEED_TRACKER_HPP
#define SEED_TRACKER_HPP

#include "aligner.hpp"
#include <set>
#include <vector>
#include <iostream>

class ReadAln {
    public:
    static u8 WIN_LEN;

    u64 ref_st_;
    Range ref_en_;
    u32 evt_st_,
        evt_en_,
        total_len_;


    ReadAln(Range ref_st, u32 evt_st);
    ReadAln(const ReadAln &r);
    ReadAln();
    u64 ref_start_base() const;
    u8 update(ReadAln &new_aln);
    void print(std::ostream &out, bool newline, bool print_all) const;
    Range ref_range() const;
    bool is_valid();

    friend bool operator< (const ReadAln &q1, const ReadAln &q2);
    friend std::ostream &operator<< (std::ostream &out, const ReadAln &a);
};

//static const ReadAln NULL_ALN;

bool operator< (const ReadAln &q1, const ReadAln &q2);
std::ostream &operator<< (std::ostream &out, const ReadAln &a);

class SeedTracker {

    public:

    std::set<ReadAln> alignments_;
    std::multiset<u32> all_lens_;
    const u64 ref_len_;
    const float mean_thresh_, top_thresh_;
    const u8 min_aln_len_;

    float max_len_, len_sum_;

    SeedTracker(u64 ref_len, float mean_thresh, float top_thresh, u8 min_aln_len, u8 win_len);

    ReadAln add_seed(Result seed);
    ReadAln add_seeds(const std::vector<Result> &seeds);

    void reset();

    std::vector<ReadAln> get_alignments(u8 min_len);

    //static double top_ratio(int min_len);
    bool check_ratio(const ReadAln &aln, double ratio);

    void print(std::ostream &out, u16 max_out);
};


#endif
