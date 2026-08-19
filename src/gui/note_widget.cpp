#include "note_widget.hpp"
#include "ui_note_widget.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPixmap>
#include <QShowEvent>

#include <algorithm>

#include "services/settingManage.hpp"
#include "utils/stringProcess.hpp"


note_widget::note_widget (QWidget *parent) : QWidget(parent), ui(new Ui::note_widget) {
    ui->setupUi(this);
    initConnect();
    expandComboBox(ui->comboBox);
    ui->splitter->setStretchFactor(0, 6);
    ui->splitter->setStretchFactor(1, 4);
    // 各页面
    initGraphic();
    initConversation();
    initUnit();
}

note_widget::~note_widget () {
    delete ui;
}

/**
 *  @brief 加载飞行高度层[图片]
 */
void note_widget::loadChinaFlightLevel () const {
    static QPixmap pixmap(":/preset/resources/documents/chinaFlightLevel.png");
    ui->presetStackWidget->setCurrentIndex(0);
    flightLevelScene->clear();
    flightLevelScene->addPixmap(pixmap);
    flightLevelScene->setSceneRect(pixmap.rect());
    fitChinaFlightLevelToWidth();
}

void note_widget::fitChinaFlightLevelToWidth () const {
    if (!flightLevelScene)
        return;
    const QSize viewportSize = ui->imageGraphicsView->viewport()->size();
    if (viewportSize.width() <= 0)
        return;
    const QRectF sceneRect = flightLevelScene->sceneRect();
    if (sceneRect.width() <= 0)
        return;
    const double baseScale = static_cast<double>(viewportSize.width()) / sceneRect.width();
    ui->imageGraphicsView->resetTransform();
    ui->imageGraphicsView->scale(baseScale, baseScale);
    ui->imageGraphicsView->horizontalScrollBar()->setValue(ui->imageGraphicsView->horizontalScrollBar()->minimum());
    ui->imageGraphicsView->verticalScrollBar()->setValue(ui->imageGraphicsView->verticalScrollBar()->minimum());
}

void note_widget::showEvent (QShowEvent *event) {
    QWidget::showEvent(event);
    if (flightLevelLoadedOnce)
        return;
    flightLevelLoadedOnce = true;
    loadChinaFlightLevel();
}

void note_widget::submitUnits () const {
    // 设置单位
    SettingsManager &manager = SettingsManager::instance();
    auto comboBoxList = ui->unitConvert->findChildren<QComboBox*>();
    std::vector<std::string> indexList;
    for (const auto item : comboBoxList)
        indexList.push_back(std::format("{}", item->currentIndex()));
    const std::string finalStr = join(indexList, " ");
    manager.set(SettingsManager::unitConvert, QString::fromStdString(finalStr), true);
}

bool note_widget::eventFilter (QObject *watched, QEvent *event) {
    if (watched == ui->imageGraphicsView->viewport() && event->type() == QEvent::Resize) {
        if (flightLevelLoadedOnce)
            fitChinaFlightLevelToWidth();
    }
    return QWidget::eventFilter(watched, event);
}

void note_widget::initConnect () {
    const auto &setting = SettingsManager::instance();
    // 存储设置
    connect(&setting, qOverload<SettingsManager::ConstKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::ConstKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::unitConvert: {
                        // 由于只有一个设置 干脆写这算了
                        const auto rawStr = val.toString().toStdString();
                        const auto strList = split(rawStr);
                        auto comboBoxList = ui->unitConvert->findChildren<QComboBox*>();
                        const auto maxNum = std::min(strList.size(), static_cast<size_t>(comboBoxList.size()));
                        for (auto i = 0; i < maxNum; ++i)
                            comboBoxList[i]->setCurrentIndex(std::stoi(std::string(strList[i])));
                        break;
                    }
                    default:
                        break;
                }
            });
}

void note_widget::initGraphic () {
    flightLevelScene = new QGraphicsScene(this);
    ui->imageGraphicsView->setScene(flightLevelScene);
    ui->imageGraphicsView->viewport()->installEventFilter(this);
    ui->imageGraphicsView->installEventFilter(this);
}

