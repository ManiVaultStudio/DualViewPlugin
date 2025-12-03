#include "src/DualViewPlugin.h"
#include "EnrichmentSettingsAction.h"

using namespace mv::gui;

EnrichmentSettingsAction::EnrichmentSettingsAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _organismPickerAction(this, "Organism"),
    _significanceThresholdMethodAction(this, "Significance Correction Method")
{
    setIconByName("dna");
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    setLabelSizingType(LabelSizingType::Auto);

    _organismPickerAction.setToolTip("Choose organism");

    _organismPickerAction.initialize(QStringList({ "Mus musculus", "Homo sapiens", "Callithrix jacchus", "Macaca mulatta"}), "Mus musculus");
    addAction(&_organismPickerAction);

    _significanceThresholdMethodAction.initialize(QStringList({ "g_SCS", "bonferroni", "fdr"}), "bonferroni");
    addAction(&_significanceThresholdMethodAction);

    auto plugin = dynamic_cast<DualViewPlugin*>(parent->parent());
    if (plugin == nullptr)
        return;

    connect(&_organismPickerAction, &OptionAction::currentTextChanged, this, [this, plugin] {
        plugin->updateEnrichmentOrganism();
        });

    connect(&_significanceThresholdMethodAction, &OptionAction::currentTextChanged, this, [this, plugin] {
        plugin->updateEnrichmentSignificanceThresholdMethod();
        });

}

void EnrichmentSettingsAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);
    _organismPickerAction.fromParentVariantMap(variantMap);
    
}

QVariantMap EnrichmentSettingsAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();

    _organismPickerAction.insertIntoVariantMap(variantMap);

    return variantMap;
}
