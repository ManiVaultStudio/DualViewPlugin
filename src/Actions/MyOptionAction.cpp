// This file is derived from "OptionAction.h" originally located at :
// core/ManiVault/src/plugins/PointData/src/actions/OptionAction.cpp
// and is licensed under the LGPL-3.0-or-later license.
// Copyright (C) 2023 BioVault (Biomedical Visual Analytics Unit LUMC - TU Delft) 
//
// Modifications:
// - This is a temporary solution to support input of multiple dimension names seperated by commas.
// - Skip match for model in setCurrentText() to allow input of multiple dimension names.

#include "MyOptionAction.h"

#include "Application.h"

#include "util/Serialization.h"

#include <QComboBox>
#include <QDebug>
#include <QHBoxLayout>
#include <QListView>
#include <QStylePainter>

using namespace mv::util;



MyOptionAction::MyOptionAction(QObject* parent, const QString& title, const QStringList& options /*= QStringList()*/, const QString& currentOption /*= ""*/) :
    WidgetAction(parent, title),
    _customModel(nullptr),
    _currentIndex(-1)
{
    setText(title);
    setDefaultWidgetFlags(WidgetFlag::Default);
    initialize(options, currentOption);
}

void MyOptionAction::initialize(const QStringList& options /*= QStringList()*/, const QString& currentOption /*= ""*/)
{
    setOptions(options);
    setCurrentText(currentOption);
}

void MyOptionAction::initialize(QAbstractItemModel& customModel, const QString& currentOption /*= ""*/, const QString& defaultOption /*= ""*/)
{
    setCustomModel(&customModel);
    setCurrentText(currentOption);
}

QStringList MyOptionAction::getOptions() const
{
    QStringList options;

    for (int rowIndex = 0; rowIndex < getModel()->rowCount(); ++rowIndex)
        options << getModel()->index(rowIndex, 0).data(Qt::DisplayRole).toString();

    return options;
}

std::uint32_t MyOptionAction::getNumberOfOptions() const
{
    return getModel()->rowCount();
}

bool MyOptionAction::hasOption(const QString& option) const
{
    if (option.isEmpty())
        return false;

    return getModel()->match(getModel()->index(0, 0), Qt::DisplayRole, option).count() == 1;
}

bool MyOptionAction::hasOptions() const
{
    return !getOptions().isEmpty();
}

void MyOptionAction::setOptions(const QStringList& options)
{
    if (_defaultModel.stringList() == options)
        return;

    const auto oldCurrentIndex  = _currentIndex;
    const auto oldCurrentText   = getCurrentText();

    if (_defaultModel.rowCount() == options.count()) {
        for (std::int32_t rowIndex = 0; rowIndex < _defaultModel.rowCount(); rowIndex++)
            _defaultModel.setData(_defaultModel.index(rowIndex, 0), options.at(rowIndex));
    }
    else {
        _defaultModel.setStringList(options);
    }

    _currentIndex = getOptions().contains(oldCurrentText) ? getOptions().indexOf(oldCurrentText) : -1;

    emit modelChanged();

    emit currentIndexChanged(_currentIndex);
    emit currentTextChanged(getCurrentText());
}

const QAbstractItemModel* MyOptionAction::getModel() const
{
    if (_customModel != nullptr)
        return _customModel;

    return &_defaultModel;
}

void MyOptionAction::connectToPublicAction(WidgetAction* publicAction, bool recursive)
{
    auto publicOptionAction = dynamic_cast<MyOptionAction*>(publicAction);

    Q_ASSERT(publicOptionAction != nullptr);

    if (publicOptionAction == nullptr)
        return;

    const auto updatePublicOptions = [this, publicOptionAction]() -> void {
        auto publicOptions = publicOptionAction->getOptions();

        publicOptions << getOptions();

        publicOptions.removeDuplicates();
        publicOptions.sort();

        publicOptionAction->setOptions(publicOptions);
    };

    updatePublicOptions();

    if (publicOptionAction->getCurrentText().isEmpty() && !getCurrentText().isEmpty())
        publicOptionAction->setCurrentText(getCurrentText());

    const auto currentTextChanged = [this, publicOptionAction](const QString& currentText) -> void {
        if (currentText.isEmpty())
            return;

        publicOptionAction->setCurrentText(currentText);
    };

    connect(this, &MyOptionAction::currentTextChanged, this, currentTextChanged);

    connect(publicOptionAction, &MyOptionAction::currentTextChanged, this, [this, publicOptionAction, currentTextChanged](const QString& currentText) -> void {
        disconnect(this, &MyOptionAction::currentTextChanged, this, nullptr);
        {
            setCurrentText(currentText);
        }
        connect(this, &MyOptionAction::currentTextChanged, this, currentTextChanged);
    });

    connect(this, &MyOptionAction::modelChanged, this, updatePublicOptions);

    if (hasOption(publicOptionAction->getCurrentText()))
        setCurrentText(publicOptionAction->getCurrentText());

    WidgetAction::connectToPublicAction(publicAction, recursive);
}

