#pragma once

#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVector>
#include <QVariantMap>

// The Markdown files beside the document, flattened into the rows a tree
// shows. Directories start collapsed and their contents are only read when
// the writer opens them, so a folder holding a whole home directory stays
// cheap.
class FileTreeModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString rootPath READ rootPath NOTIFY rootPathChanged)
    Q_PROPERTY(QString rootName READ rootName NOTIFY rootPathChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        UrlRole,
        IsDirRole,
        DepthRole,
        ExpandedRole,
    };

    explicit FileTreeModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString rootPath() const { return m_rootPath; }
    QString rootName() const;
    QString defaultDirectory() const;

    Q_INVOKABLE void setDocument(const QUrl &documentUrl);
    Q_INVOKABLE void setRootPath(const QString &path);
    Q_INVOKABLE void toggle(int row);
    Q_INVOKABLE void collapseAll();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap entry(int row) const;
    Q_INVOKABLE int rowForUrl(const QUrl &url) const;
    Q_INVOKABLE int parentRow(int row) const;
    // Whether a document lives under the folder the tree is rooted at.
    Q_INVOKABLE bool containsDocument(const QUrl &documentUrl) const;

signals:
    void rootPathChanged();

private:
    struct Row {
        QString name;
        QString path;
        bool isDir = false;
        int depth = 0;
    };

    static bool markdownFile(const QString &fileName);
    QVector<Row> buildRows() const;
    void appendRows(const QString &directory, int depth, int nesting,
                    QSet<QString> &ancestors, QVector<Row> &rows) const;
    void watchDirectories();

    QString m_rootPath;
    QVector<Row> m_rows;
    // The absolute paths of the directories the writer has opened.
    QSet<QString> m_expanded;
    QFileSystemWatcher m_watcher;
    QTimer m_refreshTimer;
};
