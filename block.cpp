#include "block.h"
#include "require.h"


namespace {

typedef std::vector<uint64_t> Stack;

void opNot(Stack* const stack)
{
    stack->back() = ~stack->back();
}

void opShl1(Stack* const stack)
{
    stack->back() <<= 1;
}

void opShr1(Stack* const stack)
{
    stack->back() >>= 1;
}

void opShr4(Stack* const stack)
{
    stack->back() >>= 4;
}

void opShr16(Stack* const stack)
{
    stack->back() >>= 16;
}

void opAnd(Stack* const stack)
{
    stack->rbegin()[1] &= stack->back();
    stack->pop_back();
}

void opOr(Stack* const stack)
{
    stack->rbegin()[1] |= stack->back();
    stack->pop_back();
}

void opXor(Stack* const stack)
{
    stack->rbegin()[1] ^= stack->back();
    stack->pop_back();
}

void opPlus(Stack* const stack)
{
    stack->rbegin()[1] += stack->back();
    stack->pop_back();
}

void opUnfold(Stack * const stack)
{
    const uint64_t value = stack->back();
    stack->pop_back();
    for (int offset = 56; offset >= 0; offset -= 8) {
        stack->push_back(0xff & (value >> offset));
    }
}

} // namespace


Block::Block(size_t initalStackSize)
  : initalStackSize_(initalStackSize)
  , stackSize_(initalStackSize)
{ }

void Block::emitNot()
{
    require (stackSize_ > 0, "emitNot: Inconsisten stack state.");
    code_.push_back(Op::NOT);
}

void Block::emitShl1()
{
    require (stackSize_ > 0, "emitShl1: Inconsisten stack state.");
    code_.push_back(Op::SHL1);
}

void Block::emitShr1()
{
    require (stackSize_ > 0, "emitShr1: Inconsisten stack state.");
    code_.push_back(Op::SHR1);
}

void Block::emitShr4()
{
    require (stackSize_ > 0, "emitShr4: Inconsisten stack state.");
    code_.push_back(Op::SHR4);
}

void Block::emitShr16()
{
    require (stackSize_ > 0, "emitShr16: Inconsisten stack state.");
    code_.push_back(Op::SHR16);
}

void Block::emitAnd()
{
    require (stackSize_ > 1, "emitAnd: Inconsisten stack state.");
    code_.push_back(Op::AND);
    --stackSize_;
}

void Block::emitOr()
{
    require (stackSize_ > 1, "emitOr: Inconsisten stack state.");
    code_.push_back(Op::OR);
    --stackSize_;
}

void Block::emitXor()
{
    require (stackSize_ > 1, "emitXor: Inconsisten stack state.");
    code_.push_back(Op::XOR);
    --stackSize_;
}

void Block::emitPlus()
{
    require (stackSize_ > 1, "emitPlus: Inconsisten stack state.");
    code_.push_back(Op::PLUS);
    --stackSize_;
}

void Block::emitUnfold()
{
    require (stackSize_ > 0, "emitUnfold: Inconsistent stack state.");
    code_.push_back(Op::UNFOLD);
    stackSize_ += 7;
}

void Block::emitStoreArg(int n)
{
    require (0 <= n && n < 8, "emitStoreArg: Unsupported N.");
    require (stackSize_ > 0, "emitStoreArg: Inconsisten stack state.");
    code_.push_back(static_cast<Op>(static_cast<int>(Op::STORE_ARG0) + n));
    --stackSize_;
}

void Block::emitLoadArg(int n)
{
    require (0 <= n && n < 8, "emitLoadArg: Unsupported N.");
    code_.push_back(static_cast<Op>(static_cast<int>(Op::LOAD_ARG0) + n));
    ++stackSize_;
}

void Block::emitLoadConst(uint64_t c)
{
    if (c < 8) {
        code_.push_back(static_cast<Op>(static_cast<int>(Op::LOAD_0) + c));
    } else {
        code_.push_back(Op::LOAD_CONST);
        code_.resize(code_.size() + 8);
        *(uint64_t*)(&code_[code_.size() - 8]) = c;
    }
    ++stackSize_;
}

void Block::emitJnz(size_t shift)
{
    require (stackSize_ > 0, "emitJnz: Inconsisten stack state.");
    require (static_cast<uint16_t>(shift) == shift, "emitJnz: Shift is too large.");
    code_.push_back(Op::JNZ);
    code_.resize(code_.size() + 2);
    *(uint16_t*)(&code_[code_.size() - 2]) = shift;
    --stackSize_;
}

