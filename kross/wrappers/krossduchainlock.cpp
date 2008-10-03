//This is file has been generated by xmltokross, you should not edit this file but the files used to generate it.

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <kross/core/manager.h>
#include <kross/core/wrapperinterface.h>
#include <language/duchain/duchainlock.h>

class KrossKDevelopDUChainLock : public QObject, public Kross::WrapperInterface
{
	Q_OBJECT
	public:
		KrossKDevelopDUChainLock(KDevelop::DUChainLock* obj, QObject* parent=0) : QObject(parent), wrapped(obj)		{ setObjectName("KDevelop::DUChainLock"); }
		void* wrappedObject() const { return wrapped; }

		Q_SCRIPTABLE bool lockForRead(unsigned int x0) { return wrapped->lockForRead(x0); }
		Q_SCRIPTABLE bool lockForRead() { return wrapped->lockForRead(); }
		Q_SCRIPTABLE void releaseReadLock() { wrapped->releaseReadLock(); }
		Q_SCRIPTABLE bool currentThreadHasReadLock() { return wrapped->currentThreadHasReadLock(); }
		Q_SCRIPTABLE bool lockForWrite(unsigned int x0=0) { return wrapped->lockForWrite(x0); }
		Q_SCRIPTABLE void releaseWriteLock() { wrapped->releaseWriteLock(); }
		Q_SCRIPTABLE bool currentThreadHasWriteLock() { return wrapped->currentThreadHasWriteLock(); }
	private:
		KDevelop::DUChainLock* wrapped;
};

class KrossKDevelopDUChainReadLocker : public QObject, public Kross::WrapperInterface
{
	Q_OBJECT
	public:
		KrossKDevelopDUChainReadLocker(KDevelop::DUChainReadLocker* obj, QObject* parent=0) : QObject(parent), wrapped(obj)		{ setObjectName("KDevelop::DUChainReadLocker"); }
		void* wrappedObject() const { return wrapped; }

		Q_SCRIPTABLE bool lock() { return wrapped->lock(); }
		Q_SCRIPTABLE void unlock() { wrapped->unlock(); }
		Q_SCRIPTABLE bool locked() const { return wrapped->locked(); }
	private:
		KDevelop::DUChainReadLocker* wrapped;
};

class KrossKDevelopDUChainWriteLocker : public QObject, public Kross::WrapperInterface
{
	Q_OBJECT
	public:
		KrossKDevelopDUChainWriteLocker(KDevelop::DUChainWriteLocker* obj, QObject* parent=0) : QObject(parent), wrapped(obj)		{ setObjectName("KDevelop::DUChainWriteLocker"); }
		void* wrappedObject() const { return wrapped; }

		Q_SCRIPTABLE bool lock() { return wrapped->lock(); }
		Q_SCRIPTABLE void unlock() { wrapped->unlock(); }
		Q_SCRIPTABLE bool locked() const { return wrapped->locked(); }
	private:
		KDevelop::DUChainWriteLocker* wrapped;
};

bool krossduchainlock_registerHandler(const QByteArray& name, Kross::MetaTypeHandler::FunctionPtr* handler)
{ Kross::Manager::self().registerMetaTypeHandler(name, handler); return false; }

namespace Handlers
{
QVariant _kDevelopDUChainWriteLockerHandler(void* type)
{
	if(!type) return QVariant();
	KDevelop::DUChainWriteLocker* t=static_cast<KDevelop::DUChainWriteLocker*>(type);
	Q_ASSERT(dynamic_cast<KDevelop::DUChainWriteLocker*>(t));
	return qVariantFromValue((QObject*) new KrossKDevelopDUChainWriteLocker(t, 0));
}
bool b_KDevelopDUChainWriteLocker=krossduchainlock_registerHandler("KDevelop::DUChainWriteLocker*", _kDevelopDUChainWriteLockerHandler);
QVariant kDevelopDUChainWriteLockerHandler(KDevelop::DUChainWriteLocker* type){ return _kDevelopDUChainWriteLockerHandler(type); }
QVariant kDevelopDUChainWriteLockerHandler(const KDevelop::DUChainWriteLocker* type) { return _kDevelopDUChainWriteLockerHandler((void*) type); }

QVariant _kDevelopDUChainReadLockerHandler(void* type)
{
	if(!type) return QVariant();
	KDevelop::DUChainReadLocker* t=static_cast<KDevelop::DUChainReadLocker*>(type);
	Q_ASSERT(dynamic_cast<KDevelop::DUChainReadLocker*>(t));
	return qVariantFromValue((QObject*) new KrossKDevelopDUChainReadLocker(t, 0));
}
bool b_KDevelopDUChainReadLocker=krossduchainlock_registerHandler("KDevelop::DUChainReadLocker*", _kDevelopDUChainReadLockerHandler);
QVariant kDevelopDUChainReadLockerHandler(KDevelop::DUChainReadLocker* type){ return _kDevelopDUChainReadLockerHandler(type); }
QVariant kDevelopDUChainReadLockerHandler(const KDevelop::DUChainReadLocker* type) { return _kDevelopDUChainReadLockerHandler((void*) type); }

QVariant _kDevelopDUChainLockHandler(void* type)
{
	if(!type) return QVariant();
	KDevelop::DUChainLock* t=static_cast<KDevelop::DUChainLock*>(type);
	Q_ASSERT(dynamic_cast<KDevelop::DUChainLock*>(t));
	return qVariantFromValue((QObject*) new KrossKDevelopDUChainLock(t, 0));
}
bool b_KDevelopDUChainLock=krossduchainlock_registerHandler("KDevelop::DUChainLock*", _kDevelopDUChainLockHandler);
QVariant kDevelopDUChainLockHandler(KDevelop::DUChainLock* type){ return _kDevelopDUChainLockHandler(type); }
QVariant kDevelopDUChainLockHandler(const KDevelop::DUChainLock* type) { return _kDevelopDUChainLockHandler((void*) type); }

}
#include "krossduchainlock.moc"
