#ifndef RUNTIME_H
#define RUNTIME_H
// *****************************************************************************
// runtime.h                                                          XL project
// *****************************************************************************
//
// File description:
//
//     Functions required for proper run-time execution of XL programs
//
//
//
//
//
//
//
// *****************************************************************************
// This software is licensed under the GNU General Public License v3
// (C) 2012, Baptiste Soulisse <baptiste.soulisse@taodyne.com>
// (C) 2011, Catherine Burvelle <catherine@taodyne.com>
// (C) 2009-2014,2019, Christophe de Dinechin <christophe@dinechin.org>
// (C) 2010-2013, Jérôme Forissier <jerome@taodyne.com>
// *****************************************************************************
// This file is part of XL
//
// XL is free software: you can r redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XL is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with XL, in a file named COPYING.
// If not, see <https://www.gnu.org/licenses/>.
// *****************************************************************************

#include "base.h"
#include "tree.h"
#include <set>
#include <istream>


XL_BEGIN
// ============================================================================
//
//    Forward declarations
//
// ============================================================================

struct Tree;
struct Block;
struct Infix;
struct Prefix;
struct Postfix;
struct Context;
struct Main;
struct SourceFile;



// ============================================================================
//
//    Runtime functions with C interface
//
// ============================================================================

Tree *xl_identity(Context*, Tree *);
Tree *xl_evaluate(Context *, Tree *);
Tree *xl_evaluate_children(Context *, Tree *);
Tree *xl_assigned_value(Context *, Tree *);
Tree *xl_named_value(Context *, Tree *);
Tree *xl_source(Tree *);
Tree *xl_set_source(Tree *value, Tree *source);
Tree *xl_error(Tree *self, text msg, Tree *a1=0, Tree *a2=0, Tree *a3=0);
Tree *xl_form_error(Context *c, Tree *tree);
Tree *xl_stack_overflow(Tree *tree);

Tree *xl_parse_tree(Context *, Tree *tree);
Tree *xl_parse_text(text source);
Tree *xl_bound(Context *, Tree *form);
bool  xl_same_text(Tree * , const char *);
bool  xl_same_shape(Tree *t1, Tree *t2);
Tree *xl_infix_match_check(Context *, Tree *value, kstring name);
Tree *xl_type_check(Context *, Tree *value, Tree *type);

Integer *xl_new_integer(longlong value);
Real    *xl_new_real(double value);
Text    *xl_new_character(char value);
Text    *xl_new_ctext(kstring value);
Text    *xl_new_text(text value);
Text    *xl_new_xtext(kstring value, longlong len, kstring open, kstring close);
Block   *xl_new_block(Block *source, Tree *child);
Prefix  *xl_new_prefix(Prefix *source, Tree *left, Tree *right);
Postfix *xl_new_postfix(Postfix *source, Tree *left, Tree *right);
Infix   *xl_new_infix(Infix *source, Tree *left, Tree *right);
Block   *xl_fill_block(Block *source, Tree *child);
Prefix  *xl_fill_prefix(Prefix *source, Tree *left, Tree *right);
Postfix *xl_fill_postfix(Postfix *source, Tree *left, Tree *right);
Infix   *xl_fill_infix(Infix *source, Tree *left, Tree *right);
Tree    *xl_real_list(Tree *self, uint n, double *values);
Tree    *xl_integer_list(Tree *self, uint n, longlong *values);
Real    *xl_integer2real(Integer *ival);

Tree *xl_new_closure(eval_fn toCall, Tree *expr, uint ntrees, ...);
Tree *xl_tree_copy(Tree *from, Tree *to);

