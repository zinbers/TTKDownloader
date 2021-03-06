#include "downloadlistwidgets.h"
#include "downloadunits.h"
#include "downloadlistitemwidget.h"
#include "downloadobject.h"
#include "downloaduiobject.h"
#include "downloadsettingmanager.h"
#include "downloadabstracttablewidget.h"
#include "downloadlistconfigmanager.h"
#include "downloadurlutils.h"
#include "downloadnumberdefine.h"
#include "downloadmessagebox.h"
#include "downloadtopareawidget.h"

#include <qmath.h>

#include <QMenu>
#include <QClipboard>
#include <QHeaderView>
#include <QApplication>

DownloadListWidgets::DownloadListWidgets(QWidget *parent)
    : DownloadAbstractTableWidget(parent)
{
    setColumnCount(1);
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    m_maxDownloadCount = 0;
    m_speedTimer.setInterval(MT_S2MS/4);
    connect(&m_speedTimer, SIGNAL(timeout()), SLOT(updateTotalSpeedLabel()));
    m_speedTimer.start();
}

DownloadListWidgets::~DownloadListWidgets()
{
    m_speedTimer.stop();
    DownloadLists lists;
    foreach(DownloadUnits *item, m_itemList)
    {
        DownloadList list;
        list.m_url = item->getUrl();
        list.m_name = QFileInfo(item->getDownloadedPath()).fileName();
        lists << list;
    }

    DownloadListConfigManager xml;
    xml.writeListConfig(lists);

    clearItems();
}

void DownloadListWidgets::init()
{
    DownloadListConfigManager xml;
    if(!xml.readListXMLConfig())
    {
        return;
    }

    DownloadLists lists;
    xml.readListConfig(lists);

    foreach(const DownloadList &list, lists)
    {
        QString url = list.m_url.trimmed();
        if(!url.isEmpty() && !findUrl(url))
        {
            addItemToList(url, list.m_name);
        }
    }
}

QString DownloadListWidgets::getClassName()
{
    return staticMetaObject.className();
}

void DownloadListWidgets::resizeWindow()
{
    int w = M_SETTING_PTR->value(DownloadSettingManager::WidgetSize).toSize().width();
    w += M_SETTING_PTR->value(DownloadSettingManager::ExpandModeChoiced).toInt();
    QHeaderView *headerview = horizontalHeader();
    headerview->resizeSection(0, 787 + w - WINDOW_WIDTH_MIN);
}

void DownloadListWidgets::reverseSelect()
{
    TTKIntSet rows;
    foreach(QTableWidgetItem *item, selectedItems())
    {
        rows.insert(item->row());
    }

    setSelectionMode(QAbstractItemView::MultiSelection);

    clearSelection();
    for(int i=0; i<rowCount(); ++i)
    {
        if(!rows.contains(i))
        {
            selectRow(i);
        }
    }

    setSelectionMode(QAbstractItemView::ExtendedSelection);
}

void DownloadListWidgets::pause()
{
    foreach(QTableWidgetItem *item, selectedItems())
    {
        int row = item->row();
        if(m_itemList.isEmpty() || row < 0)
        {
            continue;
        }

        pause(row);
    }

    getTopUrlToDownload();
}

void DownloadListWidgets::start()
{
    foreach(QTableWidgetItem *item, selectedItems())
    {
        int row = item->row();
        if(m_itemList.isEmpty() || row < 0)
        {
            return;
        }

        start(row);
    }
}

void DownloadListWidgets::addItemToList(const QString &path, const QString &name)
{
    if(path.isEmpty())
    {
        return;
    }

    int row = rowCount();
    setRowCount(row + 1);
    DownloadUnits *unit = new DownloadUnits(path, this);
    connect(unit, SIGNAL(removeItemWidget(DownloadUnits*)), SLOT(removeItemWidget(DownloadUnits*)));
    m_itemList << unit;

    DownloadListItemWidget *widget = unit->getDownloadItemWidget();
    QTableWidgetItem *item = new QTableWidgetItem;
    setItem(row, 0, item);
    setRowHeight(row, widget->height());
    setCellWidget(row, 0, widget);

    start(row, name);
}

void DownloadListWidgets::addItemToList(const QStringList &path)
{
    foreach(const QString &pa, path)
    {
        QString url = pa.trimmed();
        if(!url.isEmpty() && !findUrl(url))
        {
            addItemToList(url, QString());
        }
    }
}

void DownloadListWidgets::deleteItemFromList()
{
    deleteItemFromList(false);
}

void DownloadListWidgets::deleteItemFromList(bool file)
{
    foreach(QTableWidgetItem *item, selectedItems())
    {
        int row = item->row();
        if(m_itemList.isEmpty() || row < 0)
        {
            continue;
        }

        int state = m_itemList[row]->getState();
        QString path = m_itemList[row]->getDownloadedPath();
        if(state == 0 || state == 1)
        {
            --m_maxDownloadCount;
        }

        removeCellWidget(row, 0);
        removeRow(row);
        delete m_itemList.takeAt(row);

        if(file)
        {
            QFile::remove(path);
            QFile::remove(path + SET_FILE);
        }
    }

    getTopUrlToDownload();
}

void DownloadListWidgets::deleteItemFromListWithFile()
{
    deleteItemFromList(true);
}

