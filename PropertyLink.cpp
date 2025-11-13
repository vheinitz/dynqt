#include "PropertyLink.h"

#include <QtCore/QStringList>
#include <QtCore/QMetaMethod>
#include <QtCore/QDebug>

// ---- Helpers ---------------------------------------------------------------

static QByteArray sigPrefix(const QByteArray& methodSig)
{
    // Qt4 connect(const QObject*, const char*, ...) expects "2method(args)" for SIGNAL
    // (the '2' is the internal code for signal); safe/common technique in Qt4.
    QByteArray s("2");
    s += methodSig;
    return s;
}

static bool hasSignal(const QObject* o, const char* sig)
{
    const QMetaObject* mo = o->metaObject();
    return mo->indexOfSignal(sig) >= 0;
}

// helper: parse "Obj.prop"
bool PropertyLink::parseObjProp(const QString& spec, QString* objName, QString* propName, QString* error)
{
    QString s = spec;
    s.replace(" ", "");
    const QStringList parts = s.split('.');
    if (parts.size() != 2) {
        if (error) *error = "Use format Obj.prop";
        return false;
    }
    *objName = parts.at(0);
    *propName = parts.at(1);
    if (objName->isEmpty() || propName->isEmpty()) {
        if (error) *error = "Empty object or property name.";
        return false;
    }
    return true;
}

// make EndPoint from a specific QObject + property name
PropertyLink::EndPoint PropertyLink::makeEnd(QObject* obj, const QString& propName, QString* error)
{
    EndPoint ep;
    ep.obj = obj;
    if (!ep.obj) { if (error) *error = "Null object."; return ep; }
    const QMetaObject* mo = ep.obj->metaObject();
    const int idx = mo->indexOfProperty(propName.toLatin1().constData());
    if (idx < 0) {
        if (error) *error = QString("Property '%1' not found on '%2' (%3).")
                               .arg(propName, ep.obj->objectName(), mo->className());
        return ep;
    }
    ep.prop = mo->property(idx);
    if (!ep.prop.isReadable() || !ep.prop.isWritable()) {
        if (error) *error = QString("Property '%1' on '%2' must be readable & writable.")
                               .arg(propName, ep.obj->objectName());
        return ep;
    }
    ep.notifySignal = bestNotifySignal(ep.obj, ep.prop);
    return ep;
}

// bind left: (given QObject + property), right: "Obj.prop"
PropertyLink* PropertyLink::bindOneToSpec(QObject* leftObj, const QString& leftPropName,
                                          QObject* root, const QString& rightSpec,
                                          QString* error, QObject* parent)
{
    if (!leftObj || !root) { if (error) *error = "Invalid root or object."; return 0; }

    QString ro, rp;
    if (!parseObjProp(rightSpec, &ro, &rp, error))
        return 0;

    PropertyLink* link = new PropertyLink(parent);
    link->A = makeEnd(leftObj, leftPropName, error);
    if (!link->A.isValid()) { delete link; return 0; }

    link->B = makeEnd(root, ro, rp, error);
    if (!link->B.isValid()) { delete link; return 0; }

    link->pushAtoB();
    link->connectSignals();
    emit link->linked(QString("Linked %1  <=>  %2").arg(link->A.pretty(), link->B.pretty()));
    return link;
}

PropertyLink::PropertyLink(QObject* parent) : QObject(parent), inChange(false) {}

PropertyLink* PropertyLink::bindFromSpec(QObject* root, const QString& spec, QString* error, QObject* parent)
{
    if (!root) { if (error) *error = "No UI loaded."; return 0; }

    QString lo, lp, ro, rp;
    if (!parseSpec(spec, &lo, &lp, &ro, &rp, error))
        return 0;

    PropertyLink* link = new PropertyLink(parent);
    link->A = makeEnd(root, lo, lp, error);
    if (!link->A.isValid()) { delete link; return 0; }
    link->B = makeEnd(root, ro, rp, error);
    if (!link->B.isValid()) { delete link; return 0; }

    // Initial sync: A -> B
    link->pushAtoB();
    link->connectSignals();

    emit link->linked(QString("Linked %1  <=>  %2").arg(link->A.pretty(), link->B.pretty()));
    return link;
}

