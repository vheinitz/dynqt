// Qt 4.8 / VS2008 compatible
#include <QtGui/QApplication>
#include <QtGui/QWidget>
#include <QtGui/QVBoxLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QFrame>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QPlainTextEdit>
#include <QtGui/QPushButton>
#include <QtGui/QFileDialog>
#include <QtGui/QScrollArea>
#include <QtCore/QFile>
#include <QtCore/QMetaObject>
#include <QtUiTools/QUiLoader>
#include "PropertyLink.h"
#include <QtGui/QGroupBox>

static QString objectPath(const QObject *obj)
{
    QStringList parts;
    const QObject *p = obj;
    while (p) {
        QString name = p->objectName();
        parts.prepend(name.isEmpty() ? QString("<unnamed %1>").arg(p->metaObject()->className())
                                     : name);
        p = p->parent();
    }
    return parts.join("/");
}

class HostWindow : public QWidget
{
    Q_OBJECT
public:
    HostWindow(QWidget *parent = 0) : QWidget(parent), m_loaded(0)
    {
        setWindowTitle("Runtime UI Loader (Qt 4.8)");

        // Top controls: Load button + filter line edit
        m_loadBtn = new QPushButton("Load .ui file ");
        m_filterEdit = new QLineEdit;
        m_filterEdit->setPlaceholderText("Filter by objectName (substring, case-insensitive)");
        connect(m_loadBtn, SIGNAL(clicked()), this, SLOT(loadUi()));
        connect(m_filterEdit, SIGNAL(textChanged(QString)), this, SLOT(updateListing()));



        QHBoxLayout *topBar = new QHBoxLayout;
        topBar->addWidget(m_loadBtn);
        topBar->addWidget(new QLabel("Filter:"));
        topBar->addWidget(m_filterEdit);

        // Frame to host the loaded UI
        m_frame = new QFrame;
        m_frame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
        m_frame->setMinimumHeight(220);
        QVBoxLayout *frameLayout = new QVBoxLayout(m_frame);
        frameLayout->setContentsMargins(6,6,6,6);

        // Listing area
        m_listing = new QPlainTextEdit;
        m_listing->setReadOnly(true);

		// ---- Binding controls ----
		QHBoxLayout *bindBar = new QHBoxLayout;
		m_linkSpecEdit = new QLineEdit;
		m_linkSpecEdit->setPlaceholderText("Obj1.prop1 : Obj2.prop2  (z.B. lineEdit.text : label.text)");
		m_addLinkBtn = new QPushButton("Bind");
		bindBar->addWidget(new QLabel("Link:"));
		bindBar->addWidget(m_linkSpecEdit, 1);
		bindBar->addWidget(m_addLinkBtn);

		connect(m_addLinkBtn, SIGNAL(clicked()), this, SLOT(addBinding()));



        QVBoxLayout *root = new QVBoxLayout(this);
        root->addLayout(topBar);
        root->addWidget(new QLabel("Loaded UI preview:"));
        root->addWidget(m_frame, /*stretch*/2);
        root->addWidget(new QLabel("QObjects (filtered):"));
        root->addWidget(m_listing, /*stretch*/1);

		root->addWidget(new QLabel("Loaded UI preview:"));
		root->addWidget(m_frame, 2);
		root->addLayout(bindBar);                 // <--- neu
		root->addWidget(new QLabel("QObjects (filtered):"));
		root->addWidget(m_listing, 1);		

        resize(820, 600);
    }

private slots:

	
    void addBinding()
    {
        if (!m_loaded) { m_listing->appendPlainText("No UI loaded."); return; }
        const QString spec = m_linkSpecEdit->text();
        QString err;
        PropertyLink *link = PropertyLink::bindFromSpec(m_loaded, spec, &err, this);
        if (!link) {
            m_listing->appendPlainText(QString("Bind error: %1").arg(err));
            return;
        }
        m_links << link;
        m_listing->appendPlainText(QString("OK: %1  <=>  %2")
                                    .arg(link->a().pretty(), link->b().pretty()));
        // optional: sofort die Liste updaten
        updateListing();
    }

    void connectButtonsToClose(QWidget *root)
    {
        if (!root) return;

        // include the root if it's itself a QPushButton
        QList<QPushButton*> buttons = root->findChildren<QPushButton*>();
        if (QPushButton *asBtn = qobject_cast<QPushButton*>(root))
            buttons.prepend(asBtn);

        for (int i = 0; i < buttons.size(); ++i) {
            QPushButton *btn = buttons.at(i);
            // Avoid duplicate connections if loadUi() is called multiple times
            QObject::connect(btn, SIGNAL(clicked()),
                             qApp, SLOT(quit()),
                             Qt::UniqueConnection);
        }
    }

