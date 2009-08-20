// ****************************************************************************
//  tree.cpp                        (C) 1992-2003 Christophe de Dinechin (ddd)
//                                                            XL2 project
// ****************************************************************************
//
//   File Description:
//
//     Implementation of the parse tree elements
//
//
//
//
//
//
//
//
// ****************************************************************************
// This program is released under the GNU General Public License.
// See http://www.gnu.org/copyleft/gpl.html for details
// ****************************************************************************
// * File       : $RCSFile$
// * Revision   : $Revision$
// * Date       : $Date$
// ****************************************************************************

#include "tree.h"
#include "context.h"

XL_BEGIN

// ============================================================================
//
//    Class Tree
//
// ============================================================================

tree_position Tree::NOWHERE = ~0UL;

Tree *Tree::Run(Context *context)
// ----------------------------------------------------------------------------
//   Execute a tree.
// ----------------------------------------------------------------------------
//   [ ] -> [ this ]
{
    context->Push(this);
    return this;
}


Tree *Tree::Do(Action *action)
// ----------------------------------------------------------------------------
//   Perform an action on the tree and its descendents
// ----------------------------------------------------------------------------
{
    return action->Do(this);
}


void Tree::DoData(Action *action)
// ----------------------------------------------------------------------------
//   For data actions, run on all trees referenced by this tree
// ----------------------------------------------------------------------------
{
    // Loop on all associated data
    for (tree_data::iterator i = data.begin(); i != data.end(); i++)
        (*i).second = (*i).second->Do(action);
}


Tree *Tree::Normalize()
// ----------------------------------------------------------------------------
//   The basic tree types are normalized by definition
// ----------------------------------------------------------------------------
{
    // Verify that we don't use the default 'Normalize' for unholy trees
    XL_ASSERT(dynamic_cast<Integer *> (this)  ||
              dynamic_cast<Real *> (this)     ||
              dynamic_cast<Text *> (this)     ||
              dynamic_cast<Name *> (this)     ||
              dynamic_cast<Block *> (this)    ||
              dynamic_cast<Prefix *> (this)   ||
              dynamic_cast<Infix *> (this));

    return this;
}



// ============================================================================
// 
//   Class Integer
// 
// ============================================================================

Tree *Action::DoInteger(Integer *what)
// ----------------------------------------------------------------------------
//   Default is simply to invoke 'Do'
// ----------------------------------------------------------------------------
{
    return Do(what);
}


Tree *Integer::Do(Action *action)
// ----------------------------------------------------------------------------
//   Call specialized Integer routine in the action
// ----------------------------------------------------------------------------
{
    return action->DoInteger(this);
}



// ============================================================================
// 
//   Class Real
// 
// ============================================================================

Tree *Action::DoReal(Real *what)
// ----------------------------------------------------------------------------
//   Default is simply to invoke 'Do'
// ----------------------------------------------------------------------------
{
    return Do(what);
}


Tree *Real::Do(Action *action)
// ----------------------------------------------------------------------------
//   Call specialized Real routine in the action
// ----------------------------------------------------------------------------
{
    return action->DoReal(this);
}



// ============================================================================
// 
//   Class Text
// 
// ============================================================================

Tree *Action::DoText(Text *what)
// ----------------------------------------------------------------------------
//   Default is simply to invoke 'Do'
// ----------------------------------------------------------------------------
{
    return Do(what);
}


Tree *Text::Do(Action *action)
// ----------------------------------------------------------------------------
//   Call specialized Text routine in the action
// ----------------------------------------------------------------------------
{
    return action->DoText(this);
}



// ============================================================================
// 
//   Class Name
// 
// ============================================================================

Tree *Action::DoName(Name *what)
// ----------------------------------------------------------------------------
//   Default is simply to invoke 'Do'
// ----------------------------------------------------------------------------
{
    return Do(what);
}


Tree *Name::Do(Action *action)
// ----------------------------------------------------------------------------
//   Call specialized Name routine in the action
// ----------------------------------------------------------------------------
{
    return action->DoName(this);
}


Tree *Name::Run(Context *context)
// ----------------------------------------------------------------------------
//    Evaluate a name
// ----------------------------------------------------------------------------
//    [ ] -> [ named-value ]
{
    if (Tree *named = context->Name(value))
    {
        // We have a tree with that name in the context, put that on TOS
        context->Mark(named->Run(context));

        // Make sure we lookup the name every time
        return this;
    }

    // Otherwise, this is an error to evaluate the name
    return context->Error("Name '$1' doesn't exist", this);
}



// ============================================================================
// 
//   Class Block
// 
// ============================================================================

Tree *Action::DoBlock(Block *what)
// ----------------------------------------------------------------------------
//    Default is to firm perform action on block's child, then on self
// ----------------------------------------------------------------------------
{
    what->child = what->child->Do(this);
    return Do(what);
}


Tree *Block::Do(Action *action)
// ----------------------------------------------------------------------------
//   Run the action on the child first
// ----------------------------------------------------------------------------
{
    return action->DoBlock(this);
}


Tree *Block::Run(Context *context)
// ----------------------------------------------------------------------------
//    Execute a block
// ----------------------------------------------------------------------------
{
    // If there is a block operation, execute that operation on child
    Tree *blockOp = context->Block(Opening());
    if (blockOp)
    {
        // Leave child unevaluated for block operation to process
        context->Push(this);
        context->Push(child);
        return blockOp->Run(context);
    }

    // Otherwise, simply execute the child (i.e. optimize away block)
    return child->Run(context);
}


