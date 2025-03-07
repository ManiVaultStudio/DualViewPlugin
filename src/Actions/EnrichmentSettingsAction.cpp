#include "src/DualViewPlugin.h"
#include "EnrichmentSettingsAction.h"

using namespace mv::gui;

EnrichmentSettingsAction::EnrichmentSettingsAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _organismPickerAction(this, "Organism")
{
    setIconByName("dna");
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    setLabelSizingType(LabelSizingType::Auto);

    _organismPickerAction.setToolTip("Choose organism");

    _organismPickerAction.initialize(QStringList({ "Mus musculus", "Homo sapiens" }), "Mus musculus");
    addAction(&_organismPickerAction);


    auto plugin = dynamic_cast<DualViewPlugin*>(parent->parent());
    if (plugin == nullptr)
        return;

    connect(&_organismPickerAction, &OptionAction::currentTextChanged, this, [this, plugin] {
        plugin->updateEnrichmentOrganism();
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
