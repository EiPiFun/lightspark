/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009  Alessandro Pignotti (a.pignotti@sssup.it)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#include "abc.h"

using namespace std;

uintptr_t ABCVm::bitAnd(ISWFObject* val2, ISWFObject* val1)
{
	uintptr_t i1=val1->toInt();
	uintptr_t i2=val2->toInt();
	val1->decRef();
	val2->decRef();
	LOG(CALLS,"bitAnd_oo " << hex << i1 << '&' << i2);
	return i1&i2;
}

uintptr_t ABCVm::increment(ISWFObject* o)
{
	LOG(CALLS,"increment");

	int n=o->toInt();
	o->decRef();
	return n+1;
}

uintptr_t ABCVm::bitAnd_oi(ISWFObject* val1, intptr_t val2)
{
	uintptr_t i1=val1->toInt();
	uintptr_t i2=val2;
	val1->decRef();
	static int c=0;
	if(c%(1024*256)==0)
		cout << "bitand " << c << endl;
	c++;
	LOG(CALLS,"bitAnd_oi " << hex << i1 << '&' << i2);
	return i1&i2;
}

void ABCVm::setProperty(ISWFObject* value,ISWFObject* obj,multiname* name)
{
	LOG(CALLS,"setProperty " << *name);

	//Check to see if a proper setter method is available
	ISWFObject* owner;
	obj->setVariableByMultiname(*name,value);
	obj->decRef();
}

void ABCVm::setProperty_i(intptr_t value,ISWFObject* obj,multiname* name)
{
	LOG(CALLS,"setProperty_i " << *name);

	//Check to see if a proper setter method is available
	ISWFObject* owner;
	obj->setVariableByMultiname_i(*name,value);
	obj->decRef();
}

ISWFObject* ABCVm::convert_d(ISWFObject* o)
{
	LOG(NOT_IMPLEMENTED, "convert_d" );
	return o;
}

ISWFObject* ABCVm::convert_b(ISWFObject* o)
{
	LOG(NOT_IMPLEMENTED, "convert_b" );
	return o;
}

ISWFObject* ABCVm::convert_i(ISWFObject* o)
{
	LOG(NOT_IMPLEMENTED, "convert_i" );
	return o;
}

void ABCVm::coerce_a()
{
	LOG(NOT_IMPLEMENTED, "coerce_a" );
}

ISWFObject* ABCVm::coerce_s(ISWFObject* o)
{
	LOG(NOT_IMPLEMENTED, "coerce_s" );
	return o;
}

void ABCVm::pop(call_context* th)
{
	LOG(CALLS, "pop: DONE" );
}

void ABCVm::getLocal(call_context* th, int n)
{
	LOG(CALLS,"getLocal: DONE " << n);
}

void ABCVm::setLocal(call_context* th, int n)
{
	LOG(CALLS,"setLocal: DONE " << n);
}

intptr_t ABCVm::pushShort(intptr_t n)
{
	LOG(CALLS, "pushShort " << n );
	return n;
}

ISWFObject* ABCVm::setSlot(ISWFObject* value, ISWFObject* obj, int n)
{
	LOG(CALLS,"setSlot " << dec << n);
	obj->setSlot(n,value);
	obj->decRef();
}

ISWFObject* ABCVm::getSlot(ISWFObject* obj, int n)
{
	LOG(CALLS,"getSlot " << dec << n);
	ISWFObject* ret=obj->getSlot(n);
	ret->incRef();
	obj->decRef();
	return ret;
}

ISWFObject* ABCVm::negate(ISWFObject* v)
{
	LOG(CALLS, "negate" );
	ISWFObject* ret=new Number(-(v->toNumber()));
	v->decRef();
	return ret;
}

uintptr_t ABCVm::bitXor(ISWFObject* val2, ISWFObject* val1)
{
	int i1=val1->toInt();
	int i2=val2->toInt();
	val1->decRef();
	val2->decRef();
	LOG(CALLS,"bitXor " << hex << i1 << '^' << i2);
	return i1^i2;
}

uintptr_t ABCVm::bitOr(ISWFObject* val2, ISWFObject* val1)
{
	int i1=val1->toInt();
	int i2=val2->toInt();
	val1->decRef();
	val2->decRef();
	LOG(CALLS,"bitOr " << hex << i1 << '|' << i2);
	return i1|i2;
}

