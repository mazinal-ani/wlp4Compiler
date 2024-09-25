#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <unordered_map>

struct TreeNode {
    std::string token;
    std::string lexeme;
    int num_children;
    std::vector<TreeNode*> children;
    bool type_annotated = false;
    std::string type_annotation;
};

bool exprCheck(TreeNode* expr_node, std::unordered_map<std::string, std::string>& sym_table);
bool factorCheck(TreeNode* factor_node, std::unordered_map<std::string, std::string>& sym_table);
bool lvalueCheck(TreeNode* lvalue_node, std::unordered_map<std::string, std::string>& sym_table);
bool termCheck(TreeNode* term_node, std::unordered_map<std::string, std::string>& sym_table);
bool statementCheck(TreeNode* statement_node, std::unordered_map<std::string, std::string>& sym_table);
bool testCheck(TreeNode* test_node, std::unordered_map<std::string, std::string>& sym_table);
bool multipleStatementsCheck(TreeNode* statement_node, std::unordered_map<std::string, std::string>& sym_table);

std::unordered_set<std::string> terminals;
std::unordered_map<std::string, std::unordered_map<std::string, std::string>> symbol_table;
std::vector<std::string> procedure_order;
std::unordered_map<std::string, std::vector<std::string>> functionInfo;

void deleteNode(TreeNode* node) {
    for (auto it: node->children) {
        deleteNode(it);
    }
    delete node;
}

void typeAnnotate(TreeNode* node, std::unordered_map<std::string, std::string>& sym_table) {
    for (auto it: node->children) typeAnnotate(it, sym_table);

    if (node->token == "dcl") {
        bool is_normal_int = node->children[0]->children.size() == 1;
        node->children[1]->type_annotated = true;
        node->children[1]->type_annotation = is_normal_int ? "int" : "int*";
        for (auto it: node->children) typeAnnotate(it, sym_table);
    }

    else if (node->token == "dcls" && node->children.size() > 0) {
        node->children[3]->type_annotated = true;
        node->children[3]->type_annotation = node->children[1]->children[1]->type_annotation;
    }

    else if (node->token == "expr" || node->token == "term") {
        if (node->children.size() == 1) {
            typeAnnotate(node->children[0], sym_table);
            node->type_annotated = true;
            node->type_annotation = node->children[0]->type_annotation;
        }
        else {
            typeAnnotate(node->children[0], sym_table);
            typeAnnotate(node->children[2], sym_table);
            node->type_annotated = true;
            if (node->children[0]->type_annotation == node->children[2]->type_annotation) {
                if (node->children[1]->token == "MINUS" && node->children[0]->type_annotation == "int*") {
                    node->type_annotation = "int";
                }
                else node->type_annotation = node->children[0]->type_annotation;
            }
            else node->type_annotation = "int*";   // node->children[0]->type_annotation;
        }
    }

    else if (node->token == "factor") {
        if (node->children.size() == 1) {
            if (node->children[0]->token == "NUM" || node->children[0]->token == "NULL") {
                node->children[0]->type_annotated = true;
                node->children[0]->type_annotation = node->children[0]->token == "NUM" ? "int" : "int*";
            }
            else {
                node->children[0]->type_annotated = true;
                node->children[0]->type_annotation = sym_table[node->children[0]->lexeme];
            }
        }
        
        else if (node->children.size() == 2) {
            typeAnnotate(node->children[1], sym_table);
        }

        else if (node->children.size() == 3) {
            if (node->children[0]->token == "LPAREN") {
                typeAnnotate(node->children[1], sym_table);
            }
        }

        else if (node->children.size() == 5) typeAnnotate(node->children[3], sym_table);

        node->type_annotated = true;

        if (node->children.size() == 1) node->type_annotation = node->children[0]->type_annotation;
        else if (node->children.size() == 2) node->type_annotation = node->children[0]->token == "AMP" ? "int*" : "int";
        else if (node->children.size() == 3) {
            if (node->children[0]->token != "ID" && node->children[0]->token != "GETCHAR") node->type_annotation = node->children[1]->type_annotation;
            else node->type_annotation = "int";
        }
        else if (node->children.size() == 4) node->type_annotation = "int";
        else node->type_annotation = "int*"; // NEW INT[X] CASE
    }

    else if (node->token == "lvalue") {
        if (node->children.size() == 1) {
            node->children[0]->type_annotated = true;
            node->children[0]->type_annotation = sym_table[node->children[0]->lexeme];
        }

        else {
            typeAnnotate(node->children[1], sym_table);
        }

        node->type_annotated = true;

        if (node->children.size() == 1) {
            node->type_annotation = node->children[0]->type_annotation;
        }
        else if (node->children.size() == 2) {
            node->type_annotation = "int";
        }
        else {
             node->type_annotation = node->children[1]->type_annotation;
        }
    }
}

