#include "require.h"

#include <cctype>
#include <iostream>
#include <jit/jit.h>
#include <set>
#include <map>
#include <sstream>
#include <vector>
#include <perfmon.h>


bool nextToken(std::streambuf* const istreambuf, std::string* token)
{
    int character = istreambuf->sgetc();
    while (character != EOF && ::isspace(character)) {
        character = istreambuf->snextc();
    }
    if (character == EOF) {
        return false;
    }
    if (character != '(' && character != ')' && character != '_' && !::isalnum(character)) {
        return false;
    }

    if (character == '(') {
        token->assign(1, '(');
        istreambuf->snextc();

    } else if (character == ')') {
        token->assign(1, ')');
        istreambuf->snextc();

    } else {
        token->clear();
        do {
            token->append(1, character);
            character = istreambuf->snextc();
        } while (character != EOF && (::isalnum(character) || character == '_'));
    }
    return true;
}


bool toInteger(const std::string& input, uint64_t* result)
{
    char* endptr = nullptr;
    *result = strtoull(input.c_str(), &endptr, 0);
    return endptr && *endptr == '\0';
}


typedef std::map<std::string, jit_value_t> Variables;


bool isIdentifier(const std::string& token)
{
    static const std::set<std::string> KEYWORD = {
        "not", "shl1", "shr1", "shr4", "shr16",
        "and", "or", "xor", "plus",
        "if0",
        "lambda",
        "fold",
        "(", ")"
    };
    return !::isdigit(static_cast<unsigned char>(token.at(0))) && KEYWORD.count(token) == 0;
}


jit_value_t readBlock(jit_context_t context, jit_function_t function, const Variables& variables, std::streambuf* const istreambuf)
{
    std::string token;
    if (!nextToken(istreambuf, &token)) {
        return nullptr;
    }

    const auto it = variables.find(token);
    if (it != variables.end()) {
        return it->second;
    }

    uint64_t c;
    if (toInteger(token, &c)) {
        return jit_value_create_nint_constant(function, jit_type_ulong, c);
    }

    if (token != "(" || !nextToken(istreambuf, &token)) {
        return nullptr;
    }

    jit_value_t result = nullptr;
    if (token == "not") {
        if (auto left = readBlock(context, function, variables, istreambuf)) {
            result = jit_insn_not(function, left);
        }

    } else if (token == "shl1") {
        if (auto left = readBlock(context, function, variables, istreambuf)) {
            result = jit_insn_shl(function, left, jit_value_create_nint_constant(function, jit_type_int, 1));
        }

    } else if (token == "shr1") {
        if (auto left = readBlock(context, function, variables, istreambuf)) {
            result = jit_insn_shr(function, left, jit_value_create_nint_constant(function, jit_type_int, 1));
        }

    } else if (token == "shr4") {
        if (auto left = readBlock(context, function, variables, istreambuf)) {
            result = jit_insn_shr(function, left, jit_value_create_nint_constant(function, jit_type_int, 4));
        }

    } else if (token == "shr16") {
        if (auto left = readBlock(context, function, variables, istreambuf)) {
            result = jit_insn_shr(function, left, jit_value_create_nint_constant(function, jit_type_int, 16));
        }

    } else if (token == "and") {
        if (auto left = readBlock(context, function, variables, istreambuf)) {
            if (auto right = readBlock(context, function, variables, istreambuf)) {
                result = jit_insn_and(function, left, right);
            }
        }
    } else if (token == "or") {
        if (auto left = readBlock(context, function, variables, istreambuf)) {
            if (auto right = readBlock(context, function, variables, istreambuf)) {
                result = jit_insn_or(function, left, right);
            }
        }
    } else if (token == "xor") {
        if (auto left = readBlock(context, function, variables, istreambuf)) {
            if (auto right = readBlock(context, function, variables, istreambuf)) {
                result = jit_insn_xor(function, left, right);
            }
        }
    } else if (token == "plus") {
        if (auto left = readBlock(context, function, variables, istreambuf)) {
            if (auto right = readBlock(context, function, variables, istreambuf)) {
                result = jit_insn_add(function, left, right);
            }
        }

    } else if (token == "if0") {
        jit_value_t ifResult = jit_value_create(function, jit_type_ulong);
        jit_label_t elseLabel = jit_label_undefined;
        jit_label_t endIfLabel = jit_label_undefined;

        if (auto condition = readBlock(context, function, variables, istreambuf)) {
            jit_insn_branch_if(function, condition, &elseLabel);
            if (auto ifBodyResult = readBlock(context, function, variables, istreambuf)) {
                jit_insn_store(function, ifResult, ifBodyResult);
                jit_insn_branch(function, &endIfLabel);
                jit_insn_label(function, &elseLabel);
                if (auto ifElseResult = readBlock(context, function, variables, istreambuf)) {
                    jit_insn_store(function, ifResult, ifElseResult);
                    jit_insn_label(function, &endIfLabel);
                    result = ifResult;
                }
            }
        }
    }

    if (nextToken(istreambuf, &token) && token == ")") {
        return result;
    }
    return nullptr;
}


