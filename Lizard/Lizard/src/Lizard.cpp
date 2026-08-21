// Lizard.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
// 这是一个基于栈结构和字节码的简易解释器实现
// 保留原来的命令行关联逻辑，但核心解释器已改为字节码虚拟机

#define NOMINMAX
#include <algorithm>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cctype>
#include <vector>
#include <string>
#include <string_view>
#include <limits>
#include <unordered_map>

using namespace std;

string_view trim(const string_view& s);
bool isNumber(const string_view& token);
bool isIdentifierChar(char c);

enum class OpCode
{
    PUSH_INT,
    PUSH_STR,
    LOAD_VAR,
    STORE_VAR,
    ADD,
    SUB,
    MUL,
    DIV,
    GT,
    LT,
    GE,
    LE,
    EQ,
    NE,
    PRINT,
    JZ,
    JMP,
    RETURN,
    HALT
};

struct Value
{
    bool isString;
    long long intValue;
    string strValue;

    Value() : isString(false), intValue(0), strValue() {}
    static Value fromInt(long long x) { Value v; v.isString = false; v.intValue = x; return v; }
    static Value fromString(const string& s) { Value v; v.isString = true; v.strValue = s; return v; }
    long long asInt() const { return isString ? 0 : intValue; }
    string asString() const { return isString ? strValue : to_string(intValue); }
};

struct Instruction
{
    OpCode op;
    long long arg;
    string name;

    Instruction(OpCode op = OpCode::HALT, long long arg = 0, const string& name = "")
        : op(op), arg(arg), name(name)
    {
    }
};

unordered_map<string, long long> gConstantValues;

bool tryGetNumericLiteral(const string& expr, long long& out)
{
    string_view source = trim(expr);
    if (source.empty()) return false;
    if (source.size() >= 2 && source.front() == '"' && source.back() == '"') return false;
    if (isNumber(source))
    {
        out = stoll(string(source));
        return true;
    }
    return false;
}

struct IncrementInfo
{
    bool valid = false;
    bool isIncrement = true;
    bool isPrefix = false;
    string name;
};

bool tryParseIncrementStatement(const string& line, IncrementInfo& info)
{
    string_view source = trim(line);
    if (source.empty()) return false;

    bool isIncrement = false;
    bool isPrefix = false;
    string name;

    if (source.size() >= 3 && source[0] == '+' && source[1] == '+' && isIdentifierChar(source[2]))
    {
        isIncrement = true;
        isPrefix = true;
        name = string(trim(string_view(source.data() + 2, source.size() - 2)));
    }
    else if (source.size() >= 3 && source[0] == '-' && source[1] == '-' && isIdentifierChar(source[2]))
    {
        isIncrement = false;
        isPrefix = true;
        name = string(trim(string_view(source.data() + 2, source.size() - 2)));
    }
    else if (source.size() >= 3 && isIdentifierChar(source[0]) && source[source.size() - 1] == '+' && source[source.size() - 2] == '+')
    {
        isIncrement = true;
        isPrefix = false;
        name = string(trim(string_view(source.data(), source.size() - 2)));
    }
    else if (source.size() >= 3 && isIdentifierChar(source[0]) && source[source.size() - 1] == '-' && source[source.size() - 2] == '-')
    {
        isIncrement = false;
        isPrefix = false;
        name = string(trim(string_view(source.data(), source.size() - 2)));
    }
    else
    {
        return false;
    }

    if (name.empty()) return false;

    info.valid = true;
    info.isIncrement = isIncrement;
    info.isPrefix = isPrefix;
    info.name = name;
    return true;
}

