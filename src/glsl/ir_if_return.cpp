/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * \file ir_if_return.cpp
 *
 * If a function includes an if statement that returns from both
 * branches, then make the branches write the return val to a temp and
 * return the temp after the if statement.
 *
 * This allows inlinining in the common case of short functions that
 * return one of two values based on a condition.  This helps on
 * hardware with no branching support, and may even be a useful
 * transform on hardware supporting control flow by masked returns
 * with normal returns.
 */

#include "ir.h"

class ir_if_return_visitor : public ir_hierarchical_visitor {
public:
   ir_if_return_visitor()
   {
      this->progress = false;
   }

   ir_visitor_status visit_enter(ir_if *);

   bool progress;
};

bool
do_if_return(exec_list *instructions)
{
   ir_if_return_visitor v;

   visit_list_elements(&v, instructions);

   return v.progress;
}


ir_visitor_status
ir_if_return_visitor::visit_enter(ir_if *ir)
{
   ir_return *then_return = NULL;
   ir_return *else_return = NULL;

   /* Try to find a return statement on both sides. */
   foreach_iter(exec_list_iterator, then_iter, ir->then_instructions) {
      ir_instruction *then_ir = (ir_instruction *)then_iter.get();
      then_return = then_ir->as_return();
      if (then_return)
	 break;
   }
   if (!then_return)
      return visit_continue;

   foreach_iter(exec_list_iterator, else_iter, ir->else_instructions) {
      ir_instruction *else_ir = (ir_instruction *)else_iter.get();
      else_return = else_ir->as_return();
      if (else_return)
	 break;
   }
   if (!else_return)
      return visit_continue;

   /* Trim off any trailing instructions after the return statements
    * on both sides.
    */
   while (then_return->get_next()->get_next())
      ((ir_instruction *)then_return->get_next())->remove();
   while (else_return->get_next()->get_next())
      ((ir_instruction *)else_return->get_next())->remove();

   this->progress = true;

   if (!then_return->value) {
      then_return->remove();
      else_return->remove();
      ir->insert_after(new(ir) ir_return(NULL));
   } else {
      ir_assignment *assign;
      ir_variable *new_var = new(ir) ir_variable(then_return->value->type,
					     "if_return_tmp");
      ir->insert_before(new_var);

      assign = new(ir) ir_assignment(new(ir) ir_dereference_variable(new_var),
				     then_return->value, NULL);
      then_return->insert_before(assign);
      then_return->remove();

      assign = new(ir) ir_assignment(new(ir) ir_dereference_variable(new_var),
				     else_return->value, NULL);
      else_return->insert_before(assign);
      else_return->remove();

      ir_dereference_variable *deref = new(ir) ir_dereference_variable(new_var);
      ir->insert_after(new(ir) ir_return(deref));
   }

   return visit_continue;
}