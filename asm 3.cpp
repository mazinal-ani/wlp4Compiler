#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>

std::vector<std::string> labelOrder;
std::vector<std::map<char, std::string>> instructions;

/** Prints an error to stderr with an "ERROR: " prefix, and newline suffix.
 *
 * @param message The error to print
 */
void formatError(const std::string & message)
{
    std::cerr << "ERROR: " << message << std::endl;
}
/** Convert a string representation of a number to an unsigned integer.
 *
 * If the string is "0", then 0 is returned.  If the string starts with "0x", the string is
 * interpreted as an unsigned hexidecimal number.  If the string starts with a "0", the string is
 * interpreted as an unsigned octal number.  Otherwise, the string is interpreted as a signed
 * decimal number.
 *
 * The function name is read as "string to uint64".
 *
 * @param s The string to parse
 * @return The uint64_t representation of the string
 */
uint32_t stouint64(const std::string & s)
{
    if(s == "0")
    {
        return 0;
    }
    if(s.starts_with("0x"))
    {
        return std::stol(s.substr(2), nullptr, 16);
    }

    if(s.starts_with("0"))
    {
        return std::stol(s.substr(1), nullptr, 8);
    }

    return std::stod(s);
}


/** For a given instruction, prints the instruction in the ascii representation of a call to
 *  compileLine().
 *
 * @param instruction The name of the instruction
 * @param one The value of the first parameter
 * @param two The value of the second parameter, 0 if the instruction has < 2 parameter
 * @param three The value of the third parameter, 0 if the instruction has < 3 parameter
 */
bool compileLine(const std::string & instruction,
                 uint32_t one,
                 uint32_t two,
                 uint32_t three)
{
    uint32_t values[3] = {one, two, three};

    std::cout << "compileLine(\"" << instruction << "\", ";
    for(size_t i = 0; i < 3; i += 1)
    {
        std::cout << (values[i] != 0 ? "0x" : "")
                  << std::hex << values[i] << (i < 2 ? ", " : "");
    }
    std::cout << ");" << std::endl;

    return true;
}

enum TokenType {
    // Not a real token type we output: a unique value for initializing a TokenType when the
    // actual value is unknown
    NONE,

    DIRECTIVE,
    LABEL,
    ID,
    HEXINT,
    REG,
    DEC,
    COMMA,
    LPAREN,
    RPAREN,
};

class Token
{
public:
    const TokenType type;
    std::string lexeme;

    Token(TokenType type, std::string lexeme);
};

Token::Token(TokenType t, std::string l)
  : type(t), lexeme(l)
{
    // Nothing
}

#define TOKEN_TYPE_PRINTER(t) case t: return #t
const char * tokenTypeString(TokenType t)
{
    switch(t)
    {
        TOKEN_TYPE_PRINTER(NONE);
        TOKEN_TYPE_PRINTER(LABEL);
        TOKEN_TYPE_PRINTER(DIRECTIVE);
        TOKEN_TYPE_PRINTER(ID);
        TOKEN_TYPE_PRINTER(HEXINT);
        TOKEN_TYPE_PRINTER(REG);
        TOKEN_TYPE_PRINTER(DEC);
        TOKEN_TYPE_PRINTER(COMMA);
        TOKEN_TYPE_PRINTER(LPAREN);
        TOKEN_TYPE_PRINTER(RPAREN);
    }

    // We will never get here
    return "";
}
#undef TOKEN_TYPE_PRINTER

#define TOKEN_TYPE_READER(s, t) if(s == #t) return t
TokenType stringToTokenType(const std::string & s)
{
    TOKEN_TYPE_READER(s, NONE);
    TOKEN_TYPE_READER(s, LABEL);
    TOKEN_TYPE_READER(s, DIRECTIVE);
    TOKEN_TYPE_READER(s, ID);
    TOKEN_TYPE_READER(s, HEXINT);
    TOKEN_TYPE_READER(s, REG);
    TOKEN_TYPE_READER(s, DEC);
    TOKEN_TYPE_READER(s, COMMA);
    TOKEN_TYPE_READER(s, LPAREN);
    TOKEN_TYPE_READER(s, RPAREN);
    return NONE;
}
#undef TOKEN_TYPE_READER

