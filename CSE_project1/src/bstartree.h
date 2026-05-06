// bstartree.h - In-memory B*-tree of order d.
//
// Structurally a B*-tree is identical to a B-tree (every node carries
// (key, RID) pairs and search may stop at internal nodes).  The difference
// is in the *split policy* applied when a node would otherwise overflow:

#ifndef BSTARTREE_H
#define BSTARTREE_H

#include "common.h"
#include <vector>

struct BStarNode {
    bool                       is_leaf;
    std::vector<Key>           keys;
    std::vector<RID>           rids;
    std::vector<BStarNode*>    children;

    explicit BStarNode(bool leaf) : is_leaf(leaf) {}
    ~BStarNode() {
        for (auto* c : children) delete c;
    }
};

class BStarTree {
public:
    explicit BStarTree(int order);
    ~BStarTree();

    bool   search(Key k, RID& rid_out) const;
    void   insert(Key k, RID rid);
    bool   remove(Key k);

    long long getSplitCount()         const { return split_count_; }
    long long getRedistributeCount()  const { return redistribute_count_; }
    long long getMergeCount()         const { return merge_count_; }
    int       getOrder()              const { return d_; }
    int       getNodeCount()          const;
    long long getKeyCount()           const;
    int       getHeight()             const;
    double    getUtilization()        const;

private:
    int           d_;
    BStarNode*    root_;
    long long     split_count_        = 0;
    long long     redistribute_count_ = 0;
    long long     merge_count_        = 0;

    int maxKeys() const { return 2 * d_; }
    int minKeys() const { return d_; }

    // ---- search ----
    bool searchNode(BStarNode* node, Key k, RID& rid_out) const;

    // ---- insertion ----
    // Recursive bottom-up insert.  Returns true if the parent must absorb
    // a promoted key (legacy 1-to-2 split).  For B* we resolve overflow
    // immediately at the parent level via redistribute / 2-to-3 split,
    // so the overflow signal we use is "this node's child overflowed".
    void insertRec(BStarNode* node, Key k, RID rid);
    void handleOverflow(BStarNode* parent, int child_idx);
    void redistribute(BStarNode* parent, int left_idx, int right_idx);
    void splitTwoIntoThree(BStarNode* parent, int left_idx);   // operates on children[left_idx], [left_idx+1]
    void splitOneIntoTwo  (BStarNode* parent, int child_idx);  // fallback when no sibling exists (root/single child)

    // ---- deletion (CLRS-style, like the plain B-tree) ----
    void   removeFromNode(BStarNode* node, Key k, bool& found);
    void   removeFromLeaf(BStarNode* node, int idx);
    void   removeFromInternal(BStarNode* node, int idx);
    void   fillChild(BStarNode* node, int idx);
    void   borrowFromPrev(BStarNode* node, int idx);
    void   borrowFromNext(BStarNode* node, int idx);
    void   mergeChildren(BStarNode* node, int idx);
    std::pair<Key,RID> getPredecessor(BStarNode* node, int idx);
    std::pair<Key,RID> getSuccessor  (BStarNode* node, int idx);

    // ---- stats ----
    void countNodes(BStarNode* n, int& nodes, long long& keys,
                    long long& capacity) const;
    int  height(BStarNode* n) const;
};

#endif // BSTARTREE_H
