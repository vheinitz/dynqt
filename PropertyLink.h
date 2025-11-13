#ifndef PROPERTYLINK_H
#define PROPERTYLINK_H

#include <QtCore/QObject>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QVariant>
#include <QtCore/QPointer>

class PropertyLink : public QObject
{
    Q_OBJECT
public:
    struct EndPoint {
        QPointer<QObject> obj;
        QMetaProperty      prop;
        QByteArray         notifySignal; // "2valueChanged(int)" style for QObject::connect
        QString            pretty() const {
            return obj ? QString("%1.%2").arg(obj->objectName(), QString::fromLatin1(prop.name()))
                       : QString("<null>.<null>");
        }
        bool isValid() const { return obj && prop.isValid(); }
    };

    // Parse spec "Obj1.prop1 : Obj2.prop2" and bind; root is the loaded UI root
    static PropertyLink* bindFromSpec(QObject* root, const QString& spec, QString* error, QObject* parent = 0);
	static PropertyLink* bindOneToSpec(QObject* leftObj, const QString& leftPropName,
                                   QObject* root, const QString& rightSpec,
                                   QString* error, QObject* parent = 0);

    EndPoint a() const { return A; }
    EndPoint b() const { return B; }

signals:
    void linked(QString description);

private slots:
    void onAChanged();
    void onBChanged();

private:
    explicit PropertyLink(QObject* parent = 0);
	// Add to private section:
	static bool parseObjProp(const QString& spec, QString* objName, QString* propName, QString* error);
	static EndPoint makeEnd(QObject* obj, const QString& propName, QString* error);

    static bool parseSpec(const QString& spec, QString* leftObj, QString* leftProp,
                          QString* rightObj, QString* rightProp, QString* error);
    static EndPoint makeEnd(QObject* root, const QString& objName, const QString& propName, QString* error);
    static QByteArray bestNotifySignal(QObject* obj, const QMetaProperty& prop);

    void connectSignals();
    void pushAtoB();
    void pushBtoA();

    EndPoint A, B;
    bool inChange;
};

#endif // PROPERTYLINK_H
