#include "block.h"
#include "test_block.h"
#include <perfmon.h>
#include <cctype>
#include <iostream>
#include <set>
#include <map>
#include <sstream>


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


typedef std::map<std::string, int> Variables;

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

bool readBlock(std::streambuf* const istreambuf, const Variables& variables, Block* const block)
{
    std::string token;
    if (!nextToken(istreambuf, &token)) {
        return false;
    }

    const auto it = variables.find(token);
    if (it != variables.end()) {
        block->emitLoadArg(it->second);
        return true;
    }

    uint64_t c;
    if (toInteger(token, &c)) {
        block->emitLoadConst(c);
        return true;
    }

    if (token != "(" || !nextToken(istreambuf, &token)) {
        return false;
    }


#define OP1(name, emit)                                                 \
    if (token == name) {                                                \
        if (readBlock(istreambuf, variables, block) &&                  \
            nextToken(istreambuf, &token) && token == ")")              \
        {                                                               \
            block-> emit ();                                            \
            return true;                                                \
        }                                                               \
        return false;                                                   \
    }

#define OP2(name, emit)                                                 \
    if (token == name) {                                                \
        if (readBlock(istreambuf, variables, block) &&                  \
            readBlock(istreambuf, variables, block) &&                  \
            nextToken(istreambuf, &token) && token == ")")              \
        {                                                               \
            block-> emit ();                                            \
            return true;                                                \
        }                                                               \
        return false;                                                   \
    }

    OP1("not", emitNot);
    OP1("shl1", emitShl1);
    OP1("shr1", emitShr1);
    OP1("shr4", emitShr4);
    OP1("shr16", emitShr16);

    OP2("and", emitAnd);
    OP2("or", emitOr);
    OP2("xor", emitXor);
    OP2("plus", emitPlus);

#undef OP2
#undef OP1

    if (token == "if0") {
        Block ifBlock(0), elseBlock(0);
        if (readBlock(istreambuf, variables, block) &&
            readBlock(istreambuf, variables, &ifBlock) &&
            readBlock(istreambuf, variables, &elseBlock) &&
            nextToken(istreambuf, &token) && token == ")")
        {
            block->emitIf0(ifBlock, elseBlock);
            return true;
        }
        return false;
    }

    if (token == "fold") {
        // "(fold integer accumulator (lambda (x y) block)"
        //   lambda (x y) ...
        // x8 x7 x6 x5 x4 x3 x2 x1 accumulator $storeArg2 $storeArg3 $block ...
        //

        if (!readBlock(istreambuf, variables, block)) {
            return false;
        }
        block->emitUnfold();
        if (!readBlock(istreambuf, variables, block)) {
            return false;
        }

        std::string leftArg, rightArg;
        if (!nextToken(istreambuf, &token) || token != "(" ||
            !nextToken(istreambuf, &token) || token != "lambda" ||
            !nextToken(istreambuf, &token) || token != "(" ||
            !nextToken(istreambuf, &leftArg) || !isIdentifier(leftArg) ||
            !nextToken(istreambuf, &rightArg) || !isIdentifier(rightArg) ||
            !nextToken(istreambuf, &token) || token != ")" ||
            leftArg == rightArg)
        {
            return false;
        }

        auto foldVariables = variables;
        const int leftArgN = foldVariables.size();
        foldVariables[leftArg] = leftArgN;
        foldVariables[rightArg] = leftArgN + 1;

        Block foldBlock(0);
        if (!readBlock(istreambuf, foldVariables, &foldBlock)) {
            return false;
        }
        for (int index = 0; index < 8; ++index) {
            block->emitStoreArg(leftArgN + 1);
            block->emitStoreArg(leftArgN);
            block->emitBlock(foldBlock);
        }
        return (nextToken(istreambuf, &token) && token == ")" &&
                nextToken(istreambuf, &token) && token == ")");
    }

    return false;
}

