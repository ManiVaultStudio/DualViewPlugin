#include "ColoringActionA.h"
#include "src/DualViewPlugin.h"
#include "src/ScatterplotWidget.h"
#include "DataHierarchyItem.h"

#include <PointData/PointData.h>
#include <ClusterData/ClusterData.h>

using namespace mv::gui;

//const QColor ColoringAction::DEFAULT_CONSTANT_COLOR = qRgb(93, 93, 225);

ColoringActionA::ColoringActionA(QObject* parent, const QString& title) :
    VerticalGroupAction(parent, title),
    _dualViewPlugin(dynamic_cast<DualViewPlugin*>(parent->parent()))
    //_colorByAction(this, "Color by", { "meta data", "selected genes" })

    //_colorByModel(this),  
    //_constantColorAction(this, "Constant color", DEFAULT_CONSTANT_COLOR),
    //_dimensionAction(this, "Dimension"),
    //_colorMap1DAction(this, "1D Color map"),
    //_colorMap2DAction(this, "2D Color map")
{
    setIcon(mv::Application::getIconFont("FontAwesome").getIcon("palette"));
    setLabelSizingType(LabelSizingType::Auto);
    //setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    //setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceExpandedInGroup);

    //addAction(&_colorByAction, OptionAction::HorizontalButtons);
    //addAction(&_constantColorAction);
    //addAction(&_colorMap2DAction);
    //addAction(&_colorMap1DAction);
    //addAction(&_dimensionAction);

    //_colorByAction.setToolTip("Color by");

    //_dualViewPlugin->getWidget().addAction(&_colorByAction);
    //_dualViewPlugin->getWidget().addAction(&_dimensionAction);


    connect(&_dualViewPlugin->getEmbeddingDatasetA(), &Dataset<Points>::changed, this, [this]() {
        const auto positionDataset = _dualViewPlugin->getEmbeddingDatasetA();

        if (!positionDataset.isValid())
            return;

        std::vector<Vector3f> colors(positionDataset->getNumPoints(), Vector3f(93/255.f, 93/255.f, 225/255.f)); // back to default color
        _dualViewPlugin->getEmbeddingWidgetA().setColors(colors);

        // Optionally, reset the current color dataset if it should no longer be valid
        _currentColorDataset = Dataset<DatasetImpl>();
    });

    
    /*connect(&_colorByAction, &OptionAction::currentIndexChanged, this, [this] {
        qDebug() << "_colorByAction.currentIndexChanged to " << _colorByAction.getCurrentText() << _colorByAction.getCurrentIndex();
   
     });*/

    //connect(&_dualViewPlugin->getPositionSourceDataset(), &Dataset<Points>::changed, this, [this]() {
    //    const auto positionSourceDataset = _dualViewPlugin->getPositionSourceDataset();

    //    if (positionSourceDataset.isValid())
    //        addColorDataset(positionSourceDataset);

    //    updateColorByActionOptions();
    //});

    //connect(&_colorByAction, &OptionAction::currentIndexChanged, this, [this](const std::int32_t& currentIndex) {
    //    _dualViewPlugin->getScatterplotWidget().setColoringMode(currentIndex == 0 ? ScatterplotWidget::ColoringMode::Constant : ScatterplotWidget::ColoringMode::Data);

    //    _constantColorAction.setEnabled(currentIndex == 0);

    //    const auto currentColorDataset = getCurrentColorDataset();

    //    if (currentColorDataset.isValid()) {
    //        const auto currentColorDatasetTypeIsPointType = currentColorDataset->getDataType() == PointType;

    //        _dimensionAction.setPointsDataset(currentColorDatasetTypeIsPointType ? Dataset<Points>(currentColorDataset) : Dataset<Points>());
    //        //_dimensionAction.setVisible(currentColorDatasetTypeIsPointType);

    //        emit currentColorDatasetChanged(currentColorDataset);
    //    }
    //    else {
    //        _dimensionAction.setPointsDataset(Dataset<Points>());
    //        //_dimensionAction.setVisible(false);
    //    }

    //    updateScatterPlotWidgetColors();
    //});
    

    /*const auto updateReadOnly = [this]() {
        setEnabled(_dualViewPlugin->getPositionDataset().isValid() && _scatterplotPlugin->getScatterplotWidget().getRenderMode() == ScatterplotWidget::SCATTERPLOT);
    };

    updateReadOnly();*/


}

//QMenu* ColoringAction::getContextMenu(QWidget* parent /*= nullptr*/)
//{
//    auto menu = new QMenu("Color", parent);
//
//    const auto addActionToMenu = [menu](QAction* action) -> void {
//        auto actionMenu = new QMenu(action->text());
//
//        actionMenu->addAction(action);
//
//        menu->addMenu(actionMenu);
//    };
//
//    return menu;
//}


