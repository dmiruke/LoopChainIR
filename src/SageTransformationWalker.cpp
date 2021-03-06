#include <iostream>
#include <cstdio>
#include <vector>
#include <numeric>
#include <string>
#include <functional>
#include <set>
#include <deque>

#include <LoopChainIR/util.hpp>
#include <LoopChainIR/SageTransformationWalker.hpp>
#include <LoopChainIR/util.hpp>

using namespace std;
using namespace SageBuilder;
using namespace SageInterface;
using namespace LoopChainIR;

function_call_info::function_call_info( SgExprStatement* expr_node, SgName name, vector<SgExpression*>& parameter_expressions )
: expr_node(expr_node), name(name), parameter_expressions(parameter_expressions)
{ }

SageTransformationWalker::SageTransformationWalker( isl_ast_node* isl_root, SgScopeStatement* injection_site )
: SageTransformationWalker(isl_root, injection_site, false)
{ }

SageTransformationWalker::SageTransformationWalker( isl_ast_node* isl_root, SgScopeStatement* injection_site, bool verbose )
: depth( -1 ), verbose( verbose ), scope_stack(), isl_root( isl_root ), statement_macros(), injection_site( injection_site ), global( getGlobalScope(injection_site) ) {
  if( verbose ){
    cout << "Injection site: "<< static_cast<void*>( injection_site ) << endl
         << "Global: " << static_cast<void*>( this->get_global() ) << endl;
  }

  // Form the initial scope stack in bottom-up order (in reverse order they will appear on the stack)
  // Start with injection site, as it is inner most
  this->push_bottom( injection_site );
  // As long as we havent already encountered the global scope, push next enclosing scope
  while( this->bottom() != this->get_global() ){
    this->push_bottom( getEnclosingScope( this->bottom() ) );
    if( verbose ) cout << "Pushing scope " << static_cast<void*>( this->bottom() ) << endl;
  }

  assert( this->top() == injection_site );
  assert( this->bottom() == this->get_global() );

  SgStatement* result = isSgStatement( this->visit( this->isl_root ) );
  assert( result != NULL );

  appendStatement( result, injection_site );

  while( ! this->empty() ){
    this->pop();
  }

}

bool SageTransformationWalker::empty(){
  return this->scope_stack.empty();
}

SgScopeStatement* SageTransformationWalker::top(){
  return this->scope_stack.back();
}

SgScopeStatement* SageTransformationWalker::bottom(){
  return this->scope_stack.front();
}

SgScopeStatement* SageTransformationWalker::pop(){
  return this->pop_top();
}

SgScopeStatement* SageTransformationWalker::pop_top(){
  SgScopeStatement* ret = this->top();
  this->scope_stack.pop_back();
  return ret;
}

SgScopeStatement* SageTransformationWalker::pop_bottom(){
  SgScopeStatement* ret = this->bottom();
  this->scope_stack.pop_front();
  return ret;
}

SgScopeStatement* SageTransformationWalker::get_global(){
  return this->global;
}

void SageTransformationWalker::push( SgScopeStatement* scope ){
  this->push_top( scope );
}

void SageTransformationWalker::push_top( SgScopeStatement* scope ){
  this->scope_stack.push_back( scope );
}

void SageTransformationWalker::push_bottom( SgScopeStatement* scope ){
  this->scope_stack.push_front( scope );
}

SgVariableSymbol* SageTransformationWalker::get_symbol( std::string symbol_name ){
  SgVariableSymbol* symbol = this->symbol_maps[symbol_name];

  if( verbose ){
    std::cout << std::string(this->depth*2, ' ') << "map[" << symbol_name << "] ---> " << static_cast<void*>(symbol) << endl;
  }

  return symbol;
}

void SageTransformationWalker::set_symbol( std::string symbol_name, SgVariableSymbol* symbol ){
  this->symbol_maps[symbol_name] = symbol;
  if( this->verbose ){
    std::cout << std::string(this->depth*2, ' ') << "map[" << symbol_name << "] <--- " << static_cast<void*>(this->symbol_maps[symbol_name]) << endl;
  }
}

vector<function_call_info*>* SageTransformationWalker::getStatementMacroNodes(){
  return &(this->statement_macros);
}

SgScopeStatement* SageTransformationWalker::getInjectionRoot(){
  return this->injection_site;
}


