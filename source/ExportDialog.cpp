#include "ExportDialog.h"
#include "DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QDebug>
#include <QFileDialog>
#include <QDir>
#include <QDate>
#include <QtConcurrent>
#include <QCloseEvent>
#include <algorithm>

ExportDialog::ExportDialog(QWidget *parent)
    : QDialog(parent)
    , m_startDateCombo(new QComboBox(this))
    , m_endDateCombo(new QComboBox(this))
    , m_filePathEdit(new QLineEdit(this))
    , m_progressBar(new QProgressBar(this))
    , m_statusLabel(new QLabel("准备导出...", this))
    , m_exportButton(new QPushButton("导出", this))
    , m_cancelButton(new QPushButton("取消", this))
    , m_dateWatcher(new QFutureWatcher<QVector<QDate>>(this))
    , m_datesLoaded(false)
    , m_closing(false)
{
    setWindowTitle("导出数据");
    setMinimumSize(400, 300);

    setupUI();

    // 设置默认文件路径
    QString defaultPath = QDir::currentPath() + "/weather_export_" +
                          QDate::currentDate().toString("yyyyMMdd") + ".csv";
    m_filePathEdit->setText(defaultPath);

    // 连接异步查询完成信号
    connect(m_dateWatcher, &QFutureWatcher<QVector<QDate>>::finished,
            this, &ExportDialog::onDatesLoaded);

    // 启动后台线程获取可用日期
    QFuture<QVector<QDate>> future = QtConcurrent::run([]() {
        return DatabaseManager::instance().getAvailableDates();
    });
    m_dateWatcher->setFuture(future);
}

ExportDialog::~ExportDialog()
{
    if (m_dateWatcher->isRunning()) {
        m_dateWatcher->waitForFinished();
    }
}

void ExportDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QFormLayout* formLayout = new QFormLayout();

    QPushButton* browseButton = new QPushButton("浏览...", this);
    connect(browseButton, &QPushButton::clicked, this, &ExportDialog::onBrowseClicked);

    QHBoxLayout* fileLayout = new QHBoxLayout();
    fileLayout->addWidget(m_filePathEdit);
    fileLayout->addWidget(browseButton);

    formLayout->addRow("开始日期:", m_startDateCombo);
    formLayout->addRow("结束日期:", m_endDateCombo);
    formLayout->addRow("保存路径:", fileLayout);

    mainLayout->addLayout(formLayout);

    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    mainLayout->addWidget(m_progressBar);

    mainLayout->addWidget(m_statusLabel);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);

    connect(m_exportButton, &QPushButton::clicked, this, &ExportDialog::onExportClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &ExportDialog::onCancelClicked);
}

void ExportDialog::onDatesLoaded()
{

    if (m_closing) {
        qDebug() << "onDatesLoaded: dialog closing, ignore";
        return;
    }

    if (m_closing || m_datesLoaded) {
        return;
    }
    m_datesLoaded = true;

    QVector<QDate> dates = m_dateWatcher->result();

    // 按升序排序
    std::sort(dates.begin(), dates.end());

    m_startDateCombo->clear();
    m_endDateCombo->clear();

    for (const QDate& date : std::as_const(dates)) {
        QString dateStr = date.toString("yyyy-MM-dd");
        m_startDateCombo->addItem(dateStr, date);
        m_endDateCombo->addItem(dateStr, date);
    }

    if (!dates.isEmpty()) {
        int total = dates.size();
        int startIndex = qMax(0, total - 30);
        int endIndex = total - 1;
        m_startDateCombo->setCurrentIndex(startIndex);
        m_endDateCombo->setCurrentIndex(endIndex);
    }

    qDebug() << "日期数据加载完成，共" << dates.size() << "天";
}

void ExportDialog::closeEvent(QCloseEvent *event)
{
    if (m_closing) {
        event->accept();
        return;
    }
    m_closing = true;

    // 断开后台查询信号，避免在关闭后尝试更新 UI
    if (m_dateWatcher) {
        disconnect(m_dateWatcher, nullptr, this, nullptr);
    }

    // 接受关闭事件，让窗口正常关闭
    event->accept();
}

// 其余函数保持不变
QDate ExportDialog::startDate() const
{
    return m_startDateCombo->currentData().toDate();
}

QDate ExportDialog::endDate() const
{
    return m_endDateCombo->currentData().toDate();
}

QString ExportDialog::filePath() const
{
    return m_filePathEdit->text();
}

void ExportDialog::onBrowseClicked()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "保存文件",
        QDir::currentPath(),
        "CSV文件 (*.csv);;所有文件 (*.*)"
        );
    if (!fileName.isEmpty()) {
        m_filePathEdit->setText(fileName);
    }
}

void ExportDialog::onExportClicked()
{
    QDate start = startDate();
    QDate end = endDate();
    QString filePath = this->filePath();

    if (!start.isValid() || !end.isValid()) {
        QMessageBox::warning(this, "错误", "请选择有效的日期范围");
        return;
    }
    if (start > end) {
        QMessageBox::warning(this, "错误", "开始日期不能晚于结束日期");
        return;
    }
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "错误", "请选择保存路径");
        return;
    }

    m_exportButton->setEnabled(false);
    m_statusLabel->setText("正在导出数据...");

    emit exportRequested(start, end, filePath);
}

void ExportDialog::onCancelClicked()
{
    reject();
}

void ExportDialog::onExportProgress(int percentage)
{
    m_progressBar->setValue(percentage);
}

void ExportDialog::onExportFinished(bool success, const QString& message)
{
    m_exportButton->setEnabled(true);

    if (success) {
        m_statusLabel->setText("导出完成!");
        QMessageBox::information(this, "成功", message);
        accept();
    } else {
        m_statusLabel->setText("导出失败!");
        QMessageBox::critical(this, "错误", message);
        reject();   // 失败时也关闭对话框
    }
}

