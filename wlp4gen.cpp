#include <iostream>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <stack>

struct TreeNode {
    std::string token;
    std::string lexeme;
    int num_children;
    std::vector<TreeNode*> children;
    bool type_annotated = false;
    std::string type_annotation;
};

void handleExpr(TreeNode* node, int& pushes, bool& ptr);
void handleTerm(TreeNode* node, int& pushes, bool& ptr);
void handleFactor(TreeNode* node, int& pushes, bool& ptr);
void handleLvalues(TreeNode* node, int& pushes, bool& ptr);
void processStatements(TreeNode* node);

void printParseTree(TreeNode* node) {
    std::cout << node->token;
    for (auto it: node->children) {
        std::cout << " " << it->token;
    }
    std::cout << " " << node->lexeme;
    if (node->type_annotated) {
        std::cout << " : " << node->type_annotation;
    }
    std::cout << std::endl;
    for (auto it: node->children) {
        printParseTree(it);
    }
}

void deleteNode(TreeNode* node) {
    for (auto it: node->children) {
        deleteNode(it);
    }
    delete node;
}

std::unordered_set<std::string> terminals;
std::unordered_map<std::string, std::string> procedures_map;
std::unordered_map<std::string, std::unordered_map<std::string, std::pair<int, bool>>> variable_map;
std::unordered_map<std::string, int> max_frame_ptr;

std::string curr_func = "wain";
std::string temp_func;

bool first_parameter_of_main_is_array = false;

std::string if_branch = "1";
std::string while_branch = "1";
bool first_pass = true;

std::string incrementString(const std::string &s) {
    std::string result = s;
    int i = result.length() - 1;

    while (i >= 0 && result[i] == '9') {
        result[i] = '0';
        i--;
    }

    if (i >= 0) {
        result[i]++;
    } else {
        result = '1' + result;
    }

    return result;
}

void handleExpr(TreeNode* node, int& pushes, bool& ptr) {
    if (node->children.size() == 1) {
        handleTerm(node->children[0], pushes, ptr);
    }
    else if (node->children.size() == 3) {

        bool pointer1 = false;
        handleTerm(node->children[2], pushes, pointer1);

        int inner_pushes = 0;
        bool pointer2 = false;
        handleExpr(node->children[0], inner_pushes, pointer2);

        std::cout << "lw $10, 0($30)" << std::endl;

        std::cout << "lis $9" << std::endl;
        std::cout << ".word " << inner_pushes * 4 << std::endl;
        std::cout << "add $30, $30, $9" << std::endl;

        std::cout << "lw $11, 0($30)" << std::endl;

        if (pointer1 and !pointer2) {
            std::cout << "mult $10, $4" << std::endl;
            std::cout << "mflo $10" << std::endl;
            ptr = true;
        }
        else if (pointer2 and !pointer1) {
            std::cout << "mult $11, $4" << std::endl;
            std::cout << "mflo $11" << std::endl;
            ptr = true;
        }

        if (node->children[1]->token == "PLUS") {
            std::cout << "add $12, $10, $11" << std::endl;
        }
        else if (node->children[1]->token == "MINUS") {
            std::cout << "sub $12, $10, $11" << std::endl;
        }

        if (pointer1 && pointer2) {
            std::cout << "div $12, $4" << std::endl;
            std::cout << "mflo $12" << std::endl;
        }

        std::cout << "sw $12, -4($30)" << std::endl;
        std::cout << "sub $30, $30, $4" << std::endl;
        ++pushes;
    }
}


void handleTerm(TreeNode* node, int& pushes, bool& ptr) {
    if (node->children.size() == 1) {
        handleFactor(node->children[0], pushes, ptr);
    }
    else if (node->children.size() == 3) {
        handleFactor(node->children[2], pushes, ptr);      

        int inner_pushes = 0;
        handleTerm(node->children[0], inner_pushes, ptr);
        std::cout << "lw $13, 0($30)" << std::endl;

        std::cout << "lis $9" << std::endl;
        std::cout << ".word " << inner_pushes * 4 << std::endl;
        std::cout << "add $30, $30, $9" << std::endl;

        std::cout << "lw $14, 0($30)" << std::endl;

        if (node->children[1]->token == "STAR") {
            std::cout << "mult $13, $14" << std::endl;
            std::cout << "mflo $15" << std::endl;
        }
        else if (node->children[1]->token == "SLASH") {
            std::cout << "div $13, $14" << std::endl;
            std::cout << "mflo $15" << std::endl;
        }
        else if (node->children[1]->token == "PCT") {
            std::cout << "div $13, $14" << std::endl;
            std::cout << "mfhi $15" << std::endl;
        }

        std::cout << "sw $15, -4($30)" << std::endl;
        std::cout << "sub $30, $30, $4" << std::endl; 
        ++pushes;
    }
}