SgNode* SageTransformationWalker::visit( isl_ast_expr* node ){
  this->depth += 1;
  SgNode* result = NULL;

  switch( isl_ast_expr_get_type(node) ){
    case isl_ast_expr_error:
      result = this->visit_expr_error( node );
      break;

    case isl_ast_expr_op:
      result = this->visit_expr_op( node );
      break;

    case isl_ast_expr_id:
      result = this->visit_expr_id( node );
      break;

    case isl_ast_expr_int:
      result = this->visit_expr_int( node );
      break;

    default:
      result = this->visit_expr_unknown( node );
      break;
  }

  this->depth -= 1;
  return result;
}

SgNode* SageTransformationWalker::visit( isl_ast_node* node ){
  this->depth += 1;
  SgNode* result = NULL;

  switch( isl_ast_node_get_type(node) ){
    case isl_ast_node_error:
      result = this->visit_node_error( node );
      break;

    case isl_ast_node_for:
      result = this->visit_node_for( node );
      break;

    case isl_ast_node_if:
      result = this->visit_node_if( node );
      break;

    case isl_ast_node_block:
      result = this->visit_node_block( node );
      break;

    case isl_ast_node_mark:
      result = this->visit_node_mark( node );
      break;

    case isl_ast_node_user:
      result = this->visit_node_user( node );
      break;

    default:
      result = this->visit_node_unknown( node );
      break;
  }

  this->depth -= 1;
  return result;
}

SgNode* SageTransformationWalker::visit_expr_op(isl_ast_expr* node){
  this->depth += 1;
  SgNode* result = NULL;

  switch( isl_ast_expr_get_op_type(node) ){
    case isl_ast_op_error:
      result = this->visit_op_error( node );
      break;

    case isl_ast_op_and:
      result = this->visit_op_and( node );
      break;

    case isl_ast_op_and_then:
      result = this->visit_op_and_then( node );
      break;

    case isl_ast_op_or:
      result = this->visit_op_or( node );
      break;

    case isl_ast_op_or_else:
      result = this->visit_op_or_else( node );
      break;

    case isl_ast_op_max:
      result = this->visit_op_max( node );
      break;

    case isl_ast_op_min:
      result = this->visit_op_min( node );
      break;

    case isl_ast_op_minus:
      result = this->visit_op_minus( node );
      break;

    case isl_ast_op_add:
      result = this->visit_op_add( node );
      break;

    case isl_ast_op_sub:
      result = this->visit_op_sub( node );
      break;

    case isl_ast_op_mul:
      result = this->visit_op_mul( node );
      break;

    case isl_ast_op_div:
      result = this->visit_op_div( node );
      break;

    case isl_ast_op_fdiv_q:
      result = this->visit_op_fdiv_q( node );
      break;

    case isl_ast_op_pdiv_q:
      result = this->visit_op_pdiv_q( node );
      break;

    case isl_ast_op_pdiv_r:
      result = this->visit_op_pdiv_r( node );
      break;

    case isl_ast_op_zdiv_r:
      result = this->visit_op_zdiv_r( node );
      break;

    case isl_ast_op_cond:
      result = this->visit_op_cond( node );
      break;

    case isl_ast_op_select:
      result = this->visit_op_select( node );
      break;

    case isl_ast_op_eq:
      result = this->visit_op_eq( node );
      break;

    case isl_ast_op_le:
      result = this->visit_op_le( node );
      break;

    case isl_ast_op_lt:
      result = this->visit_op_lt( node );
      break;

    case isl_ast_op_ge:
      result = this->visit_op_ge( node );
      break;

    case isl_ast_op_gt:
      result = this->visit_op_gt( node );
      break;

    case isl_ast_op_call:
      result = this->visit_op_call( node );
      break;

    case isl_ast_op_access:
      result = this->visit_op_access( node );
      break;

    case isl_ast_op_member:
      result = this->visit_op_member( node );
      break;

    case isl_ast_op_address_of:
      result = this->visit_op_address_of( node );
      break;

    default:
      result = this->visit_op_unknown( node );
      break;
  }

  this->depth -= 1;
  return result;
}


list<SgNode*> SageTransformationWalker::visit_expr_operands(isl_ast_expr* node ){
  list<SgNode*> operands_list;

  for( int i = 0; i < isl_ast_expr_get_op_n_arg(node); i += 1 ){
    operands_list.push_back( this->visit_op_operand( node, i ) );
  }

  return operands_list;
}

