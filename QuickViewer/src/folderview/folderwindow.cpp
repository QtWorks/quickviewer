#include <QtWidgets>

#include "ui_folderwindow.h"
#include "ui_mainwindow.h"

#include "folderwindow.h"
#include "models/volumemanager.h"
#include "models/qvapplication.h"

FolderWindow::FolderWindow(QWidget *parent, Ui::MainWindow *)
    : QWidget(parent)
    , ui(new Ui::FolderWindow)
    , m_sortModeMenu(nullptr)
    , m_itemContextMenu(nullptr)
    , m_itemModel(this)
    , m_itemDelegate(parent, this)
{
    ui->setupUi(this);

#ifdef Q_OS_MACOS
    ui->menuBar->setNativeMenuBar(false);
#endif

    ui->folderView->installEventFilter(this);

    // folderView
    ui->folderView->setModel(&m_itemModel);
    ui->folderView->setItemDelegate(&m_itemDelegate);

    resetSortMode();

    // menus
    ui->menuBar->removeAction(ui->menuSort->menuAction());
    m_sortModeMenu = ui->menuSort;
    ui->menuBar->removeAction(ui->menuItemContext->menuAction());
    m_itemContextMenu = ui->menuItemContext;
}

FolderWindow::~FolderWindow()
{
    if(m_sortModeMenu)
        delete m_sortModeMenu;
    if(m_itemContextMenu)
        delete m_itemContextMenu;
    delete ui;
}

void FolderWindow::setAsToplevelWindow()
{
    ui->menuBar->setVisible(true);
    ui->folderView->header()->setVisible(true);
    QRect rect = geometry();
    ui->folderView->setColumnWidth(0, rect.width()-150);
    ui->folderView->setColumnWidth(1, 150);
    m_itemModel.setColumns(2);
}

void FolderWindow::setAsInnerWidget()
{
    ui->menuBar->setVisible(false);
    ui->folderView->header()->setVisible(false);
    m_itemModel.setColumns(1);
}

static QModelIndex selectedIdx;

bool FolderWindow::eventFilter(QObject *obj, QEvent *event)
{
//    qDebug() << obj << event << event->type();
//    QMouseEvent *mouseEvent = NULL;
    QContextMenuEvent *contextEvent = nullptr;
    switch (event->type()) {
    default:
        break;
    case QEvent::ContextMenu:
        contextEvent = dynamic_cast<QContextMenuEvent*>(event);
        QPoint inner = ui->folderView->mapFromGlobal(QCursor::pos());
        selectedIdx = ui->folderView->indexAt(inner);
        m_itemContextMenu->exec(QCursor::pos());
        return true;
    }
    return QObject::eventFilter(obj, event);
}


void FolderWindow::onActionSetAsHomeFolder_triggered()
{
    int row = selectedIdx.row();
    if(row < 0 || row >= m_volumes.size())
        return;
    const QvFolderItem& item = m_volumes[row];
    QDir dir(m_currentPath);
    qApp->setHomeFolderPath(dir.filePath(item.name));
}


void FolderWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if(e->mimeData()->hasFormat("text/uri-list"))
    {
        e->acceptProposedAction();
    }
}

void FolderWindow::dropEvent(QDropEvent *e)
{
    if(!e->mimeData()->hasUrls())
        return;
    QList<QUrl> urlList = e->mimeData()->urls();
    for (int i = 0; i < urlList.size(); i++) {
        QUrl url = urlList[i];
        QFileInfo info(url.toLocalFile());
        if(info.isDir() || info.isFile()) {
            setFolderPath(info.absoluteFilePath(), false);
            break;
        }
    }
}

void FolderWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    qApp->setFolderViewWidth(event->size().width());
    resetPathLabel(event->size().width());

}

static bool filenameLessThan(const QvFolderItem& lhs, const QvFolderItem& rhs)
{
    if(lhs.type != rhs.type)
        return lhs.type < rhs.type;
    return IFileLoader::caseInsensitiveLessThan(lhs.name, rhs.name);
}

static bool updatedAtLessThan(const QvFolderItem& lhs, const QvFolderItem& rhs)
{
    if(lhs.type != rhs.type)
        return lhs.type < rhs.type;
    return lhs.updated_at < rhs.updated_at;
}

//IFileLoader::caseInsensitiveLessThan

