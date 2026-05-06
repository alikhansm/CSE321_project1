// bstartree.cpp - In-memory B*-tree implementation.

#include "bstartree.h"

#include <algorithm>
#include <cassert>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------
BStarTree::BStarTree(int order) : d_(order) {
    root_ = new BStarNode(/*leaf=*/true);
}

BStarTree::~BStarTree() { delete root_; }

// ---------------------------------------------------------------------------
// Search (identical to plain B-tree: may stop at internal node)
// ---------------------------------------------------------------------------
bool BStarTree::search(Key k, RID& rid_out) const {
    return searchNode(root_, k, rid_out);
}

bool BStarTree::searchNode(BStarNode* node, Key k, RID& rid_out) const {
    int i = 0;
    const int n = static_cast<int>(node->keys.size());
    while (i < n && k > node->keys[i]) ++i;
    if (i < n && k == node->keys[i]) {
        rid_out = node->rids[i];
        return true;
    }
    if (node->is_leaf) return false;
    return searchNode(node->children[i], k, rid_out);
}

// ---------------------------------------------------------------------------
// Insertion - bottom-up, resolving overflow at the parent level so that
// redistribution / 2-to-3 splits become natural operations.
// ---------------------------------------------------------------------------
void BStarTree::insert(Key k, RID rid) {
    insertRec(root_, k, rid);

    if (static_cast<int>(root_->keys.size()) > maxKeys()) {
        auto* new_root = new BStarNode(/*leaf=*/false);
        new_root->children.push_back(root_);
        root_ = new_root;
        splitOneIntoTwo(root_, 0);
    }
}

void BStarTree::insertRec(BStarNode* node, Key k, RID rid) {
    if (node->is_leaf) {
        int i = 0;
        const int n = static_cast<int>(node->keys.size());
        while (i < n && node->keys[i] < k) ++i;
        node->keys.insert(node->keys.begin() + i, k);
        node->rids.insert(node->rids.begin() + i, rid);
        return;
    }

    // Internal: descend.
    int i = 0;
    const int n = static_cast<int>(node->keys.size());
    while (i < n && node->keys[i] < k) ++i;
    insertRec(node->children[i], k, rid);

    if (static_cast<int>(node->children[i]->keys.size()) > maxKeys()) {
        handleOverflow(node, i);
    }
}

void BStarTree::handleOverflow(BStarNode* parent, int child_idx) {
    const int n = static_cast<int>(parent->children.size());

    // Step 1: try redistribution with left sibling.
    if (child_idx > 0 &&
        static_cast<int>(parent->children[child_idx - 1]->keys.size()) < maxKeys()) {
        redistribute(parent, child_idx - 1, child_idx);
        return;
    }
    // Step 2: try redistribution with right sibling.
    if (child_idx < n - 1 &&
        static_cast<int>(parent->children[child_idx + 1]->keys.size()) < maxKeys()) {
        redistribute(parent, child_idx, child_idx + 1);
        return;
    }
    // Step 3: 2-to-3 split.  Prefer combining with the left sibling.
    if (child_idx > 0) {
        splitTwoIntoThree(parent, child_idx - 1);
    } else if (child_idx < n - 1) {
        splitTwoIntoThree(parent, child_idx);
    } else {
        // No sibling exists (parent has only one child).  Plain 1-to-2 split.
        splitOneIntoTwo(parent, child_idx);
    }
}