SgExpression* SageTransformationWalker::visit_op_operand( isl_ast_expr* node, int pos ){
  assertWithException( isl_ast_expr_get_op_n_arg(node) > pos, SSTR( "Invalid isl_ast_expr_op position " << pos << " > " << isl_ast_expr_get_op_n_arg(node) << " (isl_ast_expr_get_op_n_arg(node))") );
  assertWithException( pos >= 0, SSTR("Invalid isl_ast_expr_op position " << pos << " < 0.") );
  SgExpression* sg_expr = isSgExpression( this->visit( isl_ast_expr_get_op_arg( node, pos ) ) );

  assertWithException( sg_expr != NULL, "Could not build SgExpression from ISL op expression." );

  return sg_expr;
}

SgExpression* SageTransformationWalker::visit_op_lhs( isl_ast_expr* node ){
  return visit_op_operand(node, 0);
}

SgExpression* SageTransformationWalker::visit_op_rhs( isl_ast_expr* node ){
  return visit_op_operand(node, 1);
}

SgExpression* SageTransformationWalker::visit_op_unary_operand( isl_ast_expr* node ){
  return visit_op_operand(node, 0);
}

SgExpression* SageTransformationWalker::visit_op_cond_condition_operand( isl_ast_expr* node ){
  return visit_op_operand(node, 0);
}

SgExpression* SageTransformationWalker::visit_op_cond_then_operand( isl_ast_expr* node ){
  return visit_op_operand(node, 1);
}

SgExpression* SageTransformationWalker::visit_op_cond_else_operand( isl_ast_expr* node ){
  return visit_op_operand(node, 2);
}


SgExpression* SageTransformationWalker::visit_op_error(isl_ast_expr* node __attribute__((unused))){
  assertWithException( this->ENCOUNTERED_ERROR_NODE, "An error node has been encoutered during SageTransformationWalker." );
  return NULL;
}

SgExpression*  SageTransformationWalker::visit_op_and( isl_ast_expr* node ){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2, "ISL 'and' operation does not have exactly two operands." );

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgAndOp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation and @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression*  SageTransformationWalker::visit_op_and_then(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2, "ISL 'and' operation does not have exactly two operands." );

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgAndOp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation and_then @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_or(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2, "ISL 'or' operation does not have exactly two operands." );

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgOrOp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation or @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_or_else(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2, "ISL 'or' operation does not have exactly two operands." );

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgOrOp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation or_else @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_max(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) >= 2, "ISL 'max' operation does not have two or more operands." );

  int last = isl_ast_expr_get_op_n_arg(node)-1;

  SgName name( "max" );

  vector<SgExpression*> parameter_expressions;
  SgExpression* head = this->visit_op_operand( node, last );

  for( int i = last-1; i >= 0; i -= 1 ){
    parameter_expressions.clear();
    parameter_expressions.push_back( head );
    parameter_expressions.push_back( this->visit_op_operand(node, i) );

    SgExprListExp* parameters = buildExprListExp( parameter_expressions );

    head = buildFunctionCallExp( name, buildIntType(), parameters, this->get_global() );

    if( this->verbose ){
      cout << string(this->depth*2, ' ') << "max @ " << static_cast<void*>(head) << endl;
    }
  }

  return head;
}

SgExpression* SageTransformationWalker::visit_op_min(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) >= 2, "ISL 'min' operation does not have two or more operands." );

  int last = isl_ast_expr_get_op_n_arg(node)-1;

  SgName name( "min" );

  vector<SgExpression*> parameter_expressions;
  SgExpression* head = this->visit_op_operand( node, last );

  for( int i = last-1; i >= 0; i -= 1 ){
    parameter_expressions.clear();
    parameter_expressions.push_back( head );
    parameter_expressions.push_back( this->visit_op_operand(node, i) );

    SgExprListExp* parameters = buildExprListExp( parameter_expressions );

    head = buildFunctionCallExp( name, buildIntType(), parameters, this->get_global() );

    if( this->verbose ){
      cout << string(this->depth*2, ' ') << "min @ " << static_cast<void*>(head) << endl;
    }
  }

  return head;
}