void ABCVm::callProperty(call_context* th, int n, int m)
{
	arguments args(m);
	for(int i=0;i<m;i++)
		args.set(m-i-1,th->runtime_stack_pop());

	multiname name=th->context->getMultiname(n);
	LOG(CALLS,"callProperty " << name << ' ' << m);

	ISWFObject* obj=th->runtime_stack_pop();
	ISWFObject* owner;
	ISWFObject* o=obj->getVariableByMultiname(name,owner);
	if(owner)
	{
		//If o is already a function call it, otherwise find the Call method
		if(o->getObjectType()==T_FUNCTION)
		{
			IFunction* f=dynamic_cast<IFunction*>(o);
			ISWFObject* ret=f->call(obj,&args);
			th->runtime_stack_push(ret);
		}
		else if(o->getObjectType()==T_UNDEFINED)
		{
			LOG(NOT_IMPLEMENTED,"We got a Undefined function");
			th->runtime_stack_push(new Undefined);
		}
		else if(o->getObjectType()==T_DEFINABLE)
		{
			LOG(NOT_IMPLEMENTED,"We got a function not yet valid");
			Definable* d=static_cast<Definable*>(o);
			d->define(obj);
			IFunction* f=obj->getVariableByMultiname(name,owner)->toFunction();
			if(f)
			{
				ISWFObject* ret=f->call(owner,&args);
				th->runtime_stack_push(ret);
			}
			else
				abort();
		}
		else
		{
			IFunction* f=dynamic_cast<IFunction*>(o->getVariableByName("Call",owner));
			if(f)
			{
				ISWFObject* ret=f->call(o,&args);
				th->runtime_stack_push(ret);
			}
			else
			{
				LOG(CALLS,"No such function, returning Undefined");
				th->runtime_stack_push(new Undefined);
			}
		}
	}
	else
	{
		LOG(NOT_IMPLEMENTED,"Calling an undefined function");
		th->runtime_stack_push(new Undefined);
	}
	LOG(CALLS,"End of calling " << name);
	obj->decRef();
}

ISWFObject* ABCVm::getProperty(ISWFObject* obj, multiname* name)
{
	LOG(CALLS, "getProperty " << name );

	ISWFObject* owner;
	ISWFObject* ret=obj->getVariableByMultiname(*name,owner);
	if(!owner)
	{
		LOG(NOT_IMPLEMENTED,"Property not found " << name);
		ret=new Undefined;
	}
	else
	{
		if(ret->getObjectType()==T_DEFINABLE)
		{
			LOG(ERROR,"Property " << name << " is not yet valid");
			Definable* d=static_cast<Definable*>(ret);
			d->define(obj);
			ret=obj->getVariableByMultiname(*name,owner);
		}
		ret->incRef();
	}
	obj->decRef();
	return ret;
}

number_t ABCVm::divide(ISWFObject* val2, ISWFObject* val1)
{
	double num1=val1->toNumber();
	double num2=val2->toNumber();

	val1->decRef();
	val2->decRef();
	LOG(CALLS,"divide "  << num1 << '/' << num2);
	return num1/num2;
}

ISWFObject* ABCVm::getGlobalScope(call_context* th)
{
	LOG(CALLS,"getGlobalScope: " << &th->context->Global);
	th->context->Global->incRef();
	return th->context->Global;
}

ISWFObject* ABCVm::decrement(ISWFObject* o)
{
	LOG(CALLS,"decrement");

	int n=o->toInt();
	o->decRef();
	return new Integer(n-1);
}

ISWFObject* ABCVm::decrement_i(ISWFObject* o)
{
	LOG(NOT_IMPLEMENTED,"decrement_i");
	abort();
	return o;
}

bool ABCVm::ifLT(ISWFObject* obj2, ISWFObject* obj1, int offset)
{
	LOG(CALLS,"ifLT " << offset);

	//Real comparision demanded to object
	bool ret=obj1->isLess(obj2);

	obj2->decRef();
	obj1->decRef();
	return ret;
}

bool ABCVm::ifNE(ISWFObject* obj1, ISWFObject* obj2, int offset)
{
	LOG(CALLS,"ifNE " << dec << offset);

	//Real comparision demanded to object
	bool ret=!(obj1->isEqual(obj2));

	obj2->decRef();
	obj1->decRef();
	return ret;
}

intptr_t ABCVm::pushByte(intptr_t n)
{
	LOG(CALLS, "pushByte " << n );
	return n;
}

number_t ABCVm::multiply(ISWFObject* val2, ISWFObject* val1)
{
	double num1=val1->toNumber();
	double num2=val2->toNumber();
	val1->decRef();
	val2->decRef();
	LOG(CALLS,"multiply "  << num1 << '*' << num2);
	return num1*num2;
}

void ABCVm::incLocal_i(call_context* th, int n)
{
	LOG(CALLS, "incLocal_i " << n );
	if(th->locals[n]->getObjectType()==T_INTEGER)
	{
		Integer* i=dynamic_cast<Integer*>(th->locals[n]);
		i->val++;
	}
	else
	{
		LOG(NOT_IMPLEMENTED,"Cannot increment type " << th->locals[n]->getObjectType());
	}

}