jit_function_t readLambda(jit_context_t context, jit_function_t parentFunction, Variables variables, std::streambuf* const istreambuf)
{
    std::string token;
    if (!nextToken(istreambuf, &token) || token != "(" ||
        !nextToken(istreambuf, &token) || token != "lambda" ||
        !nextToken(istreambuf, &token) || token != "(")
    {
        return nullptr;
    }

    std::map<std::string, int> args;
    while (nextToken(istreambuf, &token) && isIdentifier(token)) {
        if (!args.insert({token, args.size()}).second) {
            return nullptr;
        }
    }
    if (token != ")") {
        return nullptr;
    }

    jit_function_t function = nullptr;
    {
        std::vector<jit_type_t> params(args.size(), jit_type_ulong);
        jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_ulong, &params[0], params.size(), 1);
        if (parentFunction) {
            function = jit_function_create_nested(context, signature, parentFunction);
        } else {
            function = jit_function_create(context, signature);
        }
        jit_function_set_optimization_level(function, jit_function_get_max_optimization_level());
        jit_type_free(signature);
    }

    for (const auto& arg : args) {
        variables[arg.first] = jit_value_get_param(function, arg.second);
    }

    const auto value = readBlock(context, function, variables, istreambuf);
    if (!value || !nextToken(istreambuf, &token) || token != ")") {
        jit_function_abandon(function);
        return nullptr;
    }

    jit_insn_return(function, value);
    jit_function_compile(function);
    return function;
}


jit_function_t parseLambda(jit_context_t context, const std::string& expression)
{
    PERFMON_FUNCTION_SCOPE;
    jit_context_build_start(context);

    std::stringbuf istreambuf(expression);
    const auto function = readLambda(context, nullptr, Variables(), &istreambuf);

    std::string tmp;
    if (!function || nextToken(&istreambuf, &tmp)) {
        jit_function_abandon(function);
        jit_context_build_end(context);
        return nullptr;
    }

    jit_context_build_end(context);
    return function;
}

uint64_t call(jit_function_t function, const std::vector<uint64_t>& args)
{
    std::vector<void*> argv;
    for (auto& arg : args) {
        argv.push_back((void*)&arg);
    }
    uint64_t result;
    jit_function_apply(function, &argv[0], &result);
    return result;
}


void test_read_not()
{
    const auto context = jit_context_create();
    const auto function = parseLambda(context, "(lambda (x) (not x))");
    require (function, "READ_NOT: Unable to parse lambda.");
    require (call(function, {0x0000000000000000UL}) == 0xffffffffffffffffUL &&
             call(function, {0xffffffffffffffffUL}) == 0x0000000000000000UL,
             "READ_NOT is broken");
    jit_context_destroy(context);
}

void test_read_shl1()
{
    const auto context = jit_context_create();
    const auto function = parseLambda(context, "(lambda (x) (shl1 x))");
    require (function, "READ_SHL1: Unable to parse lambda.");
    require (call(function, {0x0000000000000000UL}) == 0x0000000000000000UL &&
             call(function, {0xffffffffffffffffUL}) == 0xfffffffffffffffeUL,
             "READ_SHL1 is broken");
    jit_context_destroy(context);
}

void test_read_shr1()
{
    const auto context = jit_context_create();
    const auto function = parseLambda(context, "(lambda (x) (shr1 x))");
    require (function, "READ_SHR1: Unable to parse lambda.");
    require (call(function, {0x0000000000000000UL}) == 0x0000000000000000UL &&
             call(function, {0xffffffffffffffffUL}) == 0x7fffffffffffffffUL,
             "READ_SHR1 is broken");
    jit_context_destroy(context);
}