void BStarTree::redistribute(BStarNode* parent, int left_idx, int right_idx) {
    BStarNode* left  = parent->children[left_idx];
    BStarNode* right = parent->children[right_idx];

    // Pool: left.keys ++ [parent.keys[left_idx]] ++ right.keys
    std::vector<Key> ks;
    std::vector<RID> rs;
    ks.reserve(left->keys.size() + 1 + right->keys.size());
    rs.reserve(ks.capacity());
    ks.insert(ks.end(), left->keys.begin(), left->keys.end());
    rs.insert(rs.end(), left->rids.begin(), left->rids.end());
    ks.push_back(parent->keys[left_idx]);
    rs.push_back(parent->rids[left_idx]);
    ks.insert(ks.end(), right->keys.begin(), right->keys.end());
    rs.insert(rs.end(), right->rids.begin(), right->rids.end());

    std::vector<BStarNode*> cs;
    if (!left->is_leaf) {
        cs.reserve(left->children.size() + right->children.size());
        cs.insert(cs.end(), left->children.begin(),  left->children.end());
        cs.insert(cs.end(), right->children.begin(), right->children.end());
    }

    const int total      = static_cast<int>(ks.size());
    // Place the separator at index new_left_size; balance left/right.
    const int new_left_size = (total - 1) / 2;

    left->keys.assign(ks.begin(), ks.begin() + new_left_size);
    left->rids.assign(rs.begin(), rs.begin() + new_left_size);

    parent->keys[left_idx] = ks[new_left_size];
    parent->rids[left_idx] = rs[new_left_size];

    right->keys.assign(ks.begin() + new_left_size + 1, ks.end());
    right->rids.assign(rs.begin() + new_left_size + 1, rs.end());

    if (!left->is_leaf) {
        // children[k] sits to the left of keys[k].  After placing keys
        // 0..new_left_size-1 in `left`, `left` needs new_left_size + 1 children.
        left->children.assign(cs.begin(), cs.begin() + new_left_size + 1);
        right->children.assign(cs.begin() + new_left_size + 1, cs.end());
    }

    ++redistribute_count_;
}

void BStarTree::splitTwoIntoThree(BStarNode* parent, int left_idx) {
    BStarNode* left  = parent->children[left_idx];
    BStarNode* right = parent->children[left_idx + 1];

    // Pool of keys: left ++ sep ++ right.  Size = (2d+1) + 1 + 2d = 4d+2
    // OR (2d) + 1 + (2d+1) = 4d+2 depending on which side overflowed.
    std::vector<Key> ks;
    std::vector<RID> rs;
    ks.reserve(left->keys.size() + 1 + right->keys.size());
    rs.reserve(ks.capacity());
    ks.insert(ks.end(), left->keys.begin(), left->keys.end());
    rs.insert(rs.end(), left->rids.begin(), left->rids.end());
    ks.push_back(parent->keys[left_idx]);
    rs.push_back(parent->rids[left_idx]);
    ks.insert(ks.end(), right->keys.begin(), right->keys.end());
    rs.insert(rs.end(), right->rids.begin(), right->rids.end());

    std::vector<BStarNode*> cs;
    if (!left->is_leaf) {
        cs.reserve(left->children.size() + right->children.size());
        cs.insert(cs.end(), left->children.begin(),  left->children.end());
        cs.insert(cs.end(), right->children.begin(), right->children.end());
    }

    // We will distribute the 4d (= total - 2) non-promoted keys into three
    // nodes with sizes (size_a, size_b, size_c) as balanced as possible.
    // The two promoted keys sit at indices p = size_a and q = size_a + 1 + size_b.
    const int total            = static_cast<int>(ks.size());      // 4d + 2
    const int distribute_total = total - 2;                        // 4d
    const int size_a = (distribute_total + 2) / 3;
    const int size_b = (distribute_total + 1) / 3;
    // size_c = distribute_total / 3 (implicit)
    const int p = size_a;
    const int q = size_a + 1 + size_b;

    auto* a = left;                                 // reuse
    auto* b = new BStarNode(left->is_leaf);
    auto* c = right;                                // reuse the right node as the third

    a->keys.assign(ks.begin(),         ks.begin() + p);
    a->rids.assign(rs.begin(),         rs.begin() + p);

    b->keys.assign(ks.begin() + p + 1, ks.begin() + q);
    b->rids.assign(rs.begin() + p + 1, rs.begin() + q);

    c->keys.assign(ks.begin() + q + 1, ks.end());
    c->rids.assign(rs.begin() + q + 1, rs.end());

    if (!left->is_leaf) {
        // Each new node needs (its key count) + 1 children, and the boundary
        // children are taken from cs in order.
        a->children.assign(cs.begin(),               cs.begin() + p + 1);
        b->children.assign(cs.begin() + p + 1,       cs.begin() + q + 1);
        c->children.assign(cs.begin() + q + 1,       cs.end());
    }

    // Replace parent's separator at left_idx with two new separators
    // (ks[p], ks[q]) and insert b between a and c in the children array.
    parent->keys[left_idx] = ks[p];
    parent->rids[left_idx] = rs[p];

    parent->keys    .insert(parent->keys    .begin() + left_idx + 1, ks[q]);
    parent->rids    .insert(parent->rids    .begin() + left_idx + 1, rs[q]);
    parent->children.insert(parent->children.begin() + left_idx + 1, b);

    ++split_count_;
}

