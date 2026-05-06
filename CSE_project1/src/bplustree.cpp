// bplustree.cpp - In-memory B+-tree implementation.
// See bplustree.h for the structural conventions.
#include "bplustree.h"

#include <algorithm>
#include <cassert>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------
BPlusTree::BPlusTree(int order) : d_(order) {
    root_ = new BPlusNode(/*leaf=*/true);
}

BPlusTree::~BPlusTree() { delete root_; }

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

BPlusNode* BPlusTree::findLeaf(Key k) const {
    BPlusNode* cur = root_;
    while (!cur->is_leaf) {
        int i = 0;
        const int n = static_cast<int>(cur->keys.size());
        while (i < n && k >= cur->keys[i]) ++i;
        cur = cur->children[i];
    }
    return cur;
}

bool BPlusTree::search(Key k, RID& rid_out) const {
    BPlusNode* leaf = findLeaf(k);
    for (int i = 0; i < static_cast<int>(leaf->keys.size()); ++i) {
        if (leaf->keys[i] == k) { rid_out = leaf->rids[i]; return true; }
    }
    return false;
}

void BPlusTree::rangeQuery(Key lo, Key hi, std::vector<RID>& out) const {
    if (lo > hi) return;
    BPlusNode* leaf = findLeaf(lo);
    while (leaf) {
        for (int i = 0; i < static_cast<int>(leaf->keys.size()); ++i) {
            Key k = leaf->keys[i];
            if (k < lo) continue;
            if (k > hi) return;          // sorted leaf, sorted scan: safe to stop
            out.push_back(leaf->rids[i]);
        }
        leaf = leaf->next;
    }
}

// ---------------------------------------------------------------------------
// Insertion
// ---------------------------------------------------------------------------
void BPlusTree::insert(Key k, RID rid) {
    SplitInfo s = insertRec(root_, k, rid);
    if (s.did_split) {
        auto* new_root = new BPlusNode(/*leaf=*/false);
        new_root->keys.push_back(s.promoted_key);
        new_root->children.push_back(root_);
        new_root->children.push_back(s.new_right);
        root_ = new_root;
    }
}

BPlusTree::SplitInfo
BPlusTree::insertRec(BPlusNode* node, Key k, RID rid) {
    SplitInfo result{false, 0, nullptr};

    if (node->is_leaf) {
        // Insert in sorted order.
        int i = 0;
        const int n = static_cast<int>(node->keys.size());
        while (i < n && node->keys[i] < k) ++i;
        node->keys.insert(node->keys.begin() + i, k);
        node->rids.insert(node->rids.begin() + i, rid);

        if (static_cast<int>(node->keys.size()) <= maxKeys()) return result;

        // Leaf split: left keeps d keys, right keeps the rest (d+1 keys).
        // The first key of the right leaf is *copied* up (B+-tree behaviour).
        const int split = d_;             // number of keys to keep in left
        auto* right = new BPlusNode(/*leaf=*/true);
        right->keys.assign(node->keys.begin() + split, node->keys.end());
        right->rids.assign(node->rids.begin() + split, node->rids.end());
        node->keys.resize(split);
        node->rids.resize(split);

        // Maintain the leaf linked list.
        right->next = node->next;
        node->next  = right;

        ++split_count_;
        result.did_split    = true;
        result.promoted_key = right->keys.front();
        result.new_right    = right;
        return result;
    }

    // Internal: pick child to descend into.
    int i = 0;
    const int n = static_cast<int>(node->keys.size());
    while (i < n && k >= node->keys[i]) ++i;

    SplitInfo child_split = insertRec(node->children[i], k, rid);
    if (!child_split.did_split) return result;

    // Integrate the child's promoted key/right pointer.
    node->keys    .insert(node->keys    .begin() + i,     child_split.promoted_key);
    node->children.insert(node->children.begin() + i + 1, child_split.new_right);

    if (static_cast<int>(node->keys.size()) <= maxKeys()) return result;

    // Internal split: median is *promoted* (removed from both halves).
    const int mid   = d_;          // index of the key to promote
    auto* right = new BPlusNode(/*leaf=*/false);
    right->keys.assign(node->keys.begin() + mid + 1, node->keys.end());
    right->children.assign(node->children.begin() + mid + 1, node->children.end());

    Key promoted = node->keys[mid];
    node->keys.resize(mid);
    node->children.resize(mid + 1);

    ++split_count_;
    result.did_split    = true;
    result.promoted_key = promoted;
    result.new_right    = right;
    return result;
}

// ---------------------------------------------------------------------------
// Deletion
// ---------------------------------------------------------------------------
bool BPlusTree::remove(Key k) {
    // Special case: root is leaf.
    if (root_->is_leaf) {
        for (int i = 0; i < static_cast<int>(root_->keys.size()); ++i) {
            if (root_->keys[i] == k) {
                root_->keys.erase(root_->keys.begin() + i);
                root_->rids.erase(root_->rids.begin() + i);
                return true;
            }
        }
        return false;
    }

    bool found = removeRec(root_, k);

    // Tree shrinks: if root has 0 keys (1 child) after deletion, drop it.
    if (root_->keys.empty() && !root_->is_leaf) {
        BPlusNode* old = root_;
        root_ = root_->children[0];
        old->children.clear();
        delete old;
    }
    return found;
}

