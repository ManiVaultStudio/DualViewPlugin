#include "LoadedDatasetsAction.h"
#include "src/DualViewPlugin.h"

#include "PointData/PointData.h"
#include "ColorData/ColorData.h"
//#include "ClusterData/ClusterData.h"

#include <QMenu>

using namespace mv;
using namespace mv::gui;

LoadedDatasetsAction::LoadedDatasetsAction(QObject* parent, const QString& title) :
    GroupAction(parent, "Loaded datasets"),
    _embeddingADatasetPickerAction(this, "Position A"),
    _embeddingBDatasetPickerAction(this, "Position B"),
    _colorADatasetPickerAction(this, "Color A"),
    _colorBDatasetPickerAction(this, "Color B")
{
    setIcon(mv::Application::getIconFont("FontAwesome").getIcon("database"));
    setToolTip("Manage loaded datasets for position and/or color");

    _embeddingADatasetPickerAction.setFilterFunction([](const mv::Dataset<DatasetImpl>& dataset) -> bool {
        return dataset->getDataType() == PointType;
        });
    
    _embeddingBDatasetPickerAction.setFilterFunction([](const mv::Dataset<DatasetImpl>& dataset) -> bool {
        return dataset->getDataType() == PointType;
        });

    _colorADatasetPickerAction.setFilterFunction([](const mv::Dataset<DatasetImpl>& dataset) -> bool {
        return dataset->getDataType() == PointType || dataset->getDataType() == ColorType || dataset->getDataType() == ClusterType;
        });

    _colorBDatasetPickerAction.setFilterFunction([](const mv::Dataset<DatasetImpl>& dataset) -> bool {
        return dataset->getDataType() == PointType || dataset->getDataType() == ColorType || dataset->getDataType() == ClusterType;
        });
}

void LoadedDatasetsAction::initialize(DualViewPlugin* dualViewPlugin)
{
    Q_ASSERT(dualViewPlugin != nullptr);

    if (dualViewPlugin == nullptr)
        return;

    _dualViewPlugin = dualViewPlugin;

    // embedding A
    connect(&_embeddingADatasetPickerAction, &DatasetPickerAction::datasetPicked, [this](Dataset<DatasetImpl> pickedDataset) -> void {
        _dualViewPlugin->getEmbeddingDatasetA() = pickedDataset;
        //qDebug() << ">>>>> Picked a position A dataset " << pickedDataset->getGuiName();
    });

    connect(&_dualViewPlugin->getEmbeddingDatasetA(), &Dataset<Points>::changed, this, [this](DatasetImpl* dataset) -> void {
        _embeddingADatasetPickerAction.setCurrentDataset(dataset);
        //qDebug() << ">>>>> setCurrentDataset position A" << dataset->getGuiName();
    });

    // embedding B
    connect(&_embeddingBDatasetPickerAction, &DatasetPickerAction::datasetPicked, [this](Dataset<DatasetImpl> pickedDataset) -> void {
        _dualViewPlugin->getEmbeddingDatasetB() = pickedDataset;
        //qDebug() << ">>>>> Picked a position B dataset " << pickedDataset->getGuiName();
        });

    connect(&_dualViewPlugin->getEmbeddingDatasetB(), &Dataset<Points>::changed, this, [this](DatasetImpl* dataset) -> void {
        _embeddingBDatasetPickerAction.setCurrentDataset(dataset);
        //qDebug() << ">>>>> setCurrentDataset position B" << dataset->getGuiName();
        });

    // color A
    connect(&_colorADatasetPickerAction, &DatasetPickerAction::datasetPicked, [this](Dataset<DatasetImpl> pickedDataset) -> void {
        _dualViewPlugin->getMetaDatasetA() = pickedDataset;
        //qDebug() << ">>>>> Picked a color A dataset " << pickedDataset->getGuiName();
        });

    connect(&_dualViewPlugin->getMetaDatasetA(), &Dataset<Clusters>::changed, this, [this](Dataset<DatasetImpl> currentColorDataset) -> void {
        if (!currentColorDataset.isValid())
        {
			qDebug() << "LoadedDatasetsAction _dualViewPlugin->getMetaDatasetA() changed: currentColorDataset is not valid";
			return;
		}

        _colorADatasetPickerAction.setCurrentDataset(currentColorDataset);
        //qDebug() << ">>>>> setCurrentDataset color A" << currentColorDataset->getGuiName();
        });

    // color B
    connect(&_colorBDatasetPickerAction, &DatasetPickerAction::datasetPicked, [this](Dataset<DatasetImpl> pickedDataset) -> void {
        _dualViewPlugin->getMetaDatasetB() = pickedDataset;
        //qDebug() << ">>>>> Picked a color B dataset " << pickedDataset->getGuiName();
		});

    connect(&_dualViewPlugin->getMetaDatasetB(), &Dataset<Clusters>::changed, this, [this](Dataset<DatasetImpl> currentColorDataset) -> void {
        if (!currentColorDataset.isValid())
        {
            qDebug() << "LoadedDatasetsAction _dualViewPlugin->getMetaDatasetB() changed: currentColorDataset is not valid";
			return;
        }

		_colorBDatasetPickerAction.setCurrentDataset(currentColorDataset);
        //qDebug() << ">>>>> setCurrentDataset color B" << currentColorDataset->getGuiName();
		});

}

void LoadedDatasetsAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);

    _embeddingADatasetPickerAction.fromParentVariantMap(variantMap);
    _embeddingBDatasetPickerAction.fromParentVariantMap(variantMap);

    _colorADatasetPickerAction.fromParentVariantMap(variantMap);
    _colorBDatasetPickerAction.fromParentVariantMap(variantMap);

    // Load position A dataset
    auto positionADataset = _embeddingADatasetPickerAction.getCurrentDataset();
    if (positionADataset.isValid())
    {
        Dataset pickedDatasetA = mv::data().getDataset(positionADataset.getDatasetId());
        _dualViewPlugin->getEmbeddingDatasetA() = pickedDatasetA;
        //qDebug() << ">>>>> LoadedDatasetsAction::fromVariantMap Found a position A dataset " << pickedDatasetA->getGuiName();
    }


    // Load position B dataset
    auto positionBDataset = _embeddingBDatasetPickerAction.getCurrentDataset();
    if (positionBDataset.isValid())
    {
        Dataset pickedDatasetB = mv::data().getDataset(positionBDataset.getDatasetId());
        _dualViewPlugin->getEmbeddingDatasetB() = pickedDatasetB;
        //qDebug() << ">>>>> LoadedDatasetsAction::fromVariantMap Found a position B dataset " << pickedDatasetB->getGuiName();
    }

    // Load color A dataset
    auto colorADataset = _colorADatasetPickerAction.getCurrentDataset();
    if (colorADataset.isValid())
    {
		Dataset pickedDatasetA = mv::data().getDataset(colorADataset.getDatasetId());
		_dualViewPlugin->getMetaDatasetA() = pickedDatasetA;
		//qDebug() << ">>>>> LoadedDatasetsAction::fromVariantMap Found a color A dataset " << pickedDatasetA->getGuiName();
	}

    // Load color B dataset
	auto colorBDataset = _colorBDatasetPickerAction.getCurrentDataset();
    if (colorBDataset.isValid())
    {
		Dataset pickedDatasetB = mv::data().getDataset(colorBDataset.getDatasetId());
		_dualViewPlugin->getMetaDatasetB() = pickedDatasetB;
		//qDebug() << ">>>>> LoadedDatasetsAction::fromVariantMap Found a color B dataset " << pickedDatasetB->getGuiName();
	}

}

QVariantMap LoadedDatasetsAction::toVariantMap() const
{
    QVariantMap variantMap = GroupAction::toVariantMap();

    _embeddingADatasetPickerAction.insertIntoVariantMap(variantMap);
    _embeddingBDatasetPickerAction.insertIntoVariantMap(variantMap);
    _colorADatasetPickerAction.insertIntoVariantMap(variantMap);
    _colorBDatasetPickerAction.insertIntoVariantMap(variantMap);

    return variantMap;
}