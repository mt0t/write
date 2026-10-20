#include "filetreemodel.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

namespace {
const QString lastSaveDirectorySetting = QStringLiteral("file/lastSaveDirectory");
// A symlinked directory that points at one of its own parents would otherwise
// be followed forever; no writing folder is nested this deeply.
constexpr int maximumNesting = 32;
}

FileTreeModel::FileTreeModel(QObject *parent)
    : QAbstractListModel(parent) {
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(150);
    connect(&m_refreshTimer, &QTimer::timeout, this, &FileTreeModel::refresh);
    // Editors replace files rather than write in place, so a save arrives as
    // a directory change. Wait out the burst before re-reading the folder.
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            &m_refreshTimer, [this] { m_refreshTimer.start(); });
}

int FileTreeModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant FileTreeModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const Row &row = m_rows.at(index.row());
    switch (role) {
    case NameRole:
        return row.name;
    case PathRole:
        return row.path;
    case UrlRole:
        return QUrl::fromLocalFile(row.path);
    case IsDirRole:
        return row.isDir;
    case DepthRole:
        return row.depth;
    case ExpandedRole:
        return row.isDir && m_expanded.contains(row.path);
    default:
        return {};
    }
}

QHash<int, QByteArray> FileTreeModel::roleNames() const {
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {UrlRole, "url"},
        {IsDirRole, "isDir"},
        {DepthRole, "depth"},
        {ExpandedRole, "expanded"},
    };
}

QString FileTreeModel::rootName() const {
    if (m_rootPath.isEmpty())
        return {};
    const QString name = QFileInfo(m_rootPath).fileName();
    return name.isEmpty() ? m_rootPath : name;
}

QString FileTreeModel::defaultDirectory() const {
    const QString saved = QSettings().value(lastSaveDirectorySetting).toString();
    if (!saved.isEmpty() && QFileInfo(saved).isDir())
        return saved;

    const QString documents =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!documents.isEmpty() && QFileInfo(documents).isDir())
        return documents;

    return QDir::homePath();
}

// The tree belongs to the folder of the document being written. A document
// that has not been saved yet has no folder, so fall back to the last one
// Omawrite saved into.
void FileTreeModel::setDocument(const QUrl &documentUrl) {
    QString directory;
    if (documentUrl.isLocalFile()) {
        const QFileInfo info(documentUrl.toLocalFile());
        directory = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    }
    if (directory.isEmpty() || !QFileInfo(directory).isDir())
        directory = defaultDirectory();
    setRootPath(directory);
}

void FileTreeModel::setRootPath(const QString &path) {
    if (path.isEmpty())
        return;

    const QString root = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (root == m_rootPath) {
        refresh();
        return;
    }

    beginResetModel();
    m_rootPath = root;
    m_expanded.clear();
    m_rows = buildRows();
    endResetModel();
    watchDirectories();
    emit rootPathChanged();
}

void FileTreeModel::toggle(int row) {
    if (row < 0 || row >= m_rows.size())
        return;
    const Row target = m_rows.at(row);
    if (!target.isDir)
        return;

    if (m_expanded.contains(target.path)) {
        int descendants = 0;
        for (int i = row + 1; i < m_rows.size() && m_rows.at(i).depth > target.depth; ++i)
            ++descendants;

        beginRemoveRows(QModelIndex(), row + 1, row + descendants);
        m_expanded.remove(target.path);
        m_rows = buildRows();
        endRemoveRows();
        emit dataChanged(this->index(row), this->index(row), {ExpandedRole});
    } else {
        QSet<QString> expanded = m_expanded;
        expanded.insert(target.path);
        m_expanded = expanded;
        const QVector<Row> after = buildRows();
        const int inserted = int(after.size() - m_rows.size());

        if (inserted > 0) {
            beginInsertRows(QModelIndex(), row + 1, row + inserted);
            m_rows = after;
            endInsertRows();
        } else {
            m_rows = after;
            emit dataChanged(this->index(row), this->index(row), {ExpandedRole});
        }
    }

    watchDirectories();
}

void FileTreeModel::refresh() {
    if (m_rootPath.isEmpty())
        return;

    // Forget folders that have gone away, so they do not leave rows behind.
    for (auto it = m_expanded.begin(); it != m_expanded.end();) {
        if (QFileInfo(*it).isDir())
            ++it;
        else
            it = m_expanded.erase(it);
    }

    const QVector<Row> rows = buildRows();
    // Saving a file replaces it, which the watcher reports as a change to the
    // folder even though no name moved. Leaving the model untouched then keeps
    // the list from resetting under the writer's scroll position.
    const bool unchanged = rows.size() == m_rows.size()
        && std::equal(rows.begin(), rows.end(), m_rows.begin(),
                      [](const Row &left, const Row &right) {
                          return left.name == right.name
                              && left.path == right.path
                              && left.isDir == right.isDir
                              && left.depth == right.depth;
                      });

    watchDirectories();
    if (unchanged)
        return;

    beginResetModel();
    m_rows = rows;
    endResetModel();
}

