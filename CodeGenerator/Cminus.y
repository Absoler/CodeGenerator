%right '=' '+=' '-=' '*=' '/='
%left '==' '!='
%left '<' '>' '<=' '>='
%left '<<' '>>'
%left '||'
%left '&&'
%left '|'
%left '&' '^'
%left '+' '-'
%left '*' '/' '%'
%right '$'
%right '!'
%right '~'
%left 'else'
%right '-'
%left '++'
%left '['
%%

S : program {}
  ;
program : globalData stmts {}
	| stmts	{}
        ;
globalData : globalStmts  {
		}
	   ;
globalStmts : globalStmts globalStmt {}
	    | globalStmt {}
	    ;
globalStmt : 'static' var_decl ';' {}
	    ;
stmts	: stmts	M stmt {
			backpatch($1.nextlist, $2.instr);
			$$.nextlist = $3.nextlist;
			}
	| stmt {
			$$.nextlist = $1.nextlist;
			}
	;
stmt	: '{' stmts  '}' {
			$$.nextlist = $2.nextlist;
			}
	| fun_define  {
			returnToGlobalTable();
			}
	| if_stmt {
			$$.nextlist = $1.nextlist;
			}
	| while_stmt {
			$$.nextlist = $1.nextlist;
			}
	| var_decl ';' {
			$$.nextlist = $1.nextlist;
			}			
	| expr_stmt ';' {
			}
	| 'return' expr ';' {
			emit("return",$2.place,"","");
			setOutLiveVar($2.place);
			}
	| 'return' ';' {
			emit("return","","","");
			}
	;
fun_define   : fun_decl_head BlockLeader '{' stmts '}' {
						$$.name = $1.name;
						}
	     ;
fun_decl_head : type_spec 'id' '(' ')' {
						$$.name = $2.lexeme;
						createSymbolTable($2.lexeme, $1.width);
						addFunLabel(nextInstr, $2.lexeme);
						}
	      | type_spec 'id' '(' param_list ')' {
						$$.name = $2.lexeme;
						createSymbolTable($2.lexeme, $1.width);
						addToSymbolTable($4.itemlist);
						addFunLabel(nextInstr, $2.lexeme);
						}
	       ;
param_list : param_list ',' param {
                $$.itemlist = $1.itemlist ||  $3.itemlist;
                }
	  | param {
                $$.itemlist = $1.itemlist;
                }
	;
param : type_spec 'id' {
		        $$.itemlist = makeParam($2.lexeme,$1.type,$1.width);
				}
	  | type_spec 'id' '[' int_literal ']' {
				$$.itemlist = makeParam($2.lexeme,Array($4.lexval,$1.type),$4.lexval * $1.width);
                }
	;
int_literal : 'num' {
					$$.lexval = $1.lexeme;
					}
			;
static_var_decl : 'static' var_decl {
					//$$.code = $2.code;
					}
	;
var_decl   : type_spec 'id' {
				enter($2.lexeme,$1.type,$1.width);
				}
			| type_spec 'id' '[' int_literal ']' {
				enter($2.lexeme,Array($4.lexval,$1.type),$4.lexval * $1.width);
				}
			;
type_spec  : 'int' {
				$$.type = "int";
				$$.width = "4";
				}
			| 'void' {
				$$.type = "void";
				$$.width = "0";
				}
			;
expr_stmt : 'id' '=' expr {
				p = lookupPlace($1.lexeme);
				if (p.empty()) error();
				emit("=",$3.place,"",p);
                }
		   | addr_id '=' expr { 
				emit("$=",$3.place,"",$1.place);
		   }
		    ;