bool tryParseSimpleLoop(const string& condition, const string& body, string& varName, bool& isIncrement, long long& step, long long& bound, string& comparisonOp)
{
    IncrementInfo incrementInfo;
    if (!tryParseIncrementStatement(body, incrementInfo)) return false;

    varName = incrementInfo.name;
    isIncrement = incrementInfo.isIncrement;
    step = incrementInfo.isIncrement ? 1 : -1;

    string_view source = trim(condition);
    if (source.empty()) return false;

    vector<string_view> operators = {"<=", ">=", "==", "!=", "<", ">"};
    size_t opPos = string_view::npos;
    string_view op;
    for (const string_view& candidate : operators)
    {
        size_t pos = source.find(candidate);
        if (pos != string_view::npos)
        {
            opPos = pos;
            op = candidate;
            break;
        }
    }

    if (opPos == string_view::npos) return false;

    string_view lhs = trim(source.substr(0, opPos));
    string_view rhs = trim(source.substr(opPos + op.size()));

    long long rhsValue = 0;
    bool rhsIsNumber = tryGetNumericLiteral(string(rhs), rhsValue);
    if (!rhsIsNumber) return false;

    if (isIdentifierChar(lhs.empty() ? '\0' : lhs[0]))
    {
        varName = string(lhs);
        bound = rhsValue;
        comparisonOp = string(op);
        return true;
    }

    if (isIdentifierChar(rhs.empty() ? '\0' : rhs[0]))
    {
        varName = string(rhs);
        bound = rhsValue;
        if (op == "<") comparisonOp = ">";
        else if (op == "<=") comparisonOp = ">=";
        else if (op == ">") comparisonOp = "<";
        else if (op == ">=") comparisonOp = "<=";
        else return false;
        return true;
    }

    return false;
}

bool isPrintOfVariable(const string& line, const string& varName)
{
    string_view source = trim(line);
    if (source.rfind("print ", 0) != 0) return false;
    string_view expr = trim(source.substr(6));
    return string(expr) == varName;
}

bool tryOptimizeSimpleLoop(const string& condition, const string& body, vector<Instruction>& bytecode)
{
    string varName;
    bool isIncrement = true;
    long long step = 1;
    long long bound = 0;
    string comparisonOp;
    if (!tryParseSimpleLoop(condition, body, varName, isIncrement, step, bound, comparisonOp)) return false;

    auto it = gConstantValues.find(varName);
    if (it == gConstantValues.end()) return false;

    long long current = it->second;
    long long original = current;
    if (step == 0) return false;

    if (comparisonOp == "<")
    {
        while (current < bound) current += step;
    }
    else if (comparisonOp == "<=")
    {
        while (current <= bound) current += step;
    }
    else if (comparisonOp == ">")
    {
        while (current > bound) current += step;
    }
    else if (comparisonOp == ">=")
    {
        while (current >= bound) current += step;
    }
    else
    {
        return false;
    }

    long long delta = current - original;
    bytecode.emplace_back(OpCode::LOAD_VAR, 0, varName);
    bytecode.emplace_back(OpCode::PUSH_INT, delta);
    bytecode.emplace_back(OpCode::ADD);
    bytecode.emplace_back(OpCode::STORE_VAR, 0, varName);
    gConstantValues[varName] = current;
    return true;
}

vector<string> ReadFileFromPath(const string& Path)
{
    vector<string> fileContent;
    ifstream file(Path);
    if (!file.is_open())
    {
        cerr << "Can't open the file: " << Path << endl;
        return {};
    }

    string line;
    while (getline(file, line))
    {
        fileContent.push_back(line);
    }
    return fileContent;
}

vector<string_view> split(const string_view& str, char delimiter)
{
    vector<string_view> result;
    size_t start = 0;
    while (start <= str.size())
    {
        size_t end = str.find(delimiter, start);
        if (end == string_view::npos)
        {
            result.push_back(str.substr(start, str.size() - start));
            break;
        }
        result.push_back(str.substr(start, end - start));
        start = end + 1;
    }
    return result;
}

string_view trim(const string_view& s)
{
    auto start = s.find_first_not_of(" \t\r\n");
    auto end = s.find_last_not_of(" \t\r\n");
    return (start == string_view::npos || end == string_view::npos) ? string_view() : s.substr(start, end - start + 1);
}

bool isNumber(const string_view& token)
{
    if (token.empty()) return false;
    size_t start = 0;
    if ((token[0] == '+' || token[0] == '-') && token.size() > 1) start = 1;
    for (size_t i = start; i < token.size(); ++i)
    {
        if (!isdigit(static_cast<unsigned char>(token[i]))) return false;
    }
    return start < token.size();
}

