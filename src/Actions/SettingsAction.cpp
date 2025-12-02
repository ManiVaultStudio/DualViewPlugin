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
    _lineSettingsAction(this, "Line Settings"),
    _coloringActionB(this, "Coloring B"),
    _coloringActionA(this, "Coloring A"),
    _selectionAction(this, "Selection"),
    _reversePointSizeBAction(this, "Reverse Point Size B"),
    _dimensionSelectionAction(this, "Gene search"),
    _enrichmentAction(this, "Enrich"),
    _enrichmentSettingsAction(this, "Enrichment settings"),
    _selectionActionB(this, "Selection B")
{
    setConnectionPermissionsToForceNone();

    _currentDatasetsAction.initialize(_dualViewPlugin);

    _selectionAction.initialize(_dualViewPlugin);

    _selectionActionB.initialize(_dualViewPlugin);

    connect(&_reversePointSizeBAction, &ToggleAction::toggled, [this](bool val) {
		_dualViewPlugin->reversePointSizeB(val);
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
    _coloringActionB.fromParentVariantMap(variantMap);
    _coloringActionA.fromParentVariantMap(variantMap);
    _selectionAction.fromParentVariantMap(variantMap);
    _enrichmentSettingsAction.fromParentVariantMap(variantMap);

    _lineSettingsAction.fromParentVariantMap(variantMap);
    _selectionActionB.fromParentVariantMap(variantMap);

}

QVariantMap SettingsAction::toVariantMap() const
{
    QVariantMap variantMap = WidgetAction::toVariantMap();

    _currentDatasetsAction.insertIntoVariantMap(variantMap);
    _embeddingAPointPlotAction.insertIntoVariantMap(variantMap);
    _embeddingBPointPlotAction.insertIntoVariantMap(variantMap);
    _coloringActionB.insertIntoVariantMap(variantMap);
    _coloringActionA.insertIntoVariantMap(variantMap);
    _selectionAction.insertIntoVariantMap(variantMap);
    _enrichmentSettingsAction.insertIntoVariantMap(variantMap);

    _lineSettingsAction.insertIntoVariantMap(variantMap);
    _selectionActionB.insertIntoVariantMap(variantMap);


    return variantMap;
}
