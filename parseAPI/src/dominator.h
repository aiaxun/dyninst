/*
 * See the dyninst/COPYRIGHT file for copyright information.
 * 
 * We provide the Paradyn Tools (below described as "Paradyn")
 * on an AS IS basis, and do not warrant its validity or performance.
 * We reserve the right to update, modify, or discontinue this
 * software at any time.  We shall have no obligation to supply such
 * updates or modifications or any other form of support to you.
 * 
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _DOMINATOR_H_
#define _DOMINATOR_H_

#include "dyntypes.h"
#include "CFG.h"
#include <unordered_map>
#include <set>

namespace Dyninst{
namespace ParseAPI{

class dominatorCFG;
class dominatorBB;

class dominatorBB {
   friend class dominatorCFG;
protected:
   int dfs_no;
   int size;
   dominatorBB *semiDom;
   dominatorBB *immDom;
   dominatorBB *label;
   dominatorBB *ancestor;
   dominatorBB *parent;
   dominatorBB *child;

   Block *parseBlock;
   dominatorCFG *dom_cfg;

   std::set<dominatorBB *> bucket;
   vector<dominatorBB *> pred;
   vector<dominatorBB *> succ;   
 public:
   dominatorBB(Block *bb, dominatorCFG *dc);
   ~dominatorBB();
   dominatorBB *eval();
   void compress();
   int sdno();
};

class dominatorCFG {
   friend class dominatorBB;
 protected:
   std::unordered_map<Address, dominatorBB *> map_;
   Function *func;
   vector<dominatorBB *> all_blocks;
   vector<dominatorBB *> sorted_blocks;
   int currentDepthNo;

   dominatorBB *entryBlock;
   dominatorBB *nullNode;

   void performComputation();
   void depthFirstSearch(dominatorBB *v);
   void eval(dominatorBB *v);
   void link(dominatorBB *v, dominatorBB *w);
   dominatorBB *parseToDomBB(Block *bb);

 public:
   dominatorCFG(Function *f);
   ~dominatorCFG();

   void calcDominators();
   void calcPostDominators();
};
}
}
#endif
