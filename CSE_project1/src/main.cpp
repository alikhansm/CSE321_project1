// main.cpp - Driver for all experimental workloads required by the
//
// Workloads:
//   1. Insertion & parameter tuning  (varying d in {3, 5, 10})
//      - total execution time
//      - final node utilisation
//      - total number of node splits
//   2. Point search                  (10 000 random keys, mean lookup time)
//   3. Range query                   (avg GPA & height of male students,
//                                     IDs in [20200000, 20210000])
//   4. Deletion                      (2 000 random keys)
#include "common.h"
#include "btree.h"
#include "bstartree.h"
#include "bplustree.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// --------------------------------------------------------------------------
// CSV loader
// --------------------------------------------------------------------------
static std::vector<StudentRecord> loadCSV(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "ERROR: cannot open " << path << "\n";
        std::exit(1);
    }
    std::vector<StudentRecord> recs;
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (first) { first = false; continue; }   // skip header
        if (line.empty()) continue;
        StudentRecord r;
        // Tokenize on commas.  Names contain a single space, so this is safe.
        std::stringstream ss(line);
        std::string tok;
        std::getline(ss, tok, ','); r.student_id = std::atoi(tok.c_str());
        std::getline(ss, r.name, ',');
        std::getline(ss, tok, ','); r.gender = tok.empty() ? '?' : tok[0];
        std::getline(ss, tok, ','); r.gpa    = std::atof(tok.c_str());
        std::getline(ss, tok, ','); r.height = std::atof(tok.c_str());
        std::getline(ss, tok, ','); r.weight = std::atof(tok.c_str());
        recs.push_back(r);
    }
    return recs;
}

using Clock   = std::chrono::high_resolution_clock;
using Seconds = std::chrono::duration<double>;
using Micros  = std::chrono::duration<double, std::micro>;

// --------------------------------------------------------------------------
// Insertion experiment for one tree type, one d value
// --------------------------------------------------------------------------
struct InsertResult {
    int        d;
    std::string tree;
    double     time_sec;
    double     utilization;
    long long  splits;
    long long  redistributes;   // B*-tree only
    int        nodes;
    int        height;
};

template <typename Tree>
static double timed_insert(Tree& tree, const std::vector<StudentRecord>& recs) {
    auto t0 = Clock::now();
    for (size_t i = 0; i < recs.size(); ++i) {
        tree.insert(recs[i].student_id, static_cast<RID>(i));
    }
    auto t1 = Clock::now();
    return std::chrono::duration_cast<Seconds>(t1 - t0).count();
}

// --------------------------------------------------------------------------
// Point search experiment
// --------------------------------------------------------------------------
template <typename Tree>
static double timed_point_search(Tree& tree, const std::vector<int>& keys,
                                 const std::vector<RID>& expected_rids) {
    auto t0 = Clock::now();
    long long hits = 0;
    for (size_t i = 0; i < keys.size(); ++i) {
        RID r;
        if (tree.search(keys[i], r) && r == expected_rids[i]) ++hits;
    }
    auto t1 = Clock::now();
    if (hits != static_cast<long long>(keys.size())) {
        std::cerr << "WARNING: point search hit " << hits << "/" << keys.size()
                  << " (expected all hits)\n";
    }
    return std::chrono::duration_cast<Seconds>(t1 - t0).count();
}

// --------------------------------------------------------------------------
// Range query experiment

struct RangeResult {
    double time_sec;
    long long male_count;
    double avg_gpa;
    double avg_height;
};

static RangeResult range_btree(BTree& tree,
                               const std::vector<StudentRecord>& recs,
                               int lo, int hi) {
    auto t0 = Clock::now();
    long long male_count = 0;
    double gpa_sum = 0.0, h_sum = 0.0;
    // No range structure in a plain B-tree: we probe each ID in the range.
    for (int id = lo; id <= hi; ++id) {
        RID r;
        if (tree.search(id, r)) {
            const StudentRecord& s = recs[r];
            if (s.gender == 'M') {
                gpa_sum += s.gpa;
                h_sum   += s.height;
                ++male_count;
            }
        }
    }
    auto t1 = Clock::now();
    RangeResult res;
    res.time_sec   = std::chrono::duration_cast<Seconds>(t1 - t0).count();
    res.male_count = male_count;
    res.avg_gpa    = male_count ? gpa_sum / male_count : 0.0;
    res.avg_height = male_count ? h_sum   / male_count : 0.0;
    return res;
}

