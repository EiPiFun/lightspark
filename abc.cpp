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

//#define __STDC_LIMIT_MACROS
#include "abc.h"
#include <llvm/Module.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/LLVMContext.h>
#include <llvm/ModuleProvider.h> 
#include <llvm/Target/TargetData.h>
#include <llvm/Target/TargetSelect.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Transforms/Scalar.h> 
#include "logger.h"
#include "swftypes.h"
#include <sstream>
#include "swf.h"
#include "flashevents.h"
#include "flashdisplay.h"
#include "flashnet.h"
#include "flashsystem.h"
#include "flashutils.h"
#include "flashgeom.h"
#include "compat.h"
#include "class.h"

using namespace std;
using namespace lightspark;

extern TLSDATA SystemState* sys;
extern TLSDATA ParseThread* pt;
TLSDATA Manager* iManager=NULL;
TLSDATA Manager* dManager=NULL;

DoABCTag::DoABCTag(RECORDHEADER h, std::istream& in):ControlTag(h,in)
{
	int dest=in.tellg();
	dest+=getSize();
	in >> Flags >> Name;
	LOG(LOG_CALLS,"DoABCTag Name: " << Name);

	cout << "assign vm" << endl;
	context=new ABCContext(in);

	int pos=in.tellg();
	if(dest!=pos)
	{
		LOG(LOG_ERROR,"Corrupted ABC data: missing " << dest-in.tellg());
		abort();
	}
}

void DoABCTag::execute(RootMovieClip*)
{
	LOG(LOG_CALLS,"ABC Exec " << Name);
	sys->currentVm->addEvent(NULL,new ABCContextInitEvent(context));
	SynchronizationEvent* se=new SynchronizationEvent;
	sys->currentVm->addEvent(NULL,se);
	se->wait();
}

SymbolClassTag::SymbolClassTag(RECORDHEADER h, istream& in):ControlTag(h,in)
{
	LOG(LOG_TRACE,"SymbolClassTag");
	in >> NumSymbols;

	Tags.resize(NumSymbols);
	Names.resize(NumSymbols);

	for(int i=0;i<NumSymbols;i++)
		in >> Tags[i] >> Names[i];
}

void SymbolClassTag::execute(RootMovieClip* root)
{
	LOG(LOG_TRACE,"SymbolClassTag Exec");

	for(int i=0;i<NumSymbols;i++)
	{
		LOG(LOG_CALLS,Tags[i] << ' ' << Names[i]);
		if(Tags[i]==0)
		{
			//We have to bind this root movieclip itself, let's tell it.
			//This will be done later
			root->bindToName((const char*)Names[i]);
		}
		else
		{
			DictionaryTag* t=root->dictionaryLookup(Tags[i]);
			IInterface* base=dynamic_cast<IInterface*>(t);
			assert(base!=NULL);
			sys->currentVm->addEvent(NULL,new BindClassEvent(base,(const char*)Names[i]));
		}
	}
}

ASFUNCTIONBODY(ABCVm,print)
{
	cerr << args->at(0)->toString() << endl;
	return new Null;
}

void ABCVm::registerClasses()
{
	//Register predefined types, ASObject are enough for not implemented classes
	Global.setVariableByQName("Object","",Class<IInterface>::getClass());
	Global.setVariableByQName("Class","",Class_object::getClass());
	Global.setVariableByQName("Number","",new Number(0.0));
	Global.setVariableByQName("String","",Class<ASString>::getClass());
	Global.setVariableByQName("Array","",Class<Array>::getClass());
	Global.setVariableByQName("Function","",new Function);
	Global.setVariableByQName("undefined","",new Undefined);
	Global.setVariableByQName("Math","",Class<Math>::getClass());
	Global.setVariableByQName("Date","",Class<Date>::getClass());
	Global.setVariableByQName("RegExp","",Class<RegExp>::getClass());
	Global.setVariableByQName("QName","",Class<ASQName>::getClass());

	Global.setVariableByQName("print","",new Function(print));
	Global.setVariableByQName("trace","",new Function(print));
	Global.setVariableByQName("parseInt","",new Function(parseInt));
	Global.setVariableByQName("parseFloat","",new Function(parseFloat));
	Global.setVariableByQName("int","",new Function(_int));
	Global.setVariableByQName("toString","",new Function(ASObject::_toString));

	Global.setVariableByQName("MovieClip","flash.display",Class<MovieClip>::getClass());
	Global.setVariableByQName("DisplayObject","flash.display",Class<DisplayObject>::getClass());
	Global.setVariableByQName("Loader","flash.display",Class<Loader>::getClass());
	Global.setVariableByQName("SimpleButton","flash.display",new ASObject);
	Global.setVariableByQName("InteractiveObject","flash.display",Class<IInterface>::getClass("InteractiveObject")),
	Global.setVariableByQName("DisplayObjectContainer","flash.display",Class<DisplayObjectContainer>::getClass());
	Global.setVariableByQName("Sprite","flash.display",Class<Sprite>::getClass());
	Global.setVariableByQName("Shape","flash.display",Class<Shape>::getClass());
	Global.setVariableByQName("Stage","flash.display",Class<Stage>::getClass());
	Global.setVariableByQName("Graphics","flash.display",Class<Graphics>::getClass());
	Global.setVariableByQName("LineScaleMode","flash.display",Class<LineScaleMode>::getClass());
	Global.setVariableByQName("StageScaleMode","flash.display",Class<StageScaleMode>::getClass());
	Global.setVariableByQName("StageAlign","flash.display",Class<StageAlign>::getClass());
	Global.setVariableByQName("IBitmapDrawable","flash.display",Class<IInterface>::getClass("IBitmapDrawable"));
	Global.setVariableByQName("BitmapData","flash.display",Class<IInterface>::getClass("BitmapData"));

	Global.setVariableByQName("DropShadowFilter","flash.filters",Class<IInterface>::getClass("DropShadowFilter"));

	Global.setVariableByQName("TextField","flash.text",Class<IInterface>::getClass("TextField"));
	Global.setVariableByQName("TextFormat","flash.text",Class<IInterface>::getClass("TextFormat"));
	Global.setVariableByQName("TextFieldType","flash.text",Class<IInterface>::getClass("TextFieldType"));

	Global.setVariableByQName("XMLDocument","flash.xml",new ASObject);

	Global.setVariableByQName("ApplicationDomain","flash.system",Class<ApplicationDomain>::getClass());
	Global.setVariableByQName("LoaderContext","flash.system",Class<IInterface>::getClass("LoaderContext"));

	Global.setVariableByQName("ByteArray","flash.utils",Class<ByteArray>::getClass());
	Global.setVariableByQName("Dictionary","flash.utils",Class<Dictionary>::getClass());
	Global.setVariableByQName("Proxy","flash.utils",Class<IInterface>::getClass("Proxy"));
	Global.setVariableByQName("Timer","flash.utils",Class<Timer>::getClass());
	Global.setVariableByQName("getQualifiedClassName","flash.utils",new Function(getQualifiedClassName));
	Global.setVariableByQName("getQualifiedSuperclassName","flash.utils",new Function(getQualifiedSuperclassName));
	Global.setVariableByQName("getDefinitionByName","flash.utils",new Function(getDefinitionByName));
	Global.setVariableByQName("getTimer","flash.utils",new Function(getTimer));

	Global.setVariableByQName("ColorTransform","flash.geom",Class<ColorTransform>::getClass());
	Global.setVariableByQName("Rectangle","flash.geom",Class<Rectangle>::getClass());
	Global.setVariableByQName("Matrix","flash.geom",Class<IInterface>::getClass("Matrix"));
	Global.setVariableByQName("Point","flash.geom",Class<IInterface>::getClass("Point"));

	Global.setVariableByQName("EventDispatcher","flash.events",Class<EventDispatcher>::getClass());
	Global.setVariableByQName("Event","flash.events",Class<Event>::getClass());
	Global.setVariableByQName("MouseEvent","flash.events",Class<MouseEvent>::getClass());
	Global.setVariableByQName("ProgressEvent","flash.events",Class<ProgressEvent>::getClass());
	Global.setVariableByQName("TimerEvent","flash.events",Class<TimerEvent>::getClass());
	Global.setVariableByQName("IOErrorEvent","flash.events",Class<IOErrorEvent>::getClass());
	Global.setVariableByQName("SecurityErrorEvent","flash.events",Class<FakeEvent>::getClass("SecurityErrorEvent"));
	Global.setVariableByQName("TextEvent","flash.events",Class<FakeEvent>::getClass("TextEvent"));
	Global.setVariableByQName("ErrorEvent","flash.events",Class<FakeEvent>::getClass("ErrorEvent"));
	Global.setVariableByQName("IEventDispatcher","flash.events",Class<IEventDispatcher>::getClass());
	Global.setVariableByQName("FocusEvent","flash.events",Class<FocusEvent>::getClass());
	Global.setVariableByQName("KeyboardEvent","flash.events",Class<FakeEvent>::getClass("KeyboardEvent"));

	Global.setVariableByQName("LocalConnection","flash.net",new ASObject);
	Global.setVariableByQName("URLLoader","flash.net",Class<URLLoader>::getClass());
	Global.setVariableByQName("URLLoaderDataFormat","flash.net",Class<URLLoaderDataFormat>::getClass());
	Global.setVariableByQName("URLRequest","flash.net",Class<URLRequest>::getClass());
	Global.setVariableByQName("URLVariables","flash.net",new ASObject);
	Global.setVariableByQName("SharedObject","flash.net",Class<SharedObject>::getClass());
	Global.setVariableByQName("ObjectEncoding","flash.net",Class<ObjectEncoding>::getClass());

	Global.setVariableByQName("Capabilities","flash.system",Class<Capabilities>::getClass());

	Global.setVariableByQName("ContextMenu","flash.ui",Class<IInterface>::getClass("ContextMenu"));
	Global.setVariableByQName("ContextMenuItem","flash.ui",Class<IInterface>::getClass("ContextMenuItem"));

	Global.setVariableByQName("isNaN","",new Function(isNaN));
}

