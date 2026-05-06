// btree.h - In-memory B-tree of order d.

#ifndef BTREE_H
#define BTREE_H

#include "common.h"
#include <vector>

struct BTreeNode {
    bool                       is_leaf;
    std::vector<Key>           keys;
    std::vector<RID>           rids;      // parallel array: rids[i] is the RID for keys[i]
    std::vector<BTreeNode*>    children;  // size = keys.size()+1 if !is_leaf, else empty

    explicit BTreeNode(bool leaf) : is_leaf(leaf) {}
    ~BTreeNode() {
        for (auto* c : children) delete c;
    }
};

class BTree {
public:
    explicit BTree(int order);
    ~BTree();

    // Returns true if k exists, and writes its RID into rid_out.
    bool   search(Key k, RID& rid_out) const;
    // Inserts (k, rid).  No duplicate-key detection (the dataset uses unique IDs).
    void   insert(Key k, RID rid);
    // Returns true if k was found and removed.
    bool   remove(Key k);

    // Statistics
    long long getSplitCount()  const { return split_count_; }
    long long getMergeCount()  const { return merge_count_; }
    int       getOrder()       const { return d_; }
    int       getNodeCount()   const;
    long long getKeyCount()    const;
    int       getHeight()      const;
    // Average node utilisation = (sum of #keys) / (sum of capacity) where capacity = 2d.
    double    getUtilization() const;

private:
    int          d_;
    BTreeNode*   root_;
    long long    split_count_ = 0;
    long long    merge_count_ = 0;

    int maxKeys() const { return 2 * d_; }
    int minKeys() const { return d_; }

    // ---- search / insert helpers ----
    bool searchNode(BTreeNode* node, Key k, RID& rid_out) const;
    // Splits the i-th child of `parent`, which is assumed to have 2d keys.
    // The middle key is promoted into `parent` at position i.
    void splitChild(BTreeNode* parent, int i);
    // Inserts (k, rid) into `node` which is guaranteed to be non-full.
    void insertNonFull(BTreeNode* node, Key k, RID rid);

    // ---- delete helpers (CLRS-style) ----
    void   removeFromNode(BTreeNode* node, Key k, bool& found);
    void   removeFromLeaf(BTreeNode* node, int idx);
    void   removeFromInternal(BTreeNode* node, int idx);
    void   fillChild(BTreeNode* node, int idx);
    void   borrowFromPrev(BTreeNode* node, int idx);
    void   borrowFromNext(BTreeNode* node, int idx);
    void   mergeChildren(BTreeNode* node, int idx);
    std::pair<Key,RID> getPredecessor(BTreeNode* node, int idx);
    std::pair<Key,RID> getSuccessor  (BTreeNode* node, int idx);

    // ---- stat helpers ----
    void countNodes(BTreeNode* n, int& nodes, long long& keys,
                    long long& capacity) const;
    int  height(BTreeNode* n) const;
};

#endif // BTREE_H
