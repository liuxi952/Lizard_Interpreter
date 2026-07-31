// Lizard.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
// 这是一个基于栈结构和字节码的简易解释器实现
// 保留原来的命令行关联逻辑，但核心解释器已改为字节码虚拟机

#define NOMINMAX
#include <algorithm>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <limits>
#include <unordered_map>

using namespace std;

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

vector<string> split(const string& str, char delimiter)
{
    vector<string> result;
    string token;
    stringstream ss(str);
    while (getline(ss, token, delimiter))
    {
        result.push_back(token);
    }
    return result;
}

string trim(const string& s)
{
    auto start = s.find_first_not_of(" \t\r\n");
    auto end = s.find_last_not_of(" \t\r\n");
    return (start == string::npos || end == string::npos) ? string() : s.substr(start, end - start + 1);
}

bool isNumber(const string& token)
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

bool isOperatorToken(const string& token)
{
    static const vector<string> ops = { "+", "-", "*", "/", ">", "<", ">=", "<=", "==", "!=", "(", ")" };
    return find(ops.begin(), ops.end(), token) != ops.end();
}

int getPrecedence(const string& op)
{
    if (op == "*" || op == "/") return 3;
    if (op == "+" || op == "-") return 2;
    if (op == ">" || op == "<" || op == ">=" || op == "<=" || op == "==" || op == "!=") return 1;
    return 0;
}

vector<string> tokenizeExpression(const string& expr)
{
    vector<string> tokens;
    string current;
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
            string literal;
            literal.push_back(c);
            ++i;
            while (i < expr.size() && expr[i] != '"')
            {
                literal.push_back(expr[i]);
                ++i;
            }
            if (i < expr.size())
            {
                literal.push_back('"');
                ++i;
            }
            tokens.push_back(literal);
            current.clear();
            continue;
        }
        if (isdigit(static_cast<unsigned char>(c)) || ((c == '+' || c == '-') && i + 1 < expr.size() && isdigit(static_cast<unsigned char>(expr[i + 1])) && (i == 0 || expr[i-1] == '(' || isOperatorToken(current))))
        {
            string number;
            number.push_back(c);
            ++i;
            while (i < expr.size() && isdigit(static_cast<unsigned char>(expr[i])))
            {
                number.push_back(expr[i]);
                ++i;
            }
            tokens.push_back(number);
            current.clear();
            continue;
        }
        if (isIdentifierChar(c))
        {
            string ident;
            ident.push_back(c);
            ++i;
            while (i < expr.size() && isIdentifierChar(expr[i]))
            {
                ident.push_back(expr[i]);
                ++i;
            }
            tokens.push_back(ident);
            current = ident;
            continue;
        }
        if (c == '>' || c == '<' || c == '=' || c == '!')
        {
            string op;
            op.push_back(c);
            if (i + 1 < expr.size() && expr[i + 1] == '=')
            {
                op.push_back('=');
                ++i;
            }
            tokens.push_back(op);
            ++i;
            current = op;
            continue;
        }
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')')
        {
            string op(1, c);
            tokens.push_back(op);
            ++i;
            current = op;
            continue;
        }
        ++i;
    }
    return tokens;
}