expr : expr '+' expr {
                $$.place = newtemp($1.place);
				emit("ADD", $1.place, $3.place, $$.place);
                }
	 | expr '-' expr {
                $$.place = newtemp($1.place);
				emit("SUB", $1.place, $3.place, $$.place);
                }
	 | expr '*' expr {
                $$.place = newtemp($1.place);
				emit("MUL", $1.place, $3.place, $$.place);
                }
	 | expr '/' expr {
                $$.place = newtemp($1.place);
				emit("DIV", $1.place, $3.place, $$.place);
                }
	 | expr '&' expr {
                $$.place = newtemp($1.place);
				emit("AND", $1.place, $3.place, $$.place);
				
				}
	 | expr '|' expr {
                $$.place = newtemp($1.place);
				emit("OR", $1.place, $3.place, $$.place);
				
                }
	 | expr '^' expr {
                $$.place = newtemp($1.place);
				emit("XOR", $1.place, $3.place, $$.place);
				
                }
	 | '~' expr {
                $$.place = newtemp($2.place);
				emit("NOT", $2.place,"", $$.place);
				
                }
	 | '-' expr {
                $$.place = newtemp($2.place);
				emit("NEG", $2.place,"", $$.place);
				
                }
	 | '++' 'id' {
                $$.place = newtemp($2.place);
				emit("ACC", $2.place,"", $$.place);
				
                }
	 | expr '<<' expr {
                $$.place = newtemp($1.place);
				emit("SLL", $1.place, $3.place, $$.place);
                }
	 | expr '>>' expr {
                $$.place = newtemp($1.place);
				emit("SRL", $1.place, $3.place, $$.place);
                }
	 | '(' expr ')' {
                $$.place = $2.place;
                }

	 | 'id'   {
				$$.place = lookupPlace($1.lexeme);
            }
	 | addr_id { 
				$$.place = newtemp($1.place);
				emit("=$",$1.place,"",$$.place);
	 }
	 | 'id' '(' arg_list ')' {
		p = gen(paramStack.size());
		while (!paramStack.empty()) {
			emit("param", paramStack.top(),"","");
			paramStack.pop();
		}
		emit("call", p, $1.lexeme,"");
		enter("#return","int",4);
		$$.place = newtemp("#return");
		emit("=","#return","",$$.place);
		//$$.place = "return#";
	   }
	 | 'num' {
		$$.place = addNum($1.lexeme);
	   }
	;
arg_list  : arg_list ',' expr {
				paramStack.push($3.place);
				}
		  |  expr  {
				paramStack.push($1.place);
				}
		  | {
				//paramStack.clear();
		  }
	;
if_stmt : 'if' '(' logic_expr ')' M stmt {
		backpatch($3.truelist, $5.instr);
		$$.nextlist = merge($3.falselist, $6.nextlist);
		}
	| 'if' '(' logic_expr ')' M stmt N 'else' M stmt {
		backpatch($3.truelist, $5.instr);
		backpatch($3.falselist, $9.instr);
		$$.nextlist = merge(merge($6.nextlist, $7.instr), $10.nextlist);
		}
	;
while_stmt : 'while' M '(' logic_expr ')' M stmt {
		backpatch($7.nextlist, $2.instr);
		backpatch($4.truelist, $6.instr);
		$$.nextlist = $4.falselist;
		emit("j","","",$2.instr);
		}
	;
logic_expr	: logic_expr '&&' M logic_expr {
				backpatch($1.truelist, $3.instr);
				$$.truelist = $4.truelist;
				$$.falselist = merge($1.falselist, $4.falselist);
			}	
		| logic_expr '||' M logic_expr {
				backpatch($1.falselist, $3.instr);
				$$.truelist = merge($1.truelist, $4.truelist);
				$$.falselist = $4.falselist;
			}
		| '!' logic_expr {
				$$.truelist = $2.falselist;
				$$.falselist = $2.truelist;
			}
		| '(' logic_expr ')' {
				$$.truelist = $2.truelist;
				$$.falselist = $2.falselist;
			}
		| expr rel expr {
				$$.truelist = makelist(nextInstr);
				$$.falselist = makelist(nextInstr+1);
				emit("j"+$2.op, $1.place, $3.place, "_");
				emit("j","","","_");
			}
		| expr { 
				$$.truelist = makelist(nextInstr);
				$$.falselist = makelist(nextInstr+1);
				emit("j!=", $1.addr, "0", "_");
				emit("j","","","_");
			}
		| 'true' {
				$$.truelist = makelist(nextInstr);
				emit("j","","","_");
			}
		| 'false' {
				$$.falselist = makelist(nextInstr);
				emit("j","","","_");
			}
		;
M : {   $$.instr = "LABEL_" + gen(nextInstr);
		labelMap.insert(make_pair("LABEL_" + gen(nextInstr), nextInstr));
		}
  ;
N : { $$.instr = makelist(nextInstr);
      emit("j","","","_");
    }
  ;
BlockLeader : {
      addLeader(nextInstr);
    }
  ;
rel : '<' {$$.op = "<";}
    | '>' {$$.op = ">";}
    | '<='{$$.op = "<=";}
    | '<='{$$.op = ">=";}
    | '=='{$$.op = "==";}
    | '!='{$$.op = "!=";}
    ;
addr_id : '$' expr {
		$$.place=newtemp($2.place,true);
		emit("=",$2.place,"",$$.place);
		}
		|'id' '[' expr ']' { 
		$$.place=newtemp($3.place);
		emit("MUL",$3.place,to_string($3.width),$$.place);
		p = lookupPlace($1.lexeme);
		emit("ADD",p,$$.place,$$.place);
		}
	;