void handleFactor(TreeNode* node, int& pushes, bool& ptr) {
    if (node->children.size() == 1) {
        if (node->children[0]->token == "NUM") {
            std::string num = node->children[0]->lexeme;
            std::cout << "lis $16" << std::endl;
            std::cout << ".word " << num << std::endl;
            std::cout << "sw $16, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl; 
            ++pushes;
        }

        else if (node->children[0]->token == "ID") {
            std::cout << "lw $16, " << variable_map[curr_func][node->children[0]->lexeme].first << "($29)" << std::endl;
            std::cout << "sw $16, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl; 
            ++pushes;
            ptr = variable_map[curr_func][node->children[0]->lexeme].second;
        }

        else if (node->children[0]->token == "NULL") {
            std::cout << "lis $16" << std::endl;
            std::cout << ".word 1" << std::endl;
            std::cout << "sw $16, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl; 
            ++pushes;
        }
    }

    else if (node->children.size() == 2) {
        if (node->children[0]->token == "STAR") {
            bool ptr2 = true;
            handleFactor(node->children[1], pushes, ptr2);
            std::cout << "lw $16, 0($30)" << std::endl;
            std::cout << "lw $17, 0($16)" << std::endl;
            std::cout << "sw $17, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl;
            ++pushes;
            ptr = false;
        }

        else if (node->children[0]->token == "AMP") {
            bool ptr2 = false;
            handleLvalues(node->children[1], pushes, ptr2);
            std::cout << "sw $18, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl;
            ++pushes;
            ptr = true;
        }
    }

    else if (node->children.size() == 3) {
        if (node->children[0]->token == "LPAREN") {
            handleExpr(node->children[1], pushes, ptr);
        }
        else if (node->children[0]->token == "GETCHAR") {
            std::cout << "lis $5" << std::endl;
            std::cout << ".word 0xffff0004" << std::endl;
            std::cout << "lw $16, 0($5)" << std::endl;
            std::cout << "sw $16, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl;
            ++pushes;
        }
        else if (node->children[0]->token == "ID") {
            std::cout << "sw $31, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl;

            std::cout << "sw $29, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl;

            std::cout << "sub $29, $30, $4" << std::endl;

            std::cout << "sw $30, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl;
            
            std::cout << "lis $26" << std::endl;
            std::cout << ".word " << procedures_map[node->children[0]->lexeme] << std::endl;
            std::cout << "jalr $26" << std::endl;

            std::cout << "lw $30, 0($29)" << std::endl;

            std::cout << "add $30, $30, $4" << std::endl;
            std::cout << "add $30, $30, $4" << std::endl;

            std::cout << "lw $31, -4($30)" << std::endl;
            std::cout << "lw $29, -8($30)" << std::endl;

            std::cout << "sw $3, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl;   
            ++pushes;  
        }
    }
    else if (node->children.size() == 4) {
        if (node->children[0]->token == "ID") {
            std::cout << "sw $31, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl;

            std::cout << "sw $29, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl;

            TreeNode* arglist = node->children[2];
            int total_args = 0;

            while (arglist) {

                int pushes = 0;
                handleExpr(arglist->children[0], pushes, ptr);
                std::cout << "lw $23, 0($30)" << std::endl;

                std::cout << "lis $9" << std::endl;
                std::cout << ".word " << pushes * 4 << std::endl;
                std::cout << "add $30, $30, $9" << std::endl;

                std::cout << "sw $23, -4($30)" << std::endl;
                std::cout << "sub $30, $30, $4" << std::endl;
                ++total_args;
                arglist = arglist->children.size() == 1 ? NULL : arglist->children[2];
            }

            std::cout << "sub $29, $30, $4" << std::endl;

            std::cout << "sw $30, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl;
            
            std::cout << "lis $26" << std::endl;
            std::cout << ".word " << procedures_map[node->children[0]->lexeme] << std::endl;
            std::cout << "jalr $26" << std::endl;

            std::cout << "lw $30, 0($29)" << std::endl;

            std::cout << "lis $26" << std::endl;

            std::cout << ".word " << total_args * 4 + 8 << std::endl;
            std::cout << "add $30, $30, $26" << std::endl;

            std::cout << "lw $31, -4($30)" << std::endl;
            std::cout << "lw $29, -8($30)" << std::endl;


            std::cout << "sw $3, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl;  
            ++pushes;
        }
    }
    
    else if (node->children.size() == 5) {
        if (node->children[0]->token == "NEW") {
            handleExpr(node->children[3], pushes, ptr);
            std::cout << "lw $1, 0($30)" << std::endl;
            std::cout << "lis $8" << std::endl;
            std::cout << ".word new" << std::endl;
            std::cout << "sw $31, -4($30)" << std::endl;
            ++pushes;
            std::cout << "sub $30, $30, $4" << std::endl;
            std::cout << "add $22, $0, $30" << std::endl;
            std::cout << "jalr $8" << std::endl;
            std::cout << "add $30, $0, $22" << std::endl;
            std::cout << "lw $31, 0($30)" << std::endl;
            std::cout << "bne $3, $0, 2" << std::endl; // set invalid alloc to nullptr
            std::cout << "lis $3" << std::endl;
            std::cout << ".word 1" << std::endl;
            std::cout << "sw $3, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl;
            ++pushes;
            ptr = true;
        }
    }
}