void MyOptionAction::disconnectFromPublicAction(bool recursive)
{
    if (!isConnected())
        return;

    auto publicOptionAction = dynamic_cast<MyOptionAction*>(getPublicAction());

    Q_ASSERT(publicOptionAction != nullptr);

    if (publicOptionAction == nullptr)
        return;

    disconnect(this, &MyOptionAction::currentTextChanged, this, nullptr);
    disconnect(this, &MyOptionAction::modelChanged, this, nullptr);
    disconnect(publicOptionAction, &MyOptionAction::currentTextChanged, this, nullptr);

    auto publicOptions = publicOptionAction->getOptions();

    for (auto option : getOptions())
        publicOptions.removeAll(option);

    publicOptions.removeDuplicates();
    publicOptions.sort();

    publicOptionAction->setOptions(publicOptions);

    WidgetAction::disconnectFromPublicAction(recursive);
}

void MyOptionAction::fromVariantMap(const QVariantMap& variantMap)
{
    WidgetAction::fromVariantMap(variantMap);

    variantMapMustContain(variantMap, "Value");
    variantMapMustContain(variantMap, "IsPublic");

    if (variantMap["IsPublic"].toBool())
        setOptions(variantMap["Options"].toStringList());

    setCurrentText(variantMap["Value"].toString());
}

QVariantMap MyOptionAction::toVariantMap() const
{
    auto variantMap = WidgetAction::toVariantMap();

    variantMap.insert({
        { "Value", getCurrentText() },
        { "Options", getOptions() }
    });

    return variantMap;
}

void MyOptionAction::updateCurrentIndex()
{
    if (getModel()->rowCount() < 1) {
        _currentIndex = -1;
    } else {
        if (_currentIndex >= getModel()->rowCount())
            _currentIndex = getModel()->rowCount() - 1;
    }

    saveToSettings();
}

void MyOptionAction::setCustomModel(QAbstractItemModel* itemModel)
{
    if (itemModel == _customModel)
        return;

    _customModel = itemModel;

    emit customModelChanged(_customModel);
    emit modelChanged();

    if (_customModel)
        connect(_customModel, &QAbstractItemModel::layoutChanged, this, &MyOptionAction::updateCurrentIndex);

    updateCurrentIndex();

    emit currentIndexChanged(_currentIndex);
    emit currentTextChanged(getCurrentText());
}

bool MyOptionAction::hasCustomModel() const
{
    return _customModel != nullptr;
}

std::int32_t MyOptionAction::getCurrentIndex() const
{
    return _currentIndex;
}

void MyOptionAction::setCurrentIndex(const std::int32_t& currentIndex)
{
    if (currentIndex == _currentIndex)
        return;

    _currentIndex = currentIndex;

    updateCurrentIndex();

    emit currentIndexChanged(_currentIndex);
    emit currentTextChanged(getCurrentText());
}

QString MyOptionAction::getPlaceholderString() const
{
    return _placeholderString;
}

void MyOptionAction::setPlaceHolderString(const QString& placeholderString)
{
    if (placeholderString == _placeholderString)
        return;

    _placeholderString = placeholderString;

    emit placeholderStringChanged(_placeholderString);
}

QString MyOptionAction::getCurrentText() const
{
    if (_currentIndex < 0)
        return "";

    auto options = getOptions();

    if (_currentIndex >= options.count())
        return "";

    return getOptions()[_currentIndex];
}

void MyOptionAction::setCurrentText(const QString& currentText)
{
    // FIXME: allow multiple genes by checking for commas, should have better way
    if (currentText.contains(",")) {
        //qDebug() << "OptionAction::setCurrentText - Multi-gene input detected, skipping enforcement:" << currentText;
        return; // Do NOT enforce a single option if multiple genes are entered
    }

    if (currentText == getCurrentText())
        return;

    _currentIndex = hasOption(currentText) ? getModel()->match(getModel()->index(0, 0), Qt::DisplayRole, currentText).first().row() : -1;

    emit currentTextChanged(getCurrentText());
    emit currentIndexChanged(_currentIndex);
}

bool MyOptionAction::hasSelection() const
{
    return _currentIndex >= 0;
}

