#include "PointPlotActionB.h"
#include "ScalarSourceAction.h"
#include "src/DualViewPlugin.h"
#include "src/ScatterplotWidget.h"

#include <DataHierarchyItem.h>

using namespace gui;

PointPlotActionB::PointPlotActionB(QObject* parent, const QString& title) :
    VerticalGroupAction(parent, title),
    _dualViewPlugin(nullptr),
    _sizeAction(this, "Point size", 0.0, 100.0, DEFAULT_POINT_SIZE),
    _opacityAction(this, "Point opacity", 0.0, 100.0, DEFAULT_POINT_OPACITY),
    _pointSizeScalars(),
    _pointOpacityScalars(),
    _focusSelection(this, "Focus selection"),
    _lastOpacitySourceIndex(-1)
{
    setToolTip("Point plot settings");
    setConfigurationFlag(WidgetAction::ConfigurationFlag::NoLabelInGroup);
    setLabelSizingType(LabelSizingType::Auto);

    addAction(&_sizeAction);
    addAction(&_opacityAction);

    _sizeAction.setConnectionPermissionsToNone();
    _opacityAction.setConnectionPermissionsToNone();

    _sizeAction.getMagnitudeAction().setSuffix("px");
    _opacityAction.getMagnitudeAction().setSuffix("%");

    _opacityAction.getSourceAction().getOffsetAction().setSuffix("%");

    _focusSelection.setToolTip("Put focus on selected points by modulating the point opacity");
    _focusSelection.setDefaultWidgetFlags(ToggleAction::CheckBox);

    connect(&_sizeAction, &ScalarAction::sourceSelectionChanged, this, [this](const std::uint32_t& sourceSelectionIndex) {
        switch (sourceSelectionIndex)
        {
            case ScalarSourceModel::DefaultRow::Constant:
            {
                _sizeAction.getSourceAction().getOffsetAction().setValue(0.0f);

                break;
            }

            case ScalarSourceModel::DefaultRow::Selection:
            {
                _sizeAction.getSourceAction().getOffsetAction().setValue(_sizeAction.getMagnitudeAction().getValue());

                break;
            }

            case ScalarSourceModel::DefaultRow::DatasetStart:
                break;

            default:
                break;
        }
    });

    connect(&_opacityAction, &ScalarAction::sourceSelectionChanged, this, [this](const std::uint32_t& sourceSelectionIndex) {
        switch (sourceSelectionIndex)
        {
            case ScalarSourceModel::DefaultRow::Constant:
            {
                _opacityAction.getMagnitudeAction().setValue(50.0f);
                _opacityAction.getSourceAction().getOffsetAction().setValue(0.0f);
                _focusSelection.setChecked(false);

                break;
            }

            case ScalarSourceModel::DefaultRow::Selection:
            {
                _opacityAction.getMagnitudeAction().setValue(10.0f);
                _opacityAction.getSourceAction().getOffsetAction().setValue(90.0f);
                _focusSelection.setChecked(true);

                break;
            }

            case ScalarSourceModel::DefaultRow::DatasetStart:
            {
                _focusSelection.setChecked(false);

                break;
            }

            default:
                break;
        }
    });

    connect(&_focusSelection, &ToggleAction::toggled, this, [this](const bool& toggled) {
        const auto opacitySourceIndex  = _opacityAction.getSourceAction().getPickerAction().getCurrentIndex();

        if (toggled) {

            if (opacitySourceIndex != ScalarSourceModel::DefaultRow::Selection)
                _opacityAction.setCurrentSourceIndex(ScalarSourceModel::DefaultRow::Selection);

            _lastOpacitySourceIndex = opacitySourceIndex;
        }
        else {
            if (_lastOpacitySourceIndex != ScalarSourceModel::DefaultRow::Selection)
                _opacityAction.setCurrentSourceIndex(_lastOpacitySourceIndex);
        }
    });
}