bool readLambda(std::streambuf* const istreambuf, Block* block)
{
    std::string token;
    if (!nextToken(istreambuf, &token) || token != "(" ||
        !nextToken(istreambuf, &token) || token != "lambda" ||
        !nextToken(istreambuf, &token) || token != "(")
    {
        return false;
    }

    Variables variables;
    while (nextToken(istreambuf, &token) && isIdentifier(token)) {
        const int v = variables.size();
        variables[token] = v;
    }

    return
        token == ")" &&
        readBlock(istreambuf, variables, block) &&
        nextToken(istreambuf, &token) && token == ")";
}

Block parseLambda(const std::string& expression)
{
    PERFMON_FUNCTION_SCOPE;
    Block result(0);
    std::stringbuf istreambuf(expression);
    std::string tmp;
    require (readLambda(&istreambuf, &result) && !nextToken(&istreambuf, &tmp), "Unabled to parse lambda expression.");

    return result;
}

void test_read_not()
{
    const auto block = parseLambda("(lambda (x) (not x))");
    require (block.execute({0x0000000000000000UL}) == 0xffffffffffffffffUL &&
             block.execute({0xffffffffffffffffUL}) == 0x0000000000000000UL,
             "READ_NOT is broken");
}

void test_read_shl1()
{
    const auto block = parseLambda("(lambda (x) (shl1 x))");
    require (block.execute({0x0000000000000000UL}) == 0x0000000000000000UL &&
             block.execute({0xffffffffffffffffUL}) == 0xfffffffffffffffeUL,
             "READ_SHL1 is broken");
}

void test_read_shr1()
{
    const auto block = parseLambda("(lambda (x) (shr1 x))");
    require (block.execute({0x0000000000000000UL}) == 0x0000000000000000UL &&
             block.execute({0xffffffffffffffffUL}) == 0x7fffffffffffffffUL,
             "READ_SHR1 is broken");
}

void test_read_shr4()
{
    const auto block = parseLambda("(lambda (x) (shr4 x))");
    require (block.execute({0x0000000000000000UL}) == 0x0000000000000000UL &&
             block.execute({0xffffffffffffffffUL}) == 0x0fffffffffffffffUL,
             "READ_SHR4 is broken");
}

void test_read_shr16()
{
    const auto block = parseLambda("(lambda (x) (shr16 x))");
    require (block.execute({0x0000000000000000UL}) == 0x0000000000000000UL &&
             block.execute({0xffffffffffffffffUL}) == 0x0000ffffffffffffUL,
             "READ_SHR16 is broken");
}

void test_read_and()
{
    const auto block = parseLambda("(lambda (x y) (and x y))");
    require (block.execute({0x0000000000000000UL, 0x0000000000000000UL}) == 0x0000000000000000UL &&
             block.execute({0x0000000000000000UL, 0xffffffffffffffffUL}) == 0x0000000000000000UL &&
             block.execute({0xffffffffffffffffUL, 0x0000000000000000UL}) == 0x0000000000000000UL &&
             block.execute({0xffffffffffffffffUL, 0xffffffffffffffffUL}) == 0xffffffffffffffffUL,
             "READ_AND is broken");
}

void test_read_or()
{
    const auto block = parseLambda("(lambda (x y) (or x y))");
    require (block.execute({0x0000000000000000UL, 0x0000000000000000UL}) == 0x0000000000000000UL &&
             block.execute({0x0000000000000000UL, 0xffffffffffffffffUL}) == 0xffffffffffffffffUL &&
             block.execute({0xffffffffffffffffUL, 0x0000000000000000UL}) == 0xffffffffffffffffUL &&
             block.execute({0xffffffffffffffffUL, 0xffffffffffffffffUL}) == 0xffffffffffffffffUL,
             "READ_OR is broken");
}