/*Qname ABCContext::getQname(unsigned int mi, call_context* th) const
{
	if(mi==0)
	{
		LOG(ERROR,"Not a Qname");
		abort();
	}

	const multiname_info* m=&constant_pool.multinames[mi];
	switch(m->kind)
	{
		case 0x07:
		{
			Qname ret(getString(m->name));
			const namespace_info* n=&constant_pool.namespaces[m->ns];
			if(n->name)
				ret.ns=getString(n->name);
			ret.nskind=n->kind;

			return ret;
		}
/*		case 0x0d:
			LOG(CALLS, "QNameA");
			break;
		case 0x0f:
			LOG(CALLS, "RTQName");
			break;
		case 0x10:
			LOG(CALLS, "RTQNameA");
			break;
		case 0x11:
			LOG(CALLS, "RTQNameL");
			break;
		case 0x12:
			LOG(CALLS, "RTQNameLA");
			break;
		case 0x0e:
			LOG(CALLS, "MultinameA");
			break;
		case 0x1c:
			LOG(CALLS, "MultinameLA");
			break;*
		default:
			LOG(ERROR,"Not a Qname kind " << hex << m->kind);
			abort();
	}
}*/

//This function is used at compile time
int ABCContext::getMultinameRTData(int mi) const
{
	if(mi==0)
		return 0;

	const multiname_info* m=&constant_pool.multinames[mi];
	switch(m->kind)
	{
		case 0x07:
		case 0x09:
		case 0x0e:
			return 0;
		case 0x0f:
		case 0x1b:
			return 1;
/*		case 0x0d:
			LOG(CALLS, "QNameA");
			break;
		case 0x10:
			LOG(CALLS, "RTQNameA");
			break;
		case 0x11:
			LOG(CALLS, "RTQNameL");
			break;
		case 0x12:
			LOG(CALLS, "RTQNameLA");
			break;
		case 0x1c:
			LOG(CALLS, "MultinameLA");
			break;*/
		default:
			LOG(LOG_ERROR,"getMultinameRTData not yet implemented for this kind " << hex << m->kind);
			abort();
	}
}

//Pre: we already know that n is not zero and that we are going to use an RT multiname from getMultinameRTData
multiname* ABCContext::s_getMultiname_d(call_context* th, number_t rtd, int n)
{
	//We are allowed to access only the ABCContext, as the stack is not synced
	multiname* ret;

	multiname_info* m=&th->context->constant_pool.multinames[n];
	if(m->cached==NULL)
	{
		m->cached=new multiname;
		ret=m->cached;
		switch(m->kind)
		{
			case 0x1b:
			{
				const ns_set_info* s=&th->context->constant_pool.ns_sets[m->ns_set];
				ret->ns.reserve(s->count);
				for(int i=0;i<s->count;i++)
				{
					const namespace_info* n=&th->context->constant_pool.namespaces[s->ns[i]];
					ret->ns.push_back(nsNameAndKind(th->context->getString(n->name),n->kind));
				}
				sort(ret->ns.begin(),ret->ns.end());
				ret->name_d=rtd;
				ret->name_type=multiname::NAME_NUMBER;
				break;
			}
			default:
				LOG(LOG_ERROR,"Multiname to String not yet implemented for this kind " << hex << m->kind);
				abort();
				break;
		}
		return ret;
	}
	else
	{
		ret=m->cached;
		switch(m->kind)
		{
			case 0x1b:
			{
				ret->name_d=rtd;
				ret->name_type=multiname::NAME_NUMBER;
				break;
			}
			default:
				LOG(LOG_ERROR,"Multiname to String not yet implemented for this kind " << hex << m->kind);
				abort();
				break;
		}
		return ret;
	}
}