void PointPlotActionB::initialize(DualViewPlugin* dualViewPlugin)
{
    Q_ASSERT(dualViewPlugin != nullptr);

    if (dualViewPlugin == nullptr)
        return;

    _dualViewPlugin = dualViewPlugin;

    connect(&_dualViewPlugin->getEmbeddingDatasetB(), &Dataset<Points>::changed, this, [this]() {

        const auto positionDataset = _dualViewPlugin->getEmbeddingDatasetB();

        if (!positionDataset.isValid())
            return;

        _sizeAction.removeAllDatasets();
        _opacityAction.removeAllDatasets();

        _sizeAction.addDataset(positionDataset);
        _opacityAction.addDataset(positionDataset);


        // FIX ME: temp disabled
        /*const auto positionSourceDataset = _scatterplotPlugin->getPositionSourceDataset();

        if (positionSourceDataset.isValid()) {
            _sizeAction.addDataset(positionSourceDataset);
            _opacityAction.addDataset(positionSourceDataset);
        }*/

        updateDefaultDatasets();

        updateScatterPlotWidgetPointSizeScalars();
        updateScatterPlotWidgetPointOpacityScalars();

        _sizeAction.getSourceAction().getPickerAction().setCurrentIndex(0);
        _opacityAction.getSourceAction().getPickerAction().setCurrentIndex(0);
    });

    //connect(&_scatterplotPlugin->getPositionDataset(), &Dataset<Points>::childAdded, this, &PointPlotAction::updateDefaultDatasets);
    //connect(&_scatterplotPlugin->getPositionDataset(), &Dataset<Points>::childRemoved, this, &PointPlotAction::updateDefaultDatasets);
    connect(&_dualViewPlugin->getEmbeddingDatasetB(), &Dataset<Points>::dataSelectionChanged, this, &PointPlotActionB::updateScatterPlotWidgetPointSizeScalars);
    connect(&_dualViewPlugin->getEmbeddingDatasetB(), &Dataset<Points>::dataSelectionChanged, this, &PointPlotActionB::updateScatterPlotWidgetPointOpacityScalars);

    connect(&_sizeAction, &ScalarAction::magnitudeChanged, this, &PointPlotActionB::updateScatterPlotWidgetPointSizeScalars);
    connect(&_sizeAction, &ScalarAction::offsetChanged, this, &PointPlotActionB::updateScatterPlotWidgetPointSizeScalars);
    connect(&_sizeAction, &ScalarAction::sourceSelectionChanged, this, &PointPlotActionB::updateScatterPlotWidgetPointSizeScalars);
    connect(&_sizeAction, &ScalarAction::sourceDataChanged, this, &PointPlotActionB::updateScatterPlotWidgetPointSizeScalars);
    connect(&_sizeAction, &ScalarAction::scalarRangeChanged, this, &PointPlotActionB::updateScatterPlotWidgetPointSizeScalars);

    connect(&_opacityAction, &ScalarAction::magnitudeChanged, this, &PointPlotActionB::updateScatterPlotWidgetPointOpacityScalars);
    connect(&_opacityAction, &ScalarAction::offsetChanged, this, &PointPlotActionB::updateScatterPlotWidgetPointOpacityScalars);
    connect(&_opacityAction, &ScalarAction::sourceSelectionChanged, this, &PointPlotActionB::updateScatterPlotWidgetPointOpacityScalars);
    connect(&_opacityAction, &ScalarAction::sourceDataChanged, this, &PointPlotActionB::updateScatterPlotWidgetPointOpacityScalars);
    connect(&_opacityAction, &ScalarAction::scalarRangeChanged, this, &PointPlotActionB::updateScatterPlotWidgetPointOpacityScalars);
}

QMenu* PointPlotActionB::getContextMenu()
{
    if (_dualViewPlugin == nullptr)
        return nullptr;

    auto menu = new QMenu("Plot settings");

    const auto addActionToMenu = [menu](QAction* action) {
        auto actionMenu = new QMenu(action->text());

        actionMenu->addAction(action);

        menu->addMenu(actionMenu);
    };

    addActionToMenu(&_sizeAction);
    addActionToMenu(&_opacityAction);

    return menu;
}

void PointPlotActionB::setVisible(bool visible)
{
    _sizeAction.setVisible(visible);
    _opacityAction.setVisible(visible);
    _focusSelection.setVisible(visible);
}

void PointPlotActionB::addPointSizeDataset(const Dataset<DatasetImpl>& pointSizeDataset)
{
    if (!pointSizeDataset.isValid())
        return;

    _sizeAction.addDataset(pointSizeDataset);
}

void PointPlotActionB::addPointOpacityDataset(const Dataset<DatasetImpl>& pointOpacityDataset)
{
    if (!pointOpacityDataset.isValid())
        return;

    _opacityAction.addDataset(pointOpacityDataset);
}

void PointPlotActionB::updateDefaultDatasets()
{
    if (_dualViewPlugin == nullptr)
        return;

    auto positionDataset = Dataset<Points>(_dualViewPlugin->getEmbeddingDatasetB());

    if (!positionDataset.isValid())
        return;

    const auto children = positionDataset->getDataHierarchyItem().getChildren();

    for (auto child : children) {
        const auto childDataset = child->getDataset();
        const auto dataType     = childDataset->getDataType();

        if (dataType != PointType)
            continue;

        auto points = Dataset<Points>(childDataset);

        _sizeAction.addDataset(points);
        _opacityAction.addDataset(points);
    }
}