void handleLvalues(TreeNode* node, int& pushes, bool& ptr) {
    if (node->children.size() == 1) {
        if (node->children[0]->token == "ID") {
            std::cout << "lis $18" << std::endl;
            std::cout << ".word " << variable_map[curr_func][node->children[0]->lexeme].first << std::endl;
            std::cout << "add $18, $18, $29" << std::endl;
            std::cout << "sw $18, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl;  
            ++pushes;
            ptr = variable_map[curr_func][node->children[0]->lexeme].second;
        }   
    }

    else if (node->children.size() == 2) {
        if (node->children[0]->token == "STAR") {
            bool ptr2 = true;
            handleFactor(node->children[1], pushes, ptr2);
            std::cout << "lw $18, 0($30)" << std::endl;
            std::cout << "sw $18, -4($30)" << std::endl;
            std::cout << "sub $30, $30, $4" << std::endl; 
            ++pushes;
            ptr = true;
        }
    }

    else if (node->children.size() == 3) {
        if (node->children[0]->token == "LPAREN") {
            handleLvalues(node->children[1], pushes, ptr);
        }
    }
}

void rebuildTree(TreeNode* node) {
    std::string s;
    std::getline(std::cin, s);
    std::istringstream iss{s};
    std::string child;
    iss >> node->token;
    iss >> child;
    if (terminals.contains(node->token) || child == ".EMPTY") node->lexeme = child;
    else {++node->num_children; while (!terminals.contains(node->token) && iss >> child && child != ":") ++node->num_children;}
    if (node->num_children > 0) {
        for (int i = 0; i < node->num_children; ++i) {
            TreeNode* child_node = new TreeNode{};
            rebuildTree(child_node);
            node->children.emplace_back(child_node);
        }
    }
}

void processReturn(TreeNode* node) {
    int temp = 0;
    bool ptr = false;
    handleExpr(node, temp, ptr);
    std::cout << "lw $3, 0($30)" << std::endl;
}

void setupArgs(TreeNode* node) {
    if (node->token == "main") {
        std::cout << "sw $1, -4($30)" << std::endl;
        std::cout << "sub $30, $30, $4" << std::endl; 
        variable_map[curr_func][node->children[3]->children[1]->lexeme] = std::make_pair(8, node->children[3]->children[0]->children.size() == 2);
        first_parameter_of_main_is_array = node->children[3]->children[0]->children.size() == 2;

        std::cout << "sw $2, -4($30)" << std::endl;
        std::cout << "sub $30, $30, $4" << std::endl; 
        variable_map[curr_func][node->children[5]->children[1]->lexeme] = std::make_pair(4, node->children[5]->children[0]->children.size() == 2);
        
        std::cout << "sub $29, $30, $4" << std::endl;
        std::cout << "sub $30, $30, $4" << std::endl; 
    }
    else {
        TreeNode* params = node->children[3];
        int offset = 4;
        if (params->children.size() > 0) {
            TreeNode* paramlist = params->children[0];
            std::stack<std::pair<std::string, int>> parameters;
            while (paramlist) {
                parameters.push(std::make_pair(paramlist->children[0]->children[1]->lexeme, paramlist->children[0]->children[0]->children.size() == 2));
                paramlist = paramlist->children.size() > 1 ? paramlist->children[2] : NULL;
            }
            while (parameters.size() > 0) {
                variable_map[curr_func][parameters.top().first] = std::make_pair(offset, parameters.top().second);
                parameters.pop();
                offset += 4;
            }
        }        
    }
}

