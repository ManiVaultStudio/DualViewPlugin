#include "DimensionSelectionAction.h"
#include "src/DualViewPlugin.h"

using namespace mv::gui;

DimensionSelectionAction::DimensionSelectionAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _dimensionAction(this, "Gene")
{
    setIcon(mv::Application::getIconFont("FontAwesome").getIcon("search"));
    setLabelSizingType(LabelSizingType::Auto);

    addAction(&_dimensionAction);

    _dimensionAction.setToolTip("Select the dimension to be shown");

    auto plugin = dynamic_cast<DualViewPlugin*>(parent->parent());
    if (plugin == nullptr)
        return;

    connect(&_dimensionAction, &GenePickerAction::currentDimensionIndexChanged, [this, plugin](const std::uint32_t& currentDimensionIndex) {
        qDebug() << "DimensionSelectionAction::currentDimensionIndexChanged";
        if (_dimensionAction.getCurrentDimensionIndex() != -1) // add condition that embedding A is valid}
        {
            plugin->highlightInputGenes();
        }
        });

    connect(&plugin->getEmbeddingDatasetB(), &Dataset<Points>::changed, this, [this, plugin]() {
        qDebug() << "DimensionSelectionAction::embeddingDatasetChanged";
        auto sortedGeneNames = plugin->getEmbeddingDatasetB()->getSourceDataset<Points>()->getDimensionNames();

        std::sort(sortedGeneNames.begin(), sortedGeneNames.end(), [](const QString& a, const QString& b) {
            // Compare alphabetically first
            int minLength = std::min(a.length(), b.length());
            for (int i = 0; i < minLength; i++) {
                if (a[i] != b[i]) {
                    return a[i] < b[i];
                }
            }
            // If one is a prefix of the other, or they are identical up to the minLength, sort by length
            return a.length() < b.length();
         });

        QStringList sortedGeneNamesList;
        for (const auto& str : sortedGeneNames) {
            sortedGeneNamesList.append(str);
        }

        _dimensionAction.setDimensionNames(sortedGeneNamesList);
        _dimensionAction.setCurrentDimensionIndex(-1);
        });

}

void DimensionSelectionAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);
    _dimensionAction.fromParentVariantMap(variantMap);
    
}

QVariantMap DimensionSelectionAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();

    _dimensionAction.insertIntoVariantMap(variantMap);

    return variantMap;
}