void BStarTree::splitOneIntoTwo(BStarNode* parent, int child_idx) {
    BStarNode* child = parent->children[child_idx];
    const int total = static_cast<int>(child->keys.size());  // expected 2d+1
    const int mid   = total / 2;

    auto* right = new BStarNode(child->is_leaf);
    right->keys.assign(child->keys.begin() + mid + 1, child->keys.end());
    right->rids.assign(child->rids.begin() + mid + 1, child->rids.end());
    if (!child->is_leaf) {
        right->children.assign(child->children.begin() + mid + 1,
                               child->children.end());
        child->children.resize(mid + 1);
    }

    Key promoted_key = child->keys[mid];
    RID promoted_rid = child->rids[mid];
    child->keys.resize(mid);
    child->rids.resize(mid);

    parent->keys    .insert(parent->keys    .begin() + child_idx,     promoted_key);
    parent->rids    .insert(parent->rids    .begin() + child_idx,     promoted_rid);
    parent->children.insert(parent->children.begin() + child_idx + 1, right);

    ++split_count_;
}

// ---------------------------------------------------------------------------
// Deletion (CLRS-style, as in BTree)
// ---------------------------------------------------------------------------
bool BStarTree::remove(Key k) {
    bool found = false;
    removeFromNode(root_, k, found);
    if (root_->keys.empty() && !root_->is_leaf) {
        BStarNode* old = root_;
        root_ = root_->children[0];
        old->children.clear();
        delete old;
    }
    return found;
}

void BStarTree::removeFromNode(BStarNode* node, Key k, bool& found) {
    int idx = 0;
    const int n = static_cast<int>(node->keys.size());
    while (idx < n && node->keys[idx] < k) ++idx;

    if (idx < n && node->keys[idx] == k) {
        if (node->is_leaf) removeFromLeaf(node, idx);
        else               removeFromInternal(node, idx);
        found = true;
        return;
    }
    if (node->is_leaf) return;

    bool last_child = (idx == n);
    if (static_cast<int>(node->children[idx]->keys.size()) == minKeys()) {
        fillChild(node, idx);
    }
    if (last_child && idx > static_cast<int>(node->keys.size())) {
        removeFromNode(node->children[idx - 1], k, found);
    } else {
        removeFromNode(node->children[idx], k, found);
    }
}

void BStarTree::removeFromLeaf(BStarNode* node, int idx) {
    node->keys.erase(node->keys.begin() + idx);
    node->rids.erase(node->rids.begin() + idx);
}

void BStarTree::removeFromInternal(BStarNode* node, int idx) {
    Key k = node->keys[idx];
    if (static_cast<int>(node->children[idx]->keys.size()) > minKeys()) {
        auto pred = getPredecessor(node, idx);
        node->keys[idx] = pred.first;
        node->rids[idx] = pred.second;
        bool dummy = false;
        removeFromNode(node->children[idx], pred.first, dummy);
    } else if (static_cast<int>(node->children[idx + 1]->keys.size()) > minKeys()) {
        auto succ = getSuccessor(node, idx);
        node->keys[idx] = succ.first;
        node->rids[idx] = succ.second;
        bool dummy = false;
        removeFromNode(node->children[idx + 1], succ.first, dummy);
    } else {
        mergeChildren(node, idx);
        bool dummy = false;
        removeFromNode(node->children[idx], k, dummy);
    }
}

std::pair<Key,RID> BStarTree::getPredecessor(BStarNode* node, int idx) {
    BStarNode* cur = node->children[idx];
    while (!cur->is_leaf) cur = cur->children.back();
    return {cur->keys.back(), cur->rids.back()};
}

