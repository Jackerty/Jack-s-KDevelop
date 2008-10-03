//This is file has been generated by xmltokross, you should not edit this file but the files used to generate it.

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <kross/core/manager.h>
#include <kross/core/wrapperinterface.h>
#include <language/duchain/identifier.h>
#include <language/duchain/indexedstring.h>

class KrossKDevelopIndexedIdentifier : public QObject, public Kross::WrapperInterface
{
	Q_OBJECT
	public:
		KrossKDevelopIndexedIdentifier(KDevelop::IndexedIdentifier* obj, QObject* parent=0) : QObject(parent), wrapped(obj)		{ setObjectName("KDevelop::IndexedIdentifier"); }
		void* wrappedObject() const { return wrapped; }

		Q_SCRIPTABLE KDevelop::IndexedIdentifier operator=(const KDevelop::Identifier& x0) { return wrapped->operator=(x0); }
		Q_SCRIPTABLE bool operator==(const KDevelop::IndexedIdentifier& x0) const { return wrapped->operator==(x0); }
		Q_SCRIPTABLE bool operator!=(const KDevelop::IndexedIdentifier& x0) const { return wrapped->operator!=(x0); }
		Q_SCRIPTABLE bool operator==(const KDevelop::Identifier& x0) const { return wrapped->operator==(x0); }
		Q_SCRIPTABLE bool isEmpty() const { return wrapped->isEmpty(); }
		Q_SCRIPTABLE KDevelop::Identifier identifier() const { return wrapped->identifier(); }
		Q_PROPERTY(unsigned int  index READ getindex WRITE setindex SCRIPTABLE true)
		Q_SCRIPTABLE void setindex(const unsigned int  val) { wrapped->index=val; }
		Q_SCRIPTABLE unsigned int  getindex() const { return wrapped->index; }
	private:
		KDevelop::IndexedIdentifier* wrapped;
};

class KrossKDevelopIndexedQualifiedIdentifier : public QObject, public Kross::WrapperInterface
{
	Q_OBJECT
	public:
		KrossKDevelopIndexedQualifiedIdentifier(KDevelop::IndexedQualifiedIdentifier* obj, QObject* parent=0) : QObject(parent), wrapped(obj)		{ setObjectName("KDevelop::IndexedQualifiedIdentifier"); }
		void* wrappedObject() const { return wrapped; }

		Q_SCRIPTABLE KDevelop::IndexedQualifiedIdentifier operator=(const KDevelop::QualifiedIdentifier& x0) { return wrapped->operator=(x0); }
		Q_SCRIPTABLE bool operator==(const KDevelop::IndexedQualifiedIdentifier& x0) const { return wrapped->operator==(x0); }
		Q_SCRIPTABLE bool operator==(const KDevelop::QualifiedIdentifier& x0) const { return wrapped->operator==(x0); }
		Q_SCRIPTABLE bool isValid() const { return wrapped->isValid(); }
		Q_SCRIPTABLE KDevelop::QualifiedIdentifier identifier() const { return wrapped->identifier(); }
		Q_PROPERTY(unsigned int  index READ getindex WRITE setindex SCRIPTABLE true)
		Q_SCRIPTABLE void setindex(const unsigned int  val) { wrapped->index=val; }
		Q_SCRIPTABLE unsigned int  getindex() const { return wrapped->index; }
	private:
		KDevelop::IndexedQualifiedIdentifier* wrapped;
};

class KrossKDevelopIndexedTypeIdentifier : public QObject, public Kross::WrapperInterface
{
	Q_OBJECT
	public:
		KrossKDevelopIndexedTypeIdentifier(KDevelop::IndexedTypeIdentifier* obj, QObject* parent=0) : QObject(parent), wrapped(obj)		{ setObjectName("KDevelop::IndexedTypeIdentifier"); }
		void* wrappedObject() const { return wrapped; }