void PointPlotActionB::updateScatterPlotWidgetPointSizeScalars()
{
    if (_dualViewPlugin == nullptr)
        return;

    if (!_dualViewPlugin->getEmbeddingDatasetB().isValid())
        return;

    const auto numberOfPoints = _dualViewPlugin->getEmbeddingDatasetB()->getNumPoints();

    if (numberOfPoints != _pointSizeScalars.size())
        _pointSizeScalars.resize(numberOfPoints);

    std::fill(_pointSizeScalars.begin(), _pointSizeScalars.end(), _sizeAction.getMagnitudeAction().getValue());

    if (_sizeAction.isSourceSelection()) {
        auto positionDataset = _dualViewPlugin->getEmbeddingDatasetB();

        std::fill(_pointSizeScalars.begin(), _pointSizeScalars.end(), _sizeAction.getMagnitudeAction().getValue());

        const auto pointSizeSelectedPoints = _sizeAction.getMagnitudeAction().getValue() + _sizeAction.getSourceAction().getOffsetAction().getValue();

        std::vector<bool> selected;

        positionDataset->selectedLocalIndices(positionDataset->getSelection<Points>()->indices, selected);

        for (size_t i = 0; i < selected.size(); i++) {
            if (!selected[i])
                continue;

            _pointSizeScalars[i] = pointSizeSelectedPoints;
        }
    }

    if (_sizeAction.isSourceDataset()) {

        auto pointSizeSourceDataset = Dataset<Points>(_sizeAction.getCurrentDataset());

        if (pointSizeSourceDataset.isValid() && pointSizeSourceDataset->getNumPoints() == _dualViewPlugin->getEmbeddingDatasetA()->getNumPoints())
        {
            pointSizeSourceDataset->visitData([this, pointSizeSourceDataset, numberOfPoints](auto pointData) {
                const auto currentDimensionIndex    = _sizeAction.getSourceAction().getDimensionPickerAction().getCurrentDimensionIndex();
                const auto rangeMin                 = _sizeAction.getSourceAction().getRangeAction().getMinimum();
                const auto rangeMax                 = _sizeAction.getSourceAction().getRangeAction().getMaximum();
                const auto rangeLength              = rangeMax - rangeMin;

                if (rangeLength > 0) {
                    for (std::uint32_t pointIndex = 0; pointIndex < numberOfPoints; pointIndex++) {
                        auto pointValue = static_cast<float>(pointData[pointIndex][currentDimensionIndex]);

                        const auto pointValueClamped    = std::max(rangeMin, std::min(rangeMax, pointValue));
                        const auto pointValueNormalized = (pointValueClamped - rangeMin) / rangeLength;

                        _pointSizeScalars[pointIndex] = _sizeAction.getSourceAction().getOffsetAction().getValue() + (pointValueNormalized * _sizeAction.getMagnitudeAction().getValue());
                    }
                }
                else {
                    std::fill(_pointSizeScalars.begin(), _pointSizeScalars.end(), _sizeAction.getSourceAction().getOffsetAction().getValue() + (rangeMin * _sizeAction.getMagnitudeAction().getValue()));
                }
            });
        }
    }

    _dualViewPlugin->getEmbeddingWidgetB().setPointSizeScalars(_pointSizeScalars);
}