multiname* ABCContext::s_getMultiname(call_context* th, ASObject* rt1, int n)
{
	//We are allowe to access only the ABCContext, as the stack is not synced
	multiname* ret;
	if(n==0)
	{
		ret=new multiname;
		ret->name_s="any";
		ret->name_type=multiname::NAME_STRING;
		return ret;
	}

	multiname_info* m=&th->context->constant_pool.multinames[n];
	if(m->cached==NULL)
	{
		m->cached=new multiname;
		ret=m->cached;
		switch(m->kind)
		{
			case 0x07:
			{
				const namespace_info* n=&th->context->constant_pool.namespaces[m->ns];
				assert(n->name);
				ret->ns.push_back(nsNameAndKind(th->context->getString(n->name),n->kind));

				ret->name_s=th->context->getString(m->name);
				ret->name_type=multiname::NAME_STRING;
				break;
			}
			case 0x09:
			{
				const ns_set_info* s=&th->context->constant_pool.ns_sets[m->ns_set];
				ret->ns.reserve(s->count);
				for(int i=0;i<s->count;i++)
				{
					const namespace_info* n=&th->context->constant_pool.namespaces[s->ns[i]];
					ret->ns.push_back(nsNameAndKind(th->context->getString(n->name),n->kind));
				}
				sort(ret->ns.begin(),ret->ns.end());
				ret->name_s=th->context->getString(m->name);
				ret->name_type=multiname::NAME_STRING;
				break;
			}
			case 0x1b:
			{
				const ns_set_info* s=&th->context->constant_pool.ns_sets[m->ns_set];
				ret->ns.reserve(s->count);
				for(int i=0;i<s->count;i++)
				{
					const namespace_info* n=&th->context->constant_pool.namespaces[s->ns[i]];
					ret->ns.push_back(nsNameAndKind(th->context->getString(n->name),n->kind));
				}
				sort(ret->ns.begin(),ret->ns.end());
				if(rt1->getObjectType()==T_INTEGER)
				{
					Integer* o=static_cast<Integer*>(rt1);
					ret->name_i=o->val;
					ret->name_type=multiname::NAME_INT;
				}
				else if(rt1->getObjectType()==T_NUMBER)
				{
					Number* o=static_cast<Number*>(rt1);
					ret->name_d=o->val;
					ret->name_type=multiname::NAME_NUMBER;
				}
				else if(rt1->getObjectType()==T_QNAME)
				{
					ASQName* qname=static_cast<ASQName*>(rt1->implementation);
					ret->name_s=qname->local_name;
					ret->name_type=multiname::NAME_STRING;
				}
				else if(rt1->getObjectType()==T_OBJECT)
				{
					ret->name_o=rt1;
					ret->name_type=multiname::NAME_OBJECT;
					rt1->incRef();
				}
				else if(rt1->getObjectType()==T_STRING)
				{
					ASString* o=static_cast<ASString*>(rt1->implementation);
					ret->name_s=o->data;
					ret->name_type=multiname::NAME_STRING;
				}
				else
				{
					abort();
					//ret->name_s=rt1->toString();
					//ret->name_type=multiname::NAME_STRING;
				}
				rt1->decRef();
				break;
			}
			case 0x0f: //RTQName
			{
				assert(rt1->prototype==Class<Namespace>::getClass());
				Namespace* tmpns=static_cast<Namespace*>(rt1->implementation);
				//TODO: What is the right ns kind?
				ret->ns.push_back(nsNameAndKind(tmpns->uri,0x08));
				ret->name_type=multiname::NAME_STRING;
				ret->name_s=th->context->getString(m->name);
				rt1->decRef();
				break;
			}
	/*		case 0x0d:
				LOG(CALLS, "QNameA");
				break;
			case 0x10:
				LOG(CALLS, "RTQNameA");
				break;
			case 0x11:
				LOG(CALLS, "RTQNameL");
				break;
			case 0x12:
				LOG(CALLS, "RTQNameLA");
				break;
			case 0x0e:
				LOG(CALLS, "MultinameA");
				break;
			case 0x1c:
				LOG(CALLS, "MultinameLA");
				break;*/
			default:
				LOG(LOG_ERROR,"Multiname to String not yet implemented for this kind " << hex << m->kind);
				abort();
				break;
		}
		return ret;
	}
	else
	{
		ret=m->cached;
		switch(m->kind)
		{
			case 0x07:
			case 0x09:
			{
				//Nothing to do, the cached value is enough
				break;
			}
			case 0x1b:
			{
				if(rt1->getObjectType()==T_INTEGER)
				{
					Integer* o=static_cast<Integer*>(rt1);
					ret->name_i=o->val;
					ret->name_type=multiname::NAME_INT;
				}
				else if(rt1->getObjectType()==T_NUMBER)
				{
					Number* o=static_cast<Number*>(rt1);
					ret->name_d=o->val;
					ret->name_type=multiname::NAME_NUMBER;
				}
				else if(rt1->getObjectType()==T_QNAME)
				{
					ASQName* qname=static_cast<ASQName*>(rt1->implementation);
					ret->name_s=qname->local_name;
					ret->name_type=multiname::NAME_STRING;
				}
				else if(rt1->getObjectType()==T_OBJECT)
				{
					ret->name_o=rt1;
					ret->name_type=multiname::NAME_OBJECT;
					rt1->incRef();
				}
				else if(rt1->getObjectType()==T_STRING)
				{
					ASString* o=static_cast<ASString*>(rt1->implementation);
					ret->name_s=o->data;
					ret->name_type=multiname::NAME_STRING;
				}
				else if(rt1->getObjectType()==T_UNDEFINED ||
					rt1->getObjectType()==T_NULL)
				{
					ret->name_s="undefined";
					ret->name_type=multiname::NAME_STRING;
				}
				else
				{
					abort();
					//ret->name_s=rt1->toString();
					//ret->name_type=multiname::NAME_STRING;
				}
				rt1->decRef();
				break;
			}
			case 0x0f: //RTQName
			{
				//Reset the namespaces
				ret->ns.clear();

				assert(rt1->prototype==Class<Namespace>::getClass());
				Namespace* tmpns=static_cast<Namespace*>(rt1->implementation);
				//TODO: What is the right ns kind?
				ret->ns.push_back(nsNameAndKind(tmpns->uri,0x08));
				rt1->decRef();
				break;
			}
	/*		case 0x0d:
				LOG(CALLS, "QNameA");
				break;
			case 0x10:
				LOG(CALLS, "RTQNameA");
				break;
			case 0x11:
				LOG(CALLS, "RTQNameL");
				break;
			case 0x12:
				LOG(CALLS, "RTQNameLA");
				break;
			case 0x0e:
				LOG(CALLS, "MultinameA");
				break;
			case 0x1c:
				LOG(CALLS, "MultinameLA");
				break;*/
			default:
				LOG(LOG_ERROR,"Multiname to String not yet implemented for this kind " << hex << m->kind);
				abort();
				break;
		}
		return ret;
	}
}

//Pre: we already know that n is not zero and that we are going to use an RT multiname from getMultinameRTData
multiname* ABCContext::s_getMultiname_i(call_context* th, uintptr_t rti, int n)
{
	//We are allowed to access only the ABCContext, as the stack is not synced
	multiname* ret;

	multiname_info* m=&th->context->constant_pool.multinames[n];
	if(m->cached==NULL)
	{
		m->cached=new multiname;
		ret=m->cached;
		switch(m->kind)
		{
			case 0x1b:
			{
				const ns_set_info* s=&th->context->constant_pool.ns_sets[m->ns_set];
				ret->ns.reserve(s->count);
				for(int i=0;i<s->count;i++)
				{
					const namespace_info* n=&th->context->constant_pool.namespaces[s->ns[i]];
					ret->ns.push_back(nsNameAndKind(th->context->getString(n->name),n->kind));
				}
				sort(ret->ns.begin(),ret->ns.end());
				ret->name_i=rti;
				ret->name_type=multiname::NAME_INT;
				break;
			}
			default:
				LOG(LOG_ERROR,"Multiname to String not yet implemented for this kind " << hex << m->kind);
				abort();
				break;
		}
		return ret;
	}
	else
	{
		ret=m->cached;
		switch(m->kind)
		{
			case 0x1b:
			{
				ret->name_i=rti;
				ret->name_type=multiname::NAME_INT;
				break;
			}
			default:
				LOG(LOG_ERROR,"Multiname to String not yet implemented for this kind " << hex << m->kind);
				abort();
				break;
		}
		return ret;
	}
}