// Unary minus
SgExpression* SageTransformationWalker::visit_op_minus(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 1, "ISL 'unary minus' operation does not have exactly one operand.");

  // Get child node
  SgExpression* arg = this->visit_op_unary_operand( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildUnaryExpression<SgMinusOp>( arg );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation minus @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_add(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2," ISL 'add' operation does not have exactly two operands." );

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgAddOp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation add @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_sub(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2, "ISL 'unary minus' operation does not have exactly two operand." );

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgSubtractOp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation sub @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_mul(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2, "ISL 'multiplcation' operation does not have exactly two operand." );

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgMultiplyOp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation mul @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_div(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2, "ISL 'divide' operation does not have exactly two operand." );

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgDivideOp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation div @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_fdiv_q(isl_ast_expr* node){
  /*
  Round towards -infty
  Typically been #define floord(x, y) ( ((((x)%(y))>=0) ? ((x)/(y)) : (((x)/(y))) -1 ) )
  */
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2, "ISL 'fdivq' operation does not have exactly two operand." );

  SgExpression* x = this->visit_op_lhs( node );
  SgExpression* y = this->visit_op_rhs( node );

  SgExpression* exp = buildConditionalExp(
    // x % y >=0
    buildBinaryExpression<SgGreaterOrEqualOp>(
      buildBinaryExpression<SgModOp>( x, y ),
      buildIntVal( 0 )
    ),
    // x / y
    buildBinaryExpression<SgDivideOp>( x, y ),
    // (x / y) - 1
    buildBinaryExpression<SgSubtractOp>(
       buildBinaryExpression<SgDivideOp>( x, y ),
       buildIntVal( 1 )
    )
  );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation fdiv_q @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_pdiv_q(isl_ast_expr* node){
  /* Dividend is non-negative */
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2, "ISL 'pdivq' operation does not have exactly two operand." );

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgDivideOp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation pdiv_q @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_pdiv_r(isl_ast_expr* node){
  /* Dividend is non-negative */
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2, "ISL 'pdivr' operation does not have exactly two operand." );

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgModOp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation pdiv_r @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_zdiv_r(isl_ast_expr* node){
  /* Result only compared against zero */
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2, "ISL 'zdivr' operation does not have exactly two operand." );

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgModOp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation zdiv_r @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_cond(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 3, "ISL 'ternary' operation does not have exactly three operand." );

  SgExpression* condition = this->visit_op_cond_condition_operand( node );
  SgExpression* then_exp = this->visit_op_cond_then_operand( node );
  SgExpression* else_exp = this->visit_op_cond_else_operand( node );

  SgExpression* exp = buildConditionalExp( condition, then_exp, else_exp );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation cond @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_select(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 3, "ISL 'select' operation does not have exactly three operand." );

  SgExpression* condition = this->visit_op_cond_condition_operand( node );
  SgExpression* then_exp = this->visit_op_cond_then_operand( node );
  SgExpression* else_exp = this->visit_op_cond_else_operand( node );

  SgExpression* exp = buildConditionalExp( condition, then_exp, else_exp );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation select @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_eq(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2, "ISL 'equal-to' operation does not have exactly two operands.");

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgEqualityOp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation eq @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_le(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2, "ISL '<=' operation does not have exactly two operands." );

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgLessOrEqualOp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation le @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_lt(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2, "ISL '<' operation does not have exactly two operands." );

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgLessThanOp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation lt @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_ge(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2,  "ISL '>=' operation does not have exactly two operands." );

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgGreaterOrEqualOp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation ge @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_gt(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2,  "ISL '>' operation does not have exactly two operands."  );

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgGreaterThanOp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation gt @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExprStatement* SageTransformationWalker::visit_op_call(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) >= 1,  "ISL 'call' operation does not have a function name." );
  // Get function name
  SgName name( isl_id_get_name( isl_ast_expr_get_id( isl_ast_expr_get_op_arg( node, 0 ) ) ) );

  // Build parameters list
  vector<SgExpression*> parameter_expressions;
  for( int i = 1; i < isl_ast_expr_get_op_n_arg(node); i += 1 ){
    SgExpression* as_exp = this->visit_op_operand(node, i);

    if( this->verbose ){
      cout << string(this->depth*2, ' ') << "exp @ " << static_cast<void*>(as_exp) << endl;
    }

    parameter_expressions.push_back( as_exp );
  }

  SgExprListExp* parameters = buildExprListExp( parameter_expressions );

  // Build call
  SgExprStatement* call = buildFunctionCallStmt( name, buildVoidType(), parameters, this->get_global() );
  function_call_info* info = new function_call_info( call, name, parameter_expressions);
  statement_macros.push_back( info );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Call @ " << static_cast<void*>(call) << endl;
  }

  return call;
}