Tree *xl_boolean_cast(Context *, Tree *source, Tree *value);
Tree *xl_integer_cast(Context *, Tree *source, Tree *value);
Tree *xl_real_cast(Context *, Tree *source, Tree *value);
Tree *xl_text_cast(Context *, Tree *source, Tree *value);
Tree *xl_character_cast(Context *, Tree *source, Tree *value);
Tree *xl_tree_cast(Context *, Tree *source, Tree *value);
Tree *xl_source_cast(Context *, Tree *source, Tree *value);
Tree *xl_code_cast(Context *, Tree *source, Tree *value);
Tree *xl_lazy_cast(Context *, Tree *source, Tree *value);
Tree *xl_reference_cast(Context *, Tree *source, Tree *value);
Tree *xl_value_cast(Context *, Tree *source, Tree *value);
Tree *xl_symbol_cast(Context *, Tree *source, Tree *value);
Tree *xl_name_cast(Context *, Tree *source, Tree *value);
Tree *xl_operator_cast(Context *, Tree *source, Tree *value);
Tree *xl_infix_cast(Context *, Tree *source, Tree *value);
Tree *xl_prefix_cast(Context *, Tree *source, Tree *value);
Tree *xl_postfix_cast(Context *, Tree *source, Tree *value);
Tree *xl_block_cast(Context *, Tree *source, Tree *value);
#define xl_integer8_cast   xl_integer_cast
#define xl_integer16_cast  xl_integer_cast
#define xl_integer32_cast  xl_integer_cast
#define xl_integer64_cast  xl_integer_cast
#define xl_unsigned_cast   xl_integer_cast
#define xl_unsigned8_cast  xl_integer_cast
#define xl_unsigned16_cast xl_integer_cast
#define xl_unsigned32_cast xl_integer_cast
#define xl_unsigned64_cast xl_integer_cast
#define xl_real32_cast     xl_real_cast
#define xl_real64_cast     xl_real_cast

Tree *xl_parameter(text name, text type);
void xl_infix_to_list(Infix *infix, TreeList &list);
Tree *xl_list_to_tree(TreeList v, text infix, Infix **deepest = NULL);

Real *xl_springify(Real &value, Real &target, Real &time,
                   Real &damp, Real &kspring, Real &lt, Real &ls);



// ============================================================================
//
//   Initialization code
//
// ============================================================================

void xl_enter_builtin(Main *main, text name, Tree *to, TreeList parms, eval_fn);
void xl_enter_global(Main *main, Name *name, Name_p *address);



// ============================================================================
//
//    Call management
//
// ============================================================================

struct XLCall
// ----------------------------------------------------------------------------
//    A structure that encapsulates a call to an XL tree
// ----------------------------------------------------------------------------
{
    XLCall(text name):
        name(new Name(name)), arguments(NULL), pointer(&arguments) {}

    // Adding arguments
    XLCall &operator, (Tree *tree)
    {
        if (*pointer)
        {
            Infix *infix = new Infix(",", *pointer, tree);
            *pointer = infix;
            pointer = (Tree_p *) &infix->right;
        }
        else
        {
            *pointer = tree;
        }
        args.push_back(tree);
        return *this;
    }
    XLCall &operator, (Tree &tree) { return *this, &tree; }
    XLCall &operator, (longlong v) { return *this, new Integer(v); }
    XLCall &operator, (double  v)  { return *this, new Real(v); }
    XLCall &operator, (text  v)    { return *this, new Text(v); }

    // Calling in a given symbol context
    Tree *  operator() (SourceFile *sf);

    // Calling in a given symbol context
    Tree *  operator() (Symbols *syms = NULL,
                       bool nullIfBad = false, bool cached = true);
    Tree *  build(Symbols *syms = NULL);

public:
    Name_p      name;
    TreeList    args;
    Tree_p      arguments;
    Tree_p *    pointer;
};



// ============================================================================
//
//    Interfaces to make old and new compiler compatible (temporary)
//
// ============================================================================

Tree *xl_define(Context *, Tree *self, Tree *form, Tree *definition);
Tree *xl_evaluate_sequence(Context *, Tree *first, Tree *second);
Tree *xl_evaluate_any(Context *, Tree *form);
Tree *xl_evaluate_block(Context *, Tree *child);
Tree *xl_evaluate_code(Context *, Tree *self, Tree *code);
Tree *xl_evaluate_lazy(Context *, Tree *self, Tree *code);
Tree *xl_evaluate_in_caller(Context *, Tree *code);
Tree *xl_enter_properties(Context *, Tree *self, Tree *storage, Tree *decls);
Tree *xl_enter_constraints(Context *, Tree *self, Tree *constraints);
Tree *xl_attribute(Context *, text name, Tree *form);
Tree *xl_read_property_default(Context *, Tree *self);
Tree *xl_write_property_default(Context *, Tree *self, Tree *value);

typedef Tree *(*xl_read_property_fn)(Context *, Tree *self);
typedef Tree *(*xl_write_property_fn)(Context *, Tree *self, Tree *value);
extern xl_read_property_fn      xl_read_property;
extern xl_write_property_fn     xl_write_property;


inline bool xl_closure(Tree *tree)
// ----------------------------------------------------------------------------
//   Return true if a tree is a closure
// ----------------------------------------------------------------------------
{
    // This is a hack - We may need a more explicit way
    return tree->code && !tree->Symbols();
}



