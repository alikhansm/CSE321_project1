// common.h - Shared data definitions for CSE321 Project #1
// All three tree implementations share these types
#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>
#include <cstdint>


struct StudentRecord {
    int         student_id;
    std::string name;
    char        gender;   // 'M' or 'F'
    float       gpa;
    float       height;
    float       weight;
};

// The simplified storage model (Section 3.1 of the manual):
//   Key = Student ID, RID = array index into the in-memory record table.
using Key = int;
using RID = int;

// Empty / "not found" RID sentinel.
constexpr RID INVALID_RID = -1;

#endif // COMMON_H
