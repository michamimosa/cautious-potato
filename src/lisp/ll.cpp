#include <string.h>
#include <stdlib.h>

#include <logger.h>
#include <context.h>

#include <ll.h>

#include <lisp/ll.h>
#include <lisp/parser.h>

Logger* lisp_logger;
Logger* lisp_parser_logger;
Logger* lisp_atom_logger;

extern vword_t default_parent;
extern List<struct symbol*>* symbols;

vword_t lisp_exec(Context* context, const char* str)
{
    vword_t stack = 4096*0x100 - VWORD_SIZE;
    vword_t entry_point = 0x2000;

    SNode* ast = new SNode(LIST, (char*)str);
    lisp_parse(context, entry_point, ast);

    // set entry point and run
    context->write_word(stack, entry_point);
    return ll_eval(context, stack);
}

void init_lisp(Context* context)
{
    lisp_logger = new Logger("lisp");
    lisp_parser_logger = new Logger(lisp_logger, "parser");
    lisp_atom_logger = new Logger(lisp_parser_logger, "atom");

    symbols = new List<struct symbol*>();

    default_parent = 0;

    // system functions
    add_symbol("eval", context->add_ll_fn(ll_eval), VWORD_SIZE);
    add_symbol("deval", context->add_ll_fn(ll_deval), 2*VWORD_SIZE);
    add_symbol("nop", context->add_ll_fn(ll_nop), -1);

    add_symbol("evalparam", context->add_ll_fn(ll_eval_param), 2*VWORD_SIZE);
    add_symbol("syscall", context->add_ll_fn(ll_syscall), 6*VWORD_SIZE);

    add_symbol("load", context->add_ll_fn(ll_load), VWORD_SIZE);

    add_symbol("oif", context->add_ll_fn(ll_cond), 2*VWORD_SIZE+1);
    add_symbol("eqw", context->add_ll_fn(ll_eqw), 2*VWORD_SIZE);
    add_symbol("eqb", context->add_ll_fn(ll_eqb), 2);

    add_symbol("resw", context->add_ll_fn(ll_resw), VWORD_SIZE);
    add_symbol("setw", context->add_ll_fn(ll_setw), 2*VWORD_SIZE);
    add_symbol("resb", context->add_ll_fn(ll_resb), VWORD_SIZE);
    add_symbol("setb", context->add_ll_fn(ll_setb), VWORD_SIZE+1);
    add_symbol("map", context->add_ll_fn(ll_map), 4*VWORD_SIZE);

    add_symbol("printi", context->add_ll_fn(ll_printi), VWORD_SIZE);
    add_symbol("printb", context->add_ll_fn(ll_printb), 1);
    add_symbol("+", context->add_ll_fn(ll_addi), 2*VWORD_SIZE);
    add_symbol("-", context->add_ll_fn(ll_subi), 2*VWORD_SIZE);
    add_symbol("*", context->add_ll_fn(ll_muli), 2*VWORD_SIZE);
    add_symbol("/", context->add_ll_fn(ll_divi), 2*VWORD_SIZE);

    // lisp macro
    add_symbol("expand", context->add_ll_fn(ll_expand));
    add_symbol("quote", context->add_ll_fn(ll_quote));
    add_symbol("asm", context->add_ll_fn(ll_asm));
    add_symbol("declare", context->add_ll_fn(ll_declare));
    add_symbol("isdecl", context->add_ll_fn(ll_isdef));
    add_symbol("function", context->add_ll_fn(ll_function));
    add_symbol("macro", context->add_ll_fn(ll_macro));
    add_symbol("lmap", context->add_ll_fn(ll_lmap));
    add_symbol("progn", context->add_ll_fn(ll_progn));
}

static vword_t quote_stack = 0;