void test_read_xor()
{
    const auto block = parseLambda("(lambda (x y) (xor x y))");
    require (block.execute({0x0000000000000000UL, 0x0000000000000000UL}) == 0x0000000000000000UL &&
             block.execute({0x0000000000000000UL, 0xffffffffffffffffUL}) == 0xffffffffffffffffUL &&
             block.execute({0xffffffffffffffffUL, 0x0000000000000000UL}) == 0xffffffffffffffffUL &&
             block.execute({0xffffffffffffffffUL, 0xffffffffffffffffUL}) == 0x0000000000000000UL,
             "READ_XOR is broken");
}


void test_read_plus()
{
    const auto block = parseLambda("(lambda (x y) (plus x y))");
    require (block.execute({0x1111111111111111UL, 0x1111111111111111UL}) == 0x2222222222222222UL &&
             block.execute({0x2222222222222222UL, 0x2222222222222222UL}) == 0x4444444444444444UL &&
             block.execute({0x4444444444444444UL, 0x4444444444444444UL}) == 0x8888888888888888UL &&
             block.execute({0x8888888888888888UL, 0x8888888888888888UL}) == 0x1111111111111110UL &&
             block.execute({0xffffffffffffffffUL, 0x0000000000000001UL}) == 0x0000000000000000UL,
             "READ_PLUS is broken");
}

void test_read_loadarg()
{
    const Variables variables = {
        {"(lambda (x y z) x)", 0},
        {"(lambda (x y z) y)", 1},
        {"(lambda (x y z) z)", 2},
    };
    for (const auto& var : variables) {
        const auto block = parseLambda(var.first);
        require (block.execute({0, 1, 2}) == static_cast<uint64_t>(var.second), "READ_ARG is broken");
    }
}

void test_read_c()
{
    for (uint64_t i = 0; i < 10; ++i) {
        std::ostringstream buf;
        buf << "(lambda () " << i << ")";
        const auto block = parseLambda(buf.str());
        require (block.execute({}) == i, "READ_CONST is broken");
    }
}

void test_read_if0()
{
    const auto block = parseLambda("(lambda (x) (and 0xffffffff87654321 (if0 x 0xf0f0f0f0f0f0f0f0 0x0f0f0f0f0f0f0f0f)))");
    require (block.execute({0}) == 0xf0f0f0f080604020 &&
             block.execute({1}) == 0x0f0f0f0f07050301,
             "READ_IF0 is broken");
}

void test_read_fold()
{
    {
        const auto block = parseLambda(
                    "(lambda (x)"
                    "  (fold x 0"
                    "    (lambda (x y)"
                    "      (or x"
                    "        (shl1"
                    "          (shl1"
                    "            (shl1"
                    "              (shl1 y)"
                    "            )"
                    "          )"
                    "        )"
                    "      )"
                    "    )"
                    "  )"
                    ")");
        require (block.execute({0x0706050403020100UL}) == 0x01234567, "READ_FOLD is broken");
    }
    {
        const auto block = parseLambda(
                    "(lambda (x)"
                    "  (fold x 0"
                    "    (lambda (x y)"
                    "      (if0 x (plus 1 y) y)"
                    "    )"
                    "  )"
                    ")");
        require (block.execute({0x0101010101010101UL}) == 0, "READ_FOLD is broken");
        require (block.execute({0x0100010001000100UL}) == 4, "READ_FOLD is broken");
        require (block.execute({0x0100010001000100UL}) == 4, "READ_FOLD is broken");
        require (block.execute({0x0000000000000000UL}) == 8, "READ_FOLD is broken");
    }
}


inline void test_read_block()
{
    using namespace internal;
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
        test_read_fold();

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
        line = line.substr(line.find('(')); // drop everything before the program
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

        Block block(0);
        try {
            block = parseLambda(program);
        } catch (const std::exception& ex) {
            std::cerr << "Unable to parse: " << program << '\n';
            continue;
        }

        std::vector<uint64_t> output_values;
        PERFMON_STATEMENT("eval")
        for (uint64_t value : input_values) {
            output_values.push_back(block.execute({value}));
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
    test_block();
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
