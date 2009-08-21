#define INFIX(t1, symbol, t2, name, code)                               \
    do                                                                  \
    {                                                                   \
        Infix *xdecl = new Infix(":", new Name("x"), new Name(#t1));    \
        Infix *ydecl = new Infix(":", new Name("y"), new Name(#t2));    \
        Infix *from = new Infix(symbol, xdecl, ydecl);                  \
        name *to = new name();                                          \
        c->EnterRewrite(from, to);                                      \
    } while(0);

#define PARM(symbol, type)                                      \
        Infix *symbol##_decl = new Infix(":",                   \
                                         new Name(#symbol),     \
                                         new Name(#type));      \
        parameters.push_back(symbol##_decl);

#define PREFIX(symbol, parms, name, code)                               \
    do                                                                  \
    {                                                                   \
        tree_list parameters;                                           \
        parms;                                                          \
        if (parameters.size())                                          \
        {                                                               \
            Tree *parmtree = ParametersTree(parameters);                \
            Prefix *from = new Prefix(new Name(symbol), parmtree);      \
            name *to = new name();                                      \
            c->EnterRewrite(from, to);                                  \
        }                                                               \
        else                                                            \
        {                                                               \
            c->EnterName(symbol, new name());                           \
        }                                                               \
    } while(0);

#define POSTFIX(t1, symbol, name, code)                         \
    do                                                          \
    {                                                           \
        tree_list parameters;                                   \
        parms;                                                  \
        Tree *parmtree = ParametersTree(parameters);            \
        Prefix *from = new Postfix(parmtree, new Name(symbol)); \
        name *to = new name();                                  \
        c->EnterRewrite(from, to);                              \
    } while(0);

#define NAME(symbol, name)                      \
    do                                          \
    {                                           \
        name *value = new name(#symbol);        \
        c->EnterName(#symbol, value);           \
        symbol##_name = value;                  \
    } while (0);

#define TYPE(symbol, name)                      \
    do                                          \
    {                                           \
        name *typechecker = new name();         \
        c->EnterName(symbol, typechecker);      \
    } while(0);