SgExpression* SageTransformationWalker::visit_op_access(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) >= 2,  "ISL 'access' operation does not have exactly two operands."  );

  // Head will be the running result of an access
  // Begins as a non access expression; as the root array expressions
  SgExpression* head = this->visit_op_operand(node, 0);

  // Append accesses
  for( int i = 1; i < isl_ast_expr_get_op_n_arg(node); i += 1 ){
    // Create new access expression from head and the i'th operand
    // Replace head with the new access expression
    head = buildBinaryExpression<SgPntrArrRefExp>( head, this->visit_op_operand(node, i) );

    if( this->verbose ){
      cout << string(this->depth*2, ' ') << "access @ " << static_cast<void*>(head) << endl;
    }
  }

  return head;
}

SgExpression* SageTransformationWalker::visit_op_member(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 2,  "ISL 'member access' operation does not have exactly two operands."  );

  // Get children nodes
  SgExpression* lhs = this->visit_op_lhs( node );
  SgExpression* rhs = this->visit_op_rhs( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildBinaryExpression<SgDotExp>( lhs, rhs );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation gt @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_address_of(isl_ast_expr* node){
  assertWithException( isl_ast_expr_get_op_n_arg(node) == 1,  "ISL 'address of' operation does not have exactly two operands."  );

  // Get child node
  SgExpression* arg = this->visit_op_unary_operand( node );

  // Construct using templated buildBinaryExpression
  SgExpression* exp = buildUnaryExpression<SgAddressOfOp>( arg );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Operation address_of @ " << static_cast<void*>(exp) << endl;
  }

  return exp;
}

SgExpression* SageTransformationWalker::visit_op_unknown(isl_ast_expr* node __attribute__((unused)) ){
  assertWithException( this->ENCOUNTERED_UNKOWN_NODE, "A node of undetermined type has been visited during SageTransformationWalker." );
  return NULL;
}


SgVarRefExp* SageTransformationWalker::visit_expr_id(isl_ast_expr* node){
  SgName name( isl_id_get_name( isl_ast_expr_get_id(node) ) );
  SgVariableSymbol* symbol = get_symbol( name.getString() );

  // Get symbol from parent scope
  if( symbol == NULL ){
    symbol = lookupVariableSymbolInParentScopes( name, this->injection_site ) ;
    assertWithException( symbol != NULL, SSTR( "Could not find symbol \"" <<  name << "\" in parent scope." ) );
    this->set_symbol( name.getString(), symbol );
  }

  SgVarRefExp* var_ref = buildVarRefExp( symbol );

  if( this->verbose ){
    cout << string((this->depth+1)*2, ' ') << "var symbol @ " << static_cast<void*>(symbol) << endl;
    cout << string((this->depth+1)*2, ' ') << "var symbol name: " << symbol->get_name().getString() << endl;
    cout << string(this->depth*2, ' ') << "id: " << name.getString() << " @ " << var_ref << endl;
  }

  return var_ref;
}

SgIntVal* SageTransformationWalker::visit_expr_int(isl_ast_expr* node){
  isl_val* isl_value = isl_ast_expr_get_val( node );
  long num = isl_val_get_num_si( isl_value );
  long den = isl_val_get_den_si( isl_value );
  assert( den == 1 );

  int int_value = (int) num;
  assertWithException( ((long) int_value) == num, SSTR("Cast produced invalid number.") );

  SgIntVal* value = buildIntVal( int_value );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "int: " << int_value << " @ " << static_cast<void*>(value) << endl;
  }

  return value;
}

SgNode* SageTransformationWalker::visit_expr_unknown(isl_ast_expr* node __attribute__((unused))){
  assertWithException( this->ENCOUNTERED_UNKOWN_NODE, "A node of undetermined type has been visited during SageTransformationWalker." );
  return NULL;
}

SgNode* SageTransformationWalker::visit_expr_error(isl_ast_expr* node __attribute__((unused))){
  assertWithException( this->ENCOUNTERED_ERROR_NODE, "An error node has been encoutered during SageTransformationWalker." );
  return NULL;
}


SgNode* SageTransformationWalker::visit_node_error(isl_ast_node* node __attribute__((unused))){
  assertWithException( this->ENCOUNTERED_ERROR_NODE, "An error node has been encoutered during SageTransformationWalker." );
  return NULL;
}