bool isIdentifierChar(char c)
{
    return isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool isOperatorToken(const string_view& token)
{
    static const vector<string_view> ops = { "+", "-", "*", "/", ">", "<", ">=", "<=", "==", "!=", "(", ")" };
    return find(ops.begin(), ops.end(), token) != ops.end();
}

int getPrecedence(const string_view& op)
{
    if (op == "*" || op == "/") return 3;
    if (op == "+" || op == "-") return 2;
    if (op == ">" || op == "<" || op == ">=" || op == "<=" || op == "==" || op == "!=") return 1;
    return 0;
}

vector<string_view> tokenizeExpression(const string_view& expr)
{
    vector<string_view> tokens;
    string_view current;
    size_t i = 0;
    while (i < expr.size())
    {
        char c = expr[i];
        if (isspace(static_cast<unsigned char>(c)))
        {
            ++i;
            continue;
        }
        if (c == '"')
        {
            size_t start = i;
            ++i;
            while (i < expr.size() && expr[i] != '"')
            {
                ++i;
            }
            if (i < expr.size())
            {
                ++i;
            }
            tokens.push_back(expr.substr(start, i - start));
            current = string_view();
            continue;
        }
        if (isdigit(static_cast<unsigned char>(c)) || ((c == '+' || c == '-') && i + 1 < expr.size() && isdigit(static_cast<unsigned char>(expr[i + 1])) && (i == 0 || expr[i - 1] == '(' || isOperatorToken(current))))
        {
            size_t start = i;
            ++i;
            while (i < expr.size() && isdigit(static_cast<unsigned char>(expr[i])))
            {
                ++i;
            }
            tokens.push_back(expr.substr(start, i - start));
            current = string_view();
            continue;
        }
        if (isIdentifierChar(c))
        {
            size_t start = i;
            ++i;
            while (i < expr.size() && isIdentifierChar(expr[i]))
            {
                ++i;
            }
            tokens.push_back(expr.substr(start, i - start));
            current = expr.substr(start, i - start);
            continue;
        }
        if (c == '>' || c == '<' || c == '=' || c == '!')
        {
            size_t start = i;
            if (i + 1 < expr.size() && expr[i + 1] == '=')
            {
                ++i;
            }
            tokens.push_back(expr.substr(start, i - start + 1));
            ++i;
            current = expr.substr(start, i - start);
            continue;
        }
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')')
        {
            tokens.push_back(expr.substr(i, 1));
            ++i;
            current = expr.substr(i - 1, 1);
            continue;
        }
        ++i;
    }
    return tokens;
}

vector<string_view> infixToRPN(const vector<string_view>& tokens)
{
    vector<string_view> out;
    vector<string_view> ops;
    for (const string_view& token : tokens)
    {
        if (token.empty()) continue;
        if (isNumber(token) || (!isOperatorToken(token) && token != "(" && token != ")"))
        {
            out.push_back(token);
        }
        else if (token == "(")
        {
            ops.push_back(token);
        }
        else if (token == ")")
        {
            while (!ops.empty() && ops.back() != "(")
            {
                out.push_back(ops.back());
                ops.pop_back();
            }
            if (!ops.empty()) ops.pop_back();
        }
        else
        {
            while (!ops.empty() && ops.back() != "(" && getPrecedence(ops.back()) >= getPrecedence(token))
            {
                out.push_back(ops.back());
                ops.pop_back();
            }
            ops.push_back(token);
        }
    }
    while (!ops.empty())
    {
        out.push_back(ops.back());
        ops.pop_back();
    }
    return out;
}

