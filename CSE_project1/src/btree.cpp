// btree.cpp - In-memory B-tree of order d

#include "btree.h"

#include <algorithm>
#include <cassert>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------
BTree::BTree(int order) : d_(order) {
    root_ = new BTreeNode(/*leaf=*/true);
}

BTree::~BTree() {
    delete root_;  // recursive via BTreeNode destructor
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------
bool BTree::search(Key k, RID& rid_out) const {
    return searchNode(root_, k, rid_out);
}

bool BTree::searchNode(BTreeNode* node, Key k, RID& rid_out) const {
    int i = 0;
    const int n = static_cast<int>(node->keys.size());
    while (i < n && k > node->keys[i]) ++i;

    if (i < n && k == node->keys[i]) {
        // Hit at this node - search may terminate here in a B-tree.
        rid_out = node->rids[i];
        return true;
    }
    if (node->is_leaf) {
        return false;
    }
    return searchNode(node->children[i], k, rid_out);
}

// ---------------------------------------------------------------------------
// Insertion
// ---------------------------------------------------------------------------

void BTree::insert(Key k, RID rid) {
    if (static_cast<int>(root_->keys.size()) == maxKeys()) {
        // Grow upward: create a new root whose only child is the old root,
        // then split that child.
        auto* new_root = new BTreeNode(/*leaf=*/false);
        new_root->children.push_back(root_);
        splitChild(new_root, 0);
        root_ = new_root;
    }
    insertNonFull(root_, k, rid);
}

void BTree::splitChild(BTreeNode* parent, int i) {
    BTreeNode* full = parent->children[i];
    assert(static_cast<int>(full->keys.size()) == maxKeys());

    auto* right = new BTreeNode(full->is_leaf);

    // The middle index (0-based) of a 2d-key node is at position d.
    // Keys [0..d-1] stay in `full`, keys[d] is promoted, keys[d+1..2d-1]
    // go to the new right sibling.
    const int mid = d_;

    right->keys.assign(full->keys.begin() + mid + 1, full->keys.end());
    right->rids.assign(full->rids.begin() + mid + 1, full->rids.end());

    Key promoted_key = full->keys[mid];
    RID promoted_rid = full->rids[mid];

    if (!full->is_leaf) {
        right->children.assign(full->children.begin() + mid + 1,
                               full->children.end());
        full->children.resize(mid + 1);
    }

    full->keys.resize(mid);
    full->rids.resize(mid);

    parent->keys    .insert(parent->keys    .begin() + i,     promoted_key);
    parent->rids    .insert(parent->rids    .begin() + i,     promoted_rid);
    parent->children.insert(parent->children.begin() + i + 1, right);

    ++split_count_;
}

void BTree::insertNonFull(BTreeNode* node, Key k, RID rid) {
    int i = static_cast<int>(node->keys.size()) - 1;

    if (node->is_leaf) {
        // Slide larger keys right to make room.
        node->keys.push_back(0);
        node->rids.push_back(0);
        while (i >= 0 && k < node->keys[i]) {
            node->keys[i + 1] = node->keys[i];
            node->rids[i + 1] = node->rids[i];
            --i;
        }
        node->keys[i + 1] = k;
        node->rids[i + 1] = rid;
        return;
    }

    // Internal node: find the child to descend into.
    while (i >= 0 && k < node->keys[i]) --i;
    ++i; // now points at correct child index

    if (static_cast<int>(node->children[i]->keys.size()) == maxKeys()) {
        splitChild(node, i);
        // After the split, the median was promoted into node->keys[i].
        // Decide which side k belongs on.
        if (k > node->keys[i]) ++i;
    }
    insertNonFull(node->children[i], k, rid);
}

// ---------------------------------------------------------------------------
// Deletion
// ---------------------------------------------------------------------------

bool BTree::remove(Key k) {
    bool found = false;
    removeFromNode(root_, k, found);

    // If the root has lost all its keys but still has a child, drop it.
    if (root_->keys.empty() && !root_->is_leaf) {
        BTreeNode* old = root_;
        root_ = root_->children[0];
        old->children.clear();   // prevent the destructor from deleting the new root
        delete old;
    }
    return found;
}

void BTree::removeFromNode(BTreeNode* node, Key k, bool& found) {
    int idx = 0;
    const int n = static_cast<int>(node->keys.size());
    while (idx < n && node->keys[idx] < k) ++idx;

    if (idx < n && node->keys[idx] == k) {
        if (node->is_leaf)  removeFromLeaf(node, idx);
        else                removeFromInternal(node, idx);
        found = true;
        return;
    }

    if (node->is_leaf) {
        // not present
        return;
    }


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

void BTree::removeFromLeaf(BTreeNode* node, int idx) {
    node->keys.erase(node->keys.begin() + idx);
    node->rids.erase(node->rids.begin() + idx);
}

void BTree::removeFromInternal(BTreeNode* node, int idx) {
    Key k = node->keys[idx];

    if (static_cast<int>(node->children[idx]->keys.size()) > minKeys()) {
        // Replace with predecessor, then delete predecessor from left subtree.
        auto pred = getPredecessor(node, idx);
        node->keys[idx] = pred.first;
        node->rids[idx] = pred.second;
        bool dummy = false;
        removeFromNode(node->children[idx], pred.first, dummy);
    } else if (static_cast<int>(node->children[idx + 1]->keys.size()) > minKeys()) {
        // Replace with successor, then delete successor from right subtree.
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

std::pair<Key,RID> BTree::getPredecessor(BTreeNode* node, int idx) {
    BTreeNode* cur = node->children[idx];
    while (!cur->is_leaf) cur = cur->children.back();
    return {cur->keys.back(), cur->rids.back()};
}

std::pair<Key,RID> BTree::getSuccessor(BTreeNode* node, int idx) {
    BTreeNode* cur = node->children[idx + 1];
    while (!cur->is_leaf) cur = cur->children.front();
    return {cur->keys.front(), cur->rids.front()};
}

void BTree::fillChild(BTreeNode* node, int idx) {
    // Try to borrow a key from a richer sibling; otherwise merge.
    if (idx > 0 &&
        static_cast<int>(node->children[idx - 1]->keys.size()) > minKeys()) {
        borrowFromPrev(node, idx);
    } else if (idx < static_cast<int>(node->keys.size()) &&
               static_cast<int>(node->children[idx + 1]->keys.size()) > minKeys()) {
        borrowFromNext(node, idx);
    } else {
        // Both siblings are minimum: merge with one of them.
        if (idx < static_cast<int>(node->keys.size())) mergeChildren(node, idx);
        else                                           mergeChildren(node, idx - 1);
    }
}

void BTree::borrowFromPrev(BTreeNode* node, int idx) {
    BTreeNode* child   = node->children[idx];
    BTreeNode* sibling = node->children[idx - 1];

    // Move parent separator down to front of child, sibling's last key up.
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

void BTree::borrowFromNext(BTreeNode* node, int idx) {
    BTreeNode* child   = node->children[idx];
    BTreeNode* sibling = node->children[idx + 1];

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

void BTree::mergeChildren(BTreeNode* node, int idx) {
    BTreeNode* left  = node->children[idx];
    BTreeNode* right = node->children[idx + 1];

    // Pull the separator key down into `left`.
    left->keys.push_back(node->keys[idx]);
    left->rids.push_back(node->rids[idx]);

    // Append everything from `right`.
    left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end());
    left->rids.insert(left->rids.end(), right->rids.begin(), right->rids.end());

    if (!left->is_leaf) {
        left->children.insert(left->children.end(),
                              right->children.begin(), right->children.end());
        right->children.clear(); // prevent recursive deletion of moved children
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
void BTree::countNodes(BTreeNode* n, int& nodes, long long& keys,
                       long long& capacity) const {
    if (!n) return;
    ++nodes;
    keys     += static_cast<long long>(n->keys.size());
    capacity += maxKeys();
    if (!n->is_leaf) for (auto* c : n->children) countNodes(c, nodes, keys, capacity);
}

int BTree::getNodeCount() const {
    int nodes = 0; long long k = 0, cap = 0;
    countNodes(root_, nodes, k, cap);
    return nodes;
}

long long BTree::getKeyCount() const {
    int nodes = 0; long long k = 0, cap = 0;
    countNodes(root_, nodes, k, cap);
    return k;
}

double BTree::getUtilization() const {
    int nodes = 0; long long k = 0, cap = 0;
    countNodes(root_, nodes, k, cap);
    return cap == 0 ? 0.0 : static_cast<double>(k) / static_cast<double>(cap);
}

int BTree::height(BTreeNode* n) const {
    if (!n) return 0;
    if (n->is_leaf) return 1;
    return 1 + height(n->children.front());
}

int BTree::getHeight() const { return height(root_); }
