#include "mainwindow.h"
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsSimpleTextItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QTableView>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QPolygonF>
#include <QRandomGenerator>
#include <QtMath>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlTableModel>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QApplication>
#include <QStyle>
#include <QPainter>
#include <QDebug>

// ======================================================================
// Délégué : dessine 1, 2 ou 3 figures selon le nombre d'éléments du type
// ======================================================================
class CountDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *p, const QStyleOptionViewItem &opt,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem o = opt;
        initStyleOption(&o, index);
        o.text.clear();
        QStyle *style = o.widget ? o.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &o, p, o.widget);

        int count = index.data().toInt();
        int n = 0;
        if (count > 10)       n = 3;   // plus de 10  -> 3 figures
        else if (count >= 4)  n = 2;   // de 4 à 10   -> 2 figures
        else if (count >= 1)  n = 1;   // de 1 à 3    -> 1 figure
        if (n == 0)
            return;

        QString type = index.siblingAtColumn(1).data().toString();

        int h = opt.rect.height() - 10;
        int w = int(h * 1.4);
        int gap = 6;
        int x = opt.rect.left() + 8;
        int y = opt.rect.top() + 5;

        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(Qt::black);

        for (int i = 0; i < n; ++i) {
            QRect r(x + i * (w + gap), y, w, h);

            if (type == "rectangle") {
                p->setBrush(QColor("#a8d8ff"));
                p->drawRect(r);
            } else if (type == "ellipse") {
                p->setBrush(QColor("#ffd8a8"));
                p->drawEllipse(r);
            } else {
                p->setBrush(QColor("#b8f0b8"));
                QPolygon tri;
                tri << QPoint(r.center().x(), r.top())
                    << QPoint(r.right(), r.bottom())
                    << QPoint(r.left(), r.bottom());
                p->drawPolygon(tri);
            }
        }
        p->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(110, 36);
    }
};