		Q_SCRIPTABLE KDevelop::IndexedTypeIdentifier operator=(const KDevelop::TypeIdentifier& x0) { return wrapped->operator=(x0); }
		Q_SCRIPTABLE bool operator==(const KDevelop::IndexedTypeIdentifier& x0) const { return wrapped->operator==(x0); }
		Q_SCRIPTABLE bool operator==(const KDevelop::TypeIdentifier& x0) const { return wrapped->operator==(x0); }
		Q_SCRIPTABLE bool isValid() const { return wrapped->isValid(); }
		Q_SCRIPTABLE KDevelop::TypeIdentifier identifier() const { return wrapped->identifier(); }
		Q_PROPERTY(unsigned int  index READ getindex WRITE setindex SCRIPTABLE true)
		Q_SCRIPTABLE void setindex(const unsigned int  val) { wrapped->index=val; }
		Q_SCRIPTABLE unsigned int  getindex() const { return wrapped->index; }
	private:
		KDevelop::IndexedTypeIdentifier* wrapped;
};

class KrossKDevelopIdentifier : public QObject, public Kross::WrapperInterface
{
	Q_OBJECT
	public:
		KrossKDevelopIdentifier(KDevelop::Identifier* obj, QObject* parent=0) : QObject(parent), wrapped(obj)		{ setObjectName("KDevelop::Identifier"); }
		void* wrappedObject() const { return wrapped; }

		Q_SCRIPTABLE KDevelop::Identifier unique(int x0) { return wrapped->unique(x0); }
		Q_SCRIPTABLE bool isUnique() const { return wrapped->isUnique(); }
		Q_SCRIPTABLE int uniqueToken() const { return wrapped->uniqueToken(); }
		Q_SCRIPTABLE void setUnique(int x0) { wrapped->setUnique(x0); }
		Q_SCRIPTABLE KDevelop::IndexedString identifier() const { return wrapped->identifier(); }
		Q_SCRIPTABLE void setIdentifier(const QString& x0) { wrapped->setIdentifier(x0); }
		Q_SCRIPTABLE void setIdentifier(const KDevelop::IndexedString& x0) { wrapped->setIdentifier(x0); }
		Q_SCRIPTABLE unsigned int hash() const { return wrapped->hash(); }
		Q_SCRIPTABLE bool nameEquals(const KDevelop::Identifier& x0) const { return wrapped->nameEquals(x0); }
		Q_SCRIPTABLE KDevelop::TypeIdentifier templateIdentifier(int x0) const { return wrapped->templateIdentifier(x0); }
		Q_SCRIPTABLE unsigned int templateIdentifiersCount() const { return wrapped->templateIdentifiersCount(); }
		Q_SCRIPTABLE void appendTemplateIdentifier(const KDevelop::TypeIdentifier& x0) { wrapped->appendTemplateIdentifier(x0); }
		Q_SCRIPTABLE void clearTemplateIdentifiers() { wrapped->clearTemplateIdentifiers(); }
		Q_SCRIPTABLE void setTemplateIdentifiers(const QList< KDevelop::TypeIdentifier >& x0) { wrapped->setTemplateIdentifiers(x0); }
		Q_SCRIPTABLE QString toString() const { return wrapped->toString(); }
		Q_SCRIPTABLE bool operator==(const KDevelop::Identifier& x0) const { return wrapped->operator==(x0); }
		Q_SCRIPTABLE bool operator!=(const KDevelop::Identifier& x0) const { return wrapped->operator!=(x0); }
		Q_SCRIPTABLE KDevelop::Identifier operator=(const KDevelop::Identifier& x0) { return wrapped->operator=(x0); }
		Q_SCRIPTABLE bool isEmpty() const { return wrapped->isEmpty(); }
		Q_SCRIPTABLE unsigned int index() const { return wrapped->index(); }
	private:
		KDevelop::Identifier* wrapped;
};

class KrossKDevelopQualifiedIdentifier : public QObject, public Kross::WrapperInterface
{
	Q_OBJECT
	public:
		KrossKDevelopQualifiedIdentifier(KDevelop::QualifiedIdentifier* obj, QObject* parent=0) : QObject(parent), wrapped(obj)		{ setObjectName("KDevelop::QualifiedIdentifier"); }
		void* wrappedObject() const { return wrapped; }