multiname* ABCContext::getMultiname(unsigned int n, call_context* th)
{
	multiname* ret;
	multiname_info* m=&constant_pool.multinames[n];

	if(m->cached==NULL)
	{
		m->cached=new multiname;
		ret=m->cached;

		if(n==0)
		{
			ret->name_s="any";
			ret->name_type=multiname::NAME_STRING;
			return ret;
		}
		switch(m->kind)
		{
			case 0x07:
			{
				const namespace_info* n=&constant_pool.namespaces[m->ns];
				if(n->name)
					ret->ns.push_back(nsNameAndKind(getString(n->name),n->kind));
				else
					ret->ns.push_back(nsNameAndKind("",n->kind));

				ret->name_s=getString(m->name);
				ret->name_type=multiname::NAME_STRING;
				break;
			}
			case 0x09:
			{
				const ns_set_info* s=&constant_pool.ns_sets[m->ns_set];
				ret->ns.reserve(s->count);
				for(int i=0;i<s->count;i++)
				{
					const namespace_info* n=&constant_pool.namespaces[s->ns[i]];
					ret->ns.push_back(nsNameAndKind(getString(n->name),n->kind));
				}
				sort(ret->ns.begin(),ret->ns.end());

				ret->name_s=getString(m->name);
				ret->name_type=multiname::NAME_STRING;
				break;
			}
			case 0x1b:
			{
				const ns_set_info* s=&constant_pool.ns_sets[m->ns_set];
				ret->ns.reserve(s->count);
				for(int i=0;i<s->count;i++)
				{
					const namespace_info* n=&constant_pool.namespaces[s->ns[i]];
					ret->ns.push_back(nsNameAndKind(getString(n->name),n->kind));
				}
				sort(ret->ns.begin(),ret->ns.end());

				ASObject* n=th->runtime_stack_pop();
				if(n->getObjectType()==T_INTEGER)
				{
					Integer* o=static_cast<Integer*>(n);
					ret->name_i=o->val;
					ret->name_type=multiname::NAME_INT;
				}
				else if(n->getObjectType()==T_NUMBER)
				{
					Number* o=static_cast<Number*>(n);
					ret->name_d=o->val;
					ret->name_type=multiname::NAME_NUMBER;
				}
				else if(n->getObjectType()==T_QNAME)
				{
					ASQName* qname=static_cast<ASQName*>(n->implementation);
					ret->name_s=qname->local_name;
					ret->name_type=multiname::NAME_STRING;
				}
				else if(n->getObjectType()==T_OBJECT)
				{
					ret->name_o=n;
					ret->name_type=multiname::NAME_OBJECT;
					n->incRef();
				}
				else if(n->getObjectType()==T_STRING)
				{
					ASString* o=static_cast<ASString*>(n->implementation);
					ret->name_s=o->data;
					ret->name_type=multiname::NAME_STRING;
				}
				else
				{
					abort();
					//ret->name_s=n->toString();
					//ret->name_type=multiname::NAME_STRING;
				}
				n->decRef();
				break;
			}
			case 0x0f: //RTQName
			{
				ASObject* n=th->runtime_stack_pop();
				assert(n->prototype==Class<Namespace>::getClass());
				Namespace* tmpns=static_cast<Namespace*>(n->implementation);
				//TODO: What is the right ns kind?
				ret->ns.push_back(nsNameAndKind(tmpns->uri,0x08));
				ret->name_type=multiname::NAME_STRING;
				ret->name_s=getString(m->name);
				n->decRef();
				break;
			}
			case 0x1d:
			{
				assert(m->param_types.size()==1);
				multiname_info* td=&constant_pool.multinames[m->type_definition];
				multiname_info* p=&constant_pool.multinames[m->param_types[0]];
				const namespace_info* n=&constant_pool.namespaces[td->ns];
				ret->ns.push_back(nsNameAndKind(getString(n->name),n->kind));
				ret->name_s=getString(td->name);
				ret->name_type=multiname::NAME_STRING;
				break;
			}
	/*		case 0x0d:
				LOG(CALLS, "QNameA");
				break;
			case 0x10:
				LOG(CALLS, "RTQNameA");
				break;
			case 0x11:
				LOG(CALLS, "RTQNameL");
				break;
			case 0x12:
				LOG(CALLS, "RTQNameLA");
				break;
			case 0x0e:
				LOG(CALLS, "MultinameA");
				break;
			case 0x1c:
				LOG(CALLS, "MultinameLA");
				break;*/
			default:
				LOG(LOG_ERROR,"Multiname to String not yet implemented for this kind " << hex << m->kind);
				abort();
				break;
		}
		return ret;
	}
	else
	{
		ret=m->cached;
		if(n==0)
			return ret;
		switch(m->kind)
		{
			case 0x1d:
				cout << "PUPPA" << endl;
			case 0x07:
			case 0x09:
			{
				//Nothing to do, the cached value is enough
				break;
			}
			case 0x1b:
			{
				ASObject* n=th->runtime_stack_pop();
				if(n->getObjectType()==T_INTEGER)
				{
					Integer* o=static_cast<Integer*>(n);
					ret->name_i=o->val;
					ret->name_type=multiname::NAME_INT;
				}
				else if(n->getObjectType()==T_NUMBER)
				{
					Number* o=static_cast<Number*>(n);
					ret->name_d=o->val;
					ret->name_type=multiname::NAME_NUMBER;
				}
				else if(n->getObjectType()==T_QNAME)
				{
					ASQName* qname=static_cast<ASQName*>(n->implementation);
					ret->name_s=qname->local_name;
					ret->name_type=multiname::NAME_STRING;
				}
				else if(n->getObjectType()==T_OBJECT)
				{
					ret->name_o=n;
					ret->name_type=multiname::NAME_OBJECT;
					n->incRef();
				}
				else if(n->getObjectType()==T_STRING)
				{
					ASString* o=static_cast<ASString*>(n->implementation);
					ret->name_s=o->data;
					ret->name_type=multiname::NAME_STRING;
				}
				else
				{
					abort();
					//ret->name_s=n->toString();
					//ret->name_type=multiname::NAME_STRING;
				}
				n->decRef();
				break;
			}
			case 0x0f: //RTQName
			{
				ASObject* n=th->runtime_stack_pop();
				//Reset the namespaces
				ret->ns.clear();

				assert(n->prototype==Class<Namespace>::getClass());
				Namespace* tmpns=static_cast<Namespace*>(n->implementation);
				//TODO: What is the right kind?
				ret->ns.push_back(nsNameAndKind(tmpns->uri,0x08));
				n->decRef();
				break;
			}
	/*		case 0x0d:
				LOG(CALLS, "QNameA");
				break;
			case 0x10:
				LOG(CALLS, "RTQNameA");
				break;
			case 0x11:
				LOG(CALLS, "RTQNameL");
				break;
			case 0x12:
				LOG(CALLS, "RTQNameLA");
				break;
			case 0x0e:
				LOG(CALLS, "MultinameA");
				break;
			case 0x1c:
				LOG(CALLS, "MultinameLA");
				break;*/
			default:
				LOG(LOG_ERROR,"dMultiname to String not yet implemented for this kind " << hex << m->kind);
				abort();
				break;
		}
		ret->name_s.len();
		return ret;
	}
}

ABCContext::ABCContext(istream& in)
{
	in >> minor >> major;
	LOG(LOG_CALLS,"ABCVm version " << major << '.' << minor);
	in >> constant_pool;

	in >> method_count;
	methods.resize(method_count);
	for(int i=0;i<method_count;i++)
	{
		in >> methods[i];
		methods[i].context=this;
	}

	in >> metadata_count;
	metadata.resize(metadata_count);
	for(int i=0;i<metadata_count;i++)
		in >> metadata[i];

	in >> class_count;
	instances.resize(class_count);
	for(int i=0;i<class_count;i++)
	{
		in >> instances[i];
		LOG(LOG_CALLS,"Class " << *getMultiname(instances[i].name,NULL));
		LOG(LOG_CALLS,"Flags:");
		if((instances[i].flags)&0x01)
			LOG(LOG_CALLS,"\tSealed");
		if((instances[i].flags)&0x02)
			LOG(LOG_CALLS,"\tFinal");
		if((instances[i].flags)&0x04)
			LOG(LOG_CALLS,"\tInterface");
		if((instances[i].flags)&0x08)
			LOG(LOG_CALLS,"\tProtectedNS " << getString(constant_pool.namespaces[instances[i].protectedNs].name));
		if(instances[i].supername)
			LOG(LOG_CALLS,"Super " << *getMultiname(instances[i].supername,NULL));
		if(instances[i].interface_count)
		{
			LOG(LOG_CALLS,"Implements");
			for(int j=0;j<instances[i].interfaces.size();j++)
			{
				LOG(LOG_CALLS,"\t" << *getMultiname(instances[i].interfaces[j],NULL));
			}
		}
	}
	classes.resize(class_count);
	for(int i=0;i<class_count;i++)
		in >> classes[i];

	in >> script_count;
	scripts.resize(script_count);
	for(int i=0;i<script_count;i++)
		in >> scripts[i];

	in >> method_body_count;
	method_body.resize(method_body_count);
	for(int i=0;i<method_body_count;i++)
	{
		in >> method_body[i];

		//Link method body with method signature
		if(methods[method_body[i].method].body!=NULL)
		{
			LOG(LOG_ERROR,"Duplicate body assigment");
			abort();
		}
		else
			methods[method_body[i].method].body=&method_body[i];
	}
}

ABCVm::ABCVm(SystemState* s):shutdown(false),m_sys(s)
{
	sem_init(&event_queue_mutex,0,1);
	sem_init(&sem_event_count,0,0);
	m_sys=s;
	int_manager=new Manager;
	number_manager=new Manager;
	pthread_create(&t,NULL,(thread_worker)Run,this);
}

ABCVm::~ABCVm()
{
	pthread_cancel(t);
	pthread_join(t,NULL);
	//delete ex;
	//delete module;
}

void ABCVm::wait()
{
	pthread_join(t,NULL);
}