Dataset<DatasetImpl> ColoringActionA::getCurrentColorDataset() const
{
    return _currentColorDataset;
}

void ColoringActionA::setCurrentColorDataset(const Dataset<DatasetImpl>& colorDataset)
{
    _currentColorDataset = colorDataset;  // Store the new dataset

    qDebug() << "ColoringActionA: setCurrentColorDataset() " << colorDataset->getGuiName();
    
    updateScatterPlotWidgetColors();
}

//void ColoringAction::updateColorByActionOptions()
//{
//    auto positionDataset = _dualViewPlugin->getPositionDataset();
//
//    if (!positionDataset.isValid())
//        return;
//
//    const auto children = positionDataset->getDataHierarchyItem().getChildren();
//
//    for (auto child : children) {
//        const auto childDataset = child->getDataset();
//        const auto dataType     = childDataset->getDataType();
//
//        if (dataType == PointType || dataType == ClusterType)
//            addColorDataset(childDataset);
//    }
//}

void ColoringActionA::updateScatterPlotWidgetColors()
{
    qDebug() << "ColoringActionA: updateScatterPlotWidgetColors()";

    if (!_currentColorDataset.isValid()) {
        qDebug() << "ColoringActionA: no valid color dataset";
        return;  
    }

    // Mapping from local to global indices
    std::vector<std::uint32_t> globalIndices;

    auto positionDataset = _dualViewPlugin->getEmbeddingDatasetA();

    // Get global indices from the position dataset
    int totalNumPoints = 0;
    if (positionDataset->isDerivedData())
        totalNumPoints = positionDataset->getSourceDataset<Points>()->getFullDataset<Points>()->getNumPoints();
    else
        totalNumPoints = positionDataset->getFullDataset<Points>()->getNumPoints();

    positionDataset->getGlobalIndices(globalIndices);

    // Generate color buffer for global and local colors
    std::vector<Vector3f> globalColors(totalNumPoints);
    std::vector<Vector3f> localColors(positionDataset->getNumPoints());

    // Loop over all clusters and populate global colors
    for (const auto& cluster : _currentColorDataset.get<Clusters>()->getClusters())
        for (const auto& index : cluster.getIndices())
            globalColors[index] = Vector3f(cluster.getColor().redF(), cluster.getColor().greenF(), cluster.getColor().blueF());
    std::int32_t localColorIndex = 0;

    // Loop over all global indices and find the corresponding local color
    for (const auto& globalIndex : globalIndices)
        localColors[localColorIndex++] = globalColors[globalIndex];

    // Apply colors to scatter plot widget without modification
    _dualViewPlugin->getEmbeddingWidgetA().setColors(localColors);

    qDebug() << "ColoringAction: set colors A";
}

//void ColoringAction::updateColorMapActionScalarRange()
//{
//    const auto colorMapRange    = _dualViewPlugin->getScatterplotWidget().getColorMapRange();
//    const auto colorMapRangeMin = colorMapRange.x;
//    const auto colorMapRangeMax = colorMapRange.y;
//
//    auto& colorMapRangeAction = _colorMap1DAction.getRangeAction(ColorMapAction::Axis::X);
//
//    colorMapRangeAction.initialize({ colorMapRangeMin, colorMapRangeMax }, { colorMapRangeMin, colorMapRangeMax });
//	
//	_colorMap1DAction.getDataRangeAction(ColorMapAction::Axis::X).setRange({ colorMapRangeMin, colorMapRangeMax });
//}

//void ColoringAction::updateScatterplotWidgetColorMap()
//{
//    auto& scatterplotWidget = _dualViewPlugin->getScatterplotWidget();
//
//    switch (scatterplotWidget.getRenderMode())
//    {
//        case ScatterplotWidget::SCATTERPLOT:
//        {
//            if (_colorByAction.getCurrentIndex() == 0) {
//                QPixmap colorPixmap(1, 1);
//
//                colorPixmap.fill(_constantColorAction.getColor());
//
//                scatterplotWidget.setColorMap(colorPixmap.toImage());
//                scatterplotWidget.setScalarEffect(PointEffect::Color);
//                scatterplotWidget.setColoringMode(ScatterplotWidget::ColoringMode::Constant);
//            }
//            else if (_colorByAction.getCurrentIndex() == 1) {
//                scatterplotWidget.setColorMap(_colorMap2DAction.getColorMapImage());
//                scatterplotWidget.setScalarEffect(PointEffect::Color2D);
//                scatterplotWidget.setColoringMode(ScatterplotWidget::ColoringMode::Scatter);
//            }
//            else {
//                scatterplotWidget.setColorMap(_colorMap1DAction.getColorMapImage().mirrored(false, true));
//            }
//
//            break;
//        }
//
//        case ScatterplotWidget::DENSITY:
//            break;
//
//        case ScatterplotWidget::LANDSCAPE:
//        {
//            scatterplotWidget.setScalarEffect(PointEffect::Color);
//            scatterplotWidget.setColoringMode(ScatterplotWidget::ColoringMode::Scatter);
//            scatterplotWidget.setColorMap(_colorMap1DAction.getColorMapImage());
//            
//            break;
//        }
//
//        default:
//            break;
//    }
//
//    updateScatterPlotWidgetColorMapRange();
//}

