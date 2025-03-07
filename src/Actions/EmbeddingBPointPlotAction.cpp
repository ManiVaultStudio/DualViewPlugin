#include "EmbeddingBPointPlotAction.h"
#include "src/DualViewPlugin.h"

using namespace mv::gui;

EmbeddingBPointPlotAction::EmbeddingBPointPlotAction(QObject* parent, const QString& title) :
    VerticalGroupAction(parent, title),
    _pointPlotActionB(this, "Point Plot B")
{
    setIconByName("paint-brush");
    setToolTip("Point plot settings");
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    setLabelSizingType(LabelSizingType::Auto);

    addAction(&_pointPlotActionB.getSizeAction());
    addAction(&_pointPlotActionB.getOpacityAction());
    addAction(&_pointPlotActionB.getFocusSelection());

    auto dualViewPlugin = dynamic_cast<DualViewPlugin*>(parent->parent());
    if (dualViewPlugin == nullptr)
        return;

    _pointPlotActionB.initialize(dualViewPlugin);

}

void EmbeddingBPointPlotAction::fromVariantMap(const QVariantMap& variantMap)
{
    VerticalGroupAction::fromVariantMap(variantMap);

    _pointPlotActionB.fromParentVariantMap(variantMap);

}

QVariantMap EmbeddingBPointPlotAction::toVariantMap() const
{
    auto variantMap = VerticalGroupAction::toVariantMap();

    _pointPlotActionB.insertIntoVariantMap(variantMap);

    return variantMap;
}
