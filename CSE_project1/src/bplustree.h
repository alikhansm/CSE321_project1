// bplustree.h - In-memory B+-tree of order d.
#ifndef BPLUSTREE_H
#define BPLUSTREE_H

#include "common.h"
#include <vector>

struct BPlusNode {
    bool                       is_leaf;
    std::vector<Key>           keys;
    std::vector<RID>           rids;       // only used in leaves
    std::vector<BPlusNode*>    children;   // only used in internal nodes
    BPlusNode*                 next = nullptr;  // leaf->leaf linked list

    explicit BPlusNode(bool leaf) : is_leaf(leaf) {}
    ~BPlusNode() {
        // children vector owns child pointers; leaf->next is NOT owned.
        for (auto* c : children) delete c;
    }
};

class BPlusTree {
public:
    explicit BPlusTree(int order);
    ~BPlusTree();

    bool   search(Key k, RID& rid_out) const;
    void   insert(Key k, RID rid);
    bool   remove(Key k);

    // Range query over [lo, hi] inclusive.  Appends the matching RIDs to
    // `out`.  Uses the leaf linked list for sequential scan.
    void   rangeQuery(Key lo, Key hi, std::vector<RID>& out) const;

    long long getSplitCount()  const { return split_count_; }
    long long getMergeCount()  const { return merge_count_; }
    int       getOrder()       const { return d_; }
    int       getNodeCount()   const;
    int       getLeafCount()   const;
    long long getKeyCount()    const;
    int       getHeight()      const;
    double    getUtilization() const;

private:
    int          d_;
    BPlusNode*   root_;
    long long    split_count_ = 0;
    long long    merge_count_ = 0;

    int maxKeys() const { return 2 * d_; }
    int minKeys() const { return d_; }

    // ---- search ----
    // Walk to the leaf that would contain `k` (or where it would be).
    BPlusNode* findLeaf(Key k) const;

    // ---- insertion (recursive, returns "split-info" up the call chain) ----
    struct SplitInfo {
        bool       did_split;
        Key        promoted_key;   // for internal: the key that goes to parent
        BPlusNode* new_right;      // newly created right node
    };
    SplitInfo insertRec(BPlusNode* node, Key k, RID rid);

    // ---- deletion ----
    
    bool removeRec(BPlusNode* node, Key k);
    void fillChild(BPlusNode* node, int idx);
    void borrowFromPrev(BPlusNode* node, int idx);
    void borrowFromNext(BPlusNode* node, int idx);
    void mergeChildren(BPlusNode* node, int idx);

    // ---- stats ----
    void countNodes(BPlusNode* n, int& nodes, int& leaves,
                    long long& keys, long long& capacity) const;
    int  height(BPlusNode* n) const;
};

#endif // BPLUSTREE_H
