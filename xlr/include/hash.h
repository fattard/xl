#ifndef HASH_H
#define HASH_H
// *****************************************************************************
// hash.h                                                             XL project
// *****************************************************************************
//
// File description:
//
//     Tools to run cryptographic hash functions over XL trees.
//     Hashing is a convenient way to verify the integrity of a tree.
//
//
//
//
//
//
//
// *****************************************************************************
// This software is licensed under the GNU General Public License v3
// (C) 2010,2019, Christophe de Dinechin <christophe@dinechin.org>
// (C) 2010,2012, Jérôme Forissier <jerome@taodyne.com>
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
#include "action.h"
#include "sha1.h"
#include <string.h>

XL_BEGIN

template <class Hash = Sha1>
struct HashInfo : Info, Hash
// ----------------------------------------------------------------------------
//   Information in a tree about the hash
// ----------------------------------------------------------------------------
{
    typedef Hash data_t;
    HashInfo (const Hash &h) : Hash(h) {}
    operator data_t () { return *this; }
};


template <class Hash = Sha1>
struct TreeHashPruneAction : Action
// ----------------------------------------------------------------------------
//   Delete and reset all the hash pointers of a tree
// ----------------------------------------------------------------------------
{
    TreeHashPruneAction () {}
    Tree *Do(Tree *what)
    {
        what->Purge < HashInfo<Hash> >();
        return what;
    }
};


template <class Hash = Sha1>
struct TreeHashAction : Action
// ----------------------------------------------------------------------------
//   Apply hash algorithm recursively on tree, updating each node's hash values
// ----------------------------------------------------------------------------
{
    enum Mode
    {
      Default = 0,
      Force   = 1,   // Hash all nodes even those with a non-null hash
      Prune   = 2,   // When done, clear hash values of children to save memory
      Eager   = 4    // Prune even if something existed before
    } mode;

    typename Hash::Computation compute;
    typedef HashInfo<Hash> HInfo;

public:
    TreeHashAction (Mode mode = Default) : mode(mode), compute() {}

    void reset() { compute.reset(); }

    bool NeedHash(Tree *t)
    // ------------------------------------------------------------------------
    //   Decide if we need to hash for that tree
    // ------------------------------------------------------------------------
    {
        if (mode & Force)
            return true;
        return !t->Exists<HInfo>();
    }


    void Allocate(Tree *t)
    // ------------------------------------------------------------------------
    //    Create an info entry for the given tree
    // ------------------------------------------------------------------------
    {
        t->Purge<HInfo>();
        t->Set<HInfo>(Hash(compute));
    }


    void Compute(uint64 v)
    // ------------------------------------------------------------------------
    //   Compute the hash for integer values
    // ------------------------------------------------------------------------
    {
        byte   buf[sizeof v];
        for (uint i = 0; i < sizeof v; i++)
        {
            buf[i] = (byte) v;
            v >>= 8;
        }
        compute(buf, sizeof v);
    }


    void ComputeReal (double v)
    // ------------------------------------------------------------------------
    //    A very ugly way to compute a hash for douvble values
    // ------------------------------------------------------------------------
    {
        uint64 *val = (uint64 *) &v;
        Compute(*val);
    }


    void Compute (text t)
    // ------------------------------------------------------------------------
    //    Compute the hash for a text value
    // ------------------------------------------------------------------------
    {
        size_t len = t.length();
        Compute(len);
        compute(t.data(), len);
    }


    void SubTree(Tree *sub)
    // ------------------------------------------------------------------------
    //   Compute the hash for a sub-tree, reusing existing hash if possible
    // ------------------------------------------------------------------------
    {
        if (sub)
        {
            // Check if we need to copmute some hash
            bool needsHash = NeedHash(sub);
            if (needsHash)
            {
                TreeHashAction hashSubTree(mode);
                sub->Do(hashSubTree);
            }

            // Copmute the hash at this level from children's hash
            HInfo subHash = sub->Get<HInfo>();
            compute(subHash.hash, subHash.SIZE);

            // If we want to prune hash information after the fact
            if ((mode & Prune) && ((mode & Eager) || needsHash))
                sub->Purge<HInfo>();
        }
        else
        {
            Compute(0);
        }
    }


    Tree *Do(Tree *what)
    // ------------------------------------------------------------------------
    //   Part that is common between all trees
    // ------------------------------------------------------------------------
    {
        if (NeedHash(what))
        {
            Compute(what->Kind());
            Allocate(what);
        }
        return what;

    }

    Tree *DoInteger(Integer *what)
    // ------------------------------------------------------------------------
    //    Hash integer values
    // ------------------------------------------------------------------------
    {
        if (NeedHash(what))
            Compute(what->value);
        return Do(what);
    }


    Tree *DoReal(Real *what)
    // ------------------------------------------------------------------------
    //   Hash rwal values
    // ------------------------------------------------------------------------
    {
        if (NeedHash(what))
            ComputeReal(what->value);
        return Do(what);
    }


    Tree *DoText(Text *what)
    // ------------------------------------------------------------------------
    //   Hash text values
    // ------------------------------------------------------------------------
    {
        if (NeedHash(what))
        {
            Compute(what->opening);
            Compute(what->value);
            Compute(what->closing);
        }
        return Do(what);
    }


    Tree *DoName(Name *what)
    // ------------------------------------------------------------------------
    //   Compute the hash for a name
    // ------------------------------------------------------------------------
    {
        if (NeedHash(what))
            Compute(what->value);
        return Do(what);
    }


    Tree *DoBlock(Block *what)
    // ------------------------------------------------------------------------
    //   Compute the hash for a block
    // ------------------------------------------------------------------------
    {
        if (NeedHash(what))
        {
            Compute(what->opening);
            SubTree(what->child);
            Compute(what->closing);
        }
        return Do(what);
    }


    Tree *DoInfix(Infix *what)
    // ------------------------------------------------------------------------
    //    Compute the hash for an infix tree
    // ------------------------------------------------------------------------
    {
        if (NeedHash(what))
        {
            SubTree(what->left);
            Compute(what->name);
            SubTree(what->right);
        }
        return Do(what);
    }


    Tree *DoPrefix(Prefix *what)
    // ------------------------------------------------------------------------
    //    Compute the hash for a prefix tree
    // ------------------------------------------------------------------------
    {
        if (NeedHash(what))
        {
            SubTree(what->left);
            SubTree(what->right);
        }
        return Do(what);
    }


    Tree *DoPostfix(Postfix *what)
    // ------------------------------------------------------------------------
    //    Compute the hash for a postfix tree
    // ------------------------------------------------------------------------
    {
        if (NeedHash(what))
        {
            SubTree(what->right);
            SubTree(what->left);
        }
        return Do(what);
    }
};

extern text sha1(Tree *t);

XL_END

#endif // HASH_H