SgNode* SageTransformationWalker::visit_node_for(isl_ast_node* node){
  this->depth += 1;
  // Build inititialization statement
  SgStatement* initialization = NULL;
  SgName* name = NULL;
  {
    // Get iterator symbol
    string isl_name = string( isl_id_get_name( isl_ast_expr_get_id( isl_ast_node_for_get_iterator(node) ) ) );
    name = new SgName( isl_name );

    // Get initialization expression
    SgExpression* init_exp = isSgExpression( this->visit( isl_ast_node_for_get_init( node ) ) );

    assertWithException( init_exp != NULL, "Could not create for initialization statement." );

    // Build variable declaration
    SgAssignInitializer* initalizer = buildAssignInitializer( init_exp, buildIntType() );
    SgVariableDeclaration* var_decl = buildVariableDeclaration( *name, buildIntType(), initalizer, this->top() );

    SgVariableSymbol* symbol = SageInterface::getFirstVarSym(var_decl);
    this->set_symbol( isl_name,  symbol );

    // Building the variable decl seems sufficient.
    initialization = var_decl;
    if( this->verbose ){
      cout << string((this->depth+2)*2, ' ') << "initalizer @ " << static_cast<void*>(initalizer) << endl;
      cout << string((this->depth+1)*2, ' ') << "var_decl: " << name->getString() << " @ " << static_cast<void*>(var_decl) << endl;
      cout << string((this->depth+2)*2, ' ') << "var symbol @ " << static_cast<void*>(symbol) << endl;
      cout << string((this->depth+2)*2, ' ') << "var symbol name: " << symbol->get_name().getString() << endl;
      cout << string(this->depth*2, ' ') << "init @ " << static_cast<void*>(initialization) << endl;
    }
  }

  // Get condition expression statment
  SgExprStatement* condition = NULL;
  {
    SgExpression* as_exp = isSgExpression( this->visit( isl_ast_node_for_get_cond( node) ) );

    assertWithException( as_exp != NULL, "Could not create for conditional expression.");

    condition = buildExprStatement( as_exp );
    if( this->verbose ){
      cout << string(this->depth*2, ' ') << "cond @ " << static_cast<void*>(condition) << endl;
    }
  }

  // Get increment expression statement
  SgExpression* increment = NULL;
  {

    SgVarRefExp* var_ref = buildVarRefExp( buildInitializedName( *name, buildIntType() ), this->top() );

    // string tab((this->depth+1)*2, ' ');
    // if( this->verbose ) cout << string(this->depth*2, ' ') << "var_ref @ " << static_cast<void*>(var_ref) << endl;
    SgExpression* increment_exp = isSgExpression( this->visit( isl_ast_node_for_get_inc( node ) ) );

    assertWithException( increment_exp != NULL, "Could not create for increment expression." );

    if( this->verbose ){
      cout << string(this->depth*2, ' ') << "exp @ " << static_cast<void*>(increment_exp) << endl;
    }

    // Create increment expression: iterator = iterator + (increment_exp)
    increment = buildBinaryExpression<SgAssignOp>( copyExpression(var_ref), buildBinaryExpression<SgAddOp>( var_ref, increment_exp ) );

    if( this->verbose ){
      cout << string(this->depth*2, ' ') << "increment @ " << static_cast<void*>(increment) << endl;
    }
  }

  // TODO this is an embarrasment.
  // Create a parent for the for statement
  SgBasicBlock* dumb_wrapper = buildBasicBlock();
  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "basic block @ " << static_cast<void*>(dumb_wrapper) << endl;
  }
  this->push( dumb_wrapper );

  // TODO This is a hack.
  // Need to create empty body for for statement,
  // because we *must* make for statement before the body code.
  SgBasicBlock* body = buildBasicBlock();

  // Construct for loop node
  SgForStatement* for_stmt = buildForStatement( initialization, condition, increment, body );
  appendStatement( for_stmt, dumb_wrapper );
  {
    this->push( for_stmt );
    this->push( isSgScopeStatement( getLoopBody( for_stmt ) ) );
    SgStatement* sg_stmt = isSgStatement( this->visit( isl_ast_node_for_get_body( node ) ) );
    this->pop(); // pop body
    this->pop(); // pop for stmt

    assertWithException( sg_stmt != NULL, "Could not create for loop body." );

    if( isSgBasicBlock(sg_stmt) ){
      body = isSgBasicBlock( sg_stmt );
    } else {
      body = isSgBasicBlock( getLoopBody( for_stmt ) );

      assertWithException( body != NULL, "Could not create for scoped body." );

      appendStatement( sg_stmt, body );
    }
    setLoopBody( for_stmt, body );
  }


  // Check for parallel annotation
  {
    isl_id* maybe_annotation = isl_ast_node_get_annotation( node );
    if( maybe_annotation != NULL ){
      if( string( isl_id_get_name( maybe_annotation ) ) == string("parallel annotation") ){
        // TODO make work with OmpSupport tools
        // This does not currently add an annotation.
        //OmpSupport::OmpAttribute* pragma = OmpSupport::buildOmpAttribute( OmpSupport::e_parallel_for, for_stmt, false );
        //OmpSupport::addOmpAttribute( pragma, for_stmt );
        //OmpSupport::generatePragmaFromOmpAttribute( for_stmt );

        // Manually create prama with literal "omp parallel for" invocation.
        SgPragmaDeclaration* prama_decl = buildPragmaDeclaration( string("omp parallel for"), this->top() );
        SageInterface::insertStatement( for_stmt, prama_decl );
        if( this->verbose ){
          cout << string(this->depth*2, ' ') << "pragma declaration @ " << static_cast<void*>( prama_decl ) << endl;
        }
      }
    }
  }

  this->pop(); // pop dumb_wrapper

  this->depth -= 1;
  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "for @ " << static_cast<void*>(for_stmt) << endl;
  }

  return dumb_wrapper;
}

