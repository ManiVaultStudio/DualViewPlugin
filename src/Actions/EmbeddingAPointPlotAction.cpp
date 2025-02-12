#include "EmbeddingAPointPlotAction.h"
#include "src/DualViewPlugin.h"

using namespace mv::gui;

EmbeddingAPointPlotAction::EmbeddingAPointPlotAction(QObject* parent, const QString& title) :
    VerticalGroupAction(parent, title),
    _pointPlotActionA(this, "Point Plot A")
{
    setIcon(mv::Application::getIconFont("FontAwesome").getIcon("paint-brush"));
    setToolTip("Point plot settings");
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    setLabelSizingType(LabelSizingType::Auto);

    addAction(&_pointPlotActionA.getSizeAction());
    addAction(&_pointPlotActionA.getOpacityAction());
    addAction(&_pointPlotActionA.getFocusSelection());

    auto dualViewPlugin = dynamic_cast<DualViewPlugin*>(parent->parent());
    if (dualViewPlugin == nullptr)
        return;

    _pointPlotActionA.initialize(dualViewPlugin);

}

void EmbeddingAPointPlotAction::fromVariantMap(const QVariantMap& variantMap)
{
    VerticalGroupAction::fromVariantMap(variantMap);

    _pointPlotActionA.fromParentVariantMap(variantMap);

}

QVariantMap EmbeddingAPointPlotAction::toVariantMap() const
{
    auto variantMap = VerticalGroupAction::toVariantMap();

    _pointPlotActionA.insertIntoVariantMap(variantMap);

    return variantMap;
}