static RangeResult range_bstartree(BStarTree& tree,
                                   const std::vector<StudentRecord>& recs,
                                   int lo, int hi) {
    auto t0 = Clock::now();
    long long male_count = 0;
    double gpa_sum = 0.0, h_sum = 0.0;
    for (int id = lo; id <= hi; ++id) {
        RID r;
        if (tree.search(id, r)) {
            const StudentRecord& s = recs[r];
            if (s.gender == 'M') {
                gpa_sum += s.gpa;
                h_sum   += s.height;
                ++male_count;
            }
        }
    }
    auto t1 = Clock::now();
    RangeResult res;
    res.time_sec   = std::chrono::duration_cast<Seconds>(t1 - t0).count();
    res.male_count = male_count;
    res.avg_gpa    = male_count ? gpa_sum / male_count : 0.0;
    res.avg_height = male_count ? h_sum   / male_count : 0.0;
    return res;
}

static RangeResult range_bplustree(BPlusTree& tree,
                                   const std::vector<StudentRecord>& recs,
                                   int lo, int hi) {
    auto t0 = Clock::now();
    std::vector<RID> rids;
    rids.reserve(1024);
    tree.rangeQuery(lo, hi, rids);
    long long male_count = 0;
    double gpa_sum = 0.0, h_sum = 0.0;
    for (RID r : rids) {
        const StudentRecord& s = recs[r];
        if (s.gender == 'M') {
            gpa_sum += s.gpa;
            h_sum   += s.height;
            ++male_count;
        }
    }
    auto t1 = Clock::now();
    RangeResult res;
    res.time_sec   = std::chrono::duration_cast<Seconds>(t1 - t0).count();
    res.male_count = male_count;
    res.avg_gpa    = male_count ? gpa_sum / male_count : 0.0;
    res.avg_height = male_count ? h_sum   / male_count : 0.0;
    return res;
}

// --------------------------------------------------------------------------
// Deletion experiment
// --------------------------------------------------------------------------
template <typename Tree>
static double timed_delete(Tree& tree, const std::vector<int>& keys,
                           int& deleted_count_out) {
    auto t0 = Clock::now();
    int deleted = 0;
    for (int k : keys) if (tree.remove(k)) ++deleted;
    auto t1 = Clock::now();
    deleted_count_out = deleted;
    return std::chrono::duration_cast<Seconds>(t1 - t0).count();
}

// --------------------------------------------------------------------------
// Pretty printer
// --------------------------------------------------------------------------
static void section(const std::string& title) {
    std::cout << "\n========================================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "========================================================\n";
}