void FolderWindow::setFolderPath(QString path, bool showParent)
{
#ifdef Q_OS_WIN
    if(path.isEmpty()) {
        m_volumes.clear();
        {
            m_currentPath = "";
            QList<QFileInfo> drives = QDir::drives();
            foreach(QFileInfo drive , drives){
                m_volumes << QvFolderItem(drive.absoluteFilePath(), QvFolderItem::Dir, drive.lastModified());
            }
        }
    } else
#else
    if(path.isEmpty()) {
        path = "/";
    }
#endif
    {
        QFileInfo fileinfo(path);
        if(!fileinfo.exists())
            return;

        QDir dir;
        if(fileinfo.isDir()) {
            dir.setPath(QDir::toNativeSeparators(showParent ? fileinfo.canonicalPath() : path));
        } else {
            dir.setPath(QDir::toNativeSeparators(fileinfo.dir().path()));
        }

        m_currentPath = dir.path();
        resetPathLabel(width());

        m_volumes.clear();
        {
            QStringList subfolders = dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs, QDir::Unsorted);
            foreach(const QString& sf, subfolders) {
                QFileInfo fi(dir.absoluteFilePath(sf));
                m_volumes << QvFolderItem(sf, QvFolderItem::Dir, fi.lastModified());
            }
        }

        {
            QStringList archives;
            foreach(const QString name, dir.entryList(QDir::NoDotAndDotDot | QDir::Files, QDir::Unsorted)) {
                if(!IFileLoader::isArchiveFile(name) && !IFileLoader::isImageFile(name))
                    continue;
                archives << name;
            }
            foreach(const QString& ar, archives) {
                QFileInfo fi(dir.absoluteFilePath(ar));
                m_volumes << QvFolderItem(ar, QvFolderItem::Archive, fi.lastModified());
            }
        }
        qvEnums::FolderViewSort sortmode = qApp->FolderSortMode();
        if(sortmode == qvEnums::OrderByName) {
            qSort(m_volumes.begin(), m_volumes.end(), filenameLessThan);
        } else {
            typedef std::reverse_iterator<QList<QvFolderItem>::iterator> reverse_iterator;
//            qSort(m_volumes.rbegin(), m_volumes.rend(), updatedAtLessThan);
            qSort(reverse_iterator(m_volumes.end()), reverse_iterator(m_volumes.begin()), updatedAtLessThan);
        }
    }

    if(m_volumes.empty()) {
        m_volumes << QvFolderItem(tr("Not found any Folders or Archives", "Display when there is no display item in Folder Window"), QvFolderItem::NoItems, QDateTime());
    }
    m_itemModel.setVolumes(&m_volumes);

    if(showParent)
        onPageManager_volumeChanged(path);
}

void FolderWindow::reset()
{
    m_itemModel.setVolumes(&m_volumes);
}

void FolderWindow::resetSortMode()
{
    qvEnums::FolderViewSort sortMode = qApp->FolderSortMode();
    ui->actionOrderByName->setChecked(sortMode == qvEnums::OrderByName);
    ui->actionOrderByUpdatedAt->setChecked(sortMode == qvEnums::OrderByUpdatedAt);
    ui->sortModeButton->setText(sortMode == qvEnums::OrderByName
                                ? ui->actionOrderByName->text()
                                : ui->actionOrderByUpdatedAt->text());
}

void FolderWindow::resetPathLabel(int )
{
//    QFontMetrics fontMetrics(ui->pathLabel->font());
//    QString pathLabelTxt = fontMetrics.elidedText(
//                QDir::toNativeSeparators(m_currentPath), Qt::ElideMiddle, maxWidth-10);
//    ui->pathLabel->setText(pathLabelTxt);
    ui->pathLabel->setText(m_currentPath);
}

QString FolderWindow::itemPath(const QModelIndex &index)
{
    QDir dir(m_currentPath);
    QString filename = m_volumes[index.row()].name;
    return dir.absoluteFilePath(filename);
}

const static QKeySequence seqReturn("Return");
const static QKeySequence seqEnter("Num+Enter");
const static QKeySequence seqBackspace("Backspace");

void FolderWindow::keyPressEvent(QKeyEvent *event)
{
    QKeySequence seq(event->key() | event->modifiers());
    qDebug() << seq;
    if(seq == seqReturn || seq == seqEnter) {
        on_currentItem_triggered();
        return;
    }
    if(seq == seqBackspace) {
        if(m_historyPrev.empty())
            onParentButton_clicked();
        else
            onPrevButton_clicked();
        return;
    }
}