		Q_SCRIPTABLE void push(const KDevelop::Identifier& x0) { wrapped->push(x0); }
		Q_SCRIPTABLE void push(const KDevelop::QualifiedIdentifier& x0) { wrapped->push(x0); }
		Q_SCRIPTABLE void pop() { wrapped->pop(); }
		Q_SCRIPTABLE void clear() { wrapped->clear(); }
		Q_SCRIPTABLE bool isEmpty() const { return wrapped->isEmpty(); }
		Q_SCRIPTABLE int count() const { return wrapped->count(); }
		Q_SCRIPTABLE KDevelop::Identifier first() const { return wrapped->first(); }
		Q_SCRIPTABLE KDevelop::Identifier last() const { return wrapped->last(); }
		Q_SCRIPTABLE KDevelop::Identifier top() const { return wrapped->top(); }
		Q_SCRIPTABLE KDevelop::Identifier at(int x0) const { return wrapped->at(x0); }
		Q_SCRIPTABLE KDevelop::QualifiedIdentifier mid(int x0, int x1=-1) const { return wrapped->mid(x0, x1); }
		Q_SCRIPTABLE KDevelop::QualifiedIdentifier left(int x0) const { return wrapped->left(x0); }
		Q_SCRIPTABLE bool explicitlyGlobal() const { return wrapped->explicitlyGlobal(); }
		Q_SCRIPTABLE void setExplicitlyGlobal(bool x0) { wrapped->setExplicitlyGlobal(x0); }
		Q_SCRIPTABLE bool isQualified() const { return wrapped->isQualified(); }
		Q_SCRIPTABLE bool isExpression() const { return wrapped->isExpression(); }
		Q_SCRIPTABLE void setIsExpression(bool x0) { wrapped->setIsExpression(x0); }
		Q_SCRIPTABLE QString toString(bool x0=false) const { return wrapped->toString(x0); }
		Q_SCRIPTABLE QStringList toStringList() const { return wrapped->toStringList(); }
		Q_SCRIPTABLE KDevelop::QualifiedIdentifier operator+(const KDevelop::QualifiedIdentifier& x0) const { return wrapped->operator+(x0); }
		Q_SCRIPTABLE KDevelop::QualifiedIdentifier operator+=(const KDevelop::QualifiedIdentifier& x0) { return wrapped->operator+=(x0); }
		Q_SCRIPTABLE KDevelop::QualifiedIdentifier operator+(const KDevelop::Identifier& x0) const { return wrapped->operator+(x0); }
		Q_SCRIPTABLE KDevelop::QualifiedIdentifier operator+=(const KDevelop::Identifier& x0) { return wrapped->operator+=(x0); }
		Q_SCRIPTABLE KDevelop::QualifiedIdentifier merge(const KDevelop::QualifiedIdentifier& x0) const { return wrapped->merge(x0); }
		Q_SCRIPTABLE bool isSame(const KDevelop::QualifiedIdentifier& x0, bool x1=true) const { return wrapped->isSame(x0, x1); }
		Q_SCRIPTABLE unsigned int combineHash(unsigned int x0, unsigned int x1, KDevelop::Identifier x2) { return wrapped->combineHash(x0, x1, x2); }
		Q_SCRIPTABLE bool operator==(const KDevelop::QualifiedIdentifier& x0) const { return wrapped->operator==(x0); }
		Q_SCRIPTABLE bool operator!=(const KDevelop::QualifiedIdentifier& x0) const { return wrapped->operator!=(x0); }
		Q_SCRIPTABLE KDevelop::QualifiedIdentifier operator=(const KDevelop::QualifiedIdentifier& x0) { return wrapped->operator=(x0); }
		Q_SCRIPTABLE bool beginsWith(const KDevelop::QualifiedIdentifier& x0) const { return wrapped->beginsWith(x0); }
		Q_SCRIPTABLE unsigned int index() const { return wrapped->index(); }
		Q_SCRIPTABLE unsigned int hash() const { return wrapped->hash(); }
		Q_SCRIPTABLE void findByHash(unsigned int x0, KDevVarLengthArray< KDevelop::QualifiedIdentifier, 256 >& x1) { wrapped->findByHash(x0, x1); }
	private:
		KDevelop::QualifiedIdentifier* wrapped;
};