bool BPlusTree::removeRec(BPlusNode* node, Key k) {
    // `node` is internal.  Find the child to descend into.
    int idx = 0;
    const int n = static_cast<int>(node->keys.size());
    while (idx < n && k >= node->keys[idx]) ++idx;

    bool last_child = (idx == n);

    // Ensure the chosen child has more than minKeys before descending.
    if (static_cast<int>(node->children[idx]->keys.size()) <= minKeys()) {
        fillChild(node, idx);
        // After fillChild, idx may need adjustment if a left-merge collapsed
        // the previous child into idx-1 (so what was at idx is now at idx-1).
        if (last_child && idx > static_cast<int>(node->keys.size())) {
            --idx;
        }
    }

    BPlusNode* child = node->children[idx];

    if (child->is_leaf) {
        for (int i = 0; i < static_cast<int>(child->keys.size()); ++i) {
            if (child->keys[i] == k) {
                child->keys.erase(child->keys.begin() + i);
                child->rids.erase(child->rids.begin() + i);
                return true;
            }
        }
        return false;
    }
    return removeRec(child, k);
}

void BPlusTree::fillChild(BPlusNode* node, int idx) {
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

void BPlusTree::borrowFromPrev(BPlusNode* node, int idx) {
    BPlusNode* child   = node->children[idx];
    BPlusNode* sibling = node->children[idx - 1];

    if (child->is_leaf) {
        // Move sibling's last (key, rid) to the front of child.
        child->keys.insert(child->keys.begin(), sibling->keys.back());
        child->rids.insert(child->rids.begin(), sibling->rids.back());
        sibling->keys.pop_back();
        sibling->rids.pop_back();
        // The new boundary between sibling and child is child's first key.
        node->keys[idx - 1] = child->keys.front();
    } else {
        // Internal-node rotation through the parent.
        child->keys.insert(child->keys.begin(), node->keys[idx - 1]);
        child->children.insert(child->children.begin(), sibling->children.back());
        sibling->children.pop_back();
        node->keys[idx - 1] = sibling->keys.back();
        sibling->keys.pop_back();
    }
}

void BPlusTree::borrowFromNext(BPlusNode* node, int idx) {
    BPlusNode* child   = node->children[idx];
    BPlusNode* sibling = node->children[idx + 1];

    if (child->is_leaf) {
        child->keys.push_back(sibling->keys.front());
        child->rids.push_back(sibling->rids.front());
        sibling->keys.erase(sibling->keys.begin());
        sibling->rids.erase(sibling->rids.begin());
        // New boundary = sibling's new first key.
        node->keys[idx] = sibling->keys.front();
    } else {
        child->keys.push_back(node->keys[idx]);
        child->children.push_back(sibling->children.front());
        sibling->children.erase(sibling->children.begin());
        node->keys[idx] = sibling->keys.front();
        sibling->keys.erase(sibling->keys.begin());
    }
}

void BPlusTree::mergeChildren(BPlusNode* node, int idx) {
    BPlusNode* left  = node->children[idx];
    BPlusNode* right = node->children[idx + 1];

    if (left->is_leaf) {
        // Concatenate (key, rid) pairs.  Keep the leaf linked list intact.
        left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());
        left->rids.insert(left->rids.end(), right->rids.begin(), right->rids.end());
        left->next = right->next;
    } else {
        // Pull the parent separator down and concatenate.
        left->keys.push_back(node->keys[idx]);
        left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());
        left->children.insert(left->children.end(),
                              right->children.begin(), right->children.end());
        right->children.clear();   // prevent recursive deletion of moved children
    }

    node->keys.erase(node->keys.begin() + idx);
    node->children.erase(node->children.begin() + idx + 1);

    delete right;
    ++merge_count_;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------
void BPlusTree::countNodes(BPlusNode* n, int& nodes, int& leaves,
                           long long& keys, long long& capacity) const {
    if (!n) return;
    ++nodes;
    if (n->is_leaf) ++leaves;
    keys     += static_cast<long long>(n->keys.size());
    capacity += maxKeys();
    if (!n->is_leaf) for (auto* c : n->children) countNodes(c, nodes, leaves, keys, capacity);
}

int BPlusTree::getNodeCount() const {
    int nodes = 0, leaves = 0; long long k = 0, cap = 0;
    countNodes(root_, nodes, leaves, k, cap);
    return nodes;
}
int BPlusTree::getLeafCount() const {
    int nodes = 0, leaves = 0; long long k = 0, cap = 0;
    countNodes(root_, nodes, leaves, k, cap);
    return leaves;
}
long long BPlusTree::getKeyCount() const {
    int nodes = 0, leaves = 0; long long k = 0, cap = 0;
    countNodes(root_, nodes, leaves, k, cap);
    return k;
}
double BPlusTree::getUtilization() const {
    int nodes = 0, leaves = 0; long long k = 0, cap = 0;
    countNodes(root_, nodes, leaves, k, cap);
    return cap == 0 ? 0.0 : static_cast<double>(k) / static_cast<double>(cap);
}

int BPlusTree::height(BPlusNode* n) const {
    if (!n) return 0;
    if (n->is_leaf) return 1;
    return 1 + height(n->children.front());
}
int BPlusTree::getHeight() const { return height(root_); }
