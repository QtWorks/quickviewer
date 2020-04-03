#include <QtWidgets>
#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#include "ui_catalogwindow.h"
#include "ui_mainwindow.h"

#include "qc_init.h"
#include "catalogwindow.h"
#include "managedatabasedialog.h"
#include "qvapplication.h"
#include "flowlayout.h"

static void clearLayout(QLayout* layout)
{
    QLayoutItem *item;
    if(!layout)
        return;
    while((item = layout->takeAt(0))) {
        if (item->layout()) {
            clearLayout(item->layout());
            delete item->layout();
        }
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

CatalogWindow::CatalogWindow(QWidget *parent, Ui::MainWindow *uiMain)
    : QWidget(parent)
    , ui(new Ui::CatalogWindow)
    , m_folderViewMenu(this)
    , m_itemModel(this)
{
    ui->setupUi(this);

#ifdef Q_OS_MACOS
    ui->menuBar->setNativeMenuBar(false);
#endif

    // VolumeView
    m_itemModel.setViewMode(qApp->CatalogViewModeSetting());
    ui->volumeList->setModel(&m_itemModel);

    // SearchCombo
    connect(ui->searchCombo->lineEdit(), SIGNAL(editingFinished()), this, SLOT(onLineEdit_editingFinished()));
    ui->searchCombo->lineEdit()->setPlaceholderText(tr("Field the search term and press enter key to search by the title.", "Gray text that prompts a keyword search of Volume"));

    // TagFrame
    if(!qApp->ShowTagBar())
        ui->tagFrame->setVisible(false);
    if(ui->tagFrame->layout()) {
        clearLayout(ui->tagFrame->layout());
        delete ui->tagFrame->layout();
    }
    QLayout *layout = new FlowLayout;
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    ui->tagFrame->setLayout(layout);

    // Status Bar
    ui->statusBar->addPermanentWidget(ui->statusLabel);
    ui->statusLabel->setText(tr("Drop picture folder here and create a catalog.", "Status bar text briefly explaining how to use CatalogWindow"));

    ui->menu_View->addAction(uiMain->actionShowTagBar);
    ui->menu_View->addAction(uiMain->actionCatalogIconLongText);
    ui->menu_View->addAction(uiMain->actionCatalogTitleWithoutOptions);
    ui->menu_View->addAction(uiMain->actionSearchTitleWithOptions);

    m_folderViewMenu.addAction(ui->actionFolderViewList);
    m_folderViewMenu.addAction(ui->actionFolderViewIcon);
    m_folderViewMenu.addAction(ui->actionFolderViewIconNoText);
}

CatalogWindow::~CatalogWindow()
{
    delete ui;
}

void CatalogWindow::setThumbnailManager(ThumbnailManager *manager)
{
    m_thumbManager = manager;
    m_volumes = m_thumbManager->volumes();
    initTagButtons();

    resetViewMode();
    searchByWord(true);
}

void CatalogWindow::resetViewMode()
{
    switch(qApp->CatalogViewModeSetting()) {
    case qvEnums::List: onActionFolderViewList_triggered(); break;
    case qvEnums::Icon: onActionFolderViewIcon_triggered(); break;
    case qvEnums::IconNoText: onActionFolderViewNotext_triggered(); break;
    }

}

void CatalogWindow::setAsToplevelWindow()
{
    ui->menuBar->setVisible(true);
    ui->statusLabel->setWordWrap(false);
}

void CatalogWindow::setAsInnerWidget()
{
    ui->menuBar->setVisible(false);
    ui->statusLabel->setWordWrap(true);
    ui->searchCombo->setFocus();
}

bool CatalogWindow::isCatalogSearching()
{
    return ui->searchCombo->hasFocus();
}

void CatalogWindow::resetTagButtons(QStringList buttons, QStringList checks)
{
    // Reset Tag Buttons
    clearLayout(ui->tagFrame->layout());
    for(int i = 0; i < buttons.size(); i++) {
        QPushButton* b = new QPushButton;
        QString name = buttons[i];
        b->setText(name);
        b->setCheckable(true);
        if(checks.contains(name))
            b->setChecked(true);
        connect(b, SIGNAL(clicked(bool)), this, SLOT(on_tagButtonClicked(bool)));
        ui->tagFrame->layout()->addWidget(b);
    }

}

void CatalogWindow::initTagButtons()
{
    QStringList buttons;
    if(qApp->ShowTagBar()) {
        QMap<int, TagRecord*> tags = m_thumbManager->tagsByCount();
        if(tags.size() <= 1)
            return;

        int cnt = 0;
        foreach(int i, tags.keys()) {
            buttons << tags[i]->name;
            if(cnt++ >= 7)
                break;
        }
    }
    resetTagButtons(buttons, QStringList());
}

QStringList CatalogWindow::getTagWords()
{
    QStringList result;
    QLayout* layout = ui->tagFrame->layout();
    for(int i = 0; i < layout->count(); i++) {
        QLayoutItem *item = layout->itemAt(i);
        if (!item->widget())
            continue;
        QPushButton* b = dynamic_cast<QPushButton*>(item->widget());
        if(!b || !b->isChecked())
            continue;
        result << b->text();
    }
    return result;
}

void CatalogWindow::resetVolumes()
{
    m_itemModel.setVolumes(&m_volumeSearch);
    if(!m_volumes.size())
        return;
    QString volumestxt = QString(tr("(%1/%2) volume display.", "Text of the status bar showing [the number of hits]/[total number] of Volume"))
            .arg(m_volumeSearch.size()).arg(m_volumes.size());
    ui->statusLabel->setText(volumestxt);


}

void CatalogWindow::searchByWord(bool doForce)
{
    QString search = ui->searchCombo->currentText();

    // Tag Buttons as search words
    search += " " + getTagWords().join(" ");


    search = search.trimmed();
    if(!doForce && search == m_lastSearchWord)
        return;
    m_lastSearchWord = search;

//    int cnt = 0;
    m_volumeSearch.clear();
    SearchWords searchwords(search.toLower());
    foreach(const VolumeThumbRecord& vtr , m_volumes) {
        if(vtr.thumbnail.isEmpty())
            continue;
        QString title = qApp->SearchTitleWithOptions() ? vtr.nameNoCase : vtr.realnameNoCase;
        if(!searchwords.match(title))
            continue;
//        if(qApp->MaxShowFrontpage() < ++cnt)
//            break;
        m_volumeSearch.append(const_cast<VolumeThumbRecord*>(&vtr));
    }
    resetVolumes();
}

void CatalogWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if(e->mimeData()->hasFormat("text/uri-list"))
    {
        e->acceptProposedAction();
    }
}

void CatalogWindow::dropEvent(QDropEvent *e)
{
    ManageDatabaseDialog dialog(this);
    dialog.setThumbnailManager(m_thumbManager);
    dialog.dropEvent(e);
    dialog.exec();

    m_volumes = m_thumbManager->volumes();
    initTagButtons();
    searchByWord(true);

    resetVolumes();
}

void CatalogWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    qApp->setCatalogViewWidth(event->size().width());
}