void Block::emitJmp(size_t shift)
{
    require (static_cast<uint16_t>(shift) == shift, "emitJmp: Shift is too large.");
    code_.push_back(Op::JMP);
    code_.resize(code_.size() + 2);
    *(uint16_t*)(&code_[code_.size() - 2]) = shift;
}

void Block::emitBlock(const Block& block)
{
    require (stackSize_ >= block.initalStackSize_, "emitBlock: Inconsisten stack state.");
    code_.insert(code_.end(), block.code_.begin(), block.code_.end());
    stackSize_ = stackSize_ - block.initalStackSize_ + block.stackSize_;
}

void Block::emitIf0(const Block& ifBlock, const Block& elseBlock)
{
    require (ifBlock.initalStackSize_ == 0 && ifBlock.stackSize_ == 1,
             "emitIf0: Inconsisten ifBlock.");
    require (elseBlock.initalStackSize_ == 0 && elseBlock.stackSize_ == 1,
             "emitIf0: Inconsisten elseBlock.");
    require (stackSize_ > 0, "emitIf0: Inconsisten stack state.");

    emitJnz(ifBlock.code_.size() + /* jmp_size */ 3);
    emitBlock(ifBlock);
    emitJmp(elseBlock.code_.size());
    emitBlock(elseBlock);
    --stackSize_;
}


uint64_t Block::execute(std::vector<uint64_t> argv) const
{
    require (initalStackSize_ == 0, "execute: Block is not runnable.");
    require (stackSize_ == 1, "execute: Block incomplete.");

    argv.resize(8);

    Stack stack;

    size_t ip = 0;
    while (ip < code_.size()) {
        switch (code_[ip]) {
        case Op::NOT:
            opNot(&stack);
            ++ip;
            break;

        case Op::SHL1:
            opShl1(&stack);
            ++ip;
            break;

        case Op::SHR1:
            opShr1(&stack);
            ++ip;
            break;

        case Op::SHR4:
            opShr4(&stack);
            ++ip;
            break;

        case Op::SHR16:
            opShr16(&stack);
            ++ip;
            break;

        case Op::AND:
            opAnd(&stack);
            ++ip;
            break;

        case Op::OR:
            opOr(&stack);
            ++ip;
            break;

        case Op::XOR:
            opXor(&stack);
            ++ip;
            break;

        case Op::PLUS:
            opPlus(&stack);
            ++ip;
            break;

        case Op::UNFOLD:
            opUnfold(&stack);
            ++ip;
            break;

        case Op::STORE_ARG0:
        case Op::STORE_ARG1:
        case Op::STORE_ARG2:
        case Op::STORE_ARG3:
        case Op::STORE_ARG4:
        case Op::STORE_ARG5:
        case Op::STORE_ARG6:
        case Op::STORE_ARG7:
            argv.at(static_cast<int>(code_[ip]) - static_cast<int>(Op::STORE_ARG0)) = stack.back();
            stack.pop_back();
            ++ip;
            break;

        case Op::LOAD_ARG0:
        case Op::LOAD_ARG1:
        case Op::LOAD_ARG2:
        case Op::LOAD_ARG3:
        case Op::LOAD_ARG4:
        case Op::LOAD_ARG5:
        case Op::LOAD_ARG6:
        case Op::LOAD_ARG7:
            stack.push_back(argv.at(static_cast<int>(code_[ip]) - static_cast<int>(Op::LOAD_ARG0)));
            ++ip;
            break;

        case Op::LOAD_0:
        case Op::LOAD_1:
        case Op::LOAD_2:
        case Op::LOAD_3:
        case Op::LOAD_4:
        case Op::LOAD_5:
        case Op::LOAD_6:
        case Op::LOAD_7:
            stack.push_back(static_cast<int>(code_[ip]) - static_cast<int>(Op::LOAD_0));
            ++ip;
            break;

        case Op::LOAD_CONST:
            stack.push_back(*(const uint64_t*)&code_[ip + 1]);
            ip += 9;
            break;

        case Op::JNZ:
            if (stack.back() != 0) {
                ip += *(const uint16_t*)&code_[ip + 1];
            }
            stack.pop_back();
            ip += 3;
            break;

        case Op::JMP:
            ip += *(const uint16_t*)&code_[ip + 1];
            ip += 3;
            break;
        }
    }
    return stack.back();
}