void ABCVm::handleEvent()
{
	sem_wait(&event_queue_mutex);
	pair<EventDispatcher*,Event*> e=events_queue.front();
	events_queue.pop_front();
	sem_post(&event_queue_mutex);
	if(e.first)
		e.first->handleEvent(e.second);
	else
	{
		//Should be handled by the Vm itself
		switch(e.second->getEventType())
		{
			case BIND_CLASS:
			{
				BindClassEvent* ev=static_cast<BindClassEvent*>(e.second);
				arguments args(0);
/*				args.incRef();
				//TODO: check
				args.set(0,new Null);*/
				//TODO: move construct_instance to the event itself
				bool construct_instance=false;
				if(ev->base==sys)
					construct_instance=true;
				LOG(LOG_CALLS,"Binding of " << ev->class_name);
				buildClassAndInjectBase(ev->class_name.raw_buf(),ev->base,&args,construct_instance);
				LOG(LOG_CALLS,"End of binding of " << ev->class_name);
				break;
			}
			case SHUTDOWN:
				shutdown=true;
				break;
			case SYNC:
			{
				SynchronizationEvent* ev=static_cast<SynchronizationEvent*>(e.second);
				ev->sync();
				break;
			}
			/*case FUNCTION:
			{
				FunctionEvent* ev=static_cast<FunctionEvent*>(e.second);
				//We hope the method is binded
				ev->f->call(NULL,NULL);
				break;
			}*/
			case CONTEXT_INIT:
			{
				ABCContextInitEvent* ev=static_cast<ABCContextInitEvent*>(e.second);
				ev->context->exec();
				break;
			}
			case CONSTRUCT_OBJECT:
			{
				ConstructObjectEvent* ev=static_cast<ConstructObjectEvent*>(e.second);
				LOG(LOG_CALLS,"Building instance traits");
				ev->_class->context->buildInstanceTraits(ev->obj, ev->_class->class_index);

				LOG(LOG_CALLS,"Calling Instance init " << ev->_class->class_name);
				ev->_class->constructor->call(ev->obj,NULL,ev->obj->max_level);
				ev->sync();
				break;
			}
			default:
				LOG(LOG_ERROR,"Not supported event");
				abort();
		}
	}
}

void ABCVm::addEvent(EventDispatcher* obj ,Event* ev)
{
	sem_wait(&event_queue_mutex);
	events_queue.push_back(pair<EventDispatcher*,Event*>(obj, ev));
	sem_post(&sem_event_count);
	sem_post(&event_queue_mutex);
}

void ABCVm::buildClassAndInjectBase(const string& s, IInterface* base,arguments* args, bool construct_instance)
{
	//It seems to be acceptable for the same base to be binded multiple times,
	//We refuse to do it, as it's an undocumented behaviour, but we warn the user
	//I've seen this behaviour only for youtube
	if(base->obj!=NULL)
	{
		LOG(LOG_NOT_IMPLEMENTED,"Multiple binding on " << s << ". Not binding");
		return;
	}

	LOG(LOG_CALLS,"Setting class name to " << s);
	ASObject* derived_class=Global.getVariableByString(s);
	if(derived_class==NULL)
	{
		LOG(LOG_ERROR,"Class " << s << " not found in global");
		abort();
	}

	if(derived_class->getObjectType()==T_DEFINABLE)
	{
		LOG(LOG_CALLS,"Class " << s << " is not yet valid");
		Definable* d=static_cast<Definable*>(derived_class);
		d->define(&Global);
		LOG(LOG_CALLS,"End of deferred init of class " << s);
		derived_class=Global.getVariableByString(s);
		assert(derived_class);
	}

	assert(derived_class->getObjectType()==T_CLASS);

	//Now the class is valid, check that it's not a builtin one
	Class_base* derived_class_tmp=static_cast<Class_base*>(derived_class);
	assert(derived_class_tmp->class_index!=-1);

	base->obj=derived_class_tmp;

	if(construct_instance)
	{
		assert(derived_class_tmp->class_index>0);

		ASObject* tmp=new ASObject;
		base->obj=tmp;
		tmp->max_level=derived_class_tmp->max_level;
		tmp->prototype=derived_class_tmp;
		tmp->actualPrototype=derived_class_tmp;
		tmp->implementation=base;

		tmp->handleConstruction(args,true,true);
	}
}

inline method_info* ABCContext::get_method(unsigned int m)
{
	if(m<method_count)
		return &methods[m];
	else
	{
		LOG(LOG_ERROR,"Requested invalid method");
		return NULL;
	}
}

//We follow the Boolean() algorithm, but return a concrete result, not a Boolean object
bool lightspark::Boolean_concrete(ASObject* obj)
{
	if(obj->getObjectType()==T_STRING)
	{
		LOG(LOG_CALLS,"String to bool");
		tiny_string s=obj->toString();
		if(s.len()==0)
			return false;
		else
			return true;
	}
	else if(obj->getObjectType()==T_BOOLEAN)
	{
		Boolean* b=static_cast<Boolean*>(obj);
		LOG(LOG_CALLS,"Boolean to bool " << b->val);
		return b->val;
	}
	else if(obj->getObjectType()==T_OBJECT)
	{
		LOG(LOG_CALLS,"Object to bool");
		return true;
	}
	else if(obj->getObjectType()==T_CLASS)
	{
		LOG(LOG_CALLS,"Class to bool");
		return true;
	}
	else if(obj->getObjectType()==T_ARRAY)
	{
		LOG(LOG_CALLS,"Array to bool");
		return true;
	}
	else if(obj->getObjectType()==T_UNDEFINED)
	{
		LOG(LOG_CALLS,"Undefined to bool");
		return false;
	}
	else if(obj->getObjectType()==T_NULL)
	{
		LOG(LOG_CALLS,"Null to bool");
		return false;
	}
	else
		return false;
}

ASObject* ABCVm::newCatch(call_context* th, int n)
{
	LOG(LOG_NOT_IMPLEMENTED,"newCatch " << n);
	return new Undefined;
}

void ABCVm::not_impl(int n)
{
	LOG(LOG_NOT_IMPLEMENTED, "not implement opcode 0x" << hex << n );
	abort();
}

void ABCVm::newArray(call_context* th, int n)
{
	LOG(LOG_CALLS, "newArray " << n );
	Array* ret=Class<Array>::getInstanceS(true);
	ret->resize(n);
	for(int i=0;i<n;i++)
		ret->set(n-i-1,th->runtime_stack_pop());

	th->runtime_stack_push(ret->obj);
}

ASObject* ABCVm::getScopeObject(call_context* th, int n)
{
	ASObject* ret=th->scope_stack[n];
	ret->incRef();
	LOG(LOG_CALLS, "getScopeObject: " << ret );
	return ret;
}

ASObject* ABCVm::pushString(call_context* th, int n)
{
	tiny_string s=th->context->getString(n); 
	LOG(LOG_CALLS, "pushString " << s );
	return Class<ASString>::getInstanceS(true,s)->obj;
}

void call_context::runtime_stack_push(ASObject* s)
{
	stack[stack_index++]=s;
}

ASObject* call_context::runtime_stack_pop()
{
	if(stack_index==0)
	{
		LOG(LOG_ERROR,"Empty stack");
		abort();
	}
	ASObject* ret=stack[--stack_index];
	return ret;
}

ASObject* call_context::runtime_stack_peek()
{
	if(stack_index==0)
	{
		LOG(LOG_ERROR,"Empty stack");
		return NULL;
	}
	return stack[stack_index-1];
}

call_context::call_context(method_info* th, int level, ASObject** args, int num_args):cur_level_of_this(level)
{
	locals=new ASObject*[th->body->local_count+1];
	locals_size=th->body->local_count;
	memset(locals,0,sizeof(ASObject*)*locals_size);
	if(args)
		memcpy(locals+1,args,num_args*sizeof(ASObject*));
	stack=new ASObject*[th->body->max_stack];
	stack_index=0;
	context=th->context;
}

call_context::~call_context()
{
	assert(stack_index==0);

	for(int i=0;i<locals_size;i++)
	{
		if(locals[i])
			locals[i]->decRef();
	}
	delete[] locals;
	delete[] stack;

	for(int i=0;i<scope_stack.size();i++)
		scope_stack[i]->decRef();
}

void ABCContext::exec()
{
	//Take script entries and declare their traits
	int i=0;
	for(i;i<scripts.size()-1;i++)
	{
		LOG(LOG_CALLS, "Script N: " << i );
		method_info* m=get_method(scripts[i].init);

		LOG(LOG_CALLS, "Building script traits: " << scripts[i].trait_count );
		SyntheticFunction* mf=new SyntheticFunction(m);
		for(int j=0;j<scripts[i].trait_count;j++)
			buildTrait(&sys->currentVm->Global,&scripts[i].traits[j],mf);
	}
	//The last script entry has to be run
	LOG(LOG_CALLS, "Last script (Entry Point)");
	method_info* m=get_method(scripts[i].init);
	IFunction* entry=new SyntheticFunction(m);
	LOG(LOG_CALLS, "Building entry script traits: " << scripts[i].trait_count );
	for(int j=0;j<scripts[i].trait_count;j++)
		buildTrait(&sys->currentVm->Global,&scripts[i].traits[j]);
	entry->call(&sys->currentVm->Global,NULL,sys->currentVm->Global.max_level);
	LOG(LOG_CALLS, "End of Entry Point");

}