MyOptionAction::ComboBoxWidget::ComboBoxWidget(QWidget* parent, MyOptionAction* optionAction) :
    QComboBox(parent),
    _optionAction(optionAction)
{
    setObjectName("ComboBox");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    setMinimumWidth(200);

    const auto updateToolTip = [this, optionAction]() -> void {
        setToolTip(optionAction->hasOptions() ? QString("%1: %2").arg(optionAction->toolTip(), optionAction->getCurrentText()) : optionAction->toolTip());
    };

    const auto updateReadOnlyAndSelection = [this, optionAction]() -> void {
        if (optionAction->getCurrentIndex() < 0)
            setEnabled(optionAction->getNumberOfOptions() >= 1);
        else
            setEnabled(optionAction->getNumberOfOptions() >= 2);

        setCurrentText(optionAction->getCurrentText());
    };

    const auto updateComboBoxModel = [this, optionAction, updateToolTip, updateReadOnlyAndSelection]() -> void {
        QSignalBlocker comboBoxSignalBlocker(this);

        if (model()) {
            disconnect(model(), &QAbstractItemModel::layoutChanged, this, nullptr);
            disconnect(model(), &QAbstractItemModel::rowsInserted, this, nullptr);
            disconnect(model(), &QAbstractItemModel::rowsRemoved, this, nullptr);
        }

        setModel(const_cast<QAbstractItemModel*>(optionAction->getModel()));

        connect(model(), &QAbstractItemModel::layoutChanged, this, updateReadOnlyAndSelection);
        connect(model(), &QAbstractItemModel::rowsInserted, this, updateReadOnlyAndSelection);
        connect(model(), &QAbstractItemModel::rowsRemoved, this, updateReadOnlyAndSelection);

        updateToolTip();

        updateReadOnlyAndSelection();
    };

    connect(optionAction, &MyOptionAction::modelChanged, this, updateComboBoxModel);

    const auto updateComboBoxSelection = [this, optionAction]() -> void {
        if (optionAction->getCurrentText() == currentText())
            return;

        QSignalBlocker comboBoxSignalBlocker(this);

        setCurrentText(optionAction->getCurrentText());
    };

    connect(optionAction, &MyOptionAction::currentTextChanged, this, [this, updateComboBoxSelection, updateToolTip](const QString& currentText) {
        QSignalBlocker comboBoxSignalBlocker(this);

        updateComboBoxSelection();
        updateToolTip();

        update();
    });

    connect(optionAction, &MyOptionAction::placeholderStringChanged, this, [this]() -> void {
        update();
    });

    connect(this, qOverload<int>(&QComboBox::activated), this, [this, optionAction](const int& currentIndex) {
        optionAction->setCurrentIndex(currentIndex);
    });

    updateReadOnlyAndSelection();
    updateComboBoxModel();
    updateComboBoxSelection();
    updateToolTip();
}

void MyOptionAction::ComboBoxWidget::paintEvent(QPaintEvent* paintEvent)
{
    auto painter = QSharedPointer<QStylePainter>::create(this);

    painter->setPen(palette().color(QPalette::Text));

    auto styleOptionComboBox = QStyleOptionComboBox();

    initStyleOption(&styleOptionComboBox);

    //if (_optionAction->getCurrentIndex() == -1)
    //    styleOptionComboBox.currentIcon = Application::getIconFont("FontAwesome").getIcon("list");

    painter->drawComplexControl(QStyle::CC_ComboBox, styleOptionComboBox);

    if (_optionAction->getCurrentIndex() < 0) {
        styleOptionComboBox.palette.setBrush(QPalette::ButtonText, styleOptionComboBox.palette.brush(QPalette::ButtonText).color().lighter());

        if (!_optionAction->getPlaceholderString().isEmpty())
            styleOptionComboBox.currentText = _optionAction->getPlaceholderString();
    }

    painter->drawControl(QStyle::CE_ComboBoxLabel, styleOptionComboBox);
}