std::pair<Key,RID> BStarTree::getSuccessor(BStarNode* node, int idx) {
    BStarNode* cur = node->children[idx + 1];
    while (!cur->is_leaf) cur = cur->children.front();
    return {cur->keys.front(), cur->rids.front()};
}

void BStarTree::fillChild(BStarNode* node, int idx) {
    if (idx > 0 &&
        static_cast<int>(node->children[idx - 1]->keys.size()) > minKeys()) {
        borrowFromPrev(node, idx);
    } else if (idx < static_cast<int>(node->keys.size()) &&
               static_cast<int>(node->children[idx + 1]->keys.size()) > minKeys()) {
        borrowFromNext(node, idx);
    } else {
        if (idx < static_cast<int>(node->keys.size())) mergeChildren(node, idx);
        else                                            mergeChildren(node, idx - 1);
    }
}

void BStarTree::borrowFromPrev(BStarNode* node, int idx) {
    BStarNode* child   = node->children[idx];
    BStarNode* sibling = node->children[idx - 1];
    child->keys.insert(child->keys.begin(), node->keys[idx - 1]);
    child->rids.insert(child->rids.begin(), node->rids[idx - 1]);
    if (!child->is_leaf) {
        child->children.insert(child->children.begin(), sibling->children.back());
        sibling->children.pop_back();
    }
    node->keys[idx - 1] = sibling->keys.back();
    node->rids[idx - 1] = sibling->rids.back();
    sibling->keys.pop_back();
    sibling->rids.pop_back();
}

void BStarTree::borrowFromNext(BStarNode* node, int idx) {
    BStarNode* child   = node->children[idx];
    BStarNode* sibling = node->children[idx + 1];
    child->keys.push_back(node->keys[idx]);
    child->rids.push_back(node->rids[idx]);
    if (!child->is_leaf) {
        child->children.push_back(sibling->children.front());
        sibling->children.erase(sibling->children.begin());
    }
    node->keys[idx] = sibling->keys.front();
    node->rids[idx] = sibling->rids.front();
    sibling->keys.erase(sibling->keys.begin());
    sibling->rids.erase(sibling->rids.begin());
}

void BStarTree::mergeChildren(BStarNode* node, int idx) {
    BStarNode* left  = node->children[idx];
    BStarNode* right = node->children[idx + 1];

    left->keys.push_back(node->keys[idx]);
    left->rids.push_back(node->rids[idx]);
    left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());
    left->rids.insert(left->rids.end(), right->rids.begin(), right->rids.end());
    if (!left->is_leaf) {
        left->children.insert(left->children.end(),
                              right->children.begin(), right->children.end());
        right->children.clear();
    }
    node->keys.erase(node->keys.begin() + idx);
    node->rids.erase(node->rids.begin() + idx);
    node->children.erase(node->children.begin() + idx + 1);
    delete right;
    ++merge_count_;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------
void BStarTree::countNodes(BStarNode* n, int& nodes, long long& keys,
                           long long& capacity) const {
    if (!n) return;
    ++nodes;
    keys     += static_cast<long long>(n->keys.size());
    capacity += maxKeys();
    if (!n->is_leaf) for (auto* c : n->children) countNodes(c, nodes, keys, capacity);
}

int BStarTree::getNodeCount() const {
    int nodes = 0; long long k = 0, cap = 0;
    countNodes(root_, nodes, k, cap);
    return nodes;
}
long long BStarTree::getKeyCount() const {
    int nodes = 0; long long k = 0, cap = 0;
    countNodes(root_, nodes, k, cap);
    return k;
}
double BStarTree::getUtilization() const {
    int nodes = 0; long long k = 0, cap = 0;
    countNodes(root_, nodes, k, cap);
    return cap == 0 ? 0.0 : static_cast<double>(k) / static_cast<double>(cap);
}

int BStarTree::height(BStarNode* n) const {
    if (!n) return 0;
    if (n->is_leaf) return 1;
    return 1 + height(n->children.front());
}
int BStarTree::getHeight() const { return height(root_); }