void processDeclarations(TreeNode* node, int offset) {
    if (node->token == "dcls") {
        if (node->children.size() == 0) {
            max_frame_ptr[curr_func] = 0;
            return;
        }
        else if (node->children.size() == 5) {
            if (node->children[3]->token == "NUM") {
                TreeNode* dcl = node->children[1];
                TreeNode* num = node->children[3];
                variable_map[curr_func][dcl->children[1]->lexeme] = std::make_pair(offset, false);

                std::cout << "lis $5" << std::endl;
                std::cout << ".word " << num->lexeme << std::endl;
                std::cout << "sw $5, -4($30)" << std::endl;
                std::cout << "sub $30, $30, $4" << std::endl;

                offset -= 4;
            }
            else if (node->children[3]->token == "NULL") {
                TreeNode* dcl = node->children[1];
                variable_map[curr_func][dcl->children[1]->lexeme] = std::make_pair(offset, true);

                std::cout << "lis $5" << std::endl;
                std::cout << ".word 1" << std::endl;
                std::cout << "sw $5, -4($30)" << std::endl;
                std::cout << "sub $30, $30, $4" << std::endl;

                offset -= 4;
            }
            processDeclarations(node->children[0], offset);
        }
    }
}

void handleTest(TreeNode* node, std::string branch, bool while_loop) {
    int temp = 0;
    bool ptr = false;
    handleExpr(node->children[0], temp, ptr);

    int pushes = 0;
    bool ptr2 = false;
    handleExpr(node->children[2], pushes, ptr2);
    std::cout << "lw $5, 0($30)" << std::endl;

    std::cout << "lis $9" << std::endl;
    std::cout << ".word " << pushes * 4 << std::endl;
    std::cout << "add $30, $30, $9" << std::endl;

    std::cout << "lw $14, 0($30)" << std::endl;
    

    if (node->token == "test") {
        if (node->children[1]->token == "EQ") {
            if (!while_loop) std::cout << "beq $14, $5, " << branch << std::endl;
            else std::cout << "bne $14, $5, " << branch << std::endl;
        }
        else if (node->children[1]->token == "NE") {
            if (!while_loop) std::cout << "bne $14, $5, " << branch << std::endl;
            else std::cout << "beq $14, $5, " << branch << std::endl;
        }
        else if (node->children[1]->token == "LT") {
            if (ptr || ptr2) std::cout << "sltu $6, $14, $5" << std::endl;
            else std::cout << "slt $6, $14, $5" << std::endl;
            std::cout << "lis $7" << std::endl;
            std::cout << ".word 1" << std::endl;
            if (!while_loop) std::cout << "beq $6, $7, " << branch << std::endl;
            else {
                std::cout << "beq $6, $7, 1" << std::endl;
                std::cout << "beq $0, $0, " << branch << std::endl;
            }
        }
        else if (node->children[1]->token == "LE") {
            if (ptr || ptr2) std::cout << "sltu $6, $14, $5" << std::endl;
            else std::cout << "slt $6, $14, $5" << std::endl;
            std::cout << "lis $7" << std::endl;
            std::cout << ".word 1" << std::endl;
            if (!while_loop) {
                std::cout << "beq $6, $7, " << branch << std::endl;
                std::cout << "beq $14, $5, " << branch << std::endl;
            }
            else {
                std::cout << "beq $6, $7, 2" << std::endl;
                std::cout << "beq $14, $5, 1" << std::endl;
                std::cout << "beq $0, $0, " << branch << std::endl;
            }
        }
        else if (node->children[1]->token == "GE") {
            if (ptr || ptr2) std::cout << "sltu $6, $14, $5" << std::endl;
            else std::cout << "sltu $6, $14, $5" << std::endl;
            std::cout << "lis $7" << std::endl;
            std::cout << ".word 1" << std::endl;
            if (!while_loop) std::cout << "bne $6, $7, " << branch << std::endl;
            else {
                std::cout << "bne $6, $7, 1" << std::endl;
                std::cout << "beq $0, $0, " << branch << std::endl;
            }
        }
        else if (node->children[1]->token == "GT") {
            if (ptr || ptr2) std::cout << "sltu $6, $14, $5" << std::endl;
            else std::cout << "sltu $6, $14, $5" << std::endl;
            std::cout << "lis $7" << std::endl;
            std::cout << ".word 1" << std::endl;
            if (!while_loop) {
                std::cout << "beq $14, $5, 1" << std::endl;
                std::cout << "bne $6, $7, " << branch << std::endl;
            }
            else {
                std::cout << "beq $14, $5, 1" << std::endl;
                std::cout << "bne $6, $7, 1" << std::endl;
                std::cout << "beq $0, $0, " << branch << std::endl;
            }
        }
    }
}