vword_t eval_params(Context* context, vword_t* p, size_t l)
{
    vword_t pt = *p; // working pointer (temporal pushs)
    vbyte_t* buf = (vbyte_t*) malloc(l * sizeof(vbyte_t));
    vbyte_t* dest = buf;

    size_t i = 0;
    while(i < l && *p < 0x100000)
    {
        quote_stack = *p;

        // get snode
        SNode* sn = new SNode(LIST);
        *p += sn->read_vmem(context, *p);

        // parse it on stack
        size_t n = 0;
        vword_t pushs, fn;

        // self-eval lists
        switch(sn->type)
        {
            case INTEGER:
                pt -= lisp_parse_size(sn);
                n = lisp_parse(context, pt, sn);
                break;

            case LIST:
                pt -= lisp_parse_size(sn);
                n = lisp_parse(context, pt, sn);

                pushs = context->read_word(pt);
                pt += VWORD_SIZE;

                fn = context->read_word(pt);
                if(fn != resolve_symbol("quote")->start)
                    quote_stack = 0;

                n = pt + pushs;
                pt = ll_eval(context, pt);
                n -= pt;
                break;

            case STRING:
                pt -= VWORD_SIZE;
                context->write_word(pt, quote_stack);
                n = VWORD_SIZE;

                quote_stack += lisp_parse(context, quote_stack, sn);
                break;

            case SYMBOL:
                pt -= lisp_parse_size(sn);
                n = lisp_parse(context, pt, sn);

                pt = ll_resw(context, pt);
                break;
        }

        if((i+n) > l)
        {
            printf("getting more than needed (%d). cutting down to %d bytes\n", n, l-i);
            n = l-i;
        }

        dest += context->read(pt, n, dest);
        pt += n;
        i += n;
    }

    quote_stack = 0;

    pt -= i;
    context->write(pt, i, buf);

    free(buf);

    return pt;
}

vword_t ll_function(Context* context, vword_t p)
{
    SNode* plist = new SNode(LIST);
    p += plist->read_vmem(context, p);

    SNode* val = new SNode(LIST);
    p += val->read_vmem(context, p);

//    plist->dump();
//    val->dump();

    int nparam = plist->subnodes->numOfElements();
    size_t reqb = nparam * VWORD_SIZE;

    vword_t pt = eval_params(context, &p, reqb);

    // use stack pointer as group id for symbols
    add_symbol((char*)NULL, p, 0, default_parent);
    vword_t odp = default_parent;
    default_parent = p;

    // bind parameters
    ListIterator<SNode*> it = ListIterator<SNode*>(plist->subnodes);
    int i = 0;
    while(! it.isLast())
    {
        SNode* sn = it.getCurrent();
        vword_t start = pt + (i++)*VWORD_SIZE;
        add_symbol(sn->string, start, 0, p);

        it.next();
    }

    size_t n = pt;
    pt -= lisp_parse_size(val);
    lisp_parse(context, pt, val);

    pt = ll_eval(context, pt+VWORD_SIZE);

    n -= pt;

    // unbind parameters
    it.setFirst();
    while(! it.isLast())
    {
        SNode* sn = it.getCurrent();
        remove_symbol(sn->string, p);
        it.next();
    }
    remove_symbol(default_parent);
    default_parent = odp;

    // copy back
    vbyte_t* buf = (vbyte_t*) malloc(n * sizeof(vbyte_t));
    context->read(pt, n, buf);

    p -= n;
    context->write(p, n, buf);
    free(buf);

    return p;
}

vword_t ll_macro(Context* context, vword_t p)
{
    SNode* plist = new SNode(LIST);
    p += plist->read_vmem(context, p);

    SNode* val = new SNode(LIST);
    p += val->read_vmem(context, p);

    //plist->dump();
    //val->dump();

    if(plist->type != LIST)
    {
        printf("error: no parameterlist\n");
        return p;
    }

    int pnum = plist->subnodes->numOfElements();
    SNode** params = (SNode**) malloc(pnum * sizeof(SNode*));
    char** names = (char**) malloc(pnum * sizeof(char*));

    int i = 0;
    ListIterator<SNode*> it = ListIterator<SNode*>(plist->subnodes);
    while(! it.isLast())
    {
        SNode* c = it.getCurrent();
        if(c->type == SYMBOL)
        {
            names[i] = c->string;
            params[i] = new SNode(LIST);
            p += params[i]->read_vmem(context, p);

            i++;
        }
        else
        {
            printf("error: only symbols in paramlist allowed\n");
        }

        it.next();
    }

    val->replace(names, params, pnum);

    p -= lisp_parse_size(val);
    lisp_parse(context, p, val);
    p = ll_eval(context, p+VWORD_SIZE);

    return p;
}

