#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <random>
#include <map>
#include <thread>
#include <vector>
static const int MAX_LEVEL = 1000;


template <typename T>
class Node {
public:
    T val;
    int topLevel;
    bool marked;
    bool fullyLinked;
    shared_ptr<Node<T>> levels[MAX_LEVEL + 1];
    mutex nodeMutex;
    Node(int k) : val(k), topLevel(MAX_LEVEL), marked(false), fullyLinked(false),
        nodeMutex() {
        for (int i = 0; i < topLevel; i++)
            levels[i] = nullptr;
    }
    Node(T x, int level) : val(x), topLevel(level), marked(false), fullyLinked(false),
        nodeMutex() {
        for (int i = 0; i < topLevel; i++)
            levels[i] = nullptr;
    }
    void lock() {
        nodeMutex.lock();
    }
    void unlock() {
        nodeMutex.unlock();
    }
};



template <typename T>
class skipList_concu {

    std::shared_ptr<Node<T>> head;
    std::shared_ptr<Node<T>> tail;

    inline unsigned randomLevel() {
        int l = 0;
        while (static_cast <float> (rand()) / static_cast <float> (RAND_MAX) <= 0.5) {
            l++;
        }
        return l > MAX_LEVEL ? MAX_LEVEL : l;
    }

    inline int find(int k, shared_ptr<Node<T>> preds[], shared_ptr<Node<T>> succs[]) {
        int lFound = -1;
        std::shared_ptr<Node<T>> pred = head;
        for (int layer = MAX_LEVEL; layer >= 0; layer--) {
            std::shared_ptr<Node<T>> curr = pred->levels[layer];
            while (k > curr->val) {
                pred = curr;
                curr = pred->levels[layer];
            }
            if (lFound == -1 and k == curr->val) {
                lFound = layer;
            }
            preds[layer] = pred;
            succs[layer] = curr;
        }
        return lFound;
    }

    bool okToDelete(std::shared_ptr<Node<T>> candidate, int lFound) {
        return (candidate->fullyLinked and candidate->topLevel == lFound and !candidate->marked);
    }

public:
    skipList_concu() {
        head = std::make_shared<Node<T>>(INT_MIN, MAX_LEVEL);
        tail = std::make_shared<Node<T>>(INT_MIN, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; i++) {
            head->levels[i] = tail;
        }
    };

    bool add(T x) {
        int topLevel = randomLevel();
        std::shared_ptr<Node<T>> preds[MAX_LEVEL + 1];
        std::shared_ptr<Node<T>> succs[MAX_LEVEL + 1];

        while (true) {
            //buscamos el valor y guardamos sus predecesores y antecesores
            int  lFound = find(x, preds, succs);
            if (lFound != -1) {
                std::shared_ptr<Node<T>> nodeFound = succs[lFound];
                if (!nodeFound->marked) {
                    while (!nodeFound->fullyLinked);
                    return false;
                }
                continue;
            }

            map<shared_ptr<Node<T>>, int> locked_nodes;
            std::shared_ptr<Node<T>> pred, succ, prevPred = nullptr;
            bool valid = true;
            int layer_count = topLevel + 1;


            for (int level = 0; valid && (level <= topLevel); level++) {
                pred = preds[level];
                succ = succs[level];
                if (!(locked_nodes.count(pred))) {
                    //en lo que buscamos el lugar adecuado para su insercion bloquemos para evitar problemas
                    pred->lock();
                    locked_nodes.insert(make_pair(pred, 1));
                }
                // confirmamos que sea valido el lugar para insertar el nodo
                valid = !(pred->marked) && !(succ->marked) && (pred->levels[level] == succ);
            }
            if (!valid) {
                for (auto const& x : locked_nodes) {
                    x.first->unlock();
                }
                continue;
            }

            //creamos el nuevo nodo y lo insertamos 
            auto newNode = std::make_shared<Node<T>>(x, topLevel);
            for (int level = 0; level <= topLevel; level++) {
                newNode->levels[level] = succs[level];
            }

            for (int level = 0; level <= topLevel; level++) {
                preds[level]->levels[level] = newNode;
            }

            // marcamos como que esta vinculado
            newNode->fullyLinked = true;

            // desbloqueamos los threads restantes
            for (auto const& x : locked_nodes) {
                x.first->unlock();
            }

            return true;
        }
    }

    bool remove(int key) {
        std::shared_ptr<Node<T>> nodeToDelete = nullptr;
        bool isMarked = false;
        int topLevel = -1;
        std::shared_ptr<Node<T>> preds[MAX_LEVEL + 1];
        std::shared_ptr<Node<T>> succs[MAX_LEVEL + 1];
        for (size_t i = 0; i < preds.size(); i++) {
            preds[i] = NULL;
            succs[i] = NULL;
        }
        while (true) {
            int lFound = find(key, preds, succs);
            if (lFound != -1) {
                nodeToDelete = succs[lFound];
            }
            if (isMarked || (lFound != -1 && okToDelete(succs[lFound], lFound))) {
                if (!isMarked) {
                    topLevel = nodeToDelete->topLevel;
                    nodeToDelete->lock();
                    if (nodeToDelete->marked) {
                        nodeToDelete->unlock();
                        return false;
                    }
                    nodeToDelete->marked = true;
                    isMarked = true;
                }
                map<shared_ptr<Node<T>>, int> locked_nodes;
                shared_ptr<Node<T>> pred, succ, prevPred = nullptr;
                bool valid = true;

                for (int level = 0; valid && level <= topLevel; level++) {
                    pred = preds[level];
                    if (!(locked_nodes.count(pred))) {
                        pred->lock();
                        locked_nodes.insert(make_pair(pred, 1));
                    }
                    valid = !pred->marked && pred->levels[level] == nodeToDelete;
                }
                if (!valid) {
                    for (auto const& x : locked_nodes) {
                        x.first->unlock();
                    }
                    continue;
                }
                for (int level = topLevel; level >= 0; level--) {
                    preds[level]->levels[level] = nodeToDelete->levels[level]; // los dereferenciamos
                }
                nodeToDelete->unlock();
                for (auto const& x : locked_nodes) { // desbloquemos todo
                    x.first->unlock();
                }
                return true; // si existe
            }
            else
                return false; // si no existe
        }
    }

    bool search(int val) {

        std::shared_ptr<Node<T>> curr = head;

        for (int level = MAX_LEVEL; level >= 0; level--) {
            while (curr->levels[level] != NULL && val >= curr->levels[level]->val) {
                if (val == curr->levels[level]->val) {
                    return true;
                }
                curr = curr->levels[level];
            }
            if (val == curr->val) {
                return true;
            }
        }

        curr = curr->levels[0];

        if ((curr != NULL) && (curr->val == val)) {

            return true;
        }
        else {

            return false;
        }
    }

    bool empty() { return head->levels[0] == tail; }
};