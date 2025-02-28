#include "src/DualViewPlugin.h"
#include "EnrichmentAction.h"

using namespace mv::gui;

EnrichmentAction::EnrichmentAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _enrichmentAction(this, "Enrich")
{
    setIcon(mv::Application::getIconFont("FontAwesome").getIcon("dna"));
    setLabelSizingType(LabelSizingType::Auto);

    addAction(&_enrichmentAction);

    _enrichmentAction.setToolTip("Enrichment analysis");

    auto plugin = dynamic_cast<DualViewPlugin*>(parent->parent());
    if (plugin == nullptr)
        return;

    connect(&_enrichmentAction, &TriggerAction::triggered, [this, plugin]() {
        plugin->getEnrichmentAnalysis();
        });
  
}

void EnrichmentAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);
    _enrichmentAction.fromParentVariantMap(variantMap);
    
}

QVariantMap EnrichmentAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();

    _enrichmentAction.insertIntoVariantMap(variantMap);

    return variantMap;
}
