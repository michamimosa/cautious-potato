#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <linux/limits.h>
#include <libgen.h>

#include <ll.h>
#include <context.h>
#include <stackframe.h>

#include <lisp/reader.h>
#include <lisp/parser.h>
#include <lisp/ll.h>

#include <brainfuck/parser.h>
#include <brainfuck/ll.h>
#include <math.h>


#include <bits.h>
#include <memory/context.h>
#include <memory/accessors/linear.h>
//#include <memory/accessors/stack.h>

void readline(int fd, char* str)
{
    char c=0;
    while(1)
    {
        read(fd, &c, 1);

        if(c != '\n')
            *str++ = c;
        else
        {
            *str = '\0';
            break;
        }
    }
}

using namespace giecs;
/*
template <typename addr_t, typename val_t>
class lcore
{
    public:
        typedef memory::accessors::Stack<addr_t, val_t> Stack;

        void printi(Stack& stack) const
        {
            printf("%d\n", (int)stack.pop());
        }
};
*/
int main(int argc, char** argv)
{
    printf("Hello World!\n");

    // set up vm
    typedef Bits<6> byte;
    typedef Bits<24> word;

    auto c1 = new memory::Context<8, uint16_t>();
    auto acc = memory::accessors::Linear<8, uint16_t, uint32_t, byte>(c1);

    acc[0] = 10;
    acc[1] = 20;
    acc[2] = 16;
    acc[3] = 40;

    uint16_t buf[8];
    acc.read_page(0, buf);

    for(int i=0; i < 8; i++)
        printf("0x%x\n", (unsigned int)buf[i]);

    acc.write_page(1, buf);
    for(int i=0; i < 4; i++)
    {
        byte v = acc[21+i];
        printf("%d\n", (int)v);
    }

    /*
        auto stack = memory::accessors::Stack<uint32_t, uint32_t>(c1);
        auto core = lcore<uint32_t, uint32_t>();

    //    stack.push(Bits<8>(123));
        stack.push(uint32_t(2));
        stack.push(uint16_t(3));
        stack.push(uint8_t(4));


        core.printi(stack);
        core.printi(stack);
        core.printi(stack);
    //    core.printi(stack);
    */
    return 0;
    /*
        // place stack at end of memory
        StackFrame stack = StackFrame(context, context->upper_limit());

        init_lisp(context);
        /*    init_brainfuck(context);


                // parse a simple program
                SNode* ast = new SNode(LIST, (char*) "12 deval 1 (-1 (12 brainfuck \"++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.>++.\"))");
                asm_parse_list(context, 0x2000, ast);

                // set entry point and run
                context->write_word(stack-VWORD_SIZE, 0x2000);
                ll_eval(context, stack-VWORD_SIZE);
        */
    // loading scripts
    /*    int i;
        for(i = 1; i < argc; i++)
        {
            int fd = open(argv[i], O_RDONLY);
            size_t length = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            char* buf = (char*) malloc(length+1);
            read(fd, buf, length);
            buf[length] = '\0';

            SNode* ast = new SNode(LIST, buf);
            if(! ast->subnodes->isEmpty())
            {
                int cwd = open(".", O_RDONLY);
                chdir(dirname(argv[i]));

                stack.move(-lisp_parse_size(ast));
                lisp_parse(context, stack.ptr(), ast);
                stack.move(VWORD_SIZE);
                ll_eval(stack);

                fchdir(cwd);

                close(cwd);
                close(fd);
            }
        }

        // interpreter loop (read-eval-print)
        printf("\n\033[0;49;32mType S-expressions\033[0m\n");
        while(1)
        {
            write(1, "\033[0;49;32m>>> \033[0;49;33m", 24);
            char str[256];
            readline(0, str);
            write(1, "\033[0m", 4);

            SNode* ast = new SNode(LIST, str);
            if(! ast->subnodes->isEmpty())
            {
                //ast->dump();
                vword_t s = stack.ptr();
                stack.move(lisp_parse_size(ast));
                if(lisp_parse(stack.context, stack.ptr(), ast) > 0)
                {
                    //                stack = ll_expand(context, stack);
                    stack.move(-VWORD_SIZE);
                    ll_eval(stack);

                    size_t l = s-stack.ptr();
                    if(l > 0)
                    {
                        printf("return: ");
                        switch(l)
                        {
                            case 1:
                                ll_printb(stack);
                                break;

                            case VWORD_SIZE:
                                ll_printi(stack);
                                break;

                            default:
                                context->dump(stack.ptr(), l/VWORD_SIZE);
                        }
                    }
                }
            }
            delete ast;
        }

        delete context;
    */
    return 0;
}