void handleStatement(TreeNode* node) {
    if (node->token == "statement") {
        if (node->children.size() == 4) {
            TreeNode* statement = node;
            int temp = 0;
            bool ptr = false;
            handleLvalues(statement->children[0], temp, ptr);

            int pushes = 0;
            bool ptr2 = false;
            handleExpr(statement->children[2], pushes, ptr2);
            std::cout << "lw $14, 0($30)" << std::endl;

            std::cout << "lis $9" << std::endl;
            std::cout << ".word " << pushes * 4 << std::endl;
            std::cout << "add $30, $30, $9" << std::endl;
            std::cout << "lw $13, 0($30)" << std::endl;

            std::cout << "sw $14, 0($13)" << std::endl;
        }
        else if (node->children.size() == 5) {
            if (node->children[0]->token == "PUTCHAR") {
                int temp = 0;
                bool ptr = false;
                handleExpr(node->children[2], temp, ptr);
                std::cout << "lw $14, 0($30)" << std::endl;
                std::cout << "lis $5" << std::endl;
                std::cout << ".word 0xffff000c" << std::endl;
                std::cout << "sw $14, 0($5)" << std::endl;
            }
            else if (node->children[0]->token == "PRINTLN") {
                int temp = 0;
                bool ptr = false;
                handleExpr(node->children[2], temp, ptr);
                std::cout << "lw $1, 0($30)" << std::endl;
                std::cout << "sw $31, -4($30)" << std::endl;
                std::cout << "sub $30, $30, $4" << std::endl;
                std::cout << "add $22, $0, $30" << std::endl;
                std::cout << "lis $14" << std::endl;
                std::cout << ".word print" << std::endl;
                std::cout << "jalr $14" << std::endl;
                std::cout << "add $30, $0, $22" << std::endl;
                std::cout << "lw $31, 0($30)" << std::endl;
            }
            else if (node->children[0]->token == "DELETE") {
                int temp = 0;
                bool ptr = false;
                handleExpr(node->children[3], temp, ptr);
                std::cout << "lw $1, 0($30)" << std::endl;
                std::cout << "lis $15" << std::endl;
                std::cout << ".word 1" << std::endl;
                std::cout << "beq $1, $15, 8" << std::endl; // skip deleting a null pointer to avoid crash
                std::cout << "sw $31, -4($30)" << std::endl;
                std::cout << "sub $30, $30, $4" << std::endl;
                std::cout << "add $22, $0, $30" << std::endl;
                std::cout << "lis $14" << std::endl;
                std::cout << ".word delete" << std::endl;
                std::cout << "jalr $14" << std::endl;
                std::cout << "add $30, $0, $22" << std::endl;
                std::cout << "lw $31, 0($30)" << std::endl;
            }
        }
        else if (node->children.size() == 7) {
            std::string branch = "while" + while_branch;
            std::string branch2 = "secondwhile" + while_branch;
            while_branch = incrementString(while_branch);

            std::cout << branch2 << ":" << std::endl;
            handleTest(node->children[2], branch, true);
            processStatements(node->children[5]);
            
            std::cout << "beq $0, $0, " << branch2 << std::endl;
            std::cout << branch << ":" << std::endl;
            
        }
        else if (node->children.size() == 11) {
            
            std::string branch_2 = "if" + if_branch;
            std::string branch_1 = "else" + if_branch;
            if_branch = incrementString(if_branch);
            
            handleTest(node->children[2], branch_2, false);

            processStatements(node->children[9]);
            std::cout << "beq $0, $0, " << branch_1 << std::endl;

            std::cout << branch_2 << ":" << std::endl;
            processStatements(node->children[5]);

            std::cout << branch_1 << ":" << std::endl;
        }
    }
}

