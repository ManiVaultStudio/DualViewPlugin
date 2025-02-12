#include "EmbeddingAPointPlotAction.h"
#include "src/DualViewPlugin.h"

using namespace mv::gui;

EmbeddingAPointPlotAction::EmbeddingAPointPlotAction(QObject* parent, const QString& title) :
    VerticalGroupAction(parent, title),
    //_pointSizeActionA(this, "Point Size A", 1, 50, 10),
    //_pointSizeActionB(this, "Point Size B", 1, 50, 10),
    //_pointOpacityActionA(this, "Opacity A", 0.f, 1.f, 0.5f, 2),
    //_pointOpacityActionB(this, "Opacity B", 0.f, 1.f, 0.1f, 2)
    _pointPlotAction(this, "Point Plot A")
{
    setIcon(mv::Application::getIconFont("FontAwesome").getIcon("paint-brush"));
    setToolTip("Point plot settings");
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    setLabelSizingType(LabelSizingType::Auto);

    //addAction(&_pointSizeActionA);
    //addAction(&_pointOpacityActionA);
    //addAction(&_focusSelectionA);

    addAction(&_pointPlotAction.getSizeAction());
    addAction(&_pointPlotAction.getOpacityAction());
    addAction(&_pointPlotAction.getFocusSelection());


    /*_pointSizeActionA.setToolTip("Size of individual points for embedding A");
    _pointOpacityActionA.setToolTip("Opacity of individual points for embedding A");
    _focusSelectionA.setToolTip("Focus selection for embedding A");
    _focusSelectionA.setDefaultWidgetFlags(ToggleAction::CheckBox);*/


    auto dualViewPlugin = dynamic_cast<DualViewPlugin*>(parent->parent());
    if (dualViewPlugin == nullptr)
        return;

    _pointPlotAction.initialize(dualViewPlugin);

    //// embedding A
    //const auto updateEnabledA = [this, dualViewPlugin]() {
    //    const auto enabled = dualViewPlugin->getEmbeddingDatasetA().isValid();

    //    _pointSizeActionA.setEnabled(enabled);
    //    _pointOpacityActionA.setEnabled(enabled);

    //    };

    //updateEnabledA();

    //connect(&dualViewPlugin->getEmbeddingDatasetA(), &Dataset<Points>::changed, this, updateEnabledA);

    // connect(&_pointSizeActionA, &DecimalAction::valueChanged, [this, dualViewPlugin](float val) {
    //     dualViewPlugin->updateEmbeddingPointSizeA();
    // });

    // connect(&_pointOpacityActionA, &DecimalAction::valueChanged, [this, dualViewPlugin](float val) {
    //     dualViewPlugin->updateEmbeddingOpacityA();
    // });




}

void EmbeddingAPointPlotAction::fromVariantMap(const QVariantMap& variantMap)
{
    VerticalGroupAction::fromVariantMap(variantMap);

    /*_pointSizeActionA.fromParentVariantMap(variantMap);
    _pointOpacityActionA.fromParentVariantMap(variantMap);*/
}

QVariantMap EmbeddingAPointPlotAction::toVariantMap() const
{
    auto variantMap = VerticalGroupAction::toVariantMap();

   /* _pointSizeActionA.insertIntoVariantMap(variantMap);*/
    //_pointOpacityActionA.insertIntoVariantMap(variantMap);

    return variantMap;
}