vword_t ll_eval_param(Context* context, vword_t p)
{
    vword_t fn = context->read_word(p);
    p += VWORD_SIZE;
    vword_t l = context->read_word(p);
    p += VWORD_SIZE;

    vword_t pt = eval_params(context, &p, l);

    pt -= VWORD_SIZE;
    context->write_word(pt, fn);

    // eval
    size_t n = pt + l + VWORD_SIZE;
    pt = ll_eval(context, pt);
    n -= pt;

    // copy back
    vbyte_t* buf = (vbyte_t*) malloc(n * sizeof(vbyte_t));
    context->read(pt, n, buf);

    p -= n;
    context->write(p, n, buf);
    free(buf);

    return p;
}

vword_t ll_isdef(Context* context, vword_t p)
{
    SNode* sym = new SNode(context, p);
    p += sym->vmem_size();

    vbyte_t t = (vbyte_t)1;
    vbyte_t f = (vbyte_t)0;

    p -= 1;
    if(resolve_symbol(sym->string) == NULL)
        context->write(p, 1, &f);
    else
        context->write(p, 1, &t);

    return p;
}

vword_t ll_declare(Context* context, vword_t p)
{
    static Logger* logger = new Logger(lisp_logger, "declare");

    static vword_t def_top = 0xA0000;

    SNode* parent = new SNode(context, p);
    p += parent->vmem_size();
    SNode* name = new SNode(context, p);
    p += name->vmem_size();
    SNode* value = new SNode(context, p);
    p += value->vmem_size();

    if(name->type == SYMBOL)
    {
        if(resolve_symbol(name->string) == NULL)
        {
            size_t len = def_top;
            def_top -= lisp_parse_size(value);
            lisp_parse(context, def_top, value);

            // execute lists
            if(parent->type == LIST)
                def_top = ll_eval(context, def_top+VWORD_SIZE);

            if(value->type == LIST)
                def_top = ll_eval(context, def_top+VWORD_SIZE);

            len -= def_top;

            //logger->log(linfo, "declared \'%s\': 0x%x, %d bytes", name->string, def_top, len);
            vword_t parent_id = parent->integer;
            if(parent_id == -1)
                parent_id = default_parent;

            add_symbol(name->string, def_top, 0, parent_id);
        }
        else
            logger->log(lerror, "symbol \'%s\' already in use", name->string);
    }
    else
        logger->log(lerror, "name argument must be a symbol!");

    delete name;
    delete value;

    return p;
}

vword_t ll_asm(Context* context, vword_t p)
{
    SNode* ast = new SNode(context, p);
    p += ast->vmem_size();

    size_t l = asm_parse(context, 0x2000, ast);

    p -= VWORD_SIZE;
    context->write_word(p, 0x2000);

    return p;
}

vword_t ll_quote(Context* context, vword_t p)
{
    SNode* ast = new SNode(context, p);
    p += ast->vmem_size();

    if(ast->type == LIST && quote_stack != (vword_t)0)
    {
//		printf("quote with pointer!\n");
        p -= VWORD_SIZE;
        context->write_word(p, quote_stack);

        quote_stack += lisp_parse(context, quote_stack, ast);
    }
    else
    {
        p -= lisp_parse_size(ast);
        lisp_parse(context, p, ast);
    }

    return p;
}

vword_t ll_lmap(Context* context, vword_t p)
{
    SNode* fn = new SNode(context, p);
    p += fn->vmem_size();

    SNode* list = new SNode(context, p);
    p += list->vmem_size();

    ListIterator<SNode*> it = ListIterator<SNode*>(list->subnodes);
    while(! it.isLast())
    {
        SNode* param = it.getCurrent();

        p -= param->vmem_size();
        param->write_vmem(context, p);

        p -= lisp_parse_size(fn);
        lisp_parse(context, p, fn);
        if(fn->type == LIST)
            p += VWORD_SIZE;
        p = ll_eval(context, p);

        it.next();
    }

    return p;
}

vword_t ll_progn(Context* context, vword_t p)
{
    SNode* list = new SNode(context, p);
    p += list->vmem_size();

    ListIterator<SNode*> it = ListIterator<SNode*>(list->subnodes);
    while(! it.isLast())
    {
        SNode* fn = it.getCurrent();

        p -= lisp_parse_size(fn);
        lisp_parse(context, p, fn);

        if(fn->type == LIST)
            p += VWORD_SIZE;
        p = ll_eval(context, p);

        it.next();
    }

    return p;
}