// ============================================================================
//
//    Actions used for functional applications (temporary / obsolete)
//
// ============================================================================

XL_END
#include "action.h"
XL_BEGIN

Tree *xl_apply(Context *, Tree *code, Tree *data);
Tree *xl_range(longlong l, longlong h);
Tree *xl_assign(Context *, Tree *name, Tree *value, Tree *type=NULL);
Tree *xl_index(Context *, Tree *data, Tree *index);
Tree *xl_array_index(Context *, Tree *data, Tree *index);
Integer *xl_size(Context *, Tree *data);
typedef GCPtr<Context> Context_p;


struct ListIterator
// ----------------------------------------------------------------------------
//    Helper to perform operations on lists of items
// ----------------------------------------------------------------------------
{
    ListIterator(Context *context, Symbols *symbols, Tree *what);

public:
    Tree *      Next();
    Tree *      EvaluateRange(Tree *input);
    template<class num>   Tree *Next(num &start, num &end, num &step);

public:
    Context_p   context;
    Symbols_p   symbols;
    Tree_p      data;
    text        separator;
    longlong    startI, endI, stepI;
    double      startF, endF, stepF;
};


struct FunctionInfo : Info
// ----------------------------------------------------------------------------
//   Hold a single-argument function for a given tree
// ----------------------------------------------------------------------------
//   REVISIT: According to Wikipedia, really a Moses Schönfinkel function
{
    FunctionInfo(): function(NULL), context(NULL), symbols(NULL) {}
    virtual Tree * Apply(Tree *what) { return what; }

public:
    eval_fn        function;
    Context_p      context;
    Symbols_p      symbols;
    Tree_p         compiled;
};


struct MapFunctionInfo : FunctionInfo
// ----------------------------------------------------------------------------
//   Record the code for a map operation
// ----------------------------------------------------------------------------
{
    virtual Tree * Apply(Tree *what);
};


struct ReduceFunctionInfo : FunctionInfo
// ----------------------------------------------------------------------------
//   Record the code for a reduce operation
// ----------------------------------------------------------------------------
{
    virtual Tree * Apply(Tree *what);
};


struct FilterFunctionInfo : FunctionInfo
// ----------------------------------------------------------------------------
//   Record the code for a filter operation
// ----------------------------------------------------------------------------
{
    virtual Tree * Apply(Tree *what);
};



// ============================================================================
//
//   Loops
//
// ============================================================================

Tree *xl_integer_for_loop(Context *, Tree *self,
                          Tree *Variable,
                          longlong start, longlong end, longlong step,
                          Tree *body);
Tree *xl_real_for_loop(Context *, Tree *self,
                       Tree *Variable,
                       double start, double end, double step, Tree *body);
Tree *xl_list_for_loop(Context *, Tree *self,
                       Tree *Variable, Tree *list, Tree *body);
Tree *xl_while_loop(Context *, Tree *self,
                    Tree *Condition, Tree *body, bool TestValue);



// ============================================================================
//
//    Basic text I/O (temporary)
//
// ============================================================================

extern "C"
{
    bool xl_write_integer(longlong);
    bool xl_write_real(double);
    bool xl_write_text(kstring);
    bool xl_write_character(char c);
    bool xl_write_tree(XL::Tree *t);
    bool xl_write_cr(void);
}
Tree *xl_list_files(Context *context, Tree *patterns);
bool xl_file_exists(Context *context, Tree_p self, text path);


// ============================================================================
//
//    Loading trees from external files
//
// ============================================================================

Tree *xl_import(Context *, Tree *self, text name, int phase);
Tree *xl_load_data(Context *, Tree *self,
                   text name, text prefix,
                   text fieldSeps = ",;", text recordSeps = "\n",
                   Tree *body = NULL);
Tree *xl_load_data(Context *, Tree *self, text inputName,
                   std::istream &source, bool cached, bool statTime,
                   text prefix, text fieldSeps = ",;", text recordSeps = "\n",
                   Tree *body = NULL);
Tree *xl_add_search_path(Context *, text prefix, text dir);
Text *xl_find_in_search_path(Context *, text prefix, text file);

typedef enum { PARSING_PHASE, DECLARATION_PHASE, EXECUTION_PHASE } phase_t;
typedef Tree * (*decl_fn) (Symbols *, Tree *source, phase_t phase);
void xl_enter_declarator(text name, decl_fn fn);
Name *xl_set_override_priority(Tree *self, float priority);

XL_END


extern uint xl_recursion_count;

#endif // RUNTIME_H
