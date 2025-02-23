#pragma once

#include <iostream>
#include <memory>
#include <random>
#include <vector>

using std::vector;
using std::shared_ptr;
using std::make_shared;
using std::unique_ptr;
using std::make_unique;

template<typename KType, typename VType>
struct SkipList{
        struct Node {
            KType key;
            VType value;
            int level;
            // next[i] is the next node at level i
            vector<shared_ptr<Node>> next; 
            explicit Node(KType key_, VType value_, int level_) : 
                key(key_), 
                value(value_), 
                level(level_) {
                next.resize(level_ + 1);
            }
        };
        shared_ptr<Node> head;
        int max_level;
        float probability;
        std::mt19937 gen{std::random_device{}()};
        std::uniform_real_distribution<float> dist{0, 1};
        int randomLevel() {
            int level = 0;
            while (dist(gen) < probability && level < max_level) 
                level++;
            return level;
        }
    public:
        SkipList(int max_level_ = 16, float probability_ = 0.5):
            head(std::make_shared<Node>(KType(), VType(), max_level_)), 
            max_level(max_level_), 
            probability(probability_)
        {}

        // return 0 if the key already exists
        // return 1 if the key not exists
        int put(const KType key, const VType value) {
            vector<shared_ptr<Node>> update(max_level + 1);
            auto current = head;
        
            // Find the right place to insert the new node
            for (int i = max_level; i >= 0; i--) {
                while (current->next[i] && current->next[i]->key < key) {
                    current = current->next[i];
                }
                update[i] = current;
            }
            
            // check if the key already exists
            if (current->next[0] && current->next[0]->key == key) {
                current->next[0]->value = value;
                return 0;
            }

            const int level = randomLevel();
            // create a new node with random level
            // use move semantics to avoid copying the value
            auto new_node = make_shared<Node>(std::move(key), std::move(value), level);
        
            for (int i = 0; i <= level; ++i) {
                new_node->next[i] = update[i]->next[i];
                update[i]->next[i] = new_node;
            }
            return 1;
        }
        
        unique_ptr<VType> get(const KType& key) const {
            auto current = head;
            for (int i = max_level; i >= 0; i--) {
                while (current->next[i] && current->next[i]->key < key) {
                    current = current->next[i];
                }
            }
            current = current->next[0];
            if (current && current->key == key) {
                return make_unique<VType>(current->value);
            }
            return nullptr;
        }

        void remove(const KType key) {
            vector<shared_ptr<Node>> update(max_level + 1);
            auto current = head;
            for (int i = max_level; i >= 0; i--) {
                while (current->next[i] && current->next[i]->key < key) {
                    current = current->next[i];
                }
                update[i] = current;
            }
            current = current->next[0];
            if (current && current->key == key) {
                for (int i = 0; i <= max_level; i++) {
                    if (update[i]->next[i] != current) {
                        break;
                    }
                    update[i]->next[i] = current->next[i];
                }
            }
        }

        void print() const {
            auto current = head;
            while(current->next[0] != nullptr) {
                for(int i = 0; i <= current->next[0]->level; i++) {
                    std::cout << std::format("{:>4}", current->next[0]->key);
                }
                std::cout << std::endl;
                current = current->next[0];
            }
            current = head;
            while(current->next[0] != nullptr) {
                std::cout << std::format("{:>4}", current->next[0]->key) << " " << current->next[0]->value << std::endl;
                current = current->next[0];
            }
        }

        int get_min_max_key(KType &min_key, KType &max_key) const {
            if (head->next[0] == nullptr) {
                return -1;
            }
            min_key = head->next[0]->key;
            auto current = head;
            while(current->next[0] != nullptr) {
                current = current->next[0];
            }
            max_key = current->key;
            return 0;
        }
    };