void PointPlotActionB::updateScatterPlotWidgetPointOpacityScalars()
{
    if (_dualViewPlugin == nullptr)
        return;

    if (!_dualViewPlugin->getEmbeddingDatasetB().isValid())
        return;

    const auto numberOfPoints = _dualViewPlugin->getEmbeddingDatasetB()->getNumPoints();

    if (numberOfPoints != _pointOpacityScalars.size())
        _pointOpacityScalars.resize(numberOfPoints);

    const auto opacityMagnitude = 0.01f * _opacityAction.getMagnitudeAction().getValue();

    std::fill(_pointOpacityScalars.begin(), _pointOpacityScalars.end(), opacityMagnitude);

    if (_opacityAction.isSourceSelection()) {
        auto positionDataset    = _dualViewPlugin->getEmbeddingDatasetB();
        auto selectionSet       = positionDataset->getSelection<Points>();

        std::fill(_pointOpacityScalars.begin(), _pointOpacityScalars.end(), 0.01f * _opacityAction.getMagnitudeAction().getValue());

        const auto opacityOffset                = 0.01f * _opacityAction.getSourceAction().getOffsetAction().getValue();
        const auto pointOpacitySelectedPoints   = std::min(1.0f, opacityMagnitude + opacityOffset);

        std::vector<uint32_t> localSelectionIndices;
        positionDataset->getLocalSelectionIndices(localSelectionIndices);

        for (const auto& selectionIndex : localSelectionIndices)
            _pointOpacityScalars[selectionIndex] = pointOpacitySelectedPoints;
    }

    if (_opacityAction.isSourceDataset()) {
        auto pointOpacitySourceDataset = Dataset<Points>(_opacityAction.getCurrentDataset());

        if (pointOpacitySourceDataset.isValid() && pointOpacitySourceDataset->getNumPoints() == _dualViewPlugin->getEmbeddingDatasetA()->getNumPoints()) {
            pointOpacitySourceDataset->visitData([this, pointOpacitySourceDataset, numberOfPoints, opacityMagnitude](auto pointData) {
                const auto currentDimensionIndex    = _opacityAction.getSourceAction().getDimensionPickerAction().getCurrentDimensionIndex();
                const auto opacityOffset            = 0.01f * _opacityAction.getSourceAction().getOffsetAction().getValue();
                const auto rangeMin                 = _opacityAction.getSourceAction().getRangeAction().getMinimum();
                const auto rangeMax                 = _opacityAction.getSourceAction().getRangeAction().getMaximum();
                const auto rangeLength              = rangeMax - rangeMin;

                if (rangeLength > 0) {
                    for (std::uint32_t pointIndex = 0; pointIndex < numberOfPoints; pointIndex++) {
                        auto pointValue                 = static_cast<float>(pointData[pointIndex][currentDimensionIndex]);
                        const auto pointValueClamped    = std::max(rangeMin, std::min(rangeMax, pointValue));
                        const auto pointValueNormalized = (pointValueClamped - rangeMin) / rangeLength;

                        if (opacityOffset == 1.0f)
                            _pointOpacityScalars[pointIndex] = 1.0f;
                        else
                            _pointOpacityScalars[pointIndex] = opacityMagnitude * (opacityOffset + (pointValueNormalized / (1.0f - opacityOffset)));
                    }
                }
                else {
                    auto& rangeAction = _opacityAction.getSourceAction().getRangeAction();

                    if (rangeAction.getRangeMinAction().getValue() == rangeAction.getRangeMaxAction().getValue())
                        std::fill(_pointOpacityScalars.begin(), _pointOpacityScalars.end(), 0.0f);
                    else
                        std::fill(_pointOpacityScalars.begin(), _pointOpacityScalars.end(), 1.0f);
                }
            });
        }
    }

    _dualViewPlugin->getEmbeddingWidgetB().setPointOpacityScalars(_pointOpacityScalars);
}

void PointPlotActionB::connectToPublicAction(WidgetAction* publicAction, bool recursive)
{
    auto publicPointPlotAction = dynamic_cast<PointPlotActionB*>(publicAction);

    Q_ASSERT(publicPointPlotAction != nullptr);

    if (publicPointPlotAction == nullptr)
        return;

    if (recursive) {
        actions().connectPrivateActionToPublicAction(&_sizeAction, &publicPointPlotAction->getSizeAction(), recursive);
        actions().connectPrivateActionToPublicAction(&_opacityAction, &publicPointPlotAction->getOpacityAction(), recursive);
        actions().connectPrivateActionToPublicAction(&_focusSelection, &publicPointPlotAction->getFocusSelection(), recursive);
    }

    GroupAction::connectToPublicAction(publicAction, recursive);
}

void PointPlotActionB::disconnectFromPublicAction(bool recursive)
{
    if (!isConnected())
        return;

    if (recursive) {
        actions().disconnectPrivateActionFromPublicAction(&_sizeAction, recursive);
        actions().disconnectPrivateActionFromPublicAction(&_opacityAction, recursive);
        actions().disconnectPrivateActionFromPublicAction(&_focusSelection, recursive);
    }

    GroupAction::disconnectFromPublicAction(recursive);
}

void PointPlotActionB::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);

    _sizeAction.fromParentVariantMap(variantMap);
    _opacityAction.fromParentVariantMap(variantMap);
    _focusSelection.fromParentVariantMap(variantMap);
}

QVariantMap PointPlotActionB::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();

    _sizeAction.insertIntoVariantMap(variantMap);
    _opacityAction.insertIntoVariantMap(variantMap);
    _focusSelection.insertIntoVariantMap(variantMap);

    return variantMap;
}