void compileRPN(const vector<string_view>& rpn, vector<Instruction>& bytecode)
{
    for (const string_view& token : rpn)
    {
        if (token.empty()) continue;
        if (isNumber(token))
        {
            bytecode.emplace_back(OpCode::PUSH_INT, stoll(string(token)));
        }
        else if (token == "+")
        {
            bytecode.emplace_back(OpCode::ADD);
        }
        else if (token == "-")
        {
            bytecode.emplace_back(OpCode::SUB);
        }
        else if (token == "*")
        {
            bytecode.emplace_back(OpCode::MUL);
        }
        else if (token == "/")
        {
            bytecode.emplace_back(OpCode::DIV);
        }
        else if (token == ">")
        {
            bytecode.emplace_back(OpCode::GT);
        }
        else if (token == "<")
        {
            bytecode.emplace_back(OpCode::LT);
        }
        else if (token == ">=")
        {
            bytecode.emplace_back(OpCode::GE);
        }
        else if (token == "<=")
        {
            bytecode.emplace_back(OpCode::LE);
        }
        else if (token == "==")
        {
            bytecode.emplace_back(OpCode::EQ);
        }
        else if (token == "!=")
        {
            bytecode.emplace_back(OpCode::NE);
        }
        else
        {
            bytecode.emplace_back(OpCode::LOAD_VAR, 0, string(token));
        }
    }
}

void compileExpression(const string& expr, vector<Instruction>& bytecode)
{
    string_view source = trim(expr);
    if (source.empty())
    {
        bytecode.emplace_back(OpCode::PUSH_INT, 0);
        return;
    }
    if (source.size() >= 2 && source.front() == '"' && source.back() == '"')
    {
        bytecode.emplace_back(OpCode::PUSH_STR, 0, string(source.substr(1, source.size() - 2)));
        return;
    }
    vector<string_view> tokens = tokenizeExpression(source);
    vector<string_view> rpn = infixToRPN(tokens);
    compileRPN(rpn, bytecode);
}

void compilePrintArgument(const string& arg, vector<Instruction>& bytecode)
{
    string_view source = trim(arg);
    if (source == "endl")
    {
        bytecode.emplace_back(OpCode::PUSH_STR, 0, "\n");
        bytecode.emplace_back(OpCode::PRINT);
    }
    else if (source.size() >= 2 && source.front() == '"' && source.back() == '"')
    {
        bytecode.emplace_back(OpCode::PUSH_STR, 0, string(source.substr(1, source.size() - 2)));
        bytecode.emplace_back(OpCode::PRINT);
    }
    else
    {
        compileExpression(string(source), bytecode);
        bytecode.emplace_back(OpCode::PRINT);
    }
}

bool tryCompileIncrementStatement(const string& line, vector<Instruction>& bytecode);