vector<string> infixToRPN(const vector<string>& tokens)
{
    vector<string> out;
    vector<string> ops;
    for (const string& token : tokens)
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

void compileRPN(const vector<string>& rpn, vector<Instruction>& bytecode)
{
    for (const string& token : rpn)
    {
        if (token.empty()) continue;
        if (isNumber(token))
        {
            bytecode.emplace_back(OpCode::PUSH_INT, stoll(token));
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
            bytecode.emplace_back(OpCode::LOAD_VAR, 0, token);
        }
    }
}

void compileExpression(const string& expr, vector<Instruction>& bytecode)
{
    string source = trim(expr);
    if (source.empty())
    {
        bytecode.emplace_back(OpCode::PUSH_INT, 0);
        return;
    }
    if (source.size() >= 2 && source.front() == '"' && source.back() == '"')
    {
        bytecode.emplace_back(OpCode::PUSH_STR, 0, source.substr(1, source.size() - 2));
        return;
    }
    vector<string> tokens = tokenizeExpression(source);
    vector<string> rpn = infixToRPN(tokens);
    compileRPN(rpn, bytecode);
}

void compilePrintArgument(const string& arg, vector<Instruction>& bytecode)
{
    string source = trim(arg);
    if (source == "endl")
    {
        bytecode.emplace_back(OpCode::PUSH_STR, 0, "\n");
        bytecode.emplace_back(OpCode::PRINT);
    }
    else if (source.size() >= 2 && source.front() == '"' && source.back() == '"')
    {
        bytecode.emplace_back(OpCode::PUSH_STR, 0, source.substr(1, source.size() - 2));
        bytecode.emplace_back(OpCode::PRINT);
    }
    else
    {
        compileExpression(source, bytecode);
        bytecode.emplace_back(OpCode::PRINT);
    }
}

void compileStatement(const string& line, vector<Instruction>& bytecode)
{
    string source = trim(line);
    if (source.empty()) return;

    if (source.rfind("#import", 0) == 0)
    {
        return;
    }

    if (source.rfind("print ", 0) == 0)
    {
        string arg = trim(source.substr(6));
        compilePrintArgument(arg, bytecode);
        return;
    }

    static const vector<string> types = {"int", "long", "double", "float", "bool", "string"};
    for (const string& type : types)
    {
        if (source.rfind(type + " ", 0) == 0)
        {
            string remainder = trim(source.substr(type.size()));
            size_t eqPos = remainder.find('=');
            string name;
            string initValue;
            if (eqPos != string::npos)
            {
                name = trim(remainder.substr(0, eqPos));
                initValue = trim(remainder.substr(eqPos + 1));
            }
            else
            {
                vector<string> parts = split(remainder, ' ');
                if (!parts.empty()) name = parts[0];
                initValue = "0";
            }
            if (!initValue.empty()) compileExpression(initValue, bytecode);
            else bytecode.emplace_back(OpCode::PUSH_INT, 0);
            bytecode.emplace_back(OpCode::STORE_VAR, 0, name);
            return;
        }
    }

    size_t eqPos = source.find('=');
    if (eqPos != string::npos && (eqPos + 1 >= source.size() || source[eqPos + 1] != '='))
    {
        string name = trim(source.substr(0, eqPos));
        string value = trim(source.substr(eqPos + 1));
        compileExpression(value, bytecode);
        bytecode.emplace_back(OpCode::STORE_VAR, 0, name);
        return;
    }

    if (source.rfind("if ", 0) == 0)
    {
        string remainder = trim(source.substr(3));
        vector<string> tokens = split(remainder, ' ');
        if (tokens.size() < 3) return;
        string condition = tokens[0];
        string action = tokens[1];
        string arg = trim(remainder.substr(condition.size() + action.size() + 2));
        string elsePart;
        size_t elsePos = arg.find(" else ");
        if (elsePos != string::npos)
        {
            elsePart = trim(arg.substr(elsePos + 6));
            arg = trim(arg.substr(0, elsePos));
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
                string elseArg = trim(elsePart.substr(elseSpace + 1));
                compileStatement(elseAction + " " + elseArg, bytecode);
            }
        }
        bytecode[jmpIndex].arg = static_cast<long long>(bytecode.size());
        return;
    }

    compileExpression(source, bytecode);
    bytecode.emplace_back(OpCode::PRINT);
}

struct VM
{
    vector<Value> stack;
    unordered_map<string, Value> vars;

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

    runCommand("assoc .cl=LizardFile");
    runCommand(string("ftype LizardFile=\"") + exepath + "\" \"%1\"");

    string path(argv[1]);
    vector<string> lines = ReadFileFromPath(path);
    if (lines.empty()) return 0;

    vector<Instruction> bytecode;
    for (const string& rawLine : lines)
    {
        string line = trim(rawLine);
        if (line.empty()) continue;
        compileStatement(line, bytecode);
    }
    bytecode.emplace_back(OpCode::HALT);

    VM vm;
    vm.execute(bytecode);

    system("pause");
    return 0;
}
