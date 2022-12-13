#include <iostream>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <thread>
#include <ctime> 

#define MAX_LEVEL 1000

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;

const float P = 0.5;
using namespace std;


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