void note_widget::initConversation () const {
    // 多平台字号不一致性
    const int textSize = QFont().pointSize();
    QTextCursor cursor(ui->conversationTextEdit->document());
    cursor.select(QTextCursor::Document);
    QTextCharFormat format;
    format.setFontPointSize(textSize);
    cursor.mergeCharFormat(format);
}

/**
 * @brief 通过当前触发对象,获取同侧和异侧
 * @param name 对象名称
 * @return [另一侧 另一侧 同侧 同侧]
 */
std::tuple<QComboBox*, QLineEdit*, QComboBox*, QLineEdit*> note_widget::getUnit (const QString &name) const {
    static QString sep{"_"};
    const auto groupName = split(name, sep)[1];
    const QString cb_pre = "comboBox_" + groupName + "_";
    const QString le_pre = "lineEdit_" + groupName + "_";
    const QString end = name[name.size() - 1];
    const QString oppoEnd = (end == "1") ? "2" : "1";
    // 开找
    auto cbo = ui->unitConvert->findChild<QComboBox*>(cb_pre + oppoEnd);
    auto *cb = ui->unitConvert->findChild<QComboBox*>(cb_pre + end);
    auto *leo = ui->unitConvert->findChild<QLineEdit*>(le_pre + oppoEnd);
    auto *le = ui->unitConvert->findChild<QLineEdit*>(le_pre + end);
    return {cbo, leo, cb, le};
}

void note_widget::initUnit () const {
    // 复制模型
    ui->comboBox_dis_2->setModel(ui->comboBox_dis_1->model());
    ui->comboBox_spd_2->setModel(ui->comboBox_spd_1->model());
    ui->comboBox_wgt_2->setModel(ui->comboBox_wgt_1->model());
    // 删除指示
    for (const auto *combo : ui->unitConvert->findChildren<QComboBox*>())
        expandComboBox(combo);
    // 关联所有combobox和lineeidt
    for (const QComboBox *cb : ui->unitConvert->findChildren<QComboBox*>())
        connect(cb, &QComboBox::activated, this, &note_widget::unitConvertChange);
    for (const QLineEdit *le : ui->unitConvert->findChildren<QLineEdit*>())
        connect(le, &QLineEdit::editingFinished, this, &note_widget::unitConvertChange);
}

void note_widget::on_comboBox_activated (const int index) const {
    ui->presetStackWidget->setCurrentIndex(index);
}

double getScale (QString text) {
    static const std::vector<std::pair<QString, double>> unitScale = {
        // 距离基准 m
        {"nmi2m", 1852},
        {"km2m", 1000},
        {"m2m", 1},
        {"ft2m", 0.3048},
        // 重量基准 kg
        {"t2kg", 1000},
        {"kg2kg", 1},
        {"lb2kg", 0.45359237},
        // 速度基准 km/h
        {"mps2kmph", 3.6},
        {"kmph2kmph", 1},
        {"kn2kmph", 1.852}
    };
    const auto idx = text.replace("/", "p");
    for (auto &[word,value] : unitScale) {
        if (word.startsWith(idx))
            return value;
    }
    assert(false && "单位换算传的什么钩子东西");
    return 1;
}
/**
 * @brief 大统一,单位转换
 * @note 值优先改变对侧,单位优先改变同侧. 不是我说 写的真好吧
 */
void note_widget::unitConvertChange () const {
    QObject *senderObj = sender();
    auto [cbo,leo,cb,le] = getUnit(senderObj->objectName());
    if (qobject_cast<QComboBox*>(senderObj) == nullptr) {
        std::swap(cbo, cb);
        std::swap(leo, le);
    } else {
        submitUnits();
    }
    // 可能的交换方向
    bool isNum;
    double value = leo->text().toDouble(&isNum);
    if (!isNum) { // 对侧不是数字
        value = le->text().toDouble(&isNum);
        if (!isNum) // 同侧也不是数字
            return;
        std::swap(cbo, cb);
        std::swap(leo, le);
    }
    // 开始计算
    const double baseValue = value * getScale(cbo->currentText());
    value = baseValue / getScale(cb->currentText());
    le->setText(QString::asprintf("%.2f", value));
}