void ABCVm::construct(call_context* th, int m)
{
	LOG(CALLS, "construct " << m);
	arguments args(m);
	for(int i=0;i<m;i++)
		args.set(m-i-1,th->runtime_stack_pop());

	ISWFObject* o=th->runtime_stack_pop();

	if(o->getObjectType()==T_DEFINABLE)
	{
		LOG(ERROR,"Check");
		abort();
	/*	LOG(CALLS,"Deferred definition of property " << name);
		Definable* d=dynamic_cast<Definable*>(o);
		d->define(obj);
		o=obj->getVariableByMultiname(name,owner);
		LOG(CALLS,"End of deferred definition of property " << name);*/
	}

	LOG(CALLS,"Constructing");
	//We get a shallow copy of the object, but clean out Variables
	//TODO: should be done in the copy constructor
	ASObject* ret=dynamic_cast<ASObject*>(o->clone());
	if(!ret->Variables.empty())
		abort();
	ret->Variables.clear();

	ASObject* aso=dynamic_cast<ASObject*>(o);
	ret->prototype=aso;
	aso->incRef();
	if(aso==NULL)
		LOG(ERROR,"Class is not as ASObject");

	if(o->class_index==-2)
	{
		//We have to build the method traits
		SyntheticFunction* sf=static_cast<SyntheticFunction*>(ret);
		LOG(CALLS,"Building method traits");
		for(int i=0;i<sf->mi->body->trait_count;i++)
			th->context->buildTrait(ret,&sf->mi->body->traits[i]);
		sf->call(ret,&args);

	}
	else if(o->class_index==-1)
	{
		//The class is builtin
		LOG(CALLS,"Building a builtin class");
	}
	else
	{
		//The class is declared in the script and has an index
		LOG(CALLS,"Building instance traits");
		for(int i=0;i<th->context->instances[o->class_index].trait_count;i++)
			th->context->buildTrait(ret,&th->context->instances[o->class_index].traits[i]);
	}

	if(o->constructor)
	{
		LOG(CALLS,"Calling Instance init");
		args.incRef();
		o->constructor->call(ret,&args);
//		args.decRef();
	}

	LOG(CALLS,"End of constructing");
	th->runtime_stack_push(ret);
	o->decRef();
}

ISWFObject* ABCVm::typeOf(ISWFObject* obj)
{
	LOG(CALLS,"typeOf");
	string ret;
	switch(obj->getObjectType())
	{
		case T_UNDEFINED:
			ret="undefined";
			break;
		case T_OBJECT:
		case T_NULL:
		case T_ARRAY:
			ret="object";
			break;
		case T_BOOLEAN:
			ret="boolean";
			break;
		case T_NUMBER:
		case T_INTEGER:
			ret="number";
			break;
		case T_STRING:
			ret="string";
			break;
		case T_FUNCTION:
			ret="function";
			break;
		default:
			return new Undefined;
	}
	obj->decRef();
	return new ASString(ret);
}

void ABCVm::callPropVoid(call_context* th, int n, int m)
{
	multiname name=th->context->getMultiname(n); 
	LOG(CALLS,"callPropVoid " << name << ' ' << m);
	arguments args(m);
	for(int i=0;i<m;i++)
		args.set(m-i-1,th->runtime_stack_pop());
	ISWFObject* obj=th->runtime_stack_pop();
	ISWFObject* owner;
	ISWFObject* o=obj->getVariableByMultiname(name,owner);
	if(owner)
	{
		//If o is already a function call it, otherwise find the Call method
		if(o->getObjectType()==T_FUNCTION)
		{
			IFunction* f=dynamic_cast<IFunction*>(o);
			f->call(obj,&args);
		}
		else if(o->getObjectType()==T_UNDEFINED)
		{
			LOG(NOT_IMPLEMENTED,"We got a Undefined function");
			th->runtime_stack_push(new Undefined);
		}
		else if(o->getObjectType()==T_DEFINABLE)
		{
			LOG(NOT_IMPLEMENTED,"We got a function not yet valid");
			th->runtime_stack_push(new Undefined);
		}
		else
		{
			IFunction* f=dynamic_cast<IFunction*>(o->getVariableByName(".Call",owner));
			f->call(owner,&args);
		}
	}
	else
		LOG(NOT_IMPLEMENTED,"Calling an undefined function");

	obj->decRef();
	LOG(CALLS,"End of calling " << name);
}

void ABCVm::jump(call_context* th, int offset)
{
	LOG(CALLS,"jump " << offset);
}

bool ABCVm::ifTrue(ISWFObject* obj1, int offset)
{
	LOG(CALLS,"ifTrue " << offset);
}

number_t ABCVm::modulo(ISWFObject* val1, ISWFObject* val2)
{
	int num2=val2->toInt();
	int num1=val1->toInt();

	val1->decRef();
	val2->decRef();
	LOG(CALLS,"modulo "  << num1 << '%' << num2);
	return num1%num2;
}

number_t ABCVm::subtract(ISWFObject* val2, ISWFObject* val1)
{
	int num2=val2->toInt();
	int num1=val1->toInt();

	val1->decRef();
	val2->decRef();
	LOG(CALLS,"subtract " << num1 << '-' << num2);
	return num1-num2;
}

ISWFObject* ABCVm::pushInt(call_context* th, int n)
{
	s32 i=th->context->constant_pool.integer[n];
	LOG(CALLS, "pushInt [" << dec << n << "] " << i);
}

void ABCVm::kill(call_context* th, int n)
{
	LOG(CALLS, "kill " << n );
}

