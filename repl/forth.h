
#pragma once

#include <giecs/memory/context.h>

#include "language.h"

using namespace giecs;

namespace repl
{

namespace lang
{

template <int page_size, typename align_t>
class Forth : public Language
{
    public:
        Forth(memory::Context<page_size, align_t> const& context_)
            : context(context_)
        {
        }

        ~Forth()
        {
        }

        Language* parse(char* str)
        {
            if(strcmp(str, "exit") == 0)
            {
                delete this;
                return NULL;
            }

            std::cout << str << "\n";

            return this;
        }

        void name(char* buf)
        {
            strcpy(buf, "Forth");
        }

    private:
        memory::Context<page_size, align_t> const& context;
};

} // namespace lang

} // namespace repl

