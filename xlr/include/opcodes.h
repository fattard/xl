#ifndef OPCODES_H
#define OPCODES_H
// *****************************************************************************
// opcodes.h                                                          XL project
// *****************************************************************************
//
// File description:
//
//    Opcodes are native trees generated as part of compilation/optimization
//    to speed up execution. They represent a step in the evaluation of
//    the code.
//
//
//
//
//
//
// *****************************************************************************
// This software is licensed under the GNU General Public License v3
// (C) 2010, Catherine Burvelle <catherine@taodyne.com>
// (C) 2003-2004,2006,2009-2011,2019, Christophe de Dinechin <christophe@dinechin.org>
// (C) 2010,2012, Jérôme Forissier <jerome@taodyne.com>
// (C) 2010, Lionel Schaffhauser <lionel@taodyne.com>
// (C) 2004, Sébastien Brochet <sebbrochet@sourceforge.net>
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

#include "tree.h"
#include "context.h"

XL_BEGIN

typedef Tree * (*typecheck_fn) (Context *context, Tree *src, Tree *value);

longlong        xl_integer_arg(Tree *arg);
double          xl_real_arg(Tree *arg);
text            xl_text_arg(Tree *arg);
int             xl_character_arg(Tree *arg);
bool            xl_boolean_arg(Tree *arg);
Tree *          xl_parameter(text symbol, text type);
Tree *          xl_parameters_tree(TreeList parameters);
void            xl_set_documentation(Tree *node, text doc);
void            xl_enter_infix(Context *context, text name, native_fn fn,
                               Tree *rtype, text t1, text symbol, text t2,
                               text doc);
void            xl_enter_prefix(Context *context, text name, native_fn fn,
                                Tree *rtype, TreeList &parameters,
                                text symbol, text doc);
void            xl_enter_postfix(Context *context, text name, native_fn fn,
                                 Tree *rtype, TreeList &parameters,
                                 text symbol, text doc);
void            xl_enter_block(Context *context, text name, native_fn fn,
                               Tree *rtype,
                               text open, text type, text close, text doc);
void            xl_enter_form(Context *context, text name, native_fn fn,
                              Tree *rtype, text form, TreeList &parameters,
                              text doc);
void            xl_enter_name(Symbols *symbols, Name *name);
void            xl_enter_type(Symbols *symbols, Name *tname,
                              text castfnname, typecheck_fn tc);

#define XL_TREE(v)      (v)
#define XL_INT(v)       ((integer_t) (v))
#define XL_CHAR(v)      xl_character_arg(v)
#define XL_REAL(v)      ((real_t) (v))
#define XL_TEXT(v)      ((text_t) (v))
#define XL_BOOL(v)      ((boolean_t) (v))
#define XL_RTREE(val)   return (val)
#define XL_RINT(val)    return new XL::Integer(val)
#define XL_RREAL(val)   return new XL::Real(val)
#define XL_RTEXT(val)   return new XL::Text(val)
#define XL_RBOOL(val)   return (val) ? XL::xl_true : XL::xl_false

typedef Integer &       integer_r;
typedef Real &          real_r;
typedef Text &          text_r;
typedef Name &          boolean_r;
typedef Name &          symbol_r;
typedef Name &          name_r;
typedef Name &          operator_r;
typedef Tree &          tree_r;
typedef Tree &          source_r;
typedef Tree &          code_r;
typedef Tree &          lazy_r;
typedef Tree &          reference_r;
typedef Tree &          value_r;
typedef Infix &         infix_r;
typedef Prefix &        prefix_r;
typedef Postfix &       postfix_r;
typedef Block &         block_r;

typedef Integer_p       integer_p;
typedef Real_p          real_p;
typedef Text_p          text_p;
typedef Name_p          boolean_p;
typedef Name_p          symbol_p;
typedef Name_p          name_p;
typedef Name_p          operator_p;
typedef Tree_p          tree_p;
typedef Tree_p          source_p;
typedef Tree_p          code_p;
typedef Tree_p          lazy_p;
typedef Tree_p          reference_p;
typedef Tree_p          value_p;
typedef Infix_p         infix_p;
typedef Prefix_p        prefix_p;
typedef Postfix_p       postfix_p;
typedef Block_p         block_p;

typedef longlong        integer_t;
typedef double          real_t;
typedef text            text_t;
typedef bool            boolean_t;
typedef Name &          symbol_t;
typedef Name &          name_t;
typedef Name &          operator_t;
typedef Tree &          tree_t;
typedef Tree &          source_t;
typedef Tree &          code_t;
typedef Tree &          lazy_t;
typedef Tree &          reference_t;
typedef Tree &          value_t;
typedef Infix &         infix_t;
typedef Prefix &        prefix_t;
typedef Postfix &       postfix_t;
typedef Block &         block_t;

typedef Integer *       integer_nkp;
typedef Real *          real_nkp;
typedef Text *          text_nkp;
typedef Name *          boolean_nkp;
typedef Name *          symbol_nkp;
typedef Name *          name_nkp;
typedef Name *          operator_nkp;
typedef Tree *          tree_nkp;
typedef Tree *          source_nkp;
typedef Tree *          code_nkp;
typedef Tree *          lazy_nkp;
typedef Tree *          reference_nkp;
typedef Tree *          value_nkp;
typedef Infix *         infix_nkp;
typedef Prefix *        prefix_nkp;
typedef Postfix *       postfix_nkp;
typedef Block *         block_nkp;

XL_END

#endif // OPCODES_H
