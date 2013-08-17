#pragma once

#include "block.h"
#include "require.h"
#include <exception>
#include <iostream>

namespace internal {
namespace {

void test_not()
{
    Block block(0);
    block.emitLoadArg(0);
    block.emitNot();
    require (block.execute({0x0000000000000000UL}) == 0xffffffffffffffffUL &&
             block.execute({0xffffffffffffffffUL}) == 0x0000000000000000UL,
             "NOT is broken");
}

void test_shl1()
{
    Block block(0);
    block.emitLoadArg(0);
    block.emitShl1();
    require (block.execute({0x0000000000000000UL}) == 0x0000000000000000UL &&
             block.execute({0xffffffffffffffffUL}) == 0xfffffffffffffffeUL,
             "SHL1 is broken");
}

void test_shr1()
{
    Block block(0);
    block.emitLoadArg(0);
    block.emitShr1();
    require (block.execute({0x0000000000000000UL}) == 0x0000000000000000UL &&
             block.execute({0xffffffffffffffffUL}) == 0x7fffffffffffffffUL,
             "SHR1 is broken");
}

void test_shr4()
{
    Block block(0);
    block.emitLoadArg(0);
    block.emitShr4();
    require (block.execute({0x0000000000000000UL}) == 0x0000000000000000UL &&
             block.execute({0xffffffffffffffffUL}) == 0x0fffffffffffffffUL,
             "SHR4 is broken");
}

void test_shr16()
{
    Block block(0);
    block.emitLoadArg(0);
    block.emitShr16();
    require (block.execute({0x0000000000000000UL}) == 0x0000000000000000UL &&
             block.execute({0xffffffffffffffffUL}) == 0x0000ffffffffffffUL,
             "SHR16 is broken");
}

void test_and()
{
    Block block(0);
    block.emitLoadArg(0);
    block.emitLoadArg(1);
    block.emitAnd();
    require (block.execute({0x0000000000000000UL, 0x0000000000000000UL}) == 0x0000000000000000UL &&
             block.execute({0x0000000000000000UL, 0xffffffffffffffffUL}) == 0x0000000000000000UL &&
             block.execute({0xffffffffffffffffUL, 0x0000000000000000UL}) == 0x0000000000000000UL &&
             block.execute({0xffffffffffffffffUL, 0xffffffffffffffffUL}) == 0xffffffffffffffffUL,
             "AND is broken");
}

void test_or()
{
    Block block(0);
    block.emitLoadArg(0);
    block.emitLoadArg(1);
    block.emitOr();
    require (block.execute({0x0000000000000000UL, 0x0000000000000000UL}) == 0x0000000000000000UL &&
             block.execute({0x0000000000000000UL, 0xffffffffffffffffUL}) == 0xffffffffffffffffUL &&
             block.execute({0xffffffffffffffffUL, 0x0000000000000000UL}) == 0xffffffffffffffffUL &&
             block.execute({0xffffffffffffffffUL, 0xffffffffffffffffUL}) == 0xffffffffffffffffUL,
             "OR is broken");
}

void test_xor()
{
    Block block(0);
    block.emitLoadArg(0);
    block.emitLoadArg(1);
    block.emitXor();
    require (block.execute({0x0000000000000000UL, 0x0000000000000000UL}) == 0x0000000000000000UL &&
             block.execute({0x0000000000000000UL, 0xffffffffffffffffUL}) == 0xffffffffffffffffUL &&
             block.execute({0xffffffffffffffffUL, 0x0000000000000000UL}) == 0xffffffffffffffffUL &&
             block.execute({0xffffffffffffffffUL, 0xffffffffffffffffUL}) == 0x0000000000000000UL,
             "XOR is broken");
}


void test_plus()
{
    Block block(0);
    block.emitLoadArg(0);
    block.emitLoadArg(1);
    block.emitPlus();
    require (block.execute({0x1111111111111111UL, 0x1111111111111111UL}) == 0x2222222222222222UL &&
             block.execute({0x2222222222222222UL, 0x2222222222222222UL}) == 0x4444444444444444UL &&
             block.execute({0x4444444444444444UL, 0x4444444444444444UL}) == 0x8888888888888888UL &&
             block.execute({0x8888888888888888UL, 0x8888888888888888UL}) == 0x1111111111111110UL &&
             block.execute({0xffffffffffffffffUL, 0x0000000000000001UL}) == 0x0000000000000000UL,
             "PLUS is broken");
}

void test_unfold()
{
    Block baseBlock(0);
    baseBlock.emitLoadConst(0x0706050403020100);
    baseBlock.emitUnfold();
    for (int i = 0; i < 8; ++i) {
        baseBlock.emitStoreArg(i);
    }
    for (uint64_t i = 0; i < 8; ++i) {
        Block block = baseBlock;
        block.emitLoadArg(i);
        require (block.execute({}) == i, "UNFOLD is broken");
    }
}

void test_storearg()
{
    for (int i = 0; i < 8; ++i) {
        Block block(0);
        block.emitLoadConst(1);
        block.emitStoreArg(i);
        block.emitLoadArg(i);

        require (block.execute({0, 0, 0, 0, 0, 0, 0, 0,}) == 1, "STORE_ARG is broken");
    }
}

void test_loadarg()
{
    for (uint64_t i = 0; i < 8; ++i) {
        Block block(0);
        block.emitLoadArg(i);
        require (block.execute({0, 1, 2, 3, 4, 5, 6, 7}) == i, "LOAD_ARG is broken");
    }
}

void test_loadconst()
{
    for (uint64_t i = 0; i < 100; ++i) {
        Block block(0);
        block.emitLoadConst(i);
        require (block.execute({}) == i, "LOAD_CONST is broken");
    }
}

void test_if0()
{
    Block ifBlock(0);
    ifBlock.emitLoadConst(
        0xf0f0f0f0f0f0f0f0);

    Block elseBlock(0);
    elseBlock.emitLoadConst(
        0x0f0f0f0f0f0f0f0f);

    Block block(0);
    block.emitLoadArg(0);
    block.emitIf0(ifBlock, elseBlock);
    block.emitLoadConst(0xffffffff87654321);
    block.emitAnd();

    require (block.execute({0}) == 0xf0f0f0f080604020 &&
             block.execute({1}) == 0x0f0f0f0f07050301,
             "IF0 is broken.");
}

} } // namespace internal::


inline void test_block()
{
    using namespace internal;
    try {
        test_not();
        test_shl1();
        test_shr1();
        test_shr4();
        test_shr16();
        test_and();
        test_or();
        test_xor();
        test_plus();
        test_unfold();
        test_storearg();
        test_loadarg();
        test_loadconst();
        test_if0();

    } catch(const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        std::exit(-1);
    }
}