//void ColoringAction::updateScatterPlotWidgetColorMapRange()
//{
//    const auto& rangeAction = _colorMap1DAction.getRangeAction(ColorMapAction::Axis::X);
//
//    _dualViewPlugin->getScatterplotWidget().setColorMapRange(rangeAction.getMinimum(), rangeAction.getMaximum());
//}

//bool ColoringAction::shouldEnableColorMap() const
//{
//    if (!_dualViewPlugin->getPositionDataset().isValid())
//        return false;
//
//    const auto currentColorDataset = getCurrentColorDataset();
//
//    if (currentColorDataset.isValid() && currentColorDataset->getDataType() == ClusterType)
//        return false;
//
//    if (_scatterplotPlugin->getScatterplotWidget().getRenderMode() == ScatterplotWidget::LANDSCAPE)
//        return true;
//
//    if (_scatterplotPlugin->getScatterplotWidget().getRenderMode() == ScatterplotWidget::SCATTERPLOT && _colorByAction.getCurrentIndex() > 0)
//        return true;
//
//    return false;
//}

//void ColoringAction::updateColorMapActionsReadOnly()
//{
//    const auto currentIndex = _colorByAction.getCurrentIndex();
//
//    _colorMap1DAction.setEnabled(shouldEnableColorMap() && (currentIndex >= 2));
//    _colorMap2DAction.setEnabled(shouldEnableColorMap() && (currentIndex == 1));
//}

//void ColoringAction::connectToPublicAction(WidgetAction* publicAction, bool recursive)
//{
//    auto publicColoringAction = dynamic_cast<ColoringAction*>(publicAction);
//
//    Q_ASSERT(publicColoringAction != nullptr);
//
//    if (publicColoringAction == nullptr)
//        return;
//
//    if (recursive) {
//        actions().connectPrivateActionToPublicAction(&_colorByAction, &publicColoringAction->getColorByAction(), recursive);
//        actions().connectPrivateActionToPublicAction(&_constantColorAction, &publicColoringAction->getConstantColorAction(), recursive);
//        actions().connectPrivateActionToPublicAction(&_dimensionAction, &publicColoringAction->getDimensionAction(), recursive);
//        actions().connectPrivateActionToPublicAction(&_colorMap1DAction, &publicColoringAction->getColorMap1DAction(), recursive);
//        actions().connectPrivateActionToPublicAction(&_colorMap2DAction, &publicColoringAction->getColorMap2DAction(), recursive);
//    }
//
//    GroupAction::connectToPublicAction(publicAction, recursive);
//}

//void ColoringAction::disconnectFromPublicAction(bool recursive)
//{
//    if (!isConnected())
//        return;
//
//    if (recursive) {
//        actions().disconnectPrivateActionFromPublicAction(&_colorByAction, recursive);
//        actions().disconnectPrivateActionFromPublicAction(&_constantColorAction, recursive);
//        actions().disconnectPrivateActionFromPublicAction(&_dimensionAction, recursive);
//        actions().disconnectPrivateActionFromPublicAction(&_colorMap2DAction, recursive);
//    }
//
//    GroupAction::disconnectFromPublicAction(recursive);
//}

void ColoringActionA::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);

   // _colorByAction.fromParentVariantMap(variantMap);
    /*_constantColorAction.fromParentVariantMap(variantMap);
    _dimensionAction.fromParentVariantMap(variantMap);
    _colorMap1DAction.fromParentVariantMap(variantMap);
    _colorMap2DAction.fromParentVariantMap(variantMap);*/
}

QVariantMap ColoringActionA::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();

    //_colorByAction.insertIntoVariantMap(variantMap);
    /*_constantColorAction.insertIntoVariantMap(variantMap);
    _dimensionAction.insertIntoVariantMap(variantMap);
    _colorMap1DAction.insertIntoVariantMap(variantMap);
    _colorMap2DAction.insertIntoVariantMap(variantMap);*/

    return variantMap;
}