void processStatements(TreeNode* node) {
    if (node->token == "statements") {
        if (node->children.size() == 0) {
            return;
        }
        else if (node->children.size() == 2) {
            processStatements(node->children[0]);
            handleStatement(node->children[1]);
        }
    }
}
void processMain(TreeNode* node) {
    curr_func = "main";
    setupArgs(node);

    if (first_parameter_of_main_is_array) {
        std::cout << "sw $31, -4($30)" << std::endl;
        std::cout << "sub $30, $30, $4" << std::endl;
        std::cout << "add $22, $0, $30" << std::endl;
        std::cout << "lis $14" << std::endl;
        std::cout << ".word init" << std::endl;
        std::cout << "jalr $14" << std::endl;
        std::cout << "add $30, $0, $22" << std::endl;
        std::cout << "lw $31, 0($30)" << std::endl;
        std::cout << "add $30, $30, $4" << std::endl;
    }
    else {
        std::cout << "sw $31, -4($30)" << std::endl;
        std::cout << "sub $30, $30, $4" << std::endl;
        std::cout << "add $22, $0, $30" << std::endl;
        std::cout << "add $21, $2, $0" << std::endl;
        std::cout << "add $2, $0, $0" << std::endl;
        std::cout << "lis $14" << std::endl;
        std::cout << ".word init" << std::endl;
        std::cout << "jalr $14" << std::endl;
        std::cout << "add $30, $0, $22" << std::endl;
        std::cout << "lw $31, 0($30)" << std::endl;
        std::cout << "add $2, $21, $0" << std::endl;
        std::cout << "add $30, $30, $4" << std::endl;
    }

    processDeclarations(node->children[8], -4);

    processStatements(node->children[9]);

    processReturn(node->children[11]);

    std::cout << "jr $31" << std::endl;
}

void processProcedure(TreeNode* node) {
    curr_func = node->children[1]->lexeme;
    std::cout << procedures_map[node->children[1]->lexeme] << ":" << std::endl;

    setupArgs(node);

    processDeclarations(node->children[6], -4);

    processStatements(node->children[7]);

    processReturn(node->children[9]);

    std::cout << "jr $31" << std::endl;
}

void handleAllProcedures(TreeNode* node) {
    if (node->children.size() == 1) processMain(node->children[0]);
    else {
        handleAllProcedures(node->children[1]);
        processProcedure(node->children[0]);
    }
}

int main() {

    std::cout << ".import print" << std::endl;
    std::cout << ".import init" << std::endl;
    std::cout << ".import new" << std::endl;
    std::cout << ".import delete" << std::endl;

    std::cout << "add $29, $0, $30" << std::endl;
    std::cout << "lis $4" << std::endl;
    std::cout << ".word 4" << std::endl;

    terminals.insert("BOF"); terminals.insert("EOF"); terminals.insert("WAIN"); terminals.insert("LPAREN"); terminals.insert("INT"); terminals.insert("COMMA"); terminals.insert("RPAREN"); terminals.insert("LBRACE"); terminals.insert("RETURN"); terminals.insert("SEMI"); terminals.insert("RBRACE"); terminals.insert("STAR"); terminals.insert("ID"), terminals.insert("NUM"); terminals.insert("NULL"); terminals.insert(".EMPTY"); terminals.insert("PLUS"); terminals.insert("MINUS"); terminals.insert("SLASH"); terminals.insert("PCT"); terminals.insert("AMP"); terminals.insert("NEW"); terminals.insert("LBRACK"); terminals.insert("RBRACK"); terminals.insert("BECOMES"); terminals.insert("GETCHAR"); terminals.insert("WHILE"); terminals.insert("PRINTLN"); terminals.insert("PUTCHAR"); terminals.insert("DELETE"); terminals.insert("EQ"); terminals.insert("NE"); terminals.insert("LT"); terminals.insert("LE"); terminals.insert("GE"); terminals.insert("GT"); terminals.insert("IF"); terminals.insert("ELSE");

    TreeNode* head = new TreeNode{};
    rebuildTree(head);

    TreeNode* procedures = head->children[1];

    while (procedures->children.size() != 1){
        std::string func_name = procedures->children[0]->children[1]->lexeme;
        procedures_map[func_name] = "f" + func_name;
        procedures = procedures->children[1];
    }

    procedures = head->children[1];

    handleAllProcedures(procedures);

    deleteNode(head);

    return 0;
}