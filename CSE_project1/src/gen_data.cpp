// gen_data.cpp - Generates a CSV with N student records

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

const std::vector<std::string> kFirstNamesM = {
    "Taejoon", "Minjun", "Seojun", "Hajun", "Jiho", "Doyun", "Eunwoo",
    "Hyunwoo", "Junseo", "Jihoon", "Jaewon", "Sungmin", "Yejun", "Daniel",
    "Kevin", "Brian", "Andrew", "Jason", "Eric", "Michael", "David",
    "Joshua", "Aiden", "Liam", "Noah", "Ethan", "Lucas", "Mason", "Logan",
    "Owen", "Henry", "Jack", "Ryan", "Nathan", "Tyler", "Brandon"
};
const std::vector<std::string> kFirstNamesF = {
    "Seoyun", "Jiwoo", "Seoah", "Haeun", "Soyeon", "Yuna", "Eunseo",
    "Yejin", "Jiyoo", "Soeun", "Hayoon", "Yerin", "Sarah", "Hannah",
    "Emily", "Jessica", "Sophia", "Olivia", "Emma", "Ava", "Mia",
    "Isabella", "Charlotte", "Amelia", "Harper", "Evelyn", "Abigail",
    "Ella", "Elizabeth", "Camila", "Luna", "Aria", "Chloe", "Penelope"
};
const std::vector<std::string> kLastNames = {
    "Kim", "Lee", "Park", "Choi", "Jung", "Kang", "Cho", "Yoon", "Jang",
    "Lim", "Han", "Oh", "Seo", "Shin", "Kwon", "Hwang", "Ahn", "Song",
    "Yoo", "Hong", "Bae", "Moon", "Yang", "Son", "Baek", "Heo", "Nam",
    "No", "Ha", "Koo", "Min", "Sim", "Yum", "Joo", "Ryu", "Pyo"
};

std::string buildName(std::mt19937& rng, char gender) {
    const auto& firsts = (gender == 'M') ? kFirstNamesM : kFirstNamesF;
    std::uniform_int_distribution<size_t> df(0, firsts.size() - 1);
    std::uniform_int_distribution<size_t> dl(0, kLastNames.size() - 1);
    return firsts[df(rng)] + " " + kLastNames[dl(rng)];
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <output.csv> [count=100000] [seed=42]\n";
        return 1;
    }
    const std::string out_path = argv[1];
    const int   count = (argc >= 3) ? std::atoi(argv[2]) : 100000;
    const unsigned seed = (argc >= 4) ? static_cast<unsigned>(std::atoi(argv[3])) : 42u;

    if (count <= 0) {
        std::cerr << "count must be positive\n";
        return 1;
    }

    std::ofstream out(out_path);
    if (!out) {
        std::cerr << "Cannot open " << out_path << " for writing\n";
        return 1;
    }

    std::mt19937 rng(seed);

    // Build a pool of unique student IDs spread evenly across 7 cohorts
    // (admission years 2020..2026).  We then shuffle the pool to randomise
    // the insertion order.
    std::unordered_set<int> used;
    used.reserve(static_cast<size_t>(count) * 2);
    std::vector<int> ids;
    ids.reserve(count);
    std::uniform_int_distribution<int> year_dist(2020, 2026);
    std::uniform_int_distribution<int> tail_dist(0, 99999);

    while (static_cast<int>(ids.size()) < count) {
        int year = year_dist(rng);
        int tail = tail_dist(rng);
        int id   = year * 100000 + tail;   // e.g. 2020 * 1e5 + 12345 = 202012345
        if (used.insert(id).second) ids.push_back(id);
    }
    std::shuffle(ids.begin(), ids.end(), rng);

    std::uniform_int_distribution<int> gender_dist(0, 1);
    std::uniform_real_distribution<float> gpa_dist(0.00f, 4.30f);
    std::uniform_real_distribution<float> height_m(160.0f, 190.0f);
    std::uniform_real_distribution<float> height_f(150.0f, 178.0f);
    std::uniform_real_distribution<float> weight_m(55.0f, 95.0f);
    std::uniform_real_distribution<float> weight_f(42.0f, 75.0f);

    out << "Student ID,Name,Gender,GPA,Height,Weight\n";
    for (int i = 0; i < count; ++i) {
        char gender = gender_dist(rng) ? 'M' : 'F';
        std::string name = buildName(rng, gender);
        float gpa    = gpa_dist(rng);
        float height = (gender == 'M') ? height_m(rng) : height_f(rng);
        float weight = (gender == 'M') ? weight_m(rng) : weight_f(rng);

        const char* gender_word = (gender == 'M') ? "Male" : "Female";
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%d,%s,%s,%.2f,%.1f,%.1f,\n",
                      ids[i], name.c_str(), gender_word, gpa, height, weight);
        out << buf;
    }
    out.close();
    std::cout << "Wrote " << count << " records to " << out_path << "\n";
    return 0;
}