void ABCVm::Run(ABCVm* th)
{
	sys=th->m_sys;
	iManager=th->int_manager;
	dManager=th->number_manager;
	llvm::InitializeNativeTarget();
	th->module=new llvm::Module(llvm::StringRef("abc jit"),th->llvm_context);
	llvm::ExistingModuleProvider mp(th->module);
	llvm::EngineBuilder eb(&mp);
	eb.setEngineKind(llvm::EngineKind::JIT);
	eb.setOptLevel(llvm::CodeGenOpt::Default);
	th->ex=eb.create();
	assert(th->ex);

	th->FPM=new llvm::FunctionPassManager(&mp);
      
	th->FPM->add(new llvm::TargetData(*th->ex->getTargetData()));
#ifndef NDEBUG
	//This is pretty heavy, do not enable in release
	th->FPM->add(llvm::createVerifierPass());
#endif
	th->FPM->add(llvm::createPromoteMemoryToRegisterPass());
	th->FPM->add(llvm::createReassociatePass());
	th->FPM->add(llvm::createCFGSimplificationPass());
	th->FPM->add(llvm::createGVNPass());
	th->FPM->add(llvm::createInstructionCombiningPass());
	th->FPM->add(llvm::createLICMPass());
	th->FPM->add(llvm::createDeadStoreEliminationPass());

	th->registerFunctions();
	th->registerClasses();

	while(1)
	{
#ifndef WIN32
		timespec ts,td;
		clock_gettime(CLOCK_REALTIME,&ts);
#endif
		sem_wait(&th->sem_event_count);
		th->handleEvent();
		sys->fps_prof->event_count++;
#ifndef WIN32
		clock_gettime(CLOCK_REALTIME,&td);
		sys->fps_prof->event_time+=timeDiff(ts,td);
#endif
		if(th->shutdown)
			break;
	}
	mp.releaseModule();
}

tiny_string ABCContext::getString(unsigned int s) const
{
	if(s)
		return constant_pool.strings[s];
	else
		return "";
}

void ABCContext::buildInstanceTraits(ASObject* obj, int class_index)
{
	for(int i=0;i<instances[class_index].trait_count;i++)
		buildTrait(obj,&instances[class_index].traits[i]);
}

void ABCContext::linkTrait(ASObject* obj, const traits_info* t)
{
	const multiname* mname=getMultiname(t->name,NULL);
	//Should be a Qname
	assert(mname->ns.size()==1);

	const tiny_string& name=mname->name_s;
	const tiny_string& ns=mname->ns[0].name;
	if(t->kind>>4)
		LOG(LOG_CALLS,"Next slot has flags " << (t->kind>>4));
	switch(t->kind&0xf)
	{
		//Link the methods to the implementations
		case traits_info::Method:
		{
			LOG(LOG_CALLS,"Method trait: " << ns << "::" << name << " #" << t->method);
			method_info* m=&methods[t->method];
			assert(m->body==0);
			int level=obj->max_level;
			obj_var* var=NULL;
			do
			{
				var=obj->Variables.findObjVar(name,"",level,false);
				level--;
			}
			while(var==NULL && level>=0);
			if(var)
			{
				assert(var);
				assert(var->var);

				var->var->incRef();
				obj->setVariableByQName(name,ns,var->var,false);
			}
			else
			{
				LOG(LOG_NOT_IMPLEMENTED,"Method not linkable");
			}

			LOG(LOG_TRACE,"End Method trait: " << ns << "::" << name);
			break;
		}
		case traits_info::Getter:
		{
			LOG(LOG_CALLS,"Getter trait: " << ns << "::" << name);
			method_info* m=&methods[t->method];
			assert(m->body==0);
			int level=obj->max_level;
			obj_var* var=NULL;
			do
			{
				var=obj->Variables.findObjVar(name,"",level,false);
				level--;
			}
			while((var==NULL || var->getter==NULL) && level>=0);
			if(var)
			{
				assert(var);
				assert(var->getter);

				var->getter->incRef();
				obj->setGetterByQName(name,ns,var->getter);
			}
			else
			{
				LOG(LOG_NOT_IMPLEMENTED,"Getter not linkable");
			}
			
			LOG(LOG_TRACE,"End Getter trait: " << ns << "::" << name);
			break;
		}
		case traits_info::Setter:
		{
			LOG(LOG_CALLS,"Setter trait: " << ns << "::" << name << " #" << t->method);
			method_info* m=&methods[t->method];
			assert(m->body==0);
			int level=obj->max_level;
			obj_var* var=NULL;
			do
			{
				var=obj->Variables.findObjVar(name,"",level,false);
				level--;
			}
			while((var==NULL || var->setter==NULL) && level>=0);
			if(var)
			{
				assert(var);
				assert(var->setter);

				var->setter->incRef();
				obj->setSetterByQName(name,ns,var->setter);
			}
			else
			{
				LOG(LOG_NOT_IMPLEMENTED,"Setter not linkable");
			}
			
			LOG(LOG_TRACE,"End Setter trait: " << ns << "::" << name);
			break;
		}
//		case traits_info::Class:
//		case traits_info::Const:
//		case traits_info::Slot:
		default:
			LOG(LOG_ERROR,"Trait not supported " << name << " " << t->kind);
			abort();
			//obj->setVariableByQName(name, ns, new Undefined);
	}
}

ASObject* ABCContext::getConstant(int kind, int index)
{
	switch(kind)
	{
		case 0x01: //String
			return Class<ASString>::getInstanceS(true,constant_pool.strings[index])->obj;
		case 0x03: //Int
			return new Integer(constant_pool.integer[index]);
		case 0x06: //Double
			return new Number(constant_pool.doubles[index]);
		case 0x08: //Namespace
			assert(constant_pool.namespaces[index].name);
			return Class<Namespace>::getInstanceS(true,getString(constant_pool.namespaces[index].name))->obj;
		case 0x0a: //False
			return new Boolean(false);
		case 0x0b: //True
			return new Boolean(true);
		case 0x0c: //Null
			return new Null;
		default:
		{
			LOG(LOG_ERROR,"Constant kind " << hex << kind);
			abort();
		}
	}
}