// ======================================================================
// Fenêtre principale
// ======================================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    initDb();

    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(0, 0, 500, 400);
    auto *view = new QGraphicsView(m_scene);

    // ----- Modèle et tableau -----
    m_model = new QSqlTableModel(this);
    m_model->setTable("shapes");
    m_model->setHeaderData(0, Qt::Horizontal, "N°");
    m_model->setHeaderData(1, Qt::Horizontal, "Type");
    m_model->setHeaderData(2, Qt::Horizontal, "Liens");
    m_model->setHeaderData(4, Qt::Horizontal, "Total");
    m_model->select();

    m_table = new QTableView;
    m_table->setModel(m_model);
    m_table->hideColumn(3);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setMinimumWidth(320);
    m_table->setItemDelegateForColumn(4, new CountDelegate(m_table));
    m_table->verticalHeader()->setDefaultSectionSize(36);

    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex &cur, const QModelIndex &) {
                if (!cur.isValid()) return;
                int id = m_model->data(m_model->index(cur.row(), 0)).toInt();
                QGraphicsItem *item = m_items.value(id, nullptr);
                if (!item) return;
                m_scene->clearSelection();
                item->setSelected(true);
            });

    // ----- Filtres -----
    m_editNum = new QLineEdit;
    m_editNum->setPlaceholderText("tous");

    m_comboType = new QComboBox;
    m_comboType->addItems({"Tous", "rectangle", "ellipse", "polygone"});

    m_spinCount = new QSpinBox;
    m_spinCount->setRange(0, 999);
    m_spinCount->setSpecialValueText("tous");

    auto *form = new QFormLayout;
    form->addRow("N° :", m_editNum);
    form->addRow("Type :", m_comboType);
    form->addRow("Total = :", m_spinCount);

    connect(m_editNum,   &QLineEdit::textChanged, this, &MainWindow::applyFilter);
    connect(m_comboType, &QComboBox::currentTextChanged, this, &MainWindow::applyFilter);
    connect(m_spinCount, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::applyFilter);

    // ----- Dimensions des figures -----
    auto *spinW = new QSpinBox;
    spinW->setRange(20, 200);
    spinW->setValue(60);

    auto *spinH = new QSpinBox;
    spinH->setRange(20, 200);
    spinH->setValue(40);

    auto *spinSides = new QSpinBox;
    spinSides->setRange(3, 8);
    spinSides->setValue(3);

    auto *dims = new QHBoxLayout;
    dims->addWidget(new QLabel("Largeur :"));
    dims->addWidget(spinW);
    dims->addWidget(new QLabel("Hauteur :"));
    dims->addWidget(spinH);
    dims->addWidget(new QLabel("Côtés (polygone) :"));
    dims->addWidget(spinSides);

    // ----- Boutons -----
    auto *btnRect    = new QPushButton("Rectangle");
    auto *btnEllipse = new QPushButton("Ellipse");
    auto *btnPoly    = new QPushButton("Polygone");
    auto *btnLink    = new QPushButton("Relier");
    auto *btnDelete  = new QPushButton("Supprimer la figure sélectionnée");

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(btnRect);
    buttons->addWidget(btnEllipse);
    buttons->addWidget(btnPoly);
    buttons->addWidget(btnLink);

    auto *left = new QVBoxLayout;
    left->addWidget(view);
    left->addLayout(dims);
    left->addLayout(buttons);

    auto *right = new QVBoxLayout;
    right->addLayout(form);
    right->addWidget(m_table);
    right->addWidget(btnDelete);

    auto *layout = new QHBoxLayout;
    layout->addLayout(left, 3);
    layout->addLayout(right, 2);

    auto *central = new QWidget;
    central->setLayout(layout);
    setCentralWidget(central);
    resize(1100, 600);

    // ----- Création des figures -----
    connect(btnRect, &QPushButton::clicked, this, [this, spinW, spinH]() {
        addShape(m_scene->addRect(0, 0, spinW->value(), spinH->value(),
                                  QPen(Qt::black), QBrush(QColor("#a8d8ff"))),
                 "rectangle");
    });

    connect(btnEllipse, &QPushButton::clicked, this, [this, spinW, spinH]() {
        addShape(m_scene->addEllipse(0, 0, spinW->value(), spinH->value(),
                                     QPen(Qt::black), QBrush(QColor("#ffd8a8"))),
                 "ellipse");
    });

    connect(btnPoly, &QPushButton::clicked, this, [this, spinW, spinH, spinSides]() {
        int n = spinSides->value();
        double w = spinW->value();
        double h = spinH->value();

        QPolygonF poly;
        for (int i = 0; i < n; ++i) {
            double a = qDegreesToRadians(360.0 * i / n - 90);
            poly << QPointF(w / 2 + (w / 2) * qCos(a),
                            h / 2 + (h / 2) * qSin(a));
        }
        addShape(m_scene->addPolygon(poly, QPen(Qt::black),
                                     QBrush(QColor("#b8f0b8"))),
                 "polygone");
    });

    connect(btnLink,   &QPushButton::clicked, this, &MainWindow::linkSelected);
    connect(btnDelete, &QPushButton::clicked, this, &MainWindow::deleteSelected);
    connect(m_scene, &QGraphicsScene::changed, this, &MainWindow::updateLinks);
}

MainWindow::~MainWindow()
{
}

// ======================================================================
// Base de données
// ======================================================================
bool MainWindow::initDb()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("shapes.db");
    if (!db.open()) {
        QMessageBox::critical(nullptr, "Erreur", db.lastError().text());
        return false;
    }

    QSqlQuery q;
    q.exec("DROP TABLE IF EXISTS shapes");
    q.exec("CREATE TABLE shapes ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "type TEXT NOT NULL,"
           "links TEXT DEFAULT '',"
           "hidden INTEGER DEFAULT 0,"
           "total INTEGER DEFAULT 0)");
    return true;
}

