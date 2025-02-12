#include "EmbeddingBPointPlotAction.h"
#include "src/DualViewPlugin.h"

using namespace mv::gui;

EmbeddingBPointPlotAction::EmbeddingBPointPlotAction(QObject* parent, const QString& title) :
    VerticalGroupAction(parent, title),
    //_pointSizeActionB(this, "Point Size B", 1, 50, 10),
    //_pointOpacityActionB(this, "Opacity B", 0.f, 1.f, 0.5f, 2)
    _pointPlotActionB(this, "Point Plot B")
{
    setIcon(mv::Application::getIconFont("FontAwesome").getIcon("paint-brush"));
    setToolTip("Point plot settings");
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    setLabelSizingType(LabelSizingType::Auto);

    //addAction(&_pointSizeActionB);
    //addAction(&_pointOpacityActionB);


    addAction(&_pointPlotActionB.getSizeAction());
    addAction(&_pointPlotActionB.getOpacityAction());
    addAction(&_pointPlotActionB.getFocusSelection());

    //_pointSizeActionB.setToolTip("Size of individual points for embedding B");
   // _pointOpacityActionB.setToolTip("Opacity of individual points for embedding B");


    auto dualViewPlugin = dynamic_cast<DualViewPlugin*>(parent->parent());
    if (dualViewPlugin == nullptr)
        return;

    _pointPlotActionB.initialize(dualViewPlugin);


     // embedding B
     /*const auto updateEnabledB = [this, dualViewPlugin]() {
         const auto enabled = dualViewPlugin->getEmbeddingDatasetB().isValid();

         _pointSizeActionB.setEnabled(enabled);
         _pointOpacityActionB.setEnabled(enabled);
         };

     updateEnabledB();

     connect(&dualViewPlugin->getEmbeddingDatasetB(), &Dataset<Points>::changed, this, updateEnabledB);

     connect(&_pointSizeActionB, &DecimalAction::valueChanged, [this, dualViewPlugin](float val) {
         dualViewPlugin->updateEmbeddingPointSizeB();
         });

     connect(&_pointOpacityActionB, &DecimalAction::valueChanged, [this, dualViewPlugin](float val) {
         dualViewPlugin->updateEmbeddingOpacityB();
         });*/


}

void EmbeddingBPointPlotAction::fromVariantMap(const QVariantMap& variantMap)
{
    VerticalGroupAction::fromVariantMap(variantMap);

   /* _pointSizeActionB.fromParentVariantMap(variantMap);
    _pointOpacityActionB.fromParentVariantMap(variantMap);*/
}

QVariantMap EmbeddingBPointPlotAction::toVariantMap() const
{
    auto variantMap = VerticalGroupAction::toVariantMap();

    /*_pointSizeActionB.insertIntoVariantMap(variantMap);
    _pointOpacityActionB.insertIntoVariantMap(variantMap);*/

    return variantMap;
}
