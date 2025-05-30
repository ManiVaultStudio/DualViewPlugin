#include "SettingsAction.h"

#include "src/DualViewPlugin.h"

#include <PointData/PointData.h>

#include <QMenu>

using namespace mv::gui;

SettingsAction::SettingsAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _dualViewPlugin(dynamic_cast<DualViewPlugin*>(parent)),
    _currentDatasetsAction(this, "Current datasets"),
    _embeddingAPointPlotAction(this, "Point Plot A"),
    _embeddingBPointPlotAction(this, "Point Plot B"),
    _thresholdLinesAction(this, "Threshold Lines", 0.f, 1.f, 0.f, 5),
    _2FCThreshold(this, "log2FC Threshold", 0.f, 5.f, 1.5f, 2),
    _coloringActionB(this, "Coloring B"),
    _coloringActionA(this, "Coloring A"),
    _selectionAction(this, "Selection"),
    _reversePointSizeBAction(this, "Reverse Point Size B"),
    _dimensionSelectionAction(this, "Gene search"),
    _enrichmentAction(this, "Enrich"),
    _enrichmentSettingsAction(this, "Enrichment settings")
{
    setConnectionPermissionsToForceNone();

    _currentDatasetsAction.initialize(_dualViewPlugin);

    _selectionAction.initialize(_dualViewPlugin);

    _thresholdLinesAction.setToolTip("Threshold lines");

    _2FCThreshold.setToolTip("log2FC Threshold");

    connect(&_thresholdLinesAction, &DecimalAction::valueChanged, [this](float val) {
		_dualViewPlugin->updateThresholdLines();
	});

    connect(&_reversePointSizeBAction, &ToggleAction::toggled, [this](bool val) {
		_dualViewPlugin->reversePointSizeB(val);
	});
    
    connect(&_2FCThreshold, &DecimalAction::valueChanged, [this](float val) {
        _dualViewPlugin->updateLog2FCThreshold();
    });

}

QMenu* SettingsAction::getContextMenu()
{
    auto menu = new QMenu();

    qDebug() << "in action";
    //menu->addMenu(_embeddingBPointPlotAction.getContextMenu());
    //menu->addMenu(_plotAction.getContextMenu());
    //menu->addSeparator();

    return menu;
}

void SettingsAction::fromVariantMap(const QVariantMap& variantMap)
{
    WidgetAction::fromVariantMap(variantMap);

    _currentDatasetsAction.fromParentVariantMap(variantMap);
    _embeddingAPointPlotAction.fromParentVariantMap(variantMap);
    _embeddingBPointPlotAction.fromParentVariantMap(variantMap);
    _thresholdLinesAction.fromParentVariantMap(variantMap);
    _coloringActionB.fromParentVariantMap(variantMap);
    _coloringActionA.fromParentVariantMap(variantMap);
    _selectionAction.fromParentVariantMap(variantMap);
    _enrichmentSettingsAction.fromParentVariantMap(variantMap);

    // TODO: log2FC threshold
}

QVariantMap SettingsAction::toVariantMap() const
{
    QVariantMap variantMap = WidgetAction::toVariantMap();

    _currentDatasetsAction.insertIntoVariantMap(variantMap);
    _embeddingAPointPlotAction.insertIntoVariantMap(variantMap);
    _embeddingBPointPlotAction.insertIntoVariantMap(variantMap);
    _thresholdLinesAction.insertIntoVariantMap(variantMap);
    _coloringActionB.insertIntoVariantMap(variantMap);
    _coloringActionA.insertIntoVariantMap(variantMap);
    _selectionAction.insertIntoVariantMap(variantMap);
    _enrichmentSettingsAction.insertIntoVariantMap(variantMap);

    // TODO: log2FC threshold

    return variantMap;
}