void compileStatement(const string& line, vector<Instruction>& bytecode)
{
    string_view source = trim(line);
    if (source.empty()) return;

    if (tryCompileIncrementStatement(string(source), bytecode))
    {
        return;
    }

    if (source.rfind("#import", 0) == 0)
    {
        return;
    }

    if (source.rfind("return", 0) == 0 && (source.size() == 6 || isspace(static_cast<unsigned char>(source[6]))))
    {
        string expr = string(trim(source.substr(6)));
        if (!expr.empty())
        {
            compileExpression(expr, bytecode);
        }
        bytecode.emplace_back(OpCode::RETURN);
        return;
    }

    if (source.rfind("print ", 0) == 0)
    {
        string arg = string(trim(source.substr(6)));
        compilePrintArgument(arg, bytecode);
        return;
    }

    static const vector<string> types = {"int", "long", "double", "float", "bool", "string"};
    for (const string& type : types)
    {
        if (source.rfind(type + " ", 0) == 0)
        {
            string remainder = string(trim(source.substr(type.size())));
            size_t eqPos = remainder.find('=');
            string name;
            string initValue;
            if (eqPos != string::npos)
            {
                name = string(trim(remainder.substr(0, eqPos)));
                initValue = string(trim(remainder.substr(eqPos + 1)));
            }
            else
            {
                vector<string_view> parts = split(remainder, ' ');
                if (!parts.empty()) name = string(parts[0]);
                initValue = "0";
            }
            bool hasConstInit = false;
            long long constInitValue = 0;
            if (tryGetNumericLiteral(initValue, constInitValue))
            {
                hasConstInit = true;
            }
            if (!initValue.empty()) compileExpression(initValue, bytecode);
            else bytecode.emplace_back(OpCode::PUSH_INT, 0);
            bytecode.emplace_back(OpCode::STORE_VAR, 0, name);
            if (hasConstInit)
            {
                gConstantValues[name] = constInitValue;
            }
            return;
        }
    }

    size_t eqPos = source.find('=');
    if (eqPos != string::npos && (eqPos + 1 >= source.size() || source[eqPos + 1] != '='))
    {
        string name = string(trim(source.substr(0, eqPos)));
        string value = string(trim(source.substr(eqPos + 1)));
        long long constValue = 0;
        if (tryGetNumericLiteral(value, constValue))
        {
            gConstantValues[name] = constValue;
        }
        compileExpression(value, bytecode);
        bytecode.emplace_back(OpCode::STORE_VAR, 0, name);
        return;
    }

    if (source.rfind("if ", 0) == 0)
    {
        string remainder = string(trim(source.substr(3)));
        vector<string_view> tokens = split(remainder, ' ');
        if (tokens.size() < 3) return;
        string condition = string(tokens[0]);
        string action = string(tokens[1]);
        string arg = string(trim(remainder.substr(condition.size() + action.size() + 2)));
        string elsePart;
        size_t elsePos = arg.find(" else ");
        if (elsePos != string::npos)
        {
            elsePart = string(trim(arg.substr(elsePos + 6)));
            arg = string(trim(arg.substr(0, elsePos)));
        }
        compileExpression(condition, bytecode);
        size_t jzIndex = bytecode.size();
        bytecode.emplace_back(OpCode::JZ, 0);
        if (action == "print")
        {
            compilePrintArgument(arg, bytecode);
        }
        else if (!action.empty())
        {
            compileStatement(action + " " + arg, bytecode);
        }
        size_t jmpIndex = bytecode.size();
        bytecode.emplace_back(OpCode::JMP, 0);
        bytecode[jzIndex].arg = static_cast<long long>(bytecode.size());
        if (!elsePart.empty())
        {
            size_t elseSpace = elsePart.find(' ');
            if (elseSpace != string::npos)
            {
                string elseAction = elsePart.substr(0, elseSpace);
                string elseArg = string(trim(elsePart.substr(elseSpace + 1)));
                compileStatement(elseAction + " " + elseArg, bytecode);
            }
        }
        bytecode[jmpIndex].arg = static_cast<long long>(bytecode.size());
        return;
    }

    compileExpression(string(source), bytecode);
    bytecode.emplace_back(OpCode::PRINT);
}

void compileStatement(const string& line, vector<Instruction>& bytecode);
void compileStatement(const vector<string>& lines, size_t& index, vector<Instruction>& bytecode);
void compileBlock(const vector<string>& lines, size_t& index, vector<Instruction>& bytecode);

bool compileBlockFromRemainder(const vector<string>& lines, size_t& index, const string& remainder, vector<Instruction>& bytecode)
{
    size_t bracePos = remainder.find('{');
    if (bracePos == string::npos)
    {
        return false;
    }

    size_t closeBracePos = remainder.find('}', bracePos + 1);
    if (closeBracePos != string::npos)
    {
        string body = string(trim(remainder.substr(bracePos + 1, closeBracePos - bracePos - 1)));
        if (!body.empty())
        {
            compileStatement(body, bytecode);
        }
        ++index;
        return true;
    }

    ++index;
    compileBlock(lines, index, bytecode);
    return true;
}

string stripOuterParens(const string& s)
{
    string value = string(trim(s));
    if (value.size() >= 2 && value.front() == '(' && value.back() == ')')
    {
        return string(trim(value.substr(1, value.size() - 2)));
    }
    return value;
}

bool tryCompileIncrementStatement(const string& line, vector<Instruction>& bytecode)
{
    IncrementInfo info;
    if (!tryParseIncrementStatement(line, info)) return false;

    bytecode.emplace_back(OpCode::LOAD_VAR, 0, info.name);
    bytecode.emplace_back(OpCode::PUSH_INT, info.isIncrement ? 1 : -1);
    bytecode.emplace_back(info.isIncrement ? OpCode::ADD : OpCode::SUB);
    bytecode.emplace_back(OpCode::STORE_VAR, 0, info.name);

    auto it = gConstantValues.find(info.name);
    if (it != gConstantValues.end())
    {
        gConstantValues[info.name] = it->second + (info.isIncrement ? 1 : -1);
    }

    return true;
}