int MainWindow::insertShape(const QString &type)
{
    QSqlQuery q;
    q.prepare("INSERT INTO shapes (type) VALUES (?)");
    q.addBindValue(type);
    if (!q.exec()) {
        qDebug() << "Erreur INSERT :" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

void MainWindow::saveLinks(QGraphicsItem *item)
{
    QStringList ids;
    for (const Link &l : m_links) {
        if (l.a == item)
            ids << QString::number(l.b->data(0).toInt());
        else if (l.b == item)
            ids << QString::number(l.a->data(0).toInt());
    }

    QSqlQuery q;
    q.prepare("UPDATE shapes SET links = ? WHERE id = ?");
    q.addBindValue(ids.join(","));
    q.addBindValue(item->data(0).toInt());
    q.exec();
}

void MainWindow::updateCounts()
{
    QSqlQuery q;
    q.exec("UPDATE shapes SET total = "
           "(SELECT COUNT(*) FROM shapes s2 WHERE s2.type = shapes.type)");
}

// ======================================================================
// Filtres (changent seulement l'affichage, rien n'est supprimé)
// ======================================================================
void MainWindow::applyFilter()
{
    QStringList cond;

    bool ok = false;
    int num = m_editNum->text().trimmed().toInt(&ok);
    if (ok)
        cond << QString("id = %1").arg(num);

    if (m_comboType->currentIndex() > 0)
        cond << QString("type = '%1'").arg(m_comboType->currentText());

    if (m_spinCount->value() > 0)
        cond << QString("total = %1").arg(m_spinCount->value());

    m_model->setFilter(cond.join(" AND "));
    m_model->select();
}

// ======================================================================
// Scène
// ======================================================================
void MainWindow::addShape(QGraphicsItem *item, const QString &type)
{
    int id = insertShape(type);
    item->setData(0, id);
    m_items.insert(id, item);

    item->setFlag(QGraphicsItem::ItemIsMovable);
    item->setFlag(QGraphicsItem::ItemIsSelectable);
    item->setPos(QRandomGenerator::global()->bounded(400),
                 QRandomGenerator::global()->bounded(300));

    auto *label = new QGraphicsSimpleTextItem(QString::number(id), item);
    label->setPos(item->boundingRect().center() - label->boundingRect().center());

    updateCounts();
    m_model->select();
}

void MainWindow::linkSelected()
{
    QList<QGraphicsItem *> sel = m_scene->selectedItems();
    if (sel.size() != 2) {
        QMessageBox::warning(this, "Relier",
                             "Sélectionne exactement 2 figures (clic, puis Ctrl+clic sur la 2e).");
        return;
    }

    for (const Link &l : m_links) {
        if ((l.a == sel[0] && l.b == sel[1]) || (l.a == sel[1] && l.b == sel[0]))
            return;
    }

    auto *line = m_scene->addLine(QLineF(), QPen(Qt::darkBlue, 2));
    line->setZValue(-1);
    m_links.append({sel[0], sel[1], line});
    updateLinks();

    saveLinks(sel[0]);
    saveLinks(sel[1]);
    m_model->select();
}

void MainWindow::updateLinks()
{
    for (const Link &l : m_links) {
        l.line->setLine(QLineF(l.a->sceneBoundingRect().center(),
                               l.b->sceneBoundingRect().center()));
    }
}

// ======================================================================
// Suppression
// ======================================================================
void MainWindow::deleteSelected()
{
    QList<int> ids;
    for (QGraphicsItem *it : m_scene->selectedItems())
        ids << it->data(0).toInt();

    if (ids.isEmpty()) {
        QMessageBox::information(this, "Supprimer",
                                 "Sélectionne d'abord une figure (dans la scène ou dans le tableau).");
        return;
    }

    for (int id : ids)
        deleteShape(id);

    updateCounts();
    m_model->select();
}

void MainWindow::deleteShape(int id)
{
    QGraphicsItem *item = m_items.value(id, nullptr);
    if (!item)
        return;

    // 1) Supprimer tous les liens (lignes) de cette figure
    QList<QGraphicsItem *> neighbors;
    for (int i = m_links.size() - 1; i >= 0; --i) {
        Link l = m_links[i];
        if (l.a == item || l.b == item) {
            neighbors << (l.a == item ? l.b : l.a);
            m_scene->removeItem(l.line);
            delete l.line;
            m_links.removeAt(i);
        }
    }

    // 2) Supprimer la figure de la scène
    m_scene->removeItem(item);
    delete item;
    m_items.remove(id);

    // 3) Supprimer la ligne dans la base de données
    QSqlQuery q;
    q.prepare("DELETE FROM shapes WHERE id = ?");
    q.addBindValue(id);
    q.exec();

    // 4) Mettre à jour la colonne "Liens" des figures qui étaient reliées
    for (QGraphicsItem *n : neighbors)
        saveLinks(n);
}