bool PropertyLink::parseSpec(const QString& spec, QString* leftObj, QString* leftProp,
                             QString* rightObj, QString* rightProp, QString* error)
{
    QString s = spec;
    // allow spaces
    s.replace(" ", "");
    const QStringList parts = s.split(':');
    if (parts.size() != 2) { if (error) *error = "Use format Obj.prop:Other.prop"; return false; }

    const QStringList L = parts.at(0).split('.');
    const QStringList R = parts.at(1).split('.');
    if (L.size() != 2 || R.size() != 2) { if (error) *error = "Each side must be Obj.prop"; return false; }

    *leftObj = L.at(0);  *leftProp = L.at(1);
    *rightObj = R.at(0); *rightProp = R.at(1);
    if (leftObj->isEmpty() || leftProp->isEmpty() || rightObj->isEmpty() || rightProp->isEmpty()) {
        if (error) *error = "Empty object or property name.";
        return false;
    }
    return true;
}

PropertyLink::EndPoint PropertyLink::makeEnd(QObject* root, const QString& objName, const QString& propName, QString* error)
{
    EndPoint ep;
    ep.obj = root->findChild<QObject*>(objName);
    if (!ep.obj) {
        if (error) *error = QString("Object '%1' not found.").arg(objName);
        return ep;
    }
    const QMetaObject* mo = ep.obj->metaObject();
    const int idx = mo->indexOfProperty(propName.toLatin1().constData());
    if (idx < 0) {
        if (error) *error = QString("Property '%1' not found on '%2' (%3).")
                               .arg(propName, objName, mo->className());
        return ep;
    }
    ep.prop = mo->property(idx);
    if (!ep.prop.isReadable() || !ep.prop.isWritable()) {
        if (error) *error = QString("Property '%1' on '%2' must be readable & writable.")
                               .arg(propName, objName);
        return ep;
    }

    ep.notifySignal = bestNotifySignal(ep.obj, ep.prop);
    return ep;
}

QByteArray PropertyLink::bestNotifySignal(QObject* obj, const QMetaProperty& prop)
{
    // 1) Prefer Q_PROPERTY NOTIFY signal (if the class declares it)
    if (prop.hasNotifySignal()) {
        QMetaMethod m = prop.notifySignal();
        return sigPrefix(m.signature()); // Qt4: signature() returns const char*
    }

    // 2) Heuristics for common widgets/properties in Qt4:
    const QByteArray p = prop.name();
    const QMetaObject* mo = obj->metaObject();
    // Try a few common patterns
    struct Candidate { const char* prop; const char* signal; };
    static const Candidate cand[] = {
        {"text",           "textChanged(QString)"},
        {"plainText",      "textChanged()"},               // QTextEdit
        {"value",          "valueChanged(int)"},
        {"value",          "valueChanged(double)"},
        {"currentIndex",   "currentIndexChanged(int)"},
        {"currentText",    "currentIndexChanged(int)"},    // we’ll read text from property anyway
        {"checked",        "toggled(bool)"},
        {"checkState",     "stateChanged(int)"},
        {"sliderPosition", "sliderMoved(int)"},
        {"sliderPosition", "valueChanged(int)"},
        {"progress",       "valueChanged(int)"},
    };
    for (size_t i = 0; i < sizeof(cand)/sizeof(cand[0]); ++i) {
        if (p == cand[i].prop) {
            if (mo->indexOfSignal(cand[i].signal) >= 0)
                return sigPrefix(cand[i].signal);
        }
    }

    // 3) Last resort: no signal known; changes won't auto-propagate from this side
    return QByteArray();
}

void PropertyLink::connectSignals()
{
    // A -> B
    if (!A.notifySignal.isEmpty())
        QObject::connect(A.obj, A.notifySignal.constData(), this, SLOT(onAChanged()), Qt::UniqueConnection);

    // B -> A
    if (!B.notifySignal.isEmpty())
        QObject::connect(B.obj, B.notifySignal.constData(), this, SLOT(onBChanged()), Qt::UniqueConnection);
}

void PropertyLink::pushAtoB()
{
    if (!A.isValid() || !B.isValid()) return;
    if (inChange) return;
    inChange = true;
    QVariant v = A.obj->property(A.prop.name());
    if (v.isValid()) {
        if (!v.canConvert(B.prop.type())) v.convert(B.prop.type());
        // avoid loops if value is already equal
        QVariant current = B.obj->property(B.prop.name());
        if (current != v)
            B.obj->setProperty(B.prop.name(), v);
    }
    inChange = false;
}

void PropertyLink::pushBtoA()
{
    if (!A.isValid() || !B.isValid()) return;
    if (inChange) return;
    inChange = true;
    QVariant v = B.obj->property(B.prop.name());
    if (v.isValid()) {
        if (!v.canConvert(A.prop.type())) v.convert(A.prop.type());
        QVariant current = A.obj->property(A.prop.name());
        if (current != v)
            A.obj->setProperty(A.prop.name(), v);
    }
    inChange = false;
}

void PropertyLink::onAChanged() { pushAtoB(); }
void PropertyLink::onBChanged() { pushBtoA(); }