Block *Block::MakeBlock(Tree *child, text open, text close, tree_position pos)
// ----------------------------------------------------------------------------
//   Create the right type of block based on open and close
// ----------------------------------------------------------------------------
{
    static Block indent(NULL);
    static Parentheses paren(NULL);
    static Brackets brackets(NULL);
    static Curly curly(NULL);
    if (open == indent.Opening() && close == indent.Closing())
        return new Block(child, pos);
    else if (open == paren.Opening() && close == paren.Closing())
        return new Parentheses(child, pos);
    else if (open == brackets.Opening() && close == brackets.Closing())
        return new Brackets(child, pos);
    else if (open == curly.Opening() && close == curly.Closing())
        return new Curly(child, pos);
    return new DelimitedBlock(child, open, close, pos);
}



// ============================================================================
// 
//   Class Prefix
// 
// ============================================================================

Tree *Action::DoPrefix(Prefix *what)
// ----------------------------------------------------------------------------
//   Default is to run the action on the left, then on right
// ----------------------------------------------------------------------------
{
    what->left = what->left->Do(this);
    what->right = what->right->Do(this);
    return Do(what);
}


Tree *Prefix::Do(Action *action)
// ----------------------------------------------------------------------------
//   Run the action on the left and right children first
// ----------------------------------------------------------------------------
{
    return action->DoPrefix(this);
}


Tree *Prefix::Run(Context *context)
// ----------------------------------------------------------------------------
//    Execute a prefix node
// ----------------------------------------------------------------------------
{
    // If the name denotes a known prefix, then execute, e.g. sin X
    if (Name *name = dynamic_cast<Name *> (left))
    {
        if (Tree *prefixOp = context->Prefix(name->value))
        {
            context->Push(this);
            context->Push(right);
            return prefixOp->Run(context);
        }
    }

   // Evaluate left, e.g. in A[5] (3), evaluate A[5]
    left = left->Run(context);

    // Retrieve TOS, i.e. evaluated value of A[5]
    Tree *leftVal = context->Pop();

    // If leftVal is left, we don't know what to do with it.
    if (leftVal == left)
        return context->Error("Uknown prefix operation '$1'", this);

    // Otherwise, evaluate with that result
    context->Push(this);
    context->Push(right);
    return leftVal->Run(context);
}



// ============================================================================
// 
//   Class Postfix
// 
// ============================================================================

Tree *Action::DoPostfix(Postfix *what)
// ----------------------------------------------------------------------------
//   Default is to run the action on the right, then on the left
// ----------------------------------------------------------------------------
{
    what->right = what->right->Do(this);
    what->left = what->left->Do(this);
    return Do(what);
}


Tree *Postfix::Do(Action *action)
// ----------------------------------------------------------------------------
//   Run the action on the left and right children first
// ----------------------------------------------------------------------------
{
    return action->DoPostfix(this);
}


Tree *Postfix::Run(Context *context)
// ----------------------------------------------------------------------------
//    Execute a postfix node
// ----------------------------------------------------------------------------
{
    // If the name denotes a known postfix, then execute, e.g. sin X
    if (Name *name = dynamic_cast<Name *> (left))
    {
        if (Tree *postfixOp = context->Postfix(name->value))
        {
            context->Push(this);
            context->Push(left);
            return postfixOp->Run(context);
        }
    }

    return context->Error("Uknown postfix operation '$1'", this);
}



// ============================================================================
// 
//    Class Infix 
// 
// ============================================================================

Tree *Action::DoInfix(Infix *what)
// ----------------------------------------------------------------------------
//   Default is to run the action on children first
// ----------------------------------------------------------------------------
{
    tree_list::iterator i;
    for (i = what->list.begin(); i != what->list.end(); i++)
        *i = (*i)->Do(this);
    return Do(what);
}


Tree *Infix::Do(Action *action)
// ----------------------------------------------------------------------------
//   Run the action on the children first
// ----------------------------------------------------------------------------
{
    return action->DoInfix(this);
}


Tree *Infix::Run(Context *context)
// ----------------------------------------------------------------------------
//    Execute an infix node
// ----------------------------------------------------------------------------
{
    // If the name denotes a known infix, then execute, e.g. A+B
    if (Tree *infixOp = context->Infix(name))
    {
        tree_list::iterator i;
        bool firstOne = true;
        Tree *result = this;
        context->Push(this);
        for (i = list.begin(); i != list.end(); i++)
        {
            context->Push(*i);
            if (!firstOne)
            {
                result = infixOp->Run(context);
                context->Mark(result);
            }
            firstOne = true;
        }
        return result;
    }

    return context->Error("Uknown infix operation '$1'", this);
}



// ============================================================================
// 
//    Class Native
// 
// ============================================================================

Tree *Action::DoNative(Native *what)
// ----------------------------------------------------------------------------
//   Default is simply to invoke 'Do'
// ----------------------------------------------------------------------------
{
    return Do(what);
}


Tree *Native::Do(Action *action)
// ----------------------------------------------------------------------------
//   For native nodes, default actions will do
// ----------------------------------------------------------------------------
{
    return action->DoNative(this);
}


Tree *Native::Run(Context *context)
// ----------------------------------------------------------------------------
//    Execute a infix node
// ----------------------------------------------------------------------------
{
    return context->Error("Uknown native operation '$1'", this);
}



// ============================================================================
//
//    Output: Show a possibly parenthesized rendering of the tree
//
// ============================================================================

XL_END