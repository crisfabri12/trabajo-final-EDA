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
using namespace std;


#define MAX_LEVEL 1000
const float P = 0.5;


using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;




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



template <typename Type>
struct node
{
    Type value;
    node** levels;
    node(int level, Type& value)
    {
        levels = new node * [level + 1];
        memset(levels, 0, sizeof(node*) * (level + 1));
        this->value = value;
    }


};


template <typename Type>
struct skiplist_secuen
{
    node<Type>* header;
    Type value;
    int level;
    skiplist_secuen()
    {
        header = new node<Type>(MAX_LEVEL, value);
        level = 0;
    }


    void print();

    Type get(Type val);

    void insert(Type val);

    void delete_(Type val);

};


float frand()
{
    return (float)rand() / RAND_MAX;
}

int random_level()
{
    static bool first = true;
    if (first)
    {
        srand((unsigned)time(NULL));
        first = false;
    }
    int lvl = (int)(log(frand()) / log(1. - P));
    return lvl < MAX_LEVEL ? lvl : MAX_LEVEL;
}


template <typename Type>
void skiplist_secuen<Type>::insert(Type val)
{
    node<Type>* x = header;
    node<Type>* update[MAX_LEVEL + 1];
    memset(update, 0, sizeof(node<Type>*) * (MAX_LEVEL + 1));
    for (int i = level; i >= 0; i--)
    {
        while (x->levels[i] != NULL && x->levels[i]->value < val)
        {
            x = x->levels[i];
        }
        update[i] = x;
    }

    x = x->levels[0];
    if (x == NULL || x->value != val)
    {
        int lvl = random_level();//asd
        if (lvl > level)
        {
            for (int i = level + 1; i <= lvl; i++)
            {
                update[i] = header;
            }
            level = lvl;
        }
        x = new node<Type>(lvl, val);
        for (int i = 0; i <= lvl; i++)
        {
            x->levels[i] = update[i]->levels[i];
            update[i]->levels[i] = x;
        }
    }
}

template <typename Type>
void skiplist_secuen<Type>::delete_(Type val)
{
    node<Type>* x = header;
    node<Type>* update[MAX_LEVEL + 1];
    memset(update, 0, sizeof(node<Type>*) * (MAX_LEVEL + 1));
    for (int i = level; i >= 0; i--)
    {
        while (x->levels[i] != NULL && x->levels[i]->value < val)
        {
            x = x->levels[i];
        }
        update[i] = x;
    }

    x = x->levels[0];
    if (x->value == val)
    {
        for (int i = 0; i <= level; i++)
        {
            if (update[i]->levels[i] != x)
                break;
            update[i]->levels[i] = x->levels[i];
        }
        delete x;
        while (level > 0 && header->levels[level] == NULL)
        {
            level--;
        }
    }
}

template <typename Type>
void skiplist_secuen<Type>::print()
{
    cout << "\n*****Skip List*****" << "\n";
    for (int i = 0; i <= level; i++)
    {
        node<Type>* node = header->levels[i];
        cout << "Nivel " << i << ": ";
        while (node != NULL)
        {
            cout << node->value << " ";
            node = node->levels[i];
        }
        cout << "\n";
    }
};

template <typename Type>
Type skiplist_secuen<Type>::get(Type val)
{
    node<Type>* x = header;
    for (int i = level; i >= 0; i--)
    {
        while (x->levels[i] != NULL && x->levels[i]->value <= val)
        {
            if (x->levels[i]->value == val) {
                cout << "Si existe el nodo " << val << endl;
                return x->levels[i]->value;
            }
            x = x->levels[i];
        }
    }
    x = x->levels[0];
    if (x != NULL && x->value == val) {
        cout << "Si existe el nodo " << val << endl;
        return x->value;
    }
    else {
        cout << "No existe el nodo " << val << endl;
        return -1;
    };
}



int main() {


    // ============================================================== SKIP LIST CONCURRENTE =================================================================================


    auto l = skipList_concu<int>();

    l.empty();
    int n = 100;


    auto millisec_start_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    for (int i = 1; i < n; i = i + 4) {
        std::thread t4(&skipList_concu<int>::add, l, rand() % n);
        std::thread t1(&skipList_concu<int>::add, l, rand() % n);
        std::thread t2(&skipList_concu<int>::add, l, rand() % n);
        std::thread t3(&skipList_concu<int>::add, l, rand() % n);
        t1.detach();
        t2.detach();
        t3.detach();
        t4.detach();
    }
    auto millisec_end_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    cout << "Tiempo de insercion paralelo: " << millisec_end_epoch - millisec_start_epoch << " ms" << endl;
    /*
    l.empty();
    millisec_start_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    for (int i = 1; i < n; i= i +4) {
        std::thread t1(&skipList_concu<int>::search, l, rand() % n);
        std::thread t2(&skipList_concu<int>::search, l, rand() % n);
        std::thread t3(&skipList_concu<int>::search, l, rand() % n);
        std::thread t4(&skipList_concu<int>::search, l, rand() % n);
        t1.detach();
        t2.detach();
        t3.detach();
        t4.detach();
    }
    millisec_end_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    cout << "Tiempo de busqueda paralelo: " << millisec_end_epoch - millisec_start_epoch << " ms" << endl;

    */


    /*
    l.empty();
    millisec_start_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    for (int i = 1; i < n; i= i +4) {
        std::thread t1(&skipList_concu<int>::remove, l, rand() % n);
        std::thread t2(&skipList_concu<int>::remove, l, rand() % n);
        std::thread t3(&skipList_concu<int>::remove, l, rand() % n);
        std::thread t4(&skipList_concu<int>::remove, l, rand() % n);
        t1.detach();
        t2.detach();
        t3.detach();
        t4.detach();
    }
    millisec_end_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    cout << "Tiempo de borrado paralelo: " << millisec_end_epoch - millisec_start_epoch << " ms" << endl;

    */



    // ============================================================== SKIP LIST SECUENCIAL =================================================================================

    skiplist_secuen<int> ss;


    millisec_start_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    n = 1000;
    for (int i = 0; i < n; i++) {
        ss.insert(rand() % n);
    }

    millisec_end_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    cout << "Tiempo de insercion secuencial: " << millisec_end_epoch - millisec_start_epoch << " ms" << endl;

    /*

    millisec_start_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    for (int i = 0; i < n; i = i++) {
        ss.get(rand() % n);
    }
    millisec_end_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    cout << "Tiempo de busqueda secuencial: " << millisec_end_epoch - millisec_start_epoch << " ms" << endl;

    */

    /*

    millisec_start_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    for (int i = 0; i < n; i = i++) {
        ss.delete_(rand() % n);
    }
    millisec_end_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    cout << "Tiempo de boraddo secuencial: " << millisec_end_epoch - millisec_start_epoch << " ms" << endl;

    */


    return 0;
}