void DownloadListWidgets::removeItemWidget(DownloadUnits *unit)
{
    if(m_itemList.isEmpty())
    {
        return;
    }

    int row = m_itemList.indexOf(unit);
    if(row == -1)
    {
        return;
    }

    emit downloadingFinished(unit->getDownloadedPath(), unit->getUrl());

    --m_maxDownloadCount;
    removeCellWidget(row, 0);
    delete m_itemList.takeAt(row);
    removeRow(row);

    getTopUrlToDownload();
}

void DownloadListWidgets::listCellClicked(int row, int column)
{
    Q_UNUSED(column);
    if(row < 0)
    {
        return;
    }

    stateChanged(row);
}

void DownloadListWidgets::openFileDir()
{
    if(rowCount() == 0 || currentRow() < 0)
    {
        return;
    }

    QString path = m_itemList[currentRow()]->getDownloadedPath();

    if(!DownloadUtils::Url::openUrl(QFileInfo(path).absoluteFilePath(), true))
    {
        DownloadMessageBox message;
        message.setText(tr("The origin one does not exist!"));
        message.exec();
    }
}

void DownloadListWidgets::startClicked()
{
    if(rowCount() == 0 || currentRow() < 0)
    {
        return;
    }

    start(currentRow());
}

void DownloadListWidgets::pauseClicked()
{
    if(rowCount() == 0 || currentRow() < 0)
    {
        return;
    }

    pause(currentRow());
    getTopUrlToDownload();
}

void DownloadListWidgets::copyUrlClicked()
{
    if(rowCount() == 0 || currentRow() < 0)
    {
        return;
    }

    QClipboard *clipBoard = QApplication::clipboard();
    clipBoard->setText(m_itemList[ currentRow() ]->getUrl());
}

void DownloadListWidgets::updateTotalSpeedLabel()
{
    float total = 0;
    foreach(DownloadUnits *item, m_itemList)
    {
        total += item->getDownloadItemWidget()->getPercent();
    }
    DownloadTopAreaWidget::instance()->updateRemoteSpeedText(m_itemList.isEmpty() ? 0 : ceil(total/m_itemList.count()*100));
}

void DownloadListWidgets::contextMenuEvent(QContextMenuEvent *event)
{
    DownloadAbstractTableWidget::contextMenuEvent(event);

    QMenu rightClickMenu(this);
    rightClickMenu.setStyleSheet(DownloadUIObject::MMenuStyle02);

    int row = currentRow();
    rightClickMenu.addAction(tr("Open File"), this, SLOT(openFileDir()))->setEnabled(row > -1);
    rightClickMenu.addSeparator();

    bool downloadState = false;
    if(row > -1 && row < m_itemList.count())
    {
        int s = m_itemList[row]->getState();
        downloadState = (s == 0 || s == 1);
    }
    if(downloadState)
    {
        rightClickMenu.addAction(QIcon(":/contextMenu/lb_stop_normal"), tr("Pause"), this, SLOT(pauseClicked()));
    }
    else
    {
        rightClickMenu.addAction(QIcon(":/contextMenu/lb_start_normal"), tr("NewDownload"), this, SLOT(startClicked()))->setEnabled(row > -1);
    }

    rightClickMenu.addAction(QIcon(":/tiny/btn_close_hover"), tr("Delete"), this, SLOT(deleteItemFromList()))->setEnabled(row > -1);
    rightClickMenu.addAction(QIcon(":/tiny/btn_close_normal"), tr("Delete With File"), this, SLOT(deleteItemFromListWithFile()))->setEnabled(row > -1);
    rightClickMenu.addAction(tr("Sort"));
    rightClickMenu.addAction(tr("Selected All"), this, SLOT(selectAll()));
    rightClickMenu.addSeparator();
    rightClickMenu.addAction(tr("Copy Url"), this, SLOT(copyUrlClicked()))->setEnabled(row > -1);

    rightClickMenu.exec(QCursor::pos());
}

void DownloadListWidgets::clearItems()
{
    while(!m_itemList.isEmpty())
    {
        delete m_itemList.takeLast();
    }
    clear();
}

void DownloadListWidgets::stateChanged(int row)
{
    int state = m_itemList[row]->getState();
    emit downloadStateChanged(state != 1 && state != 0);
}

void DownloadListWidgets::start(int row, const QString &name)
{
    if(row < 0)
    {
        return;
    }

    DownloadUnits *units = m_itemList[row];
    if(m_maxDownloadCount < 3)
    {
        ++m_maxDownloadCount;
        units->start(name);
        stateChanged(row);
    }
    else
    {
        units->setStateChanged(tr("D_Queue"));
    }
}

void DownloadListWidgets::pause(int row)
{
    if(row < 0)
    {
        return;
    }

    --m_maxDownloadCount;
    m_maxDownloadCount = qMax(m_maxDownloadCount, 0);
    m_itemList[row]->pause();
    stateChanged(row);
}

void DownloadListWidgets::getTopUrlToDownload()
{
    int index = -1;
    if(!m_itemList.isEmpty() && m_maxDownloadCount < 3)
    {
        foreach(DownloadUnits *item, m_itemList)
        {
            ++index;
            if(item->getState() == 3)
            {
                start(index);
                break;
            }
        }
    }
}

bool DownloadListWidgets::findUrl(const QString &path) const
{
    bool state = false;
    foreach(DownloadUnits *item, m_itemList)
    {
        if(item->getUrl() == path)
        {
            state = true;
            break;
        }
    }

    return state;
}