void test_read_shr4()
{
    const auto context = jit_context_create();
    const auto function = parseLambda(context, "(lambda (x) (shr4 x))");
    require (function, "READ_SHR4: Unable to parse lambda.");
    require (call(function, {0x0000000000000000UL}) == 0x0000000000000000UL &&
             call(function, {0xffffffffffffffffUL}) == 0x0fffffffffffffffUL,
             "READ_SHR4 is broken");
    jit_context_destroy(context);
}

void test_read_shr16()
{
    const auto context = jit_context_create();
    const auto function = parseLambda(context, "(lambda (x) (shr16 x))");
    require (function, "READ_SHR16: Unable to parse lambda.");
    require (call(function, {0x0000000000000000UL}) == 0x0000000000000000UL &&
             call(function, {0xffffffffffffffffUL}) == 0x0000ffffffffffffUL,
             "READ_SHR16 is broken");
    jit_context_destroy(context);
}

void test_read_and()
{
    const auto context = jit_context_create();
    const auto function = parseLambda(context, "(lambda (x y) (and x y))");
    require (function, "READ_AND: Unable to parse lambda.");
    require (call(function, {0x0000000000000000UL, 0x0000000000000000UL}) == 0x0000000000000000UL &&
             call(function, {0x0000000000000000UL, 0xffffffffffffffffUL}) == 0x0000000000000000UL &&
             call(function, {0xffffffffffffffffUL, 0x0000000000000000UL}) == 0x0000000000000000UL &&
             call(function, {0xffffffffffffffffUL, 0xffffffffffffffffUL}) == 0xffffffffffffffffUL,
             "READ_AND is broken");
    jit_context_destroy(context);
}

void test_read_or()
{
    const auto context = jit_context_create();
    const auto function = parseLambda(context, "(lambda (x y) (or x y))");
    require (function, "READ_OR: Unable to parse lambda.");
    require (call(function, {0x0000000000000000UL, 0x0000000000000000UL}) == 0x0000000000000000UL &&
             call(function, {0x0000000000000000UL, 0xffffffffffffffffUL}) == 0xffffffffffffffffUL &&
             call(function, {0xffffffffffffffffUL, 0x0000000000000000UL}) == 0xffffffffffffffffUL &&
             call(function, {0xffffffffffffffffUL, 0xffffffffffffffffUL}) == 0xffffffffffffffffUL,
             "READ_OR is broken");
    jit_context_destroy(context);
}

void test_read_xor()
{
    const auto context = jit_context_create();
    const auto function = parseLambda(context, "(lambda (x y) (xor x y))");
    require (function, "READ_XOR: Unable to parse lambda.");
    require (call(function, {0x0000000000000000UL, 0x0000000000000000UL}) == 0x0000000000000000UL &&
             call(function, {0x0000000000000000UL, 0xffffffffffffffffUL}) == 0xffffffffffffffffUL &&
             call(function, {0xffffffffffffffffUL, 0x0000000000000000UL}) == 0xffffffffffffffffUL &&
             call(function, {0xffffffffffffffffUL, 0xffffffffffffffffUL}) == 0x0000000000000000UL,
             "READ_XOR is broken");
    jit_context_destroy(context);
}


void test_read_plus()
{
    const auto context = jit_context_create();
    const auto function = parseLambda(context, "(lambda (x y) (plus x y))");
    require (function, "READ_PLUS: Unable to parse lambda.");
    require (call(function, {0x1111111111111111UL, 0x1111111111111111UL}) == 0x2222222222222222UL &&
             call(function, {0x2222222222222222UL, 0x2222222222222222UL}) == 0x4444444444444444UL &&
             call(function, {0x4444444444444444UL, 0x4444444444444444UL}) == 0x8888888888888888UL &&
             call(function, {0x8888888888888888UL, 0x8888888888888888UL}) == 0x1111111111111110UL &&
             call(function, {0xffffffffffffffffUL, 0x0000000000000001UL}) == 0x0000000000000000UL,
             "READ_PLUS is broken");
    jit_context_destroy(context);
}


void test_read_loadarg()
{
    const std::map<std::string, int> expressions = {
        {"(lambda (x y z) x)", 0},
        {"(lambda (x y z) y)", 1},
        {"(lambda (x y z) z)", 2},
    };
    const std::vector<uint64_t> args = {
        0, 1, 2
    };
    for (const auto& expression : expressions) {
        auto context = jit_context_create();
        auto function = parseLambda(context, expression.first);
        require (function, "READ_ARG: Unable to parse lambda.");
        require (call(function, args) == static_cast<uint64_t>(expression.second), "READ_ARG is broken");
        jit_context_destroy(context);
    }
}