MyOptionAction::LineEditWidget::LineEditWidget(QWidget* parent, MyOptionAction* optionAction) :
    QLineEdit(parent),
    _optionAction(optionAction),
    _completer()
{
    setObjectName("LineEdit");
    setCompleter(&_completer);

    _completer.setCaseSensitivity(Qt::CaseInsensitive);
    _completer.setFilterMode(Qt::MatchContains);
    _completer.setCompletionColumn(0);
    _completer.setCompletionMode(QCompleter::PopupCompletion);

    const auto updateCompleterModel = [this, optionAction]() {
        _completer.setModel(const_cast<QAbstractItemModel*>(optionAction->getModel()));
    };

    connect(optionAction, &MyOptionAction::modelChanged, this, updateCompleterModel);

    const auto updateText = [this, optionAction]() -> void {
        QSignalBlocker lineEditBlocker(this);

        setText(optionAction->getCurrentText());
    };

    connect(optionAction, &MyOptionAction::currentTextChanged, this, updateText);

    connect(this, &QLineEdit::editingFinished, this, [this, optionAction]() {
        optionAction->setCurrentText(text());
    });

    connect(&_completer, QOverload<const QString&>::of(&QCompleter::activated), [this, optionAction](const QString& text) {
        optionAction->setCurrentText(text);
    });

    connect(&_completer, QOverload<const QString&>::of(&QCompleter::highlighted), [this, optionAction](const QString& text) {
        optionAction->setCurrentText(text);
    });

    updateCompleterModel();
    updateText();
}

MyOptionAction::ButtonsWidget::ButtonsWidget(QWidget* parent, MyOptionAction* optionAction, const Qt::Orientation& orientation) :
    QWidget(parent)
{
    setObjectName("Buttons");

    QBoxLayout* layout = nullptr;

    switch (orientation)
    {
        case Qt::Horizontal:
            layout = new QHBoxLayout();
            break;

        case Qt::Vertical:
            layout = new QVBoxLayout();
            break;

        default:
            break;
    }

    layout->setContentsMargins(0, 0, 0, 0);

    setLayout(layout);

    const auto updateLayout = [this, layout, optionAction]() -> void {
        QLayoutItem* layoutItem;

        while ((layoutItem = layout->takeAt(0)) != nullptr) {
            delete layoutItem->widget();
            delete layoutItem;
        }

        std::int32_t optionIndex = 0;

        QVector<QPushButton*> pushButtons;

        for (const auto& option : optionAction->getOptions()) {
            auto pushButton = new QPushButton(option);

            pushButton->setCheckable(true);
            pushButton->setToolTip(optionAction->text() + ": " + option);

            layout->addWidget(pushButton);

            connect(pushButton, &QPushButton::clicked, this, [optionAction, optionIndex]() {
                optionAction->setCurrentIndex(optionIndex);
            });

            optionIndex++;

            pushButtons << pushButton;
        }

        disconnect(optionAction, &MyOptionAction::currentIndexChanged, this, nullptr);

        const auto updatePushButtonCheckState = [pushButtons, optionAction]() {
            for (auto pushButton : pushButtons)
                pushButton->setChecked(pushButtons.indexOf(pushButton) == optionAction->getCurrentIndex());
        };

        updatePushButtonCheckState();

        connect(optionAction, &MyOptionAction::currentIndexChanged, this, updatePushButtonCheckState);
    };
    
    updateLayout();

    connect(optionAction, &MyOptionAction::modelChanged, this, updateLayout);
}

QWidget* MyOptionAction::getWidget(QWidget* parent, const std::int32_t& widgetFlags)
{
    auto widget = new WidgetActionWidget(parent, this, widgetFlags);
    auto layout = new QHBoxLayout();

    if (!widget->isPopup())
        layout->setContentsMargins(0, 0, 0, 0);

    if (widgetFlags & WidgetFlag::ComboBox)
        layout->addWidget(new MyOptionAction::ComboBoxWidget(parent, this));

    if (widgetFlags & WidgetFlag::LineEdit)
        layout->addWidget(new MyOptionAction::LineEditWidget(parent, this));

    if (widgetFlags & WidgetFlag::HorizontalButtons)
        layout->addWidget(new ButtonsWidget(parent, this, Qt::Horizontal));

    if (widgetFlags & WidgetFlag::VerticalButtons)
        layout->addWidget(new ButtonsWidget(parent, this, Qt::Vertical));

    if (widgetFlags & WidgetFlag::Clearable) {
        auto clearSelectionAction = new TriggerAction(parent, "Clear");

        clearSelectionAction->setIconByName("times");
        clearSelectionAction->setToolTip("Clear the current selection");

        connect(clearSelectionAction, &TriggerAction::triggered, this, [this]() -> void {
            setCurrentIndex(-1);
        });

        const auto updateReadOnly = [this, clearSelectionAction]() -> void {
            clearSelectionAction->setEnabled(getCurrentIndex() >= 0);
        };

        updateReadOnly();

        connect(this, &MyOptionAction::currentIndexChanged, this, updateReadOnly);

        layout->addWidget(clearSelectionAction->createWidget(widget, TriggerAction::Icon));
    }

    widget->setLayout(layout);

    return widget;
}


