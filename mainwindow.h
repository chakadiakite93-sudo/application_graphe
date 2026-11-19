#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include <QMap>

class QGraphicsScene;
class QGraphicsItem;
class QGraphicsLineItem;
class QSqlTableModel;
class QTableView;
class QLineEdit;
class QComboBox;
class QSpinBox;

struct Link {
    QGraphicsItem *a;
    QGraphicsItem *b;
    QGraphicsLineItem *line;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void addShape(QGraphicsItem *item, const QString &type);
    void linkSelected();
    void updateLinks();
    void deleteSelected();
    void deleteShape(int id);
    void applyFilter();

    bool initDb();
    int  insertShape(const QString &type);
    void saveLinks(QGraphicsItem *item);
    void updateCounts();

    QGraphicsScene *m_scene;
    QSqlTableModel *m_model;
    QTableView     *m_table;
    QLineEdit      *m_editNum;
    QComboBox      *m_comboType;
    QSpinBox       *m_spinCount;

    QList<Link> m_links;
    QMap<int, QGraphicsItem *> m_items;
};

#endif // MAINWINDOW_H