SgNode* SageTransformationWalker::visit_node_if(isl_ast_node* node){
  SgExpression* condition_node = isSgExpression( this->visit( isl_ast_node_if_get_cond(node) ) );
  assertWithException( condition_node != NULL, "Could not create conditional expression." );

  SgStatement* then_node = isSgStatement( this->visit( isl_ast_node_if_get_then(node) ) );
  assertWithException( then_node != NULL, "Could not create then statement.");

  // Always wrap statements as blocks
  if( !isSgBasicBlock(then_node) ){
    SgBasicBlock* block = buildBasicBlock( then_node );
    then_node = block;
  }

  SgStatement* else_node = NULL;
  if( isl_ast_node_if_has_else( node ) ){
    else_node = isSgStatement( this->visit( isl_ast_node_if_get_else(node) ) );
    assertWithException( else_node != NULL, "Could not create else statement." );

    if( isSgBasicBlock( else_node) ){
      SgBasicBlock* block = buildBasicBlock( else_node );
      else_node = block;
    }
  }

  SgIfStmt* if_stmt = buildIfStmt( condition_node, then_node, else_node );

  return if_stmt;
}

SgNode* SageTransformationWalker::visit_node_block(isl_ast_node* node){
  SgBasicBlock* block = buildBasicBlock();
  assertWithException( block != NULL, "Could not create empty basic block." );
  this->push( block );

  if( this->verbose ){
    cout << string(this->depth*2, ' ') << "Block @ "<< static_cast<void*>(block) << endl;
  }

  isl_ast_node_list* list = isl_ast_node_block_get_children(node);
  for( int i = 0; i < isl_ast_node_list_n_ast_node(list); i += 1 ){
    isl_ast_node* node = isl_ast_node_list_get_ast_node(list, i);
    SgStatement* sg_stmt = isSgStatement( this->visit( node ) );
    assertWithException( sg_stmt != NULL, "Could not create statement of block." );

    appendStatement( sg_stmt, block );
  }

  this->pop();

  return block;
}

SgNode* SageTransformationWalker::visit_node_mark(isl_ast_node* node __attribute__((unused)) ){
  assertWithException( this->VISIT_TO_NODE_NOT_IMPLEMENTED, "SageTransformationWalker::visit_node_mark Not implemented." );
  return NULL;
}

SgNode* SageTransformationWalker::visit_node_user(isl_ast_node* node){
  return this->visit( isl_ast_node_user_get_expr(node) );
}

SgNode* SageTransformationWalker::visit_node_unknown(isl_ast_node* node __attribute__((unused)) ){
  assertWithException( this->ENCOUNTERED_UNKOWN_NODE, "A node of undetermined type has been visited during SageTransformationWalker." );
  return NULL;
}
