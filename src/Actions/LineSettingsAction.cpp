#include "src/DualViewPlugin.h"
#include "LineSettingsAction.h"

using namespace mv::gui;

LineSettingsAction::LineSettingsAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _thresholdLinesAction(this, "Background", 0.f, 1.f, 0.9f, 3),
    _log2FCThreshold(this, "log2FC", 0.f, 5.f, 1.5f, 2)
{
    setIconByName("sliders");
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    setLabelSizingType(LabelSizingType::Auto);

    _thresholdLinesAction.setToolTip("Expression threshold for background lines");
    _log2FCThreshold.setToolTip("log2FC Threshold");

    addAction(&_thresholdLinesAction);
    addAction(&_log2FCThreshold);

    auto plugin = dynamic_cast<DualViewPlugin*>(parent->parent());
    if (plugin == nullptr)
        return;

    connect(&_thresholdLinesAction, &DecimalAction::valueChanged, [this, plugin](float val) {
        plugin->updateThresholdLines();
        });

    connect(&_log2FCThreshold, &DecimalAction::valueChanged, [this, plugin](float val) {
        plugin->updateLog2FCThreshold();
        });

}

void LineSettingsAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);
    _thresholdLinesAction.fromParentVariantMap(variantMap);
    _log2FCThreshold.fromParentVariantMap(variantMap);
    
}

QVariantMap LineSettingsAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();

    _thresholdLinesAction.insertIntoVariantMap(variantMap);
    _log2FCThreshold.insertIntoVariantMap(variantMap);

    return variantMap;
}