void CatalogWindow::on_treeItemChanged(QString)
{
    //ui->pathCombo->setCurrentText(QDir::toNativeSeparators(path));
}

void CatalogWindow::onFolderViewButton_clicked()
{
    QWidget* widget = ui->folderViewButton;

    QPoint p = widget->mapToGlobal(QPoint(0, widget->height()));
    m_folderViewMenu.exec(p);
}

void CatalogWindow::onActionFolderViewList_triggered()
{
    qApp->setCatalogViewModeSetting(qvEnums::List);
    ui->actionFolderViewList->setChecked(true);
    ui->actionFolderViewIcon->setChecked(false);
    ui->actionFolderViewIconNoText->setChecked(false);
    m_itemModel.setViewMode(qvEnums::List);
    ui->volumeList->setResizeMode(QListView::Adjust);
    if(qApp->IconLongText()) {
        ui->volumeList->setGridSize(QSize(300, 100));
        ui->volumeList->setTextElideMode(Qt::ElideRight);
    } else {
        ui->volumeList->setGridSize(QSize(200, 100));
        ui->volumeList->setTextElideMode(Qt::ElideNone);
    }
    ui->volumeList->setViewMode(QListView::ListMode);
    ui->volumeList->setUniformItemSizes(true);

    resetVolumes();
}