void FolderWindow::mousePressEvent(QMouseEvent *event)
{
    // 5 buttons mouse forward for browsers
    if(event->button() == Qt::ForwardButton) {
        onNextButton_clicked();
        return;
    }
    // 5 buttons mouse back for browsers
    if(event->button() == Qt::BackButton) {
        onPrevButton_clicked();
        return;
    }
}

void FolderWindow::onHomeButton_clicked()
{
    if(m_historyPrev.contains(m_currentPath))
        m_historyPrev.removeOne(m_currentPath);
    m_historyPrev << m_currentPath;
    setFolderPath(qApp->HomeFolderPath());
    emit openVolume(qApp->HomeFolderPath());
}

void FolderWindow::onPrevButton_clicked()
{
    if(m_historyPrev.empty())
        return;
    if(m_historyNext.contains(m_currentPath))
        m_historyNext.removeOne(m_currentPath);
    m_historyNext << m_currentPath;
    QString path = m_historyPrev.takeLast();

    setFolderPath(path, false);
    emit openVolume(path);
}

void FolderWindow::onNextButton_clicked()
{
    if(m_historyNext.empty())
        return;
    if(m_historyPrev.contains(m_currentPath))
        m_historyPrev.removeOne(m_currentPath);
    m_historyPrev << m_currentPath;
    QString path = m_historyNext.takeLast();
    setFolderPath(path, false);
    emit openVolume(path);
}

void FolderWindow::onParentButton_clicked()
{
    if(m_currentPath.isEmpty())
        return;
    QFileInfo info(m_currentPath);
    setFolderPath(m_currentPath == info.canonicalPath() ? "" : info.canonicalPath(), false);
    emit openVolume(info.canonicalPath());
}

void FolderWindow::onReloadButton_clicked()
{
    if(m_currentPath.isEmpty())
        return;
    setFolderPath(m_currentPath, false);
}

void FolderWindow::onPageManager_volumeChanged(QString path)
{
    QFileInfo info(QDir::toNativeSeparators(path));
    if(!info.exists() || m_currentPath != info.canonicalPath())
        return;
    QString name = info.fileName();
    int row = -1;
    foreach(const QvFolderItem& item, m_volumes) {
        row++;
        if(name != item.name)
            continue;
        QModelIndex midx = m_itemModel.index(row, 0, QModelIndex());
        ui->folderView->setCurrentIndex(midx);

        break;
    }
}

void FolderWindow::on_itemSingleClicked(const QModelIndex &index)
{
    int row = index.row();
    if(row >= m_volumes.size())
        return;
    QvFolderItem& item = m_volumes[row];
    if(item.type == QvFolderItem::NoItems)
        return;
    QDir dir(m_currentPath);
    QString subpath = dir.absoluteFilePath(item.name);
    emit openVolume(subpath);
}

void FolderWindow::on_itemDoubleClicked(const QModelIndex &index)
{
    int row = index.row();
    if(row >= m_volumes.size())
        return;
    QvFolderItem& item = m_volumes[row];
    if(item.type == QvFolderItem::NoItems)
        return;
    QDir dir(m_currentPath);
    QString subpath = dir.absoluteFilePath(item.name);
    emit openVolume(subpath);

    if(m_historyPrev.contains(m_currentPath))
        m_historyPrev.removeOne(m_currentPath);
    m_historyPrev << m_currentPath;
    setFolderPath(subpath, false);
}

void FolderWindow::on_currentItem_triggered()
{
    on_itemDoubleClicked(ui->folderView->currentIndex());
}

void FolderWindow::onSortModeButton_clicked()
{
    QWidget* widget = ui->sortModeButton;

    QPoint p = widget->mapToGlobal(QPoint(0, widget->height()));
    m_sortModeMenu->exec(p);
}

void FolderWindow::onActionOrderByName_triggered()
{
    qApp->setFolderSortMode(qvEnums::OrderByName);
    resetSortMode();
    onReloadButton_clicked();
}

void FolderWindow::onActionOrderByUpdatedAt_triggered()
{
    qApp->setFolderSortMode(qvEnums::OrderByUpdatedAt);
    resetSortMode();
    onReloadButton_clicked();
}

void FolderWindow::closeEvent(QCloseEvent *e)
{
    QWidget::closeEvent(e);
    emit closed();
}