QVariantMap FileTreeModel::entry(int row) const {
    if (row < 0 || row >= m_rows.size())
        return {};

    const Row &item = m_rows.at(row);
    return {
        {QStringLiteral("name"), item.name},
        {QStringLiteral("path"), item.path},
        {QStringLiteral("url"), QUrl::fromLocalFile(item.path)},
        {QStringLiteral("isDir"), item.isDir},
        {QStringLiteral("depth"), item.depth},
        {QStringLiteral("expanded"), item.isDir && m_expanded.contains(item.path)},
    };
}

int FileTreeModel::rowForUrl(const QUrl &url) const {
    if (!url.isLocalFile())
        return -1;

    const QString path = QDir::cleanPath(QFileInfo(url.toLocalFile()).absoluteFilePath());
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).path == path)
            return i;
    }
    return -1;
}

int FileTreeModel::parentRow(int row) const {
    if (row < 0 || row >= m_rows.size())
        return -1;

    const int depth = m_rows.at(row).depth;
    for (int i = row - 1; i >= 0; --i) {
        if (m_rows.at(i).depth < depth)
            return i;
    }
    return -1;
}

void FileTreeModel::collapseAll() {
    if (m_expanded.isEmpty())
        return;

    beginResetModel();
    m_expanded.clear();
    m_rows = buildRows();
    endResetModel();
    watchDirectories();
}

bool FileTreeModel::containsDocument(const QUrl &documentUrl) const {
    if (m_rootPath.isEmpty())
        return false;
    // A document that has never been saved has no folder of its own, so it
    // must not move a tree that is already open.
    if (!documentUrl.isLocalFile())
        return true;

    const QFileInfo info(documentUrl.toLocalFile());
    const QString directory = QDir::cleanPath(
        info.isDir() ? info.absoluteFilePath() : info.absolutePath());
    if (directory == m_rootPath)
        return true;

    const QString prefix = m_rootPath.endsWith(QLatin1Char('/'))
        ? m_rootPath : m_rootPath + QLatin1Char('/');
    return directory.startsWith(prefix);
}

// Markdown is what Omawrite writes; plain text is what it can also read.
// Everything else beside the document would only get in the way.
bool FileTreeModel::markdownFile(const QString &fileName) {
    static const QStringList suffixes = {
        QStringLiteral("md"), QStringLiteral("markdown"), QStringLiteral("mdown"),
        QStringLiteral("mkd"), QStringLiteral("txt"),
    };
    return suffixes.contains(QFileInfo(fileName).suffix().toLower());
}

QVector<FileTreeModel::Row> FileTreeModel::buildRows() const {
    QVector<Row> rows;
    if (m_rootPath.isEmpty() || !QFileInfo(m_rootPath).isDir())
        return rows;

    QSet<QString> ancestors;
    const QString canonical = QFileInfo(m_rootPath).canonicalFilePath();
    if (!canonical.isEmpty())
        ancestors.insert(canonical);

    appendRows(m_rootPath, 0, 0, ancestors, rows);
    return rows;
}

void FileTreeModel::appendRows(const QString &directory, int depth, int nesting,
                               QSet<QString> &ancestors, QVector<Row> &rows) const {
    if (nesting > maximumNesting)
        return;

    const QFileInfoList found = QDir(directory).entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::NoSort);

    QVector<QFileInfo> entries;
    entries.reserve(found.size());
    for (const QFileInfo &info : found) {
        if (info.fileName().startsWith(QLatin1Char('.')))
            continue;
        if (!info.isDir() && !markdownFile(info.fileName()))
            continue;
        entries.append(info);
    }

    // Folders first, then files, each in the order a person reads names.
    std::sort(entries.begin(), entries.end(), [](const QFileInfo &left, const QFileInfo &right) {
        if (left.isDir() != right.isDir())
            return left.isDir();
        const int folded = QString::compare(left.fileName(), right.fileName(),
                                            Qt::CaseInsensitive);
        return folded != 0 ? folded < 0 : left.fileName() < right.fileName();
    });

    for (const QFileInfo &info : entries) {
        const bool isDirectory = info.isDir();
        rows.append({info.fileName(), info.absoluteFilePath(), isDirectory, depth});

        if (!isDirectory || !m_expanded.contains(info.absoluteFilePath()))
            continue;

        const QString canonical = info.canonicalFilePath();
        if (!canonical.isEmpty()) {
            if (ancestors.contains(canonical))
                continue;
            ancestors.insert(canonical);
        }
        appendRows(info.absoluteFilePath(), depth + 1, nesting + 1, ancestors, rows);
        if (!canonical.isEmpty())
            ancestors.remove(canonical);
    }
}

void FileTreeModel::watchDirectories() {
    QStringList wanted;
    if (!m_rootPath.isEmpty() && QFileInfo(m_rootPath).isDir())
        wanted.append(m_rootPath);
    for (const QString &path : m_expanded) {
        if (QFileInfo(path).isDir() && !wanted.contains(path))
            wanted.append(path);
    }

    // Only touch the watcher for what actually changed: removing and re-adding
    // a path on every refresh could drop changes arriving in between.
    const QStringList watched = m_watcher.directories();
    for (const QString &path : watched) {
        if (!wanted.contains(path))
            m_watcher.removePath(path);
    }
    for (const QString &path : wanted) {
        if (!watched.contains(path))
            m_watcher.addPath(path);
    }
}
