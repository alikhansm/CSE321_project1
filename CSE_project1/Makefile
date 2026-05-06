# Makefile for CSE321 Project #1
# B-tree, B*-tree, and B+-tree implementation

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
SRCDIR   := src
DATADIR  := data
RESDIR   := results

# Source files for the experiment driver
EXP_SRCS := $(SRCDIR)/main.cpp \
            $(SRCDIR)/btree.cpp \
            $(SRCDIR)/bstartree.cpp \
            $(SRCDIR)/bplustree.cpp

GEN_SRCS := $(SRCDIR)/gen_data.cpp

EXP_HDRS := $(SRCDIR)/common.h \
            $(SRCDIR)/btree.h \
            $(SRCDIR)/bstartree.h \
            $(SRCDIR)/bplustree.h

# Default target: build both binaries
all: gen_data run_experiments

# Build the data generator
gen_data: $(GEN_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $(GEN_SRCS)

# Build the experiment driver
run_experiments: $(EXP_SRCS) $(EXP_HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(EXP_SRCS)

# Generate the dataset (100,000 students by default)
gendata: gen_data | $(DATADIR)
	./gen_data $(DATADIR)/students.csv 100000 42

# Run the full experiment suite
run: run_experiments | $(RESDIR)
	./run_experiments $(DATADIR)/students.csv $(RESDIR)

# Ensure output directories exist
$(DATADIR) $(RESDIR):
	mkdir -p $@

# Cleanup
clean:
	rm -f gen_data run_experiments

distclean: clean
	rm -f $(DATADIR)/students.csv $(RESDIR)/*.csv

.PHONY: all gendata run clean distclean