void typeAnnotateAllProcedures(TreeNode* head) {
    while (head->children.size() > 1) {
        std::unordered_map<std::string, std::string>& sym_table = symbol_table[head->children[0]->children[1]->lexeme];
        typeAnnotate(head->children[0], sym_table);
        head = head->children[1];
    }
    typeAnnotate(head->children[0], symbol_table["wain"]);
}

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

bool lvalueCheck(TreeNode* lvalue_node, std::unordered_map<std::string, std::string>& sym_table) {
    if (lvalue_node->children.size() == 1) return sym_table.contains(lvalue_node->children[0]->lexeme);
    if (lvalue_node->children.size() == 2) return factorCheck(lvalue_node->children[1], sym_table);
    return lvalueCheck(lvalue_node->children[1], sym_table);
}

bool factorCheck(TreeNode* factor_node, std::unordered_map<std::string, std::string>& sym_table) {
    if (factor_node->children.size() == 1) {
        if (factor_node->children[0]->token == "ID" && !sym_table.contains(factor_node->children[0]->lexeme)) return false;
        return true;
    }

    if (factor_node->children.size() == 2) {
        if (factor_node->children[0]->token == "AMP") return lvalueCheck(factor_node->children[1], sym_table);
        else return factorCheck(factor_node->children[1], sym_table);
    }

    if (factor_node->children.size() == 3) {
        if (factor_node->children[0]->token == "ID") return symbol_table.contains(factor_node->children[0]->lexeme);
        else if (factor_node->children[0]->token == "GETCHAR") return true;
        return exprCheck(factor_node->children[1], sym_table);
    }

    if (factor_node->children.size() == 4) {
        return symbol_table.contains(factor_node->children[0]->lexeme);
    }

    return exprCheck(factor_node->children[3], sym_table);
}

bool termCheck(TreeNode* term_node, std::unordered_map<std::string, std::string>& sym_table) {
    if (term_node->children.size() == 1) return factorCheck(term_node->children[0], sym_table);
    if (!factorCheck(term_node->children[2], sym_table)) return false;
    return termCheck(term_node->children[0], sym_table);
}

bool exprCheck(TreeNode* expr_node, std::unordered_map<std::string, std::string>& sym_table) {
    if (expr_node->children.size() == 1) return termCheck(expr_node->children[0], sym_table);

    if (!termCheck(expr_node->children[2], sym_table)) return false;

    return exprCheck(expr_node->children[0], sym_table);
}

bool statementCheck(TreeNode* statement_node, std::unordered_map<std::string, std::string>& sym_table) {
    if (statement_node->children.size() == 4) {
        return lvalueCheck(statement_node->children[0], sym_table) && exprCheck(statement_node->children[2], sym_table);
    }
    if (statement_node->children.size() == 5) {
        if (statement_node->children[0]->token == "DELETE") return exprCheck(statement_node->children[3], sym_table);
        else return exprCheck(statement_node->children[2], sym_table);
    }
    if (statement_node->children.size() == 7) {
        return testCheck(statement_node->children[2], sym_table) && multipleStatementsCheck(statement_node->children[5], sym_table);
    }
    else {
        return testCheck(statement_node->children[2], sym_table) && multipleStatementsCheck(statement_node->children[5], sym_table) && multipleStatementsCheck(statement_node->children[9], sym_table);
    }
}

bool testCheck(TreeNode* test_node, std::unordered_map<std::string, std::string>& sym_table) {
    return exprCheck(test_node->children[0], sym_table) && exprCheck(test_node->children[2], sym_table);
}

bool multipleStatementsCheck(TreeNode* statement_node, std::unordered_map<std::string, std::string>& sym_table) {
    if (statement_node->children.size() == 0) return true;
    if (!statementCheck(statement_node->children[1], sym_table)) return false;
    return multipleStatementsCheck(statement_node->children[0], sym_table);
}

bool semanticCheck(TreeNode* main_node) {
    TreeNode* statements = main_node->children[1]->token == "WAIN" ? main_node->children[9] : main_node->children[7];
    if (!multipleStatementsCheck(statements, symbol_table[main_node->children[1]->lexeme])) return false;

    if (main_node->children[1]->token == "WAIN") {
        std::string first_param_name = main_node->children[3]->children[1]->lexeme;
        TreeNode* second_param = main_node->children[5];
        std::string second_param_name = second_param->children[1]->lexeme;

        if (first_param_name == second_param_name) return false;

        if (second_param->children[0]->children.size() > 1) return false;

        return exprCheck(main_node->children[11], symbol_table["wain"]);
    }
    else {
        //TreeNode* params = main_node->children[3];
        //if (!paramsSemanticCheck(params)) return false;
        // might have to semantic check dcls
        return exprCheck(main_node->children[9], symbol_table[main_node->children[1]->lexeme]);
    }
    
}