void ABCContext::buildTrait(ASObject* obj, const traits_info* t, IFunction* deferred_initialization)
{
	const multiname* mname=getMultiname(t->name,NULL);
	//Should be a Qname
	assert(mname->ns.size()==1);

	const tiny_string& name=mname->name_s;
	const tiny_string& ns=mname->ns[0].name;

	if(t->kind>>4)
		LOG(LOG_CALLS,"Next slot has flags " << (t->kind>>4));
	switch(t->kind&0xf)
	{
		case traits_info::Class:
		{
			ASObject* ret;
			if(deferred_initialization)
				ret=new ScriptDefinable(deferred_initialization);
			else
				ret=new Undefined;

			obj->setVariableByQName(name, ns, ret, false);
			
			LOG(LOG_CALLS,"Slot "<< t->slot_id << " type Class name " << ns << "::" << name << " id " << t->classi);
			if(t->slot_id)
				obj->initSlot(t->slot_id, name, ns);
			break;
		}
		case traits_info::Getter:
		{
			LOG(LOG_CALLS,"Getter trait: " << ns << "::" << name << " #" << t->method);
			//syntetize method and create a new LLVM function object
			method_info* m=&methods[t->method];
			IFunction* f=new SyntheticFunction(m);

			//We have to override if there is a method with the same name,
			//even if the namespace are different, if both are protected
			if(obj->actualPrototype && t->kind&0x20 && 
				obj->actualPrototype->use_protected && ns==obj->actualPrototype->protected_ns)
			{
				//Walk the super chain and find variables to override
				Class_base* cur=obj->actualPrototype->super;
				for(int i=(obj->max_level-1);i>=0;i--)
				{
					assert(cur);
					if(cur->use_protected)
					{
						obj_var* var=obj->Variables.findObjVar(name,cur->protected_ns,i,false);
						if(var)
						{
							//A superclass defined a protected method that we have to override.
							//TODO: incref variable?
							obj->setGetterByQName(name,cur->protected_ns,f);
						}
					}
					cur=cur->super;
				}
			}
			obj->setGetterByQName(name,ns,f);
			
			LOG(LOG_TRACE,"End Getter trait: " << ns << "::" << name);
			break;
		}
		case traits_info::Setter:
		{
			LOG(LOG_CALLS,"Setter trait: " << ns << "::" << name << " #" << t->method);
			//syntetize method and create a new LLVM function object
			method_info* m=&methods[t->method];

			IFunction* f=new SyntheticFunction(m);

			//We have to override if there is a method with the same name,
			//even if the namespace are different, if both are protected
			if(obj->actualPrototype && t->kind&0x20 && 
				obj->actualPrototype->use_protected && ns==obj->actualPrototype->protected_ns)
			{
				//Walk the super chain and find variables to override
				Class_base* cur=obj->actualPrototype->super;
				for(int i=(obj->max_level-1);i>=0;i--)
				{
					assert(cur);
					if(cur->use_protected)
					{
						obj_var* var=obj->Variables.findObjVar(name,cur->protected_ns,i,false);
						if(var)
						{
							//A superclass defined a protected method that we have to override.
							//TODO: incref variable?
							obj->setSetterByQName(name,cur->protected_ns,f);
						}
					}
					cur=cur->super;
				}
			}
			obj->setSetterByQName(name,ns,f);
			
			LOG(LOG_TRACE,"End Setter trait: " << ns << "::" << name);
			break;
		}
		case traits_info::Method:
		{
			LOG(LOG_CALLS,"Method trait: " << ns << "::" << name << " #" << t->method);
			//syntetize method and create a new LLVM function object
			method_info* m=&methods[t->method];
			IFunction* f=new SyntheticFunction(m);

			//We have to override if there is a method with the same name,
			//even if the namespace are different, if both are protected
			if(obj->actualPrototype && t->kind&0x20 && 
				obj->actualPrototype->use_protected && ns==obj->actualPrototype->protected_ns)
			{
				//Walk the super chain and find variables to override
				Class_base* cur=obj->actualPrototype->super;
				for(int i=(obj->max_level-1);i>=0;i--)
				{
					assert(cur);
					if(cur->use_protected)
					{
						obj_var* var=obj->Variables.findObjVar(name,cur->protected_ns,i,false);
						if(var)
						{
							//A superclass defined a protected method that we have to override.
							//TODO: incref variable?
							obj->setVariableByQName(name,cur->protected_ns,f,false);
						}
					}
					cur=cur->super;
				}
			}

			obj->setVariableByQName(name,ns,f,false);
			LOG(LOG_TRACE,"End Method trait: " << ns << "::" << name);
			break;
		}
		case traits_info::Const:
		{
			multiname* type=getMultiname(t->type_name,NULL);
			ASObject* ret;
			//If the index is valid we set the constant
			if(t->vindex)
			{
				ret=getConstant(t->vkind,t->vindex);
				obj->setVariableByQName(name, ns, ret, false);
				if(t->slot_id)
					obj->initSlot(t->slot_id, name, ns);
			}
			else
			{
				ret=obj->getVariableByQName(name,ns).obj;
				assert(ret==NULL);

				assert(deferred_initialization==NULL);

				ret=new Undefined;
				obj->setVariableByQName(name, ns, ret, false);
			}
			LOG(LOG_CALLS,"Const "<<name<<" type "<<*type);
			if(t->slot_id)
				obj->initSlot(t->slot_id, name,ns );
			break;
		}
		case traits_info::Slot:
		{
			multiname* type=getMultiname(t->type_name,NULL);
			if(t->vindex)
			{
				ASObject* ret=getConstant(t->vkind,t->vindex);
				obj->setVariableByQName(name, ns, ret, false);
				if(t->slot_id)
					obj->initSlot(t->slot_id, name, ns);

				LOG(LOG_CALLS,"Slot " << t->slot_id << ' ' <<name<<" type "<<*type);
				break;
			}
			else
			{
				//else fallthrough
				LOG(LOG_CALLS,"Slot "<< t->slot_id<<  " vindex 0 "<<name<<" type "<<*type);
				objAndLevel previous_definition=obj->getVariableByQName(name,ns);
				if(previous_definition.obj)
				{
					if(obj->actualPrototype)
						assert(previous_definition.level<obj->actualPrototype->max_level);
					else
						assert(previous_definition.level<obj->max_level);
				}

				ASObject* ret;
				if(deferred_initialization)
					ret=new ScriptDefinable(deferred_initialization);
				else
				{
					//TODO: find nice way to handle default construction
					if(type->name_type==multiname::NAME_STRING && type->name_s=="int" && 
							type->ns.size()==1 && type->ns[0].name=="")
						ret=abstract_i(0);
					else
						ret=new Undefined;
				}
				obj->setVariableByQName(name, ns, ret, false);

				if(t->slot_id)
					obj->initSlot(t->slot_id, name,ns );
				break;
			}
		}
		default:
			LOG(LOG_ERROR,"Trait not supported " << name << " " << t->kind);
			obj->setVariableByQName(name, ns, new Undefined, false);
	}
}


ASObject* method_info::getOptional(int i)
{
	assert(i<options.size());
	return context->getConstant(options[i].kind,options[i].val);
}

istream& lightspark::operator>>(istream& in, u32& v)
{
	int i=0;
	uint8_t t,t2;
	v.val=0;
	do
	{
		in.read((char*)&t,1);
		t2=t;
		t&=0x7f;

		v.val|=(t<<i);
		i+=7;
		if(i==35)
		{
			if(t>15)
				LOG(LOG_ERROR,"parsing u32");
			break;
		}
	}
	while(t2&0x80);
	return in;
}

istream& lightspark::operator>>(istream& in, s32& v)
{
	int i=0;
	uint8_t t,t2;
	v.val=0;
	do
	{
		in.read((char*)&t,1);
		t2=t;
		t&=0x7f;

		v.val|=(t<<i);
		i+=7;
		if(i==35)
		{
			if(t>15)
			{
				LOG(LOG_ERROR,"parsing s32");
			}
			break;
		}
	}
	while(t2&0x80);
/*	//Sign extension usage not clear at all
	if(t2&0x40)
	{
		//Sign extend
		for(i;i<32;i++)
			v.val|=(1<<i);
	}*/
	return in;
}

istream& lightspark::operator>>(istream& in, s24& v)
{
	int i=0;
	v.val=0;
	uint8_t t;
	for(i=0;i<24;i+=8)
	{
		in.read((char*)&t,1);
		v.val|=(t<<i);
	}

	if(t&0x80)
	{
		//Sign extend
		for(i;i<32;i++)
			v.val|=(1<<i);
	}
	return in;
}

istream& lightspark::operator>>(istream& in, u30& v)
{
	int i=0;
	uint8_t t,t2;
	v.val=0;
	do
	{
		in.read((char*)&t,1);
		t2=t;
		t&=0x7f;

		v.val|=(t<<i);
		i+=7;
		if(i>29)
			LOG(LOG_ERROR,"parsing u30");
	}
	while(t2&0x80);
	if(v.val&0xc0000000)
			LOG(LOG_ERROR,"parsing u30");
	return in;
}

istream& lightspark::operator>>(istream& in, u8& v)
{
	uint8_t t;
	in.read((char*)&t,1);
	v.val=t;
	return in;
}

istream& lightspark::operator>>(istream& in, u16& v)
{
	uint16_t t;
	in.read((char*)&t,2);
	v.val=t;
	return in;
}

istream& lightspark::operator>>(istream& in, d64& v)
{
	//Should check if this is right
	in.read((char*)&v.val,8);
	return in;
}

istream& lightspark::operator>>(istream& in, string_info& v)
{
	in >> v.size;
	//TODO: String are expected to be UTF-8 encoded.
	//This temporary implementation assume ASCII, so fail if high bit is set
	uint8_t t;
	string tmp;
	tmp.reserve(v.size);
	for(int i=0;i<v.size;i++)
	{
		in.read((char*)&t,1);
		tmp.push_back(t);
		if(t&0x80)
			LOG(LOG_NOT_IMPLEMENTED,"Multibyte not handled");
	}
	v.val=tmp.c_str();
	return in;
}

