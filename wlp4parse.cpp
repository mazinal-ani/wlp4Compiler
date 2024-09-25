#include <iostream>
#include <utility>
#include <unordered_map>
#include <stack>
#include <sstream>
#include <vector>
#include <set>
#include "wlp4data.h"

struct ParseTreeNode {
    std::string token;
    std::string lexeme;
    std::vector<ParseTreeNode*> children;
};

struct Production {
    std::string lhs; std::string rhs; int rhs_length;
};

struct Token {
    std::string token_type; std::string lexeme; ParseTreeNode* tree_node;
};

int curr_state = 0;
std::unordered_map<int, Production> productions;
std::unordered_map<int, std::unordered_map<std::string, int>> transitions;
std::unordered_map<int, std::unordered_map<std::string, int>> reductions;
std::set<int> accepting_states;
std::stack<int> state_stack;
int shifted_terminals = 0;
std::stack<Token> seen;
std::stack<Token> unseen_inverted;
std::stack<Token> unseen;


void errorOut() {
    std::cerr << "ERROR at " << shifted_terminals + 1 << std::endl;
}

void shift(Token next, bool increment_terminal_counter) {
    unseen.pop();
    int next_state = transitions[curr_state][next.token_type];
    if (increment_terminal_counter) ++shifted_terminals;
    seen.push(next);
    state_stack.push(next_state);
    curr_state = next_state;
}

void print(ParseTreeNode* node) {
    if (node->children.empty()) {
        std::cout << node->token << " " << node -> lexeme << std::endl;
        return;
    }
    int children_length = int((node->children).size());
    std::cout << node->token;
    for (int i = children_length; i > 0; --i) {
        std::cout << " " << node->children[i-1]->token;
    }
    std::cout << std::endl;
    for (int i = children_length; i > 0; --i) {
        print(node->children[i-1]);
    }
}

void deleteNode(ParseTreeNode* node) {
    for (auto child: node->children) deleteNode(child);
    delete node;
}

void deleteStackNodes() {
    while (!seen.empty()) {
        deleteNode(seen.top().tree_node);
        seen.pop();
    }
    while (!unseen.empty()) {
        deleteNode(unseen.top().tree_node);
        unseen.pop();
    }
}

int main() {
    std::string s;
    std::istringstream input{WLP4_COMBINED};
    if (std::getline(input, s) && s != ".CFG") {
        std::cerr << "Error: CFG not provided at start of the file!" << std::endl;
        return 1;
    }
    int num_derivations = 0;
    while (std::getline(input, s) && s != ".TRANSITIONS") {
        size_t space_position = s.find(' ');
        std::string lhs; std::string rhs;
        if (space_position == std::string::npos) {
            std::cerr << "Error: Production rule does not contain LHS/RHS! String: " << s << std::endl;
            return 0;
        }
        lhs = s.substr(0, space_position);
        rhs = s.substr(space_position + 1);
        std::istringstream rhs_counter{rhs};
        int rhs_length = 0; std::string rhs_counter_str;
        while (rhs_counter >> rhs_counter_str) if (rhs_counter_str != ".EMPTY") ++rhs_length;
        productions[num_derivations] = Production{lhs=lhs, rhs=rhs, rhs_length=rhs_length};
        ++num_derivations;
    }
    while (std::getline(input, s) && s != ".REDUCTIONS") {
        std::istringstream transition_line{s}; int start_state; std::string token; int end_state;
        transition_line >> start_state; transition_line >> token; transition_line >> end_state;
        transitions[start_state][token] = end_state;
    }
    while (std::getline(input, s) && s != ".END") {
        std::istringstream reduction_line{s}; int start_state; int cfg_rule; std::string token;
        reduction_line >> start_state; reduction_line >> cfg_rule; reduction_line >> token;
        reductions[start_state][token] = cfg_rule;
        if (!accepting_states.contains(start_state)) accepting_states.insert(start_state);
    }

    std::istream& stdinput = std::cin;

    ParseTreeNode* BOF_node = new ParseTreeNode{"BOF", "BOF", std::vector<ParseTreeNode*>()};
    unseen_inverted.push(Token{"BOF", "BOF", BOF_node});

    while (std::getline(stdinput, s)) {
       std::istringstream input_text{s}; std::string token; std::string lexeme;
       while (input_text >> token) {
            input_text >> lexeme;
            ParseTreeNode* node = new ParseTreeNode{token, lexeme, std::vector<ParseTreeNode*>()};
            unseen_inverted.push(Token{token, lexeme, node});
       }
    }


    ParseTreeNode* EOF_node = new ParseTreeNode{"EOF", "EOF", std::vector<ParseTreeNode*>()};
    unseen_inverted.push(Token{"EOF", "EOF", EOF_node});

    while (!unseen_inverted.empty()) {
        unseen.push(unseen_inverted.top());
        unseen_inverted.pop();
    }

    ParseTreeNode* start_node = new ParseTreeNode{"start", "", std::vector<ParseTreeNode*>()};

    while (true) {
        Token next = !unseen.empty() ? unseen.top() : Token{".ACCEPT", ".ACCEPT"};

        if (transitions[curr_state].contains(next.token_type)) {
            shift(next, (next.token_type != "BOF" && next.token_type != "EOF"));
        }

        else if (accepting_states.contains(curr_state)) {
            if (reductions[curr_state].contains(next.token_type)) {
                int reduction_rule = reductions[curr_state][next.token_type];
                int num_to_pop = productions[reduction_rule].rhs_length;
                ParseTreeNode* curr_node = new ParseTreeNode{productions[reduction_rule].lhs, "", std::vector<ParseTreeNode*>()};
                if (productions[reduction_rule].rhs == ".EMPTY") curr_node->lexeme = ".EMPTY";
                for (int i = 0; i < num_to_pop; ++i) state_stack.pop();
                for (int i = 0; i < num_to_pop; ++i) {
                    if (state_stack.empty()) start_node->children.emplace_back(seen.top().tree_node);
                    else curr_node->children.emplace_back(seen.top().tree_node);
                    seen.pop(); 
                }
                if (state_stack.empty()) {
                    seen.push(Token{productions[reduction_rule].lhs, ""});
                    delete curr_node;
                    break;
                }
                unseen.push(Token{productions[reduction_rule].lhs, "", curr_node});
                curr_state = state_stack.top();
                if (!transitions[curr_state].contains(unseen.top().token_type)) {deleteStackNodes(); if (start_node) delete start_node; errorOut(); return 1;}
                shift(unseen.top(), false);
            }
            else {deleteStackNodes(); if (start_node) delete start_node; errorOut(); return 1;}
        }

        else {deleteStackNodes(); if (start_node) delete start_node; errorOut(); return 1;}

    }

    print(start_node);

    deleteNode(start_node);
}