std::ostream & operator<<(std::ostream & out, const Token token)
{
    out << tokenTypeString(token.type) << " " << token.lexeme;
    return out;
}

bool registerCheck(int reg) {return 0 <= reg && reg <= 31;}

bool checkDecimalRange(std::string dec, int range = 32) {
    long long num = std::stoll(dec);
    if (range == 32 && (num > INT32_MAX || num < INT32_MIN)) return false;
    else if (range == 16 && (num > INT16_MAX || num < INT16_MIN)) return false;
    return true;
}

bool checkHexRange(std::string hex, int range = 16) {
    long long num = std::stoll(hex, nullptr, 16);
    if (range == 32 && (num > UINT32_MAX || num < 0)) return false;
    else if (range == 16 && (num > UINT16_MAX || num < 0)) return false;
    return true;
}

bool ensureRegisterCorrectness(Token token) {
    if (token.type != REG) return false;
    if (!checkDecimalRange(token.lexeme, 16)) return false;
    if (!registerCheck(std::stoi(token.lexeme))) return false;
    return true;
}

/** Entrypoint for the assembler.  The first parameter (optional) is a mips assembly file to
 *  read.  If no parameter is specified, read assembly from stdin.  Prints machine code to stdout.
 *  If invalid assembly is found, prints an error to stderr, stops reading assembly, and return a
 *  non-0 value.
 *
 * If the file is not found, print an error and returns a non-0 value.
 *
 * @return 0 on success, non-0 on error
 */