void CatalogWindow::onActionFolderViewIcon_triggered()
{
    qApp->setCatalogViewModeSetting(qvEnums::Icon);
    ui->actionFolderViewList->setChecked(false);
    ui->actionFolderViewIcon->setChecked(true);
    ui->actionFolderViewIconNoText->setChecked(false);
    m_itemModel.setViewMode(qvEnums::Icon);
    ui->volumeList->setResizeMode(QListView::Adjust);
    if(qApp->IconLongText()) {
        ui->volumeList->setGridSize(QSize(150, 170));
        ui->volumeList->setTextElideMode(Qt::ElideRight);
    } else {
        ui->volumeList->setGridSize(QSize(150, 120));
        ui->volumeList->setTextElideMode(Qt::ElideNone);
    }
    ui->volumeList->setViewMode(QListView::IconMode);
    ui->volumeList->setUniformItemSizes(true);

    resetVolumes();
}

void CatalogWindow::onActionFolderViewNotext_triggered()
{
    qApp->setCatalogViewModeSetting(qvEnums::IconNoText);
    ui->actionFolderViewList->setChecked(false);
    ui->actionFolderViewIcon->setChecked(false);
    ui->actionFolderViewIconNoText->setChecked(true);
    m_itemModel.setViewMode(qvEnums::IconNoText);
    ui->volumeList->setResizeMode(QListView::Adjust);
    ui->volumeList->setViewMode(QListView::IconMode);
    ui->volumeList->setGridSize(QSize(100, 100));
    ui->volumeList->setUniformItemSizes(true);

    resetVolumes();
}

void CatalogWindow::onManageCatalogButton_clicked()
{
    ManageDatabaseDialog dialog(this);
    dialog.setThumbnailManager(m_thumbManager);
    dialog.exec();

    m_volumes = m_thumbManager->volumes();
    initTagButtons();
    searchByWord(true);
}

void CatalogWindow::onSearchCombo_editTextChanged(QString search)
{
    qDebug() << search;
//    if(m_volumes.size() < qApp->MaxSearchByCharChanged())
        searchByWord();
    return;
}

void CatalogWindow::onSearchCombo_currentIndexChanged(QString search)
{
    qDebug() << "on_searchTextIndexChanged: " << search;
    searchByWord();
}

void CatalogWindow::onLineEdit_editingFinished()
{
    qDebug() << "on_searchTextFinished:";
    searchByWord();
}


void CatalogWindow::on_itemDoubleClicked(const QModelIndex &index)
{
    int row = index.row();
    if(row >= m_volumeSearch.size())
        return;
    emit openVolume(m_volumeSearch[row]->path);

    // reset tag buttons as current book
    QList<TagRecord> tags = m_thumbManager->getTagsFromVolumeId(m_volumeSearch[row]->id);
    QStringList tagtxt;
    foreach(const TagRecord& t, tags) {
        tagtxt << t.name;
    }

    resetTagButtons(tagtxt, getTagWords());
}

void CatalogWindow::onActionSearchTitleWithOptions_triggered(bool enable)
{
    qApp->setSearchTitleWithOptions(enable);
    searchByWord(true);
}

void CatalogWindow::onActionCatalogTitleWithoutOptions_triggered(bool enable)
{
    qApp->setTitleWithoutOptions(enable);
    searchByWord(true);
}

void CatalogWindow::on_tagButtonClicked(bool )
{
    searchByWord();
}

void CatalogWindow::on_showTagBar_triggered(bool enable)
{
    qApp->setShowTagBar(enable);
    ui->tagFrame->setVisible(enable);
    if(enable) {
        initTagButtons();
    } else {
        resetTagButtons(QStringList(),QStringList());
    }
    searchByWord();
}

void CatalogWindow::closeEvent(QCloseEvent *e)
{
    QWidget::closeEvent(e);
    emit closed();
}

SearchWords::SearchWords(const QString &searchNoCase)
{
    if(searchNoCase.isEmpty()) {
        isEmpty = true;
        return;
    }
    isEmpty = false;
    foreach(const QString& s, searchNoCase.trimmed().split(" ")) {
        if(s.isEmpty())
            continue;
        if(s[0] == '-')
            nomatches << s.mid(1);
        else
            matches << s;
    }
}

bool SearchWords::match(const QString &targetNoCase)
{
    if(isEmpty)
        return true;
    foreach(const QString& s, matches) {
        if(!targetNoCase.contains(s))
            return false;
    }
    foreach(const QString& s, nomatches) {
        if(targetNoCase.contains(s))
            return false;
    }
    return true;
}