void compileBlock(const vector<string>& lines, size_t& index, vector<Instruction>& bytecode)
{
    while (index < lines.size())
    {
        string current = string(trim(lines[index]));
        if (current.empty())
        {
            ++index;
            continue;
        }
        if (current == "}")
        {
            ++index;
            return;
        }
        if (current == "{")
        {
            ++index;
            continue;
        }

        size_t bracePos = current.find('{');
        if (bracePos != string::npos)
        {
            size_t closeBracePos = current.find('}', bracePos + 1);
            if (closeBracePos != string::npos)
            {
                string body = string(trim(current.substr(bracePos + 1, closeBracePos - bracePos - 1)));
                if (!body.empty())
                {
                    compileStatement(body, bytecode);
                }
                ++index;
                return;
            }
        }

        compileStatement(lines, index, bytecode);
    }
}

void compileStatement(const vector<string>& lines, size_t& index, vector<Instruction>& bytecode)
{
    if (index >= lines.size()) return;

    string line = string(trim(lines[index]));
    if (line.empty())
    {
        ++index;
        return;
    }

    if (line == "}")
    {
        ++index;
        return;
    }

    if (line.rfind("if", 0) == 0)
    {
        string remainder = string(trim(line.substr(2)));
        string condition;
        if (!remainder.empty() && remainder[0] == '(')
        {
            size_t closePos = remainder.find(')');
            if (closePos != string::npos)
            {
                condition = stripOuterParens(remainder.substr(0, closePos + 1));
                remainder = trim(remainder.substr(closePos + 1));
            }
        }
        else
        {
            size_t bracePos = remainder.find('{');
            if (bracePos != string::npos)
            {
                condition = string(trim(remainder.substr(0, bracePos)));
                remainder = string(trim(remainder.substr(bracePos)));
            }
        }

        size_t bracePos = remainder.find('{');
        if (bracePos != string::npos)
        {
            compileExpression(condition, bytecode);
            size_t jzIndex = bytecode.size();
            bytecode.emplace_back(OpCode::JZ, 0);

            if (!compileBlockFromRemainder(lines, index, remainder, bytecode))
            {
                ++index;
            }

            size_t jmpIndex = bytecode.size();
            bytecode.emplace_back(OpCode::JMP, 0);
            size_t elseStart = bytecode.size();

            size_t nextIndex = index;
            while (nextIndex < lines.size() && trim(lines[nextIndex]).empty()) ++nextIndex;
            if (nextIndex < lines.size())
            {
                string nextLine = string(trim(lines[nextIndex]));
                if (nextLine.rfind("else", 0) == 0)
                {
                    ++nextIndex;
                    while (nextIndex < lines.size() && trim(lines[nextIndex]).empty()) ++nextIndex;
                    if (nextIndex < lines.size())
                    {
                        string elseLine = string(trim(lines[nextIndex]));
                        if (elseLine == "{")
                        {
                            ++nextIndex;
                            index = nextIndex;
                            compileBlock(lines, index, bytecode);
                        }
                        else if (elseLine.rfind("if", 0) == 0)
                        {
                            index = nextIndex;
                            compileStatement(lines, index, bytecode);
                        }
                        else
                        {
                            index = nextIndex;
                            compileStatement(lines, index, bytecode);
                        }
                    }
                }
            }

            bytecode[jzIndex].arg = static_cast<long long>(elseStart);
            bytecode[jmpIndex].arg = static_cast<long long>(bytecode.size());
            return;
        }
    }

    if (line.rfind("while", 0) == 0)
    {
        string remainder = string(trim(line.substr(5)));
        string condition;
        if (!remainder.empty() && remainder[0] == '(')
        {
            size_t closePos = remainder.find(')');
            if (closePos != string::npos)
            {
                condition = stripOuterParens(remainder.substr(0, closePos + 1));
                remainder = trim(remainder.substr(closePos + 1));
            }
        }
        else
        {
            size_t bracePos = remainder.find('{');
            if (bracePos != string::npos)
            {
                condition = string(trim(remainder.substr(0, bracePos)));
                remainder = string(trim(remainder.substr(bracePos)));
            }
        }

        size_t bracePos = remainder.find('{');
        if (bracePos != string::npos)
        {
            size_t closeBracePos = remainder.find('}', bracePos + 1);
            if (closeBracePos != string::npos)
            {
                string body = string(trim(remainder.substr(bracePos + 1, closeBracePos - bracePos - 1)));
                string varName;
                bool isIncrement = true;
                long long step = 1;
                long long bound = 0;
                string comparisonOp;
                if (tryParseSimpleLoop(condition, body, varName, isIncrement, step, bound, comparisonOp))
                {
                    size_t nextIndex = index + 1;
                    while (nextIndex < lines.size() && trim(lines[nextIndex]).empty()) ++nextIndex;
                    if (nextIndex < lines.size() && !isPrintOfVariable(string(trim(lines[nextIndex])), varName))
                    {
                        if (tryOptimizeSimpleLoop(condition, body, bytecode))
                        {
                            ++index;
                            return;
                        }
                    }
                }
            }

            size_t condStart = bytecode.size();
            compileExpression(condition, bytecode);
            size_t jzIndex = bytecode.size();
            bytecode.emplace_back(OpCode::JZ, 0);

            if (!compileBlockFromRemainder(lines, index, remainder, bytecode))
            {
                ++index;
            }

            bytecode.emplace_back(OpCode::JMP, static_cast<long long>(condStart));
            bytecode[jzIndex].arg = static_cast<long long>(bytecode.size());
            return;
        }

        string body = string(trim(remainder));
        if (!body.empty())
        {
            size_t condStart = bytecode.size();
            compileExpression(condition, bytecode);
            size_t jzIndex = bytecode.size();
            bytecode.emplace_back(OpCode::JZ, 0);

            compileStatement(body, bytecode);

            bytecode.emplace_back(OpCode::JMP, static_cast<long long>(condStart));
            bytecode[jzIndex].arg = static_cast<long long>(bytecode.size());
            return;
        }
    }

    compileStatement(line, bytecode);
    ++index;
}