class KrossKDevelopTypeIdentifier : public KrossKDevelopQualifiedIdentifier
{
	Q_OBJECT
	public:
		KrossKDevelopTypeIdentifier(KDevelop::TypeIdentifier* obj, QObject* parent=0) : KrossKDevelopQualifiedIdentifier(obj, parent), wrapped(obj)
	{ setObjectName("KDevelop::TypeIdentifier"); }
		void* wrappedObject() const { return wrapped; }

		Q_SCRIPTABLE bool isReference() const { return wrapped->isReference(); }
		Q_SCRIPTABLE void setIsReference(bool x0) { wrapped->setIsReference(x0); }
		Q_SCRIPTABLE bool isConstant() const { return wrapped->isConstant(); }
		Q_SCRIPTABLE void setIsConstant(bool x0) { wrapped->setIsConstant(x0); }
		Q_SCRIPTABLE int pointerDepth() const { return wrapped->pointerDepth(); }
		Q_SCRIPTABLE void setPointerDepth(int x0) { wrapped->setPointerDepth(x0); }
		Q_SCRIPTABLE bool isConstPointer(int x0) const { return wrapped->isConstPointer(x0); }
		Q_SCRIPTABLE void setIsConstPointer(int x0, bool x1) { wrapped->setIsConstPointer(x0, x1); }
		Q_SCRIPTABLE bool isSame(const KDevelop::TypeIdentifier& x0, bool x1=true) const { return wrapped->isSame(x0, x1); }
		Q_SCRIPTABLE QString toString(bool x0=false) const { return wrapped->toString(x0); }
		Q_SCRIPTABLE bool operator==(const KDevelop::TypeIdentifier& x0) const { return wrapped->operator==(x0); }
		Q_SCRIPTABLE bool operator!=(const KDevelop::TypeIdentifier& x0) const { return wrapped->operator!=(x0); }
	private:
		KDevelop::TypeIdentifier* wrapped;
};

bool krossidentifier_registerHandler(const QByteArray& name, Kross::MetaTypeHandler::FunctionPtr* handler)
{ Kross::Manager::self().registerMetaTypeHandler(name, handler); return false; }