istream& lightspark::operator>>(istream& in, namespace_info& v)
{
	in >> v.kind >> v.name;
	if(v.kind!=0x05 && v.kind!=0x08 && v.kind!=0x16 && v.kind!=0x17 && v.kind!=0x18 && v.kind!=0x19 && v.kind!=0x1a)
	{
		LOG(LOG_ERROR,"Unexpected namespace kind");
		abort();
	}
	return in;
}

istream& lightspark::operator>>(istream& in, method_body_info& v)
{
	in >> v.method >> v.max_stack >> v.local_count >> v.init_scope_depth >> v.max_scope_depth >> v.code_length;
	v.code.resize(v.code_length);
	for(int i=0;i<v.code_length;i++)
		in.read(&v.code[i],1);

	in >> v.exception_count;
	v.exceptions.resize(v.exception_count);
	for(int i=0;i<v.exception_count;i++)
		in >> v.exceptions[i];

	in >> v.trait_count;
	v.traits.resize(v.trait_count);
	for(int i=0;i<v.trait_count;i++)
		in >> v.traits[i];
	return in;
}

istream& lightspark::operator >>(istream& in, ns_set_info& v)
{
	in >> v.count;

	v.ns.resize(v.count);
	for(int i=0;i<v.count;i++)
	{
		in >> v.ns[i];
		if(v.ns[i]==0)
			LOG(LOG_ERROR,"0 not allowed");
	}
	return in;
}

istream& lightspark::operator>>(istream& in, multiname_info& v)
{
	in >> v.kind;

	switch(v.kind)
	{
		case 0x07:
		case 0x0d:
			in >> v.ns >> v.name;
			break;
		case 0x0f:
		case 0x10:
			in >> v.name;
			break;
		case 0x11:
		case 0x12:
			break;
		case 0x09:
		case 0x0e:
			in >> v.name >> v.ns_set;
			break;
		case 0x1b:
		case 0x1c:
			in >> v.ns_set;
			break;
		case 0x1d:
		{
			in >> v.type_definition;
			u8 num_params;
			in >> num_params;
			v.param_types.resize(num_params);
			for(int i=0;i<num_params;i++)
			{
				u30 t;
				in >> t;
				v.param_types[i]=t;
			}
			break;
		}
		default:
			LOG(LOG_ERROR,"Unexpected multiname kind " << hex << v.kind);
			abort();
			break;
	}
	return in;
}

istream& lightspark::operator>>(istream& in, method_info& v)
{
	in >> v.param_count;
	in >> v.return_type;

	v.param_type.resize(v.param_count);
	for(int i=0;i<v.param_count;i++)
		in >> v.param_type[i];
	
	in >> v.name >> v.flags;
	if(v.flags&0x08)
	{
		in >> v.option_count;
		v.options.resize(v.option_count);
		for(int i=0;i<v.option_count;i++)
		{
			in >> v.options[i].val >> v.options[i].kind;
			if(v.options[i].kind>0x1a)
				LOG(LOG_ERROR,"Unexpected options type");
		}
	}
	if(v.flags&0x80)
	{
		v.param_names.resize(v.param_count);
		for(int i=0;i<v.param_count;i++)
			in >> v.param_names[i];
	}
	return in;
}

istream& lightspark::operator>>(istream& in, script_info& v)
{
	in >> v.init >> v.trait_count;
	v.traits.resize(v.trait_count);
	for(int i=0;i<v.trait_count;i++)
		in >> v.traits[i];
	return in;
}

istream& lightspark::operator>>(istream& in, class_info& v)
{
	in >> v.cinit >> v.trait_count;
	v.traits.resize(v.trait_count);
	for(int i=0;i<v.trait_count;i++)
	{
		in >> v.traits[i];
	}
	return in;
}

istream& lightspark::operator>>(istream& in, metadata_info& v)
{
	in >> v.name;
	in >> v.item_count;

	v.items.resize(v.item_count);
	for(int i=0;i<v.item_count;i++)
	{
		in >> v.items[i].key >> v.items[i].value;
	}
	return in;
}

istream& lightspark::operator>>(istream& in, traits_info& v)
{
	in >> v.name >> v.kind;
	switch(v.kind&0xf)
	{
		case traits_info::Slot:
		case traits_info::Const:
			in >> v.slot_id >> v.type_name >> v.vindex;
			if(v.vindex)
				in >> v.vkind;
			break;
		case traits_info::Class:
			in >> v.slot_id >> v.classi;
			break;
		case traits_info::Function:
			in >> v.slot_id >> v.function;
			break;
		case traits_info::Getter:
		case traits_info::Setter:
		case traits_info::Method:
			in >> v.disp_id >> v.method;
			break;
		default:
			LOG(LOG_ERROR,"Unexpected kind " << v.kind);
			break;
	}

	if(v.kind&traits_info::Metadata)
	{
		in >> v.metadata_count;
		v.metadata.resize(v.metadata_count);
		for(int i=0;i<v.metadata_count;i++)
			in >> v.metadata[i];
	}
	return in;
}

istream& lightspark::operator >>(istream& in, exception_info& v)
{
	in >> v.from >> v.to >> v.target >> v.exc_type >> v.var_name;
	return in;
}

istream& lightspark::operator>>(istream& in, instance_info& v)
{
	in >> v.name >> v.supername >> v.flags;
	if(v.flags&instance_info::ClassProtectedNs)
		in >> v.protectedNs;

	in >> v.interface_count;
	v.interfaces.resize(v.interface_count);
	for(int i=0;i<v.interface_count;i++)
	{
		in >> v.interfaces[i];
		if(v.interfaces[i]==0)
			abort();
	}

	in >> v.init;

	in >> v.trait_count;
	v.traits.resize(v.trait_count);
	for(int i=0;i<v.trait_count;i++)
		in >> v.traits[i];
	return in;
}

istream& lightspark::operator>>(istream& in, cpool_info& v)
{
	in >> v.int_count;
	v.integer.resize(v.int_count);
	for(int i=1;i<v.int_count;i++)
		in >> v.integer[i];

	in >> v.uint_count;
	v.uinteger.resize(v.uint_count);
	for(int i=1;i<v.uint_count;i++)
		in >> v.uinteger[i];

	in >> v.double_count;
	v.doubles.resize(v.double_count);
	for(int i=1;i<v.double_count;i++)
		in >> v.doubles[i];

	in >> v.string_count;
	v.strings.resize(v.string_count);
	for(int i=1;i<v.string_count;i++)
		in >> v.strings[i];

	in >> v.namespace_count;
	v.namespaces.resize(v.namespace_count);
	for(int i=1;i<v.namespace_count;i++)
		in >> v.namespaces[i];

	in >> v.ns_set_count;
	v.ns_sets.resize(v.ns_set_count);
	for(int i=1;i<v.ns_set_count;i++)
		in >> v.ns_sets[i];

	in >> v.multiname_count;
	v.multinames.resize(v.multiname_count);
	for(int i=1;i<v.multiname_count;i++)
		in >> v.multinames[i];

	return in;
}

ASFUNCTIONBODY(lightspark,_int)
{
	//Int is specified as 32bit
	return abstract_i(args->at(0)->toInt()&0xffffffff);
}

ASFUNCTIONBODY(lightspark,parseInt)
{
	if(args->at(0)->getObjectType()==T_UNDEFINED)
		return new Undefined;
	else
	{
		return new Integer(atoi(args->at(0)->toString().raw_buf()));
	}
}

ASFUNCTIONBODY(lightspark,parseFloat)
{
	if(args->at(0)->getObjectType()==T_UNDEFINED)
		return new Undefined;
	else
	{
		return new Integer(atof(args->at(0)->toString().raw_buf()));
	}
}

intptr_t ABCVm::s_toInt(ASObject* o)
{
	if(o->getObjectType()!=T_INTEGER)
		abort();
	intptr_t ret=o->toInt();
	o->decRef();
	return ret;
}

ASFUNCTIONBODY(lightspark,isNaN)
{
	if(args->at(0)->getObjectType()==T_UNDEFINED)
		return abstract_b(true);
	else if(args->at(0)->getObjectType()==T_INTEGER)
		return abstract_b(false);
	else if(args->at(0)->getObjectType()==T_NUMBER)
		return abstract_b(false);
	else
		abort();
}

ASFUNCTIONBODY(lightspark,undefinedFunction)
{
	LOG(LOG_NOT_IMPLEMENTED,"Function not implemented");
	return NULL;
}