bool buildTable(TreeNode* node, int null_num_none, std::unordered_map<std::string, std::string>& sym_table) { // 1 = none, 2 = null, 3 = num
    if (node->token == "dcls" && node->children.size() == 0) return true;

    else if (node->token == "dcls") {
        if (node->children[3]->token == "NUM") return buildTable(node->children[1], 3, sym_table) && buildTable(node->children[0], 1, sym_table);
        else return buildTable(node->children[1], 2, sym_table) && buildTable(node->children[0], 1, sym_table);
    }

    else if (node->token == "dcl") {
        bool is_normal_int = node->children[0]->children.size() == 1;
        std::string var_name = node->children[1]->lexeme;
        if (sym_table.contains(var_name)) return false;
        sym_table[var_name] = is_normal_int ? "int" : "int*";

        if (is_normal_int && null_num_none == 2 || !is_normal_int && null_num_none == 3) return false;

        return true;
    }

    else {
        if (node->token == "factor" && node->children.size() > 1 && node->children[0]->token == "ID") {
            if (!symbol_table.contains(node->children[0]->lexeme)) return false;
        }
        for (auto it: node->children) {
            if (!buildTable(it, 1, sym_table)) return false;
        }
    }

    return true;
}

bool setupTables(TreeNode* node) {

    if (node->children.size() == 1) {
        symbol_table["wain"] = std::unordered_map<std::string, std::string>();
        procedure_order.emplace_back("wain");
        
        functionInfo["wain"] = std::vector<std::string>();
        if (node->children[0]->children[3]->children[0]->children.size() > 1) functionInfo["wain"].emplace_back("int*");
        else functionInfo["wain"].emplace_back("int");
        functionInfo["wain"].emplace_back("int");

        return buildTable(node->children[0], 1, symbol_table["wain"]);
    }

    if (node->children[0]->children[1]->lexeme == "wain") return false;
    if (symbol_table.contains(node->children[0]->children[1]->lexeme)) return false;
    symbol_table[node->children[0]->children[1]->lexeme] = std::unordered_map<std::string, std::string>();

    functionInfo[node->children[0]->children[1]->lexeme] = std::vector<std::string>();
    if (node->children[0]->children[3]->children.size() > 0) {
        TreeNode* tempParamIterator = node->children[0]->children[3]->children[0];
        while (tempParamIterator->children.size() > 1) {
            if (tempParamIterator->children[0]->children[0]->children.size() > 1) {
                functionInfo[node->children[0]->children[1]->lexeme].emplace_back("int*");
            }
            else {
                functionInfo[node->children[0]->children[1]->lexeme].emplace_back("int");
            }
            tempParamIterator = tempParamIterator->children[2];
        }
        if (tempParamIterator->children[0]->children[0]->children.size() > 1) {
            functionInfo[node->children[0]->children[1]->lexeme].emplace_back("int*");
        }
        else {
            functionInfo[node->children[0]->children[1]->lexeme].emplace_back("int");
        }
    }  

    procedure_order.emplace_back(node->children[0]->children[1]->lexeme);
    if (!buildTable(node->children[0], 1, symbol_table[node->children[0]->children[1]->lexeme])) return false;
    return setupTables(node->children[1]);

}

void rebuildTree(TreeNode* node) {
    std::string s;
    std::getline(std::cin, s);
    std::istringstream iss{s};
    std::string child;
    iss >> node->token;
    iss >> child;
    if (terminals.contains(node->token) || child == ".EMPTY") node->lexeme = child;
    else {++node->num_children; while (!terminals.contains(node->token) && iss >> child) ++node->num_children;}
    if (node->num_children > 0) {
        for (int i = 0; i < node->num_children; ++i) {
            TreeNode* child_node = new TreeNode{};
            rebuildTree(child_node);
            node->children.emplace_back(child_node);
        }
    }
}

void formatError(std::string err_msg) {
    std::cerr << err_msg << std::endl;
}

bool checkReferencing(TreeNode* node) {
    if (node->token == "factor" && node->children.size() == 2) {
        if (node->children[0]->token == "STAR" && node->children[1]->type_annotation == "int") return false;
        if (node->children[0]->token == "AMP" && node->children[1]->type_annotation == "int*") return false; // ADDED & CHECK
    }
    else if (node->token == "lvalue" && node->children.size() == 2) {
        if (node->children[0]->token == "STAR" && node->children[1]->type_annotation == "int") return false;
    }
    for (auto it: node->children) {
        if (!checkReferencing(it)) return false;
    }
    return true;
}