namespace Handlers
{
QVariant _kDevelopTypeIdentifierHandler(void* type)
{
	if(!type) return QVariant();
	KDevelop::TypeIdentifier* t=static_cast<KDevelop::TypeIdentifier*>(type);
	Q_ASSERT(dynamic_cast<KDevelop::TypeIdentifier*>(t));
	return qVariantFromValue((QObject*) new KrossKDevelopTypeIdentifier(t, 0));
}
bool b_KDevelopTypeIdentifier=krossidentifier_registerHandler("KDevelop::TypeIdentifier*", _kDevelopTypeIdentifierHandler);
QVariant kDevelopTypeIdentifierHandler(KDevelop::TypeIdentifier* type){ return _kDevelopTypeIdentifierHandler(type); }
QVariant kDevelopTypeIdentifierHandler(const KDevelop::TypeIdentifier* type) { return _kDevelopTypeIdentifierHandler((void*) type); }

QVariant _kDevelopQualifiedIdentifierHandler(void* type)
{
	if(!type) return QVariant();
	KDevelop::QualifiedIdentifier* t=static_cast<KDevelop::QualifiedIdentifier*>(type);
	Q_ASSERT(dynamic_cast<KDevelop::QualifiedIdentifier*>(t));
	if(dynamic_cast<KDevelop::TypeIdentifier*>(t)) return _kDevelopTypeIdentifierHandler(type);
	else return qVariantFromValue((QObject*) new KrossKDevelopQualifiedIdentifier(t, 0));
}
bool b_KDevelopQualifiedIdentifier=krossidentifier_registerHandler("KDevelop::QualifiedIdentifier*", _kDevelopQualifiedIdentifierHandler);
QVariant kDevelopQualifiedIdentifierHandler(KDevelop::QualifiedIdentifier* type){ return _kDevelopQualifiedIdentifierHandler(type); }
QVariant kDevelopQualifiedIdentifierHandler(const KDevelop::QualifiedIdentifier* type) { return _kDevelopQualifiedIdentifierHandler((void*) type); }

QVariant _kDevelopIdentifierHandler(void* type)
{
	if(!type) return QVariant();
	KDevelop::Identifier* t=static_cast<KDevelop::Identifier*>(type);
	Q_ASSERT(dynamic_cast<KDevelop::Identifier*>(t));
	return qVariantFromValue((QObject*) new KrossKDevelopIdentifier(t, 0));
}
bool b_KDevelopIdentifier=krossidentifier_registerHandler("KDevelop::Identifier*", _kDevelopIdentifierHandler);
QVariant kDevelopIdentifierHandler(KDevelop::Identifier* type){ return _kDevelopIdentifierHandler(type); }
QVariant kDevelopIdentifierHandler(const KDevelop::Identifier* type) { return _kDevelopIdentifierHandler((void*) type); }

QVariant _kDevelopIndexedTypeIdentifierHandler(void* type)
{
	if(!type) return QVariant();
	KDevelop::IndexedTypeIdentifier* t=static_cast<KDevelop::IndexedTypeIdentifier*>(type);
	Q_ASSERT(dynamic_cast<KDevelop::IndexedTypeIdentifier*>(t));
	return qVariantFromValue((QObject*) new KrossKDevelopIndexedTypeIdentifier(t, 0));
}
bool b_KDevelopIndexedTypeIdentifier=krossidentifier_registerHandler("KDevelop::IndexedTypeIdentifier*", _kDevelopIndexedTypeIdentifierHandler);
QVariant kDevelopIndexedTypeIdentifierHandler(KDevelop::IndexedTypeIdentifier* type){ return _kDevelopIndexedTypeIdentifierHandler(type); }
QVariant kDevelopIndexedTypeIdentifierHandler(const KDevelop::IndexedTypeIdentifier* type) { return _kDevelopIndexedTypeIdentifierHandler((void*) type); }

QVariant _kDevelopIndexedQualifiedIdentifierHandler(void* type)
{
	if(!type) return QVariant();
	KDevelop::IndexedQualifiedIdentifier* t=static_cast<KDevelop::IndexedQualifiedIdentifier*>(type);
	Q_ASSERT(dynamic_cast<KDevelop::IndexedQualifiedIdentifier*>(t));
	return qVariantFromValue((QObject*) new KrossKDevelopIndexedQualifiedIdentifier(t, 0));
}
bool b_KDevelopIndexedQualifiedIdentifier=krossidentifier_registerHandler("KDevelop::IndexedQualifiedIdentifier*", _kDevelopIndexedQualifiedIdentifierHandler);
QVariant kDevelopIndexedQualifiedIdentifierHandler(KDevelop::IndexedQualifiedIdentifier* type){ return _kDevelopIndexedQualifiedIdentifierHandler(type); }
QVariant kDevelopIndexedQualifiedIdentifierHandler(const KDevelop::IndexedQualifiedIdentifier* type) { return _kDevelopIndexedQualifiedIdentifierHandler((void*) type); }

QVariant _kDevelopIndexedIdentifierHandler(void* type)
{
	if(!type) return QVariant();
	KDevelop::IndexedIdentifier* t=static_cast<KDevelop::IndexedIdentifier*>(type);
	Q_ASSERT(dynamic_cast<KDevelop::IndexedIdentifier*>(t));
	return qVariantFromValue((QObject*) new KrossKDevelopIndexedIdentifier(t, 0));
}
bool b_KDevelopIndexedIdentifier=krossidentifier_registerHandler("KDevelop::IndexedIdentifier*", _kDevelopIndexedIdentifierHandler);
QVariant kDevelopIndexedIdentifierHandler(KDevelop::IndexedIdentifier* type){ return _kDevelopIndexedIdentifierHandler(type); }
QVariant kDevelopIndexedIdentifierHandler(const KDevelop::IndexedIdentifier* type) { return _kDevelopIndexedIdentifierHandler((void*) type); }

}
#include "krossidentifier.moc"