void test_read_c()
{
    for (uint64_t i = 0; i < 10; ++i) {
        std::ostringstream buf;
        buf << "(lambda () " << i << ")";

        auto context = jit_context_create();
        auto function = parseLambda(context, buf.str());
        require (function, "READ_CONST: Unable to parse lambda.");
        require (call(function, {}) == i, "READ_CONST is broken");
        jit_context_destroy(context);
    }
}

void test_read_if0()
{
    const auto context = jit_context_create();
    const auto function = parseLambda(context, "(lambda (x) (and 0xffffffff87654321 (if0 x 0xf0f0f0f0f0f0f0f0 0x0f0f0f0f0f0f0f0f)))");
    require (function, "READ_IF0: Unable to parse lambda.");
    require (call(function, {0}) == 0xf0f0f0f080604020 &&
             call(function, {1}) == 0x0f0f0f0f07050301,
             "READ_IF0 is broken");
    jit_context_destroy(context);
}


inline void test_read_block()
{
    try {
        test_read_not();
        test_read_shl1();
        test_read_shr1();
        test_read_shr4();
        test_read_shr16();
        test_read_and();
        test_read_or();
        test_read_xor();
        test_read_plus();
        test_read_loadarg();
        test_read_c();
        test_read_if0();

    } catch(const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        std::exit(-1);
    }
}


#include <boost/functional/hash.hpp>
#include <mutex>
#include <thread>

std::mutex g_io_mutex;

std::string nextProgram()
{
    PERFMON_FUNCTION_SCOPE;
    std::lock_guard<std::mutex> lock(g_io_mutex);
    for (std::string line; std::getline(std::cin, line); ) {
        auto index = line.find('('); // drop everything before the program
        if (index != std::string::npos) {
            line = line.substr(index);
        } else {
            continue;
        }
        while (!line.empty() && ::isspace(static_cast<unsigned char>(line.back()))) {
            line.resize(line.size() - 1);
        }
        if (!line.empty()) {
            return line;
        }
    }
    return {};
}

void putResult(uint64_t hash, const std::string& program)
{
    PERFMON_FUNCTION_SCOPE;
    std::lock_guard<std::mutex> lock(g_io_mutex);
    std::cout << hash << '\t' << program << '\n';
}

void threadMain(const std::vector<uint64_t>& input_values)
{
    for (;;) {
        const std::string program = nextProgram();
        if (program.empty()) {
            break;
        }

        const auto context = jit_context_create();
        const auto function = parseLambda(context, program);
        if (!function) {
            std::cerr << "Unable to parse: " << program << '\n';
            jit_context_destroy(context);
            continue;
        }

        std::vector<uint64_t> output_values;
        PERFMON_STATEMENT ("eval")
        for (const uint64_t& value : input_values) {
            uint64_t result;
            void* argv[] = { (void*)&value };
            jit_function_apply(function, argv, &result);
            output_values.push_back(result);
        }

        if (output_values.size() == 1) {
            putResult(output_values.front(), program);
        } else {
            putResult(boost::hash_range(output_values.begin(), output_values.end()), program);
        }
    }
}

void usage()
{
    std::cerr << "usage: arg1 arg2 ... < expressions\n\n";
    std::exit(-1);
}


std::vector<uint64_t> parseArguments(int argc, char** argv)
{
    std::vector<uint64_t> result;
    for (int index = 1; index < argc; ++index) {
        uint64_t argument;
        if (toInteger(argv[index], &argument)) {
            result.push_back(argument);
        } else {
            std::cerr << "Illegal argument: " << argv[index] << std::endl;
            std::exit(-1);
        }
    }
    return result;
}


int main(int argc, char** argv)
{
    test_read_block();

    if (argc < 2) {
        usage();
    }

    if (!isatty(1)) {
        std::cin.sync_with_stdio(false);
        std::cout.sync_with_stdio(false);
    }

    const auto input_values = parseArguments(argc, argv);

    std::vector<std::thread> thread_group;
    try {
        for (int index = 0; index < 3; ++index) {
            thread_group.emplace_back(threadMain, std::cref(input_values));
        }
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << '\n';
        return -1;
    }

    for (auto& thread : thread_group) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    for (const auto& counter : PERFMON_COUNTERS) {
        std::cerr << counter.Name() << ": " << counter.Calls() << ' ' << counter.Seconds() << "seconds\n";
    }

    return 0;
}