bool recursiveCheckCalls(TreeNode* node) {
    if (node->token == "factor" && node->children.size() > 1 && node->children[0]->token == "ID") {
        std::string function_name = node->children[0]->lexeme;
        if (node->children.size() == 3) {
            if (functionInfo[function_name].size() == 0) return true;
            else return false;
        }
        else {
            int counter = 0; TreeNode* temp = node->children[2];
            while (temp->children.size() > 1) {
                if (counter >= functionInfo[function_name].size()) return false;
                if (temp->children[0]->type_annotation != functionInfo[function_name][counter]) return false;
                ++counter;
                temp = temp->children[2];
            }
            if (counter >= functionInfo[function_name].size()) return false;
            if (temp->children[0]->type_annotation != functionInfo[function_name][counter]) return false;
            ++counter;
            if (counter != functionInfo[function_name].size()) return false;
            return true;
        }
    }
    for (auto it: node->children) {
        if (!recursiveCheckCalls(it)) return false;
    }
    return true;
}

bool checkCallsReturnValues(TreeNode* head) {
    TreeNode* temp = head->children[1];
    while (temp->children.size() > 1) {
        TreeNode* procedure = temp->children[0];
        if (procedure->children[9]->type_annotation != "int") return false;
        if (!recursiveCheckCalls(procedure->children[9])) return false;
        temp = temp->children[1];
    }
    TreeNode* wain = temp->children[0];
    if (wain->children[11]->type_annotation != "int") return false;
    if (!recursiveCheckCalls(wain->children[11])) return false;
    return true;
}

bool checkTestTypeErrors(TreeNode* test_node) {
    return test_node->children[0]->type_annotation == test_node->children[2]->type_annotation;
}

bool checkStatementTypeErrors(TreeNode* head) {
    if (head->token != "statement") {
        for (auto it: head->children) {
            if (!checkStatementTypeErrors(it)) return false;
        }
        return true;
    }
    if (head->children.size() == 4) {
        if (head->children[0]->type_annotation != head->children[2]->type_annotation) return false;
    }
    if (head->children.size() == 5) {
        if (head->children[0]->token == "DELETE") {
            return (head->children[3]->type_annotation == "int*");
        }
        else return (head->children[2]->type_annotation == "int");
    }
    if (head->children.size() == 7) {
        if (!checkTestTypeErrors(head->children[2]) || !checkStatementTypeErrors(head->children[5])) return false;
    }
    if (head->children.size() == 11) {
        if (!checkTestTypeErrors(head->children[2]) || !checkStatementTypeErrors(head->children[5]) || !checkStatementTypeErrors(head->children[9])) return false;
    }
    return true;
}


int main() {
    terminals.insert("BOF"); terminals.insert("EOF"); terminals.insert("WAIN"); terminals.insert("LPAREN"); terminals.insert("INT"); terminals.insert("COMMA"); terminals.insert("RPAREN"); terminals.insert("LBRACE"); terminals.insert("RETURN"); terminals.insert("SEMI"); terminals.insert("RBRACE"); terminals.insert("STAR"); terminals.insert("ID"), terminals.insert("NUM"); terminals.insert("NULL"); terminals.insert(".EMPTY"); terminals.insert("PLUS"); terminals.insert("MINUS"); terminals.insert("SLASH"); terminals.insert("PCT"); terminals.insert("AMP"); terminals.insert("NEW"); terminals.insert("LBRACK"); terminals.insert("RBRACK"); terminals.insert("BECOMES"); terminals.insert("GETCHAR"); terminals.insert("WHILE"); terminals.insert("PRINTLN"); terminals.insert("PUTCHAR"); terminals.insert("DELETE"); terminals.insert("EQ"); terminals.insert("NE"); terminals.insert("LT"); terminals.insert("LE"); terminals.insert("GE"); terminals.insert("GT"); terminals.insert("IF"); terminals.insert("ELSE"); 

    TreeNode* head = new TreeNode{};
    rebuildTree(head);

    if (!setupTables(head->children[1])) {formatError("ERROR"); deleteNode(head); return 1;}

    if (procedure_order.back() != "wain") {formatError("ERROR"); deleteNode(head); return 1;}

    TreeNode* semantic_check_procedures = head->children[1];

    while (semantic_check_procedures->children.size() > 0) {

        if (!semanticCheck(semantic_check_procedures->children[0])) {formatError("ERROR"); deleteNode(head); return 1;}

        if (semantic_check_procedures->children.size() == 2) semantic_check_procedures = semantic_check_procedures->children[1];
        else break;
    }

    typeAnnotateAllProcedures(head->children[1]);
    
    if (!checkReferencing(head)) {formatError("ERROR"); deleteNode(head); return 1;}

    if (!checkCallsReturnValues(head)) {formatError("ERROR"); deleteNode(head); return 1;}

    if (!checkStatementTypeErrors(head)) {formatError("ERROR"); deleteNode(head); return 1;}

    printParseTree(head);

    deleteNode(head);

    return 0;
}