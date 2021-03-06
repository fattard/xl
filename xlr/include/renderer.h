#ifndef RENDERER_H
#define RENDERER_H
// *****************************************************************************
// renderer.h                                                         XL project
// *****************************************************************************
//
// File description:
//
//     Rendering of XL trees
//
//
//
//
//
//
//
//
// *****************************************************************************
// This software is licensed under the GNU General Public License v3
// (C) 2003-2007,2009-2010,2012-2013,2019, Christophe de Dinechin <christophe@dinechin.org>
// (C) 2010-2012, Jérôme Forissier <jerome@taodyne.com>
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
#include <ostream>

XL_BEGIN

struct Syntax;
typedef std::map<text,Tree_p>                      formats_table;
typedef std::map<Tree_p,text>                      highlights_table;
typedef std::pair<std::streampos, std::streampos>  stream_range;
typedef std::vector<stream_range>                  stream_ranges;
typedef std::map<text, stream_ranges>              highlight_result;

struct Renderer
// ----------------------------------------------------------------------------
//   Render a tree to some ostream
// ----------------------------------------------------------------------------
{
    // Construction
    Renderer(std::ostream &out, text styleFile, Syntax &stx);
    Renderer(std::ostream &out, Renderer *from = renderer);

    // Selecting the style sheet file
    void                SelectStyleSheet(text styleFile,
                                         text syntaxFile = "xl.syntax");

    // Rendering proper
    void                RenderFile (Tree *what);
    void                Render (Tree *what);
    void                RenderBody(Tree *what);
    void                RenderSeparators(char c);
    void                RenderText(text format);
    void                RenderIndents();
    void                RenderFormat(Tree *format);
    void                RenderFormat(text self, text format);
    void                RenderFormat(text self, text format, text generic);
    void                RenderFormat(text self, text f, text g1, text g2);
    Tree *              ImplicitBlock(Tree *t);
    bool                IsAmbiguousPrefix(Tree *test, bool testL, bool testR);
    bool                IsSubFunctionInfix(Tree *t);
    int                 InfixPriority(Tree *test);

    std::ostream &      output;
    Syntax &            syntax;
    formats_table       formats;
    highlights_table    highlights;
    highlight_result    highlighted;
    uint                indent;
    text                self;
    Tree_p              left;
    Tree_p              right;
    text                current_quote;
    int                 priority;
    bool                had_space;
    bool                had_newline;
    bool                had_punctuation;
    bool                need_separator;
    bool                need_newline;
    bool                no_indents;

    static Renderer *   renderer;
};

std::ostream& operator<< (std::ostream&out, XL::Tree *t);

XL_END

// For use in a debugger
extern "C" const char *debug(XL::Tree *);
extern "C" const char *debugp(XL::Tree *);

#endif // RENDERER_H
