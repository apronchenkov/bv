#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

enum class Op : uint8_t {
    NOT, SHL1, SHR1, SHR4, SHR16,
    AND, OR, XOR, PLUS,
    UNFOLD,

    STORE_ARG0, STORE_ARG1, STORE_ARG2, STORE_ARG3, STORE_ARG4, STORE_ARG5, STORE_ARG6, STORE_ARG7,

    LOAD_ARG0, LOAD_ARG1, LOAD_ARG2, LOAD_ARG3, LOAD_ARG4, LOAD_ARG5, LOAD_ARG6, LOAD_ARG7,

    LOAD_0, LOAD_1, LOAD_2, LOAD_3, LOAD_4, LOAD_5, LOAD_6, LOAD_7,
    LOAD_CONST,

    JNZ, JMP
};

class Block {
public:
    explicit Block(size_t initalStackSize);

    uint64_t execute(std::vector<uint64_t> argv) const;

    void emitNot();
    void emitShl1();
    void emitShr1();
    void emitShr4();
    void emitShr16();
    void emitAnd();
    void emitOr();
    void emitXor();
    void emitPlus();
    void emitUnfold();

    void emitStoreArg(int n);
    void emitLoadArg(int n);
    void emitLoadConst(uint64_t c);

    void emitJnz(size_t shift);
    void emitJmp(size_t shift);
    void emitBlock(const Block& block);
    void emitIf0(const Block& ifBlock, const Block& elseBlock);

private:
    size_t initalStackSize_;
    size_t stackSize_;
    std::vector<Op> code_;
};