// --------------------------------------------------------------------------
// Main
// --------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <data.csv> [results_dir]\n";
        return 1;
    }
    const std::string data_path    = argv[1];
    const std::string results_dir  = (argc >= 3) ? argv[2] : "results";

    // Force a deterministic locale: avoid surprises in printf with %f.
    setlocale(LC_NUMERIC, "C");

    // ---------------- Load data ----------------
    std::cout << "Loading dataset from " << data_path << " ...\n";
    auto t0 = Clock::now();
    std::vector<StudentRecord> records = loadCSV(data_path);
    auto t1 = Clock::now();
    std::cout << "  Loaded " << records.size() << " records in "
              << std::chrono::duration_cast<Seconds>(t1 - t0).count()
              << " s\n";

    // The Student ID is the Key, the array index is the RID.


    // ---------------- 1. Insertion experiments ----------------
    section("1) Insertion & Parameter Tuning");
    std::vector<int> orders = {3, 5, 10};
    std::vector<InsertResult> insert_results;

   
    for (int d : orders) {
        // ---- B-tree ----
        {
            BTree t(d);
            double sec = timed_insert(t, records);
            InsertResult r{d, "B-tree", sec, t.getUtilization(),
                           t.getSplitCount(), 0,
                           t.getNodeCount(), t.getHeight()};
            insert_results.push_back(r);
        }
        // ---- B*-tree ----
        {
            BStarTree t(d);
            double sec = timed_insert(t, records);
            InsertResult r{d, "B*-tree", sec, t.getUtilization(),
                           t.getSplitCount(), t.getRedistributeCount(),
                           t.getNodeCount(), t.getHeight()};
            insert_results.push_back(r);
        }
        // ---- B+-tree ----
        {
            BPlusTree t(d);
            double sec = timed_insert(t, records);
            InsertResult r{d, "B+-tree", sec, t.getUtilization(),
                           t.getSplitCount(), 0,
                           t.getNodeCount(), t.getHeight()};
            insert_results.push_back(r);
        }
    }

    std::printf("%-3s  %-9s  %10s  %14s  %10s  %14s  %8s  %6s\n",
                "d", "Tree", "Time(s)", "Utilization", "Splits",
                "Redistributes", "Nodes", "Height");
    std::printf("---  ---------  ----------  --------------  ----------"
                "  --------------  --------  ------\n");
    for (const auto& r : insert_results) {
        std::printf("%-3d  %-9s  %10.4f  %13.2f%%  %10lld  %14lld  %8d  %6d\n",
                    r.d, r.tree.c_str(), r.time_sec,
                    r.utilization * 100.0, r.splits, r.redistributes,
                    r.nodes, r.height);
    }

    // ---------------- 2-4. Use fixed d for the rest -----------
    constexpr int D = 10;  // a comfortable middle of the road
    std::cout << "\n[For experiments 2-4 we reuse trees built with d = "
              << D << "]\n";

    BTree     btree    (D);
    BStarTree bstar    (D);
    BPlusTree bplus    (D);
    for (size_t i = 0; i < records.size(); ++i) {
        btree.insert(records[i].student_id, static_cast<RID>(i));
        bstar.insert(records[i].student_id, static_cast<RID>(i));
        bplus.insert(records[i].student_id, static_cast<RID>(i));
    }

    // ---------------- 2. Point search ----------------
    section("2) Point Search (10,000 random keys)");
    constexpr int kQueryCount = 10000;
    std::mt19937 rng(123456789);
    std::vector<int> q_idx(records.size());
    for (size_t i = 0; i < records.size(); ++i) q_idx[i] = static_cast<int>(i);
    std::shuffle(q_idx.begin(), q_idx.end(), rng);
    q_idx.resize(kQueryCount);

    std::vector<int> q_keys(kQueryCount);
    std::vector<RID> q_rids(kQueryCount);
    for (int i = 0; i < kQueryCount; ++i) {
        q_keys[i] = records[q_idx[i]].student_id;
        q_rids[i] = q_idx[i];
    }

    double t_btree_search  = timed_point_search(btree, q_keys, q_rids);
    double t_bstar_search  = timed_point_search(bstar, q_keys, q_rids);
    double t_bplus_search  = timed_point_search(bplus, q_keys, q_rids);

    auto print_search = [&](const char* name, double total) {
        double mean_us = (total / kQueryCount) * 1e6;
        std::printf("  %-9s  total=%9.4f s   mean=%8.4f us/query\n",
                    name, total, mean_us);
    };
    print_search("B-tree",   t_btree_search);
    print_search("B*-tree",  t_bstar_search);
    print_search("B+-tree",  t_bplus_search);

    // ---------------- 3. Range query ----------------
    section("3) Range Query: male students with ID in [202000000, 202100000]");

    constexpr int LO = 202000000;
    constexpr int HI = 202100000;

    RangeResult r_b   = range_btree    (btree, records, LO, HI);
    RangeResult r_bs  = range_bstartree(bstar, records, LO, HI);
    RangeResult r_bp  = range_bplustree(bplus, records, LO, HI);

    std::printf("  %-9s  time=%9.4f s   males=%6lld   avg_gpa=%5.2f   avg_h=%6.1f\n",
                "B-tree",  r_b.time_sec,  r_b.male_count,  r_b.avg_gpa,  r_b.avg_height);
    std::printf("  %-9s  time=%9.4f s   males=%6lld   avg_gpa=%5.2f   avg_h=%6.1f\n",
                "B*-tree", r_bs.time_sec, r_bs.male_count, r_bs.avg_gpa, r_bs.avg_height);
    std::printf("  %-9s  time=%9.4f s   males=%6lld   avg_gpa=%5.2f   avg_h=%6.1f\n",
                "B+-tree", r_bp.time_sec, r_bp.male_count, r_bp.avg_gpa, r_bp.avg_height);

    // ---------------- 4. Deletion ----------------
    section("4) Deletion (2,000 random keys)");
    constexpr int kDeleteCount = 2000;
    std::vector<int> del_idx(records.size());
    for (size_t i = 0; i < records.size(); ++i) del_idx[i] = static_cast<int>(i);
    std::shuffle(del_idx.begin(), del_idx.end(), rng);
    del_idx.resize(kDeleteCount);

    std::vector<int> del_keys(kDeleteCount);
    for (int i = 0; i < kDeleteCount; ++i) del_keys[i] = records[del_idx[i]].student_id;

    int n1, n2, n3;
    double t_btree_del = timed_delete(btree, del_keys, n1);
    double t_bstar_del = timed_delete(bstar, del_keys, n2);
    double t_bplus_del = timed_delete(bplus, del_keys, n3);

    std::printf("  %-9s  time=%9.4f s   deleted=%5d   util_after=%6.2f%%   nodes=%d\n",
                "B-tree",  t_btree_del, n1,
                btree.getUtilization() * 100.0, btree.getNodeCount());
    std::printf("  %-9s  time=%9.4f s   deleted=%5d   util_after=%6.2f%%   nodes=%d\n",
                "B*-tree", t_bstar_del, n2,
                bstar.getUtilization() * 100.0, bstar.getNodeCount());
    std::printf("  %-9s  time=%9.4f s   deleted=%5d   util_after=%6.2f%%   nodes=%d\n",
                "B+-tree", t_bplus_del, n3,
                bplus.getUtilization() * 100.0, bplus.getNodeCount());

    // ---------------- Verify integrity post-delete ----------------
    section("5) Post-Deletion Search Integrity");
    int found_b = 0, found_bs = 0, found_bp = 0;
    int missing_b = 0, missing_bs = 0, missing_bp = 0;

    // Build a set of deleted IDs for fast lookup
    std::vector<bool> is_deleted(records.size(), false);
    for (int idx : del_idx) is_deleted[idx] = true;

    for (size_t i = 0; i < records.size(); ++i) {
        RID r;
        bool b_hit  = btree.search(records[i].student_id, r);
        bool bs_hit = bstar.search(records[i].student_id, r);
        bool bp_hit = bplus.search(records[i].student_id, r);
        if (is_deleted[i]) {
            if (!b_hit ) ++missing_b;
            if (!bs_hit) ++missing_bs;
            if (!bp_hit) ++missing_bp;
        } else {
            if (b_hit ) ++found_b;
            if (bs_hit) ++found_bs;
            if (bp_hit) ++found_bp;
        }
    }
    std::printf("  Expected: %zu kept records found, %d deleted records absent\n",
                records.size() - kDeleteCount, kDeleteCount);
    std::printf("  B-tree :  found=%d   deleted-confirmed=%d\n",
                found_b,  missing_b);
    std::printf("  B*-tree:  found=%d   deleted-confirmed=%d\n",
                found_bs, missing_bs);
    std::printf("  B+-tree:  found=%d   deleted-confirmed=%d\n",
                found_bp, missing_bp);

    // ---------------- Write CSV summary ----------------
    {
        std::ofstream f(results_dir + "/insertion_results.csv");
        f << "d,tree,time_sec,utilization,splits,redistributes,nodes,height\n";
        for (const auto& r : insert_results) {
            f << r.d << "," << r.tree << "," << r.time_sec << ","
              << r.utilization << "," << r.splits << ","
              << r.redistributes << "," << r.nodes << "," << r.height << "\n";
        }
    }
    {
        std::ofstream f(results_dir + "/search_results.csv");
        f << "tree,d,total_time_sec,mean_us_per_query\n";
        f << "B-tree,"  << D << "," << t_btree_search << "," << (t_btree_search/kQueryCount)*1e6 << "\n";
        f << "B*-tree," << D << "," << t_bstar_search << "," << (t_bstar_search/kQueryCount)*1e6 << "\n";
        f << "B+-tree," << D << "," << t_bplus_search << "," << (t_bplus_search/kQueryCount)*1e6 << "\n";
    }
    {
        std::ofstream f(results_dir + "/range_results.csv");
        f << "tree,d,time_sec,male_count,avg_gpa,avg_height\n";
        f << "B-tree,"  << D << "," << r_b .time_sec << "," << r_b .male_count << ","
                              << r_b .avg_gpa  << "," << r_b .avg_height << "\n";
        f << "B*-tree," << D << "," << r_bs.time_sec << "," << r_bs.male_count << ","
                              << r_bs.avg_gpa  << "," << r_bs.avg_height << "\n";
        f << "B+-tree," << D << "," << r_bp.time_sec << "," << r_bp.male_count << ","
                              << r_bp.avg_gpa  << "," << r_bp.avg_height << "\n";
    }
    {
        std::ofstream f(results_dir + "/deletion_results.csv");
        f << "tree,d,time_sec,deleted,util_after,nodes_after\n";
        f << "B-tree,"  << D << "," << t_btree_del << "," << n1 << ","
                                << btree.getUtilization() << "," << btree.getNodeCount() << "\n";
        f << "B*-tree," << D << "," << t_bstar_del << "," << n2 << ","
                                << bstar.getUtilization() << "," << bstar.getNodeCount() << "\n";
        f << "B+-tree," << D << "," << t_bplus_del << "," << n3 << ","
                                << bplus.getUtilization() << "," << bplus.getNodeCount() << "\n";
    }
    std::cout << "\nResult CSVs written under " << results_dir << "/\n";

    return 0;
}