int main(int argc, char * argv[])
{
    if(argc > 2)
    {
        std::cerr << "Usage:" << std::endl
                  << "\tasm [$FILE]" << std::endl
                  << std::endl
                  << "If $FILE is unspecified or if $FILE is `-`, read the assembly from standard "
                  << "in. Otherwise, read the assembly from $FILE." << std::endl;
        return 1;
    }

    std::ifstream fp;
    std::istream &in =
        (argc > 1 && std::string(argv[1]) != "-")
      ? [&]() -> std::istream& {
            fp.open(argv[1]);
            return fp;
        }()
      : std::cin;

    if(!fp && argc > 1)
    {
        formatError((std::stringstream() << "file '" << argv[1] << "' not found!").str());
        return 1;
    }

    std::vector<Token> tokens;
    std::map<std::string, uint32_t> labelMap;
    std::set<std::string> instructionIDs;
    instructionIDs.insert({"add", "sub", "mult", "multu", "div", "divu", "mfhi", "mflo", "lis", "slt", "sltu", "jr", "jalr", "beq", "bne", "lw", "sw"});
    std::set<std::string> foreignIDs;

    while(!in.eof())
    {
        std::string line;
        std::getline( in, line );
	if(line == "")
	{
            continue;
	}

        std::string tokenType;
        std::string lexeme;

        std::stringstream lineParser(line);
        lineParser >> tokenType;
        if(tokenType != "NEWLINE")
        {
            lineParser >> lexeme;
        }

        tokens.push_back(Token(stringToTokenType(tokenType), lexeme));
    }

    for (auto&token: tokens) {
        if (token.type == REG && token.lexeme[0] == '$') {
            token.lexeme = token.lexeme.substr(1);
        }
    }

    bool errored = false;

    int lineCount = 0;

    bool needsNewline = false;

    for(int pc = 0; pc < tokens.size(); ++pc)
    {
        std::map<char, std::string> instruction;
        instruction['f'] = "0"; instruction['s'] = "0"; instruction['t'] = "0";

        if (tokens[pc].type == NONE) needsNewline = false;

        if (tokens[pc].type == REG || tokens[pc].type == DEC || tokens[pc].type == HEXINT || tokens[pc].type == COMMA || tokens[pc].type == RPAREN || tokens[pc].type == LPAREN) {
            std::cerr << "ERROR: invalid character on its own line " << tokens[pc].lexeme << std::endl;
            errored = true;
            break;
        }
        else if (needsNewline) {
            std::cerr << "ERROR: no newline after token! PC: " << pc << std::endl;
            errored = true;
            break;
        }

        if (tokens[pc].type != NONE && tokens[pc].type != LABEL) needsNewline = true;

        if (tokens[pc].type == LABEL) {
            if (labelMap.contains(tokens[pc].lexeme)) {
                std::cerr << "ERROR: label already present" << std::endl;
                errored = true;
                break;
            }
            std::string cleanedLabel = tokens[pc].lexeme.substr(0, tokens[pc].lexeme.size()-1);
            labelMap[cleanedLabel] = lineCount * 4;
            labelOrder.emplace_back(cleanedLabel);
        }

        else if (tokens[pc].type == ID && (tokens[pc].lexeme == "sw" || tokens[pc].lexeme == "lw")) {
            instruction['a'] = tokens[pc].lexeme;
            ++pc; if (pc >= tokens.size()) {std::cerr << "ERROR: out of bounds" << std::endl; errored = true; break;}
            if (!ensureRegisterCorrectness(tokens[pc])) {
                std::cerr << "ERROR: invalid first register provided for sw/lw!" << std::endl;
                errored = true;
                break;
            }
            instruction['f'] = tokens[pc].lexeme;
            ++pc; if (pc >= tokens.size()) {std::cerr << "ERROR: out of bounds" << std::endl; errored = true; break;}
            if (tokens[pc].type != COMMA) {
                std::cerr << "ERROR: no comma provided!" << std::endl;
                errored = true;
                break;
            }
            ++pc; if (pc >= tokens.size()) {std::cerr << "ERROR: out of bounds" << std::endl; errored = true; break;}
            if (tokens[pc].type != DEC && tokens[pc].type != HEXINT) {
                std::cerr << "ERROR: offset not provided as a dec/hex number for sw/lw!" << std::endl;
                errored = true;
                break;
            }
            if (tokens[pc].type == DEC && (!checkDecimalRange(tokens[pc].lexeme, 16) || std::stol(tokens[pc].lexeme) % 4 != 0)) {
                std::cerr << "ERROR: offset is not a multiple of 4 for sw/lw, or is not a valid int16!" << std::endl;
                errored = true;
                break;
            }
            else if (tokens[pc].type == HEXINT && (!checkHexRange(tokens[pc].lexeme.substr(2), 16) || stouint64(tokens[pc].lexeme) % 4 != 0)) {
                std::cerr << "ERROR: offset is not a multiple of 4 for sw/lw, or is not a valid int16! (hexint)" << std::endl;
                errored = true;
                break;
            }
            if (tokens[pc].type == HEXINT) tokens[pc].lexeme = tokens[pc].lexeme.substr(2);
            if (tokens[pc].type == HEXINT) tokens[pc].lexeme = std::to_string(std::stol(tokens[pc].lexeme, nullptr, 16));
            instruction['s'] = std::to_string((uint16_t) std::stol(tokens[pc].lexeme));
            ++pc; if (pc >= tokens.size()) {std::cerr << "ERROR: out of bounds" << std::endl; errored = true; break;}
            if (tokens[pc].type != LPAREN) {
                std::cerr << "ERROR: no left parenthesis provided!" << std::endl;
                errored = true;
                break;
            }
            ++pc; if (pc >= tokens.size()) {std::cerr << "ERROR: out of bounds" << std::endl; errored = true; break;}
            if (!ensureRegisterCorrectness(tokens[pc])) {
                std::cerr << "ERROR: invalid second register provided for sw/lw!" << std::endl;
                errored = true;
                break;
            }
            instruction['t'] = tokens[pc].lexeme;
            ++pc; if (pc >= tokens.size()) {std::cerr << "ERROR: out of bounds" << std::endl; errored = true; break;}
            if (tokens[pc].type != RPAREN) {
                std::cerr << "ERROR: no right parenthesis provided!" << std::endl;
                errored = true;
                break;
            }
        }

        else if (tokens[pc].type == ID) {
            std::string idName = tokens[pc].lexeme;
            instruction['a'] = idName;
            if (!instructionIDs.contains(idName)) {
                std::cerr << "ERROR: invalid ID name!" << std::endl;
                errored = true;
                break;
            }


            ++pc; if (pc >= tokens.size()) {std::cerr << "ERROR: out of bounds" << std::endl; errored = true; break;}
            if (!ensureRegisterCorrectness(tokens[pc])) {
                std::cerr << "ERROR: invalid first register/register not provided for instruction!" << std::endl;
                errored = true;
                break;
            }
            instruction['f'] = tokens[pc].lexeme;
            if (idName == "mfhi" || idName == "mflo" || idName == "lis" || idName == "jr" || idName == "jalr") {
                instructions.emplace_back(instruction);
                ++lineCount;
                continue;
            }


            ++pc; if (pc >= tokens.size()) {std::cerr << "ERROR: out of bounds" << std::endl; errored = true; break;}
            if (tokens[pc].type != COMMA) {
                std::cerr << "ERROR: no comma provided!" << std::endl;
                errored = true;
                break;
            }


            ++pc; if (pc >= tokens.size()) {std::cerr << "ERROR: out of bounds" << std::endl; errored = true; break;}
            if (!ensureRegisterCorrectness(tokens[pc])) {
                std::cerr << "ERROR: invalid second register/register not provided for instruction!" << std::endl;
                errored = true;
                break;
            }
            instruction['s'] = tokens[pc].lexeme;
            if (idName == "mult" || idName == "multu" || idName == "div" || idName == "divu") {
                instructions.emplace_back(instruction);
                ++lineCount;
                continue;
            }


            ++pc; if (pc >= tokens.size()) {std::cerr << "ERROR: out of bounds" << std::endl; errored = true; break;}
            if (tokens[pc].type != COMMA) {
                std::cerr << "ERROR: no comma provided!" << std::endl;
                errored = true;
                break;
            }


            ++pc; if (pc >= tokens.size()) {std::cerr << "ERROR: out of bounds" << std::endl; errored = true; break;}
            if (tokens[pc].type == ID) {
                if (idName != "beq" && idName != "bne") {
                    std::cerr << "ERROR: label ID provided for an instruction that isnt BEQ or BNE!" << std::endl;
                    errored = true;
                    break;
                }
                if (instructionIDs.contains(tokens[pc].lexeme)) {
                    std::cerr << "ERROR: instruction ID provided!" << std::endl;
                    errored = true;
                    break;
                }
                foreignIDs.insert(tokens[pc].lexeme);
            }
            else if (tokens[pc].type == REG) {
                if (idName == "bne" || idName == "beq") {
                    std::cerr << "ERROR: register provided for beq/bne!" << std::endl;
                    errored = true;
                    break;
                }
                if (!ensureRegisterCorrectness(tokens[pc])) {
                    std::cerr << "ERROR: invalid 3rd register provided for instruction!" << std::endl;
                    errored = true;
                    break;
                }
            }
            else if (tokens[pc].type == DEC || tokens[pc].type == HEXINT) {
                if (idName != "bne" && idName != "beq") {
                    std::cerr << "ERROR: decimal provided for instruction that isn't beq/bne!" << std::endl;
                    errored = true;
                    break;
                }
                if (tokens[pc].type == HEXINT) tokens[pc].lexeme = tokens[pc].lexeme.substr(2);
                if (tokens[pc].type == DEC && !checkDecimalRange(tokens[pc].lexeme, 16)) {
                    std::cerr << "ERROR: beq/bne decimal out of range!" << std::endl;
                    errored = true;
                    break;
                }
                else if (tokens[pc].type == HEXINT && !checkHexRange(tokens[pc].lexeme, 16)) {
                    std::cerr << "ERROR: beq/bne hexint out of range!" << std::endl;
                    errored = true;
                    break;
                }
            }
            else {
                std::cerr << "ERROR: invalid third token for this instruction!" << std::endl;
                errored = true;
                break;
            }
            if (tokens[pc].type != DEC && tokens[pc].type != HEXINT) instruction['t'] = tokens[pc].lexeme;
            else if (tokens[pc].type == HEXINT) instruction['t'] = std::to_string((uint16_t) std::stol(tokens[pc].lexeme, nullptr, 16));
            else instruction['t'] = std::to_string((uint16_t) std::stoi(tokens[pc].lexeme));
        }


        else if (tokens[pc].type == DIRECTIVE) {
            if (tokens[pc].lexeme != ".word") {
                std::cerr << "ERROR: unsupported directive provided!" << std::endl;
                errored = true;
                break;
            }
            instruction['a'] = tokens[pc].lexeme.substr(1);
            ++pc; if (pc >= tokens.size()) {std::cerr << "ERROR: out of bounds" << std::endl; errored = true; break;}
            if (tokens[pc].type != ID && tokens[pc].type != DEC && tokens[pc].type != HEXINT) {
                std::cerr << "ERROR: invalid token provided after .word directive!" << std::endl;
                errored = true;
                break;
            }
            if (instructionIDs.contains(tokens[pc].lexeme)) {
                std::cerr << "ERROR: instruction ID provided as word!" << std::endl;
                errored = true;
                break;
            }
            if (tokens[pc].type == DEC && !checkDecimalRange(tokens[pc].lexeme, 32)) {
                std::cerr << "ERROR: decimal out of range" << std::endl;
                errored = true;
                break;
            }
            else if (tokens[pc].type == HEXINT && !checkHexRange(tokens[pc].lexeme.substr(2), 32)) {
                std::cerr << "ERROR: hex out of range" << std::endl;
                errored = true;
                break;
            }
            if (tokens[pc].type == HEXINT || tokens[pc].type == DEC) instruction['f'] = std::to_string(stouint64(tokens[pc].lexeme));
            else instruction['f'] = tokens[pc].lexeme;
        }

        if (instruction.contains('a')) {
            instructions.emplace_back(instruction);
        }
        if (tokens[pc].type != LABEL && tokens[pc].type != NONE) ++lineCount;
    }

    for (const auto&id: foreignIDs) {
        if (!labelMap.contains(id)) {
            std::cerr << "ERROR: ID not in label map provided!" << std::endl;
            errored = true;
            break;
        }
    }

    if (!errored) {
        for (int i = 0; i < instructions.size(); ++i) {
            for (auto&pair: instructions[i]) {
                if (labelMap.contains(pair.second)) {
                    if (instructions[i]['a'] == "beq" || instructions[i]['a'] == "bne") {
                        pair.second = std::to_string((std::uint16_t) ((std::int32_t) (labelMap[pair.second] / 4) - (i + 1)));
                    }
                    else {
                        pair.second = std::to_string((std::uint32_t) labelMap[pair.second]);
                    }
                }
            }
        }

        for (const auto &label : labelOrder) {
            std::cerr << label << " " << (std::uint32_t) labelMap[label] << std::endl;
        }

        for (auto&instruction : instructions) {
            std::cout << instruction['a'];
            for (auto&pair: instruction) {
                if (pair.first != 'a') std::cout << " " << pair.second;
            }
            std::cout << std::endl;
        }
    }

    return 0;
}