struct VM
{
    vector<Value> stack;
    unordered_map<string, Value> vars;
    bool hasReturn = false;
    Value returnValue;

    void push(const Value& v)
    {
        stack.push_back(v);
    }

    Value pop()
    {
        if (stack.empty()) return Value::fromInt(0);
        Value v = stack.back();
        stack.pop_back();
        return v;
    }

    void execute(const vector<Instruction>& bytecode)
    {
        size_t pc = 0;
        while (pc < bytecode.size())
        {
            const Instruction& ins = bytecode[pc];
            switch (ins.op)
            {
            case OpCode::PUSH_INT:
                push(Value::fromInt(ins.arg));
                break;
            case OpCode::PUSH_STR:
                push(Value::fromString(ins.name));
                break;
            case OpCode::LOAD_VAR:
            {
                auto it = vars.find(ins.name);
                if (it != vars.end()) push(it->second);
                else push(Value::fromInt(0));
                break;
            }
            case OpCode::STORE_VAR:
            {
                Value v = pop();
                vars[ins.name] = v;
                break;
            }
            case OpCode::ADD:
            {
                Value b = pop();
                Value a = pop();
                push(Value::fromInt(a.asInt() + b.asInt()));
                break;
            }
            case OpCode::SUB:
            {
                Value b = pop();
                Value a = pop();
                push(Value::fromInt(a.asInt() - b.asInt()));
                break;
            }
            case OpCode::MUL:
            {
                Value b = pop();
                Value a = pop();
                push(Value::fromInt(a.asInt() * b.asInt()));
                break;
            }
            case OpCode::DIV:
            {
                Value b = pop();
                Value a = pop();
                long long divisor = b.asInt();
                if (divisor == 0)
                {
                    cerr << "[Runtime error] Division by zero" << endl;
                    push(Value::fromInt(0));
                }
                else
                {
                    push(Value::fromInt(a.asInt() / divisor));
                }
                break;
            }
            case OpCode::GT:
            {
                Value b = pop();
                Value a = pop();
                push(Value::fromInt(a.asInt() > b.asInt() ? 1 : 0));
                break;
            }
            case OpCode::LT:
            {
                Value b = pop();
                Value a = pop();
                push(Value::fromInt(a.asInt() < b.asInt() ? 1 : 0));
                break;
            }
            case OpCode::GE:
            {
                Value b = pop();
                Value a = pop();
                push(Value::fromInt(a.asInt() >= b.asInt() ? 1 : 0));
                break;
            }
            case OpCode::LE:
            {
                Value b = pop();
                Value a = pop();
                push(Value::fromInt(a.asInt() <= b.asInt() ? 1 : 0));
                break;
            }
            case OpCode::EQ:
            {
                Value b = pop();
                Value a = pop();
                push(Value::fromInt(a.asInt() == b.asInt() ? 1 : 0));
                break;
            }
            case OpCode::NE:
            {
                Value b = pop();
                Value a = pop();
                push(Value::fromInt(a.asInt() != b.asInt() ? 1 : 0));
                break;
            }
            case OpCode::PRINT:
            {
                Value v = pop();
                if (v.isString)
                {
                    cout << v.strValue;
                }
                else
                {
                    cout << v.intValue;
                }
                cout << endl;
                break;
            }
            case OpCode::JZ:
            {
                Value v = pop();
                if (v.asInt() == 0)
                {
                    pc = static_cast<size_t>(ins.arg);
                    continue;
                }
                break;
            }
            case OpCode::JMP:
                pc = static_cast<size_t>(ins.arg);
                continue;
            case OpCode::RETURN:
            {
                if (!stack.empty())
                {
                    returnValue = pop();
                }
                else
                {
                    returnValue = Value::fromInt(0);
                }
                hasReturn = true;
                return;
            }
            case OpCode::HALT:
                return;
            }
            ++pc;
        }
    }
};

void runCommand(const string& command)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    string cmdLine = string("cmd.exe /c ") + command + " >nul 2>&1";
    vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');

    BOOL ok = CreateProcessA(
        NULL,
        cmdBuf.data(),
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi);

    if (ok)
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else
    {
        DWORD err = GetLastError();
        cerr << "CreateProcess failed: " << err << endl;
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        cerr << "Usage: Lizard <script.cl>" << endl;
        return 1;
    }

    char exeFullPath[MAX_PATH];
    GetModuleFileNameA(NULL, exeFullPath, MAX_PATH);
    string exepath(exeFullPath);

    // 在main开头参数判断位置插入
    if (argc >= 2 && string(argv[1]) == "--register") {
        char exeFullPath[MAX_PATH];
        GetModuleFileNameA(NULL, exeFullPath, MAX_PATH);
        string exepath(exeFullPath);
        runCommand("assoc .cl=LizardFile");
        string shell = "ftype LizardFile=\"" + exepath + "\" \"%1\"";
        runCommand(shell);
        return 0;
    }


    string path(argv[1]);
    vector<string> lines = ReadFileFromPath(path);
    if (lines.empty()) return 0;

    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    vector<Instruction> bytecode;
    size_t index = 0;
    while (index < lines.size())
    {
        string line = string(trim(lines[index]));
        if (line.empty())
        {
            ++index;
            continue;
        }
        compileStatement(lines, index, bytecode);
    }
    bytecode.emplace_back(OpCode::HALT);

    VM vm;
    vm.execute(bytecode);

    QueryPerformanceCounter(&end);
    double cpu_time_used = static_cast<double>(end.QuadPart - start.QuadPart) / freq.QuadPart;
    int exitValue = 0;
    if (vm.hasReturn)
    {
        exitValue = static_cast<int>(vm.returnValue.asInt());
    }

    cout << "-------------------------------" << endl;
    cout << "Process exited after " << cpu_time_used << " seconds with return value " << exitValue << endl;

    system("pause"); //终止程序

    return exitValue;
}