	//Detects link definitions from .ui files, custom properties beginning with _
	void HostWindow::setupDynamicBindings()
	{
		if (!m_loaded) return;

		// Collect all objects (root + descendants)
		QList<QObject*> all = m_loaded->findChildren<QObject*>();
		all.prepend(m_loaded);

		int okCount = 0, errCount = 0;

		for (int i = 0; i < all.size(); ++i) {
			QObject* o = all.at(i);
			const QList<QByteArray> dyns = o->dynamicPropertyNames();
			for (int j = 0; j < dyns.size(); ++j) {
				const QByteArray& name = dyns.at(j);
				if (name.isEmpty() || name.at(0) != '_')
					continue;

				const QString leftProp = QString::fromLatin1(name.mid(1)); // strip leading '_'
				const QVariant val = o->property(name.constData());
				const QString rightSpec = val.toString().trimmed();         // e.g. "le2.text"

				if (rightSpec.isEmpty())
					continue;

				QString err;
				PropertyLink* link = PropertyLink::bindOneToSpec(o, leftProp, m_loaded, rightSpec, &err, this);
				if (link) {
					m_links << link;
					++okCount;
					m_listing->appendPlainText(QString("Dyn OK: %1  <=>  %2")
						.arg(link->a().pretty(), link->b().pretty()));
				} else {
					++errCount;
					m_listing->appendPlainText(QString("Dyn ERR: %1._%2 -> %3  | %4")
						.arg(o->objectName(), leftProp, rightSpec, err));
				}
			}
		}

		if (okCount || errCount) {
			m_listing->appendPlainText(QString("Dynamic bindings: %1 ok, %2 errors.").arg(okCount).arg(errCount));
		}
	}


    void loadUi()
    {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Open UI File"), QString(), tr("Qt Designer UI (*.ui)"));
        if (path.isEmpty())
            return;

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            m_listing->setPlainText(QString("Error: cannot open %1").arg(path));
            return;
        }

        // Clear previous widget from the frame
        clearFrame();

        QUiLoader loader;
        // Parent the loaded widget to the frame so ownership is managed
        //QWidget *w = loader.load(&f, m_frame);
		QWidget *w = loader.load(&f, 0);
        f.close();

        if (!w) {
            m_listing->setPlainText(QString("Error: failed to load %1").arg(path));
            return;
        }

        // Fit it into the frame's layout
        w->setObjectName(w->objectName().isEmpty() ? "LoadedRoot" : w->objectName());
        //m_frame->layout()->addWidget(w);
        w->show();

        m_loaded = w;
        m_currentFile = path;

		//connectButtonsToClose(m_loaded);
		setupDynamicBindings();

        updateListing(); // refresh the object list
    }

    void updateListing()
    {
        m_listing->clear();

        if (!m_loaded) {
            m_listing->setPlainText("No UI loaded yet.");
            return;
        }

        const QString filt = m_filterEdit->text();
        QStringList lines;

        // include root
        QList<const QObject*> all;
        all << m_loaded;
        // then all descendants
        const QList<QObject*> kids = m_loaded->findChildren<QObject*>();
        for (int i = 0; i < kids.size(); ++i) all << kids.at(i);

        for (int i = 0; i < all.size(); ++i) {
            const QObject *o = all.at(i);
            const QString name = o->objectName();
            if (!filt.isEmpty()) {
                if (name.indexOf(filt, 0, Qt::CaseInsensitive) == -1)
                    continue;
            }
            const char *cls = o->metaObject()->className();
            lines << QString("%1 | %2 | objectName=\"%3\"")
                      .arg(objectPath(o))
                      .arg(QString::fromLatin1(cls))
                      .arg(name.isEmpty() ? QString("<empty>") : name);
        }

        if (lines.isEmpty())
            lines << "(No objects match current filter.)";

        if (!m_currentFile.isEmpty())
            lines.prepend(QString("Loaded file: %1").arg(m_currentFile));

        m_listing->setPlainText(lines.join("\n"));
    }

private:
    void clearFrame()
    {
        if (QLayout *lay = m_frame->layout()) {
            while (QLayoutItem *it = lay->takeAt(0)) {
                if (QWidget *w = it->widget()) {
                    w->hide();
                    w->deleteLater();
                }
                delete it;
            }
        }
        m_loaded = 0;
        m_currentFile.clear();
		qDeleteAll(m_links);
		m_links.clear();
    }

private:
    QPushButton   *m_loadBtn;
    QLineEdit     *m_filterEdit;
    QPlainTextEdit*m_listing;
    QFrame        *m_frame;
    QWidget       *m_loaded;
    QString        m_currentFile;
	QLineEdit   *m_linkSpecEdit;
	QPushButton *m_addLinkBtn;
	QList<PropertyLink*> m_links;

};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    HostWindow w;
    w.show();
    return app.exec();
}

#include "app.moc"
