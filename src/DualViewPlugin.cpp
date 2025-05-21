#include "DualViewPlugin.h"

//#include "ChartWidget.h"
#include "ScatterplotWidget.h"
#include "EmbeddingLinesWidget.h"

#include <DatasetsMimeData.h>

#include <vector>
#include <random>
#include <unordered_set>

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QMimeData>
#include <QDebug>
#include <QTableWidget>

#include <actions/ViewPluginSamplerAction.h>

#include <chrono>

//#include <QWebEnginePage>
//#include <QWebEngineView>
#include <QStyledItemDelegate>
//#include <QWebChannel>


Q_PLUGIN_METADATA(IID "studio.manivault.DualViewPlugin")

using namespace mv;

// Example code for control column width in QTableWidget
class ElidedItemDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();

        QString text = index.data(Qt::DisplayRole).toString();
        QFontMetrics fm(option.font);
        QString elidedText = fm.elidedText(text, Qt::ElideRight, option.rect.width());

        // Draw the text
        painter->drawText(option.rect, Qt::AlignLeft | Qt::AlignVCenter, elidedText);

        painter->restore();
    }
};

DualViewPlugin::DualViewPlugin(const PluginFactory* factory) :
    ViewPlugin(factory),
    //_chartWidget(nullptr),
    _embeddingDropWidgetA(nullptr),
    _embeddingDropWidgetB(nullptr),
    _sampleScopeWidget(nullptr),
    _sampleScopeChannel(nullptr),
    _sampleScopeCommObject(nullptr),
    _embeddingDatasetA(),
    _embeddingDatasetB(),
    _embeddingWidgetA(new ScatterplotWidget(this)),
    _embeddingWidgetB(new ScatterplotWidget(this)),
    _embeddingLinesWidget(new EmbeddingLinesWidget()),
    _colorMapAction(this, "Color map", "RdYlBu"),
    _settingsAction(this, "SettingsAction"),
    _embeddingAToolbarAction(this, "Toolbar A"),
    _embeddingBToolbarAction(this, "Toolbar B"),
    _embeddingASecondaryToolbarAction(this, "Secondary Toolbar A"),
    _embeddingBSecondaryToolbarAction(this, "Secondary Toolbar B"),
    _linesToolbarAction(this, "Lines Toolbar")
{

    _settingsAction.getEnrichmentAction().setDefaultWidgetFlag(GroupAction::WidgetFlag::NoMargins);
    _settingsAction.getDimensionSelectionAction().setDefaultWidgetFlag(GroupAction::WidgetFlag::NoMargins);

    // toolbars A
    _embeddingAToolbarAction.addAction(&_settingsAction.getEnrichmentSettingsAction());
    _embeddingAToolbarAction.addAction(&_settingsAction.getEnrichmentAction());
    _embeddingAToolbarAction.addAction(&_settingsAction.getEmbeddingAPointPlotAction());
    _embeddingAToolbarAction.addAction(&getSamplerAction());
    _embeddingAToolbarAction.addAction(&_settingsAction.getSelectionAction());
    _embeddingAToolbarAction.addAction(&_settingsAction.getDimensionSelectionAction());


    //_embeddingASecondaryToolbarAction.addAction(&_embeddingWidgetA->getNavigationAction());
    // TODO FIXME: this is a hack to add the navigation action to the secondary toolbar
    auto test = _embeddingWidgetA->getPointRendererNavigator().getNavigationAction().getActions();
    auto testAction = test[4];
    testAction->setParent(&_settingsAction);
    _embeddingASecondaryToolbarAction.addAction(testAction);

    auto focusSelectionActionA = new ToggleAction(this, "Focus selection A");
    focusSelectionActionA->setIconByName("mouse-pointer");
    connect(focusSelectionActionA, &ToggleAction::toggled, this, [this](bool toggled) -> void {
        _settingsAction.getEmbeddingAPointPlotAction().getPointPlotAction().getFocusSelection().setChecked(toggled);
        });
    connect(&_settingsAction.getEmbeddingAPointPlotAction().getPointPlotAction().getFocusSelection(), &ToggleAction::toggled, this, [this, focusSelectionActionA](bool toggled) -> void {
        focusSelectionActionA->setChecked(toggled);
        });

    _embeddingASecondaryToolbarAction.addAction(focusSelectionActionA);

    // toolbars B
    _embeddingBToolbarAction.addAction(&_settingsAction.getEmbeddingBPointPlotAction());
    _embeddingBToolbarAction.addAction(&_settingsAction.getReversePointSizeBAction());

    //_embeddingBSecondaryToolbarAction.addAction(&_embeddingWidgetB->getNavigationAction());
    // TODO FIXME: this is a hack to add the navigation action to the secondary toolbar
    auto testB = _embeddingWidgetB->getPointRendererNavigator().getNavigationAction().getActions();
    auto testActionB = testB[4];
    testActionB->setParent(&_settingsAction);
    _embeddingBSecondaryToolbarAction.addAction(testActionB);

    auto focusSelectionActionB = new ToggleAction(this, "Focus selection B");
    focusSelectionActionA->setIconByName("mouse-pointer");
    connect(focusSelectionActionB, &ToggleAction::toggled, this, [this](bool toggled) -> void {
        _settingsAction.getEmbeddingBPointPlotAction().getPointPlotActionB().getFocusSelection().setChecked(toggled);
        });
    connect(&_settingsAction.getEmbeddingBPointPlotAction().getPointPlotActionB().getFocusSelection(), &ToggleAction::toggled, this, [this, focusSelectionActionB](bool toggled) -> void {
        focusSelectionActionB->setChecked(toggled);
        });

    _embeddingBSecondaryToolbarAction.addAction(focusSelectionActionB);

    // toolbar line widget
    _linesToolbarAction.addAction(&_settingsAction.getThresholdLinesAction());

    // context menu
    connect(_embeddingWidgetA, &ScatterplotWidget::customContextMenuRequested, this, [this](const QPoint& point) {
        if (!_embeddingDatasetA.isValid())
            return;

        _embeddingWidgetA->setContextMenuPolicy(Qt::CustomContextMenu);

        auto contextMenu = new QMenu(); //auto contextMenu = _settingsAction.getContextMenu();
        contextMenu->addSeparator();

        _embeddingDatasetA->populateContextMenu(contextMenu);
        contextMenu->exec(_embeddingWidgetA->mapToGlobal(point));
        });

    connect(_embeddingWidgetB, &ScatterplotWidget::customContextMenuRequested, this, [this](const QPoint& point) {
        if (!_embeddingDatasetB.isValid())
            return;

        _embeddingWidgetB->setContextMenuPolicy(Qt::CustomContextMenu);

        auto contextMenu = new QMenu();
        contextMenu->addSeparator();

        _embeddingDatasetB->populateContextMenu(contextMenu);
        contextMenu->exec(_embeddingWidgetB->mapToGlobal(point));
        });

    // Instantiate new drop widget for gene embedding
    _embeddingDropWidgetA = new DropWidget(_embeddingWidgetA);
    _embeddingDropWidgetA->setDropIndicatorWidget(new DropWidget::DropIndicatorWidget(&getWidget(), "No data loaded", "Drag the gene embedding in this view"));
    _embeddingDropWidgetA->initialize([this](const QMimeData* mimeData) -> DropWidget::DropRegions {

        // A drop widget can contain zero or more drop regions
        DropWidget::DropRegions dropRegions;

        const auto datasetsMimeData = dynamic_cast<const DatasetsMimeData*>(mimeData);

        if (datasetsMimeData == nullptr)
            return dropRegions;

        if (datasetsMimeData->getDatasets().count() > 1)
            return dropRegions;

        const auto dataset = datasetsMimeData->getDatasets().first();
        const auto datasetGuiName = dataset->text();
        const auto datasetId = dataset->getId();
        const auto dataType = dataset->getDataType();
        const auto dataTypes = DataTypes({ PointType, ClusterType });

        //check if the data type can be dropped
        if (!dataTypes.contains(dataType))
            dropRegions << new DropWidget::DropRegion(this, "Incompatible data", "This type of data is not supported", "exclamation-circle", false);

        //Points dataset is about to be dropped
        if (dataType == PointType) {

            if (datasetId == getCurrentEmebeddingDataSetID(_embeddingDatasetA)) {
                dropRegions << new DropWidget::DropRegion(this, "Warning", "Data already loaded", "exclamation-circle", false);
            }
            else {
                // Get points datset from the core
                auto candidateDataset = mv::data().getDataset<Points>(datasetId);
                qDebug() << " point dataset is dropped";

                // Establish drop region description
                const auto description = QString("Visualize %1 as points ").arg(datasetGuiName);

                if (!_embeddingDatasetA.isValid()) {

                    // Load as point positions when no dataset is currently loaded
                    dropRegions << new DropWidget::DropRegion(this, "Point position", description, "map-marker-alt", true, [this, candidateDataset]() {
                        _embeddingDatasetA = candidateDataset;
                        _embeddingDropWidgetA->setShowDropIndicator(false);
                        });
                }
                else {
                    if (_embeddingDatasetA != candidateDataset && candidateDataset->getNumDimensions() >= 2) {

                        // The number of points is equal, so offer the option to replace the existing points dataset
                        dropRegions << new DropWidget::DropRegion(this, "Point position", description, "map-marker-alt", true, [this, candidateDataset]() {
                            _embeddingDatasetA = candidateDataset;
                            });
                    }
                }
            }
        }

        //Cluster dataset is about to be dropped
        if (dataType == ClusterType) {

            // Get clusters dataset from the core
            auto candidateDataset = mv::data().getDataset<Clusters>(datasetId);

            // Establish drop region description
            const auto description = QString("Color points by %1").arg(candidateDataset->text());

            // Only allow user to color by clusters when there is a positions dataset loaded
            if (_embeddingDatasetA.isValid()) {

                //qDebug() << " Not implemented yet";
                if (_settingsAction.getColoringActionA().getCurrentColorDataset() == candidateDataset) {

                    qDebug() << "The clusters dataset is already loaded";
                    dropRegions << new DropWidget::DropRegion(this, "Warning", "Data already loaded", "exclamation-circle", false);
                }
                else {

                    // Use the clusters set for points color
                    dropRegions << new DropWidget::DropRegion(this, "Color", description, "palette", true, [this, candidateDataset]() {
                        _settingsAction.getColoringActionA().setCurrentColorDataset(candidateDataset);

                        _metaDatasetA = candidateDataset;
                        qDebug() << "DropWidget metaDatasetA is set to " << _metaDatasetA->getGuiName();
                        });
                }
            }
            else {
                // Only allow user to color by clusters when there is a positions dataset loaded
                dropRegions << new DropWidget::DropRegion(this, "No points data loaded", "Clusters can only be visualized in concert with points data", "exclamation-circle", false);
            }
        }

        return dropRegions;

        });

    // Instantiate new drop widget for 2D embedding B
    _embeddingDropWidgetB = new DropWidget(_embeddingWidgetB);
    _embeddingDropWidgetB->setDropIndicatorWidget(new DropWidget::DropIndicatorWidget(&getWidget(), "No data loaded", "Drag the cell embedding in this view"));
    _embeddingDropWidgetB->initialize([this](const QMimeData* mimeData) -> DropWidget::DropRegions {

        // A drop widget can contain zero or more drop regions
        DropWidget::DropRegions dropRegions;

        const auto datasetsMimeData = dynamic_cast<const DatasetsMimeData*>(mimeData);

        if (datasetsMimeData == nullptr)
            return dropRegions;

        if (datasetsMimeData->getDatasets().count() > 1)
            return dropRegions;

        const auto dataset = datasetsMimeData->getDatasets().first();
        const auto datasetGuiName = dataset->text();
        const auto datasetId = dataset->getId();
        const auto dataType = dataset->getDataType();
        const auto dataTypes = DataTypes({ PointType, ClusterType });

        //check if the data type can be dropped
        if (!dataTypes.contains(dataType))
            dropRegions << new DropWidget::DropRegion(this, "Incompatible data", "This type of data is not supported", "exclamation-circle", false);

        //Points dataset is about to be dropped
        if (dataType == PointType) {

            if (datasetId == getCurrentEmebeddingDataSetID(_embeddingDatasetB)) {
                dropRegions << new DropWidget::DropRegion(this, "Warning", "Data already loaded", "exclamation-circle", false);
            }
            else {
                // Get points datset from the core
                auto candidateDataset = mv::data().getDataset<Points>(datasetId);
                qDebug() << " point dataset is dropped";

                // Establish drop region description
                const auto description = QString("Visualize %1 as points ").arg(datasetGuiName);

                if (!_embeddingDatasetB.isValid()) {

                    // Load as point positions when no dataset is currently loaded
                    dropRegions << new DropWidget::DropRegion(this, "Point position", description, "map-marker-alt", true, [this, candidateDataset]() {
                        _embeddingDatasetB = candidateDataset;
                        _embeddingDropWidgetB->setShowDropIndicator(false);
                        });
                }
                else {
                    if (_embeddingDatasetB != candidateDataset && candidateDataset->getNumDimensions() >= 2) {

                        // The number of points is equal, so offer the option to replace the existing points dataset
                        dropRegions << new DropWidget::DropRegion(this, "Point position", description, "map-marker-alt", true, [this, candidateDataset]() {
                            _embeddingDatasetB = candidateDataset;
                            });
                    }
                }
            }
        }

        //Cluster dataset is about to be dropped
        if (dataType == ClusterType) {

            // Get clusters dataset from the core
            auto candidateDataset = mv::data().getDataset<Clusters>(datasetId);

            // Establish drop region description
            const auto description = QString("Color points by %1").arg(candidateDataset->text());

            // Only allow user to color by clusters when there is a positions dataset loaded
            if (_embeddingDatasetB.isValid()) {

                //qDebug() << " Not implemented yet";
                if (_settingsAction.getColoringActionB().getCurrentColorDataset() == candidateDataset) {

                    qDebug() << "The clusters dataset is already loaded";
                    dropRegions << new DropWidget::DropRegion(this, "Warning", "Data already loaded", "exclamation-circle", false);
                }
                else {

                    // Use the clusters set for points color
                    dropRegions << new DropWidget::DropRegion(this, "Color", description, "palette", true, [this, candidateDataset]() {
                        _settingsAction.getColoringActionB().setCurrentColorDataset(candidateDataset);

                        _metaDatasetB = candidateDataset;
                        qDebug() << "DropWidget metaDatasetB is set to " << _metaDatasetB->getGuiName();

                        });
                }
            }
            else {
                // Only allow user to color by clusters when there is a positions dataset loaded
                dropRegions << new DropWidget::DropRegion(this, "No points data loaded", "Clusters can only be visualized in concert with points data", "exclamation-circle", false);
            }
        }
        return dropRegions;

        });


    // Add tooltip to the scatterplot widget - TODO: only works for embedding A now
    auto& selectionAction = _settingsAction.getSelectionAction();

    getSamplerAction().initialize(this, &selectionAction.getPixelSelectionAction(), &selectionAction.getSamplerPixelSelectionAction());

    // Example code for adding a QTable widget in the tooltip
   /* auto widgetSampleScope = new QWidget();
    auto layoutSampleScope = new QVBoxLayout();

    widgetSampleScope->setLayout(layoutSampleScope);*/

    /* auto widget = new QWebEngineView();
     auto channel = new QWebChannel(widget->page());
     auto chartCommObject = new ChartCommObject();
     channel->registerObject("qtBridge", chartCommObject);
     widget->page()->setWebChannel(channel);*/
    _sampleScopeWidget = new QWebEngineView();
    _sampleScopeChannel = new QWebChannel(_sampleScopeWidget->page());
    _sampleScopeCommObject = new ChartCommObject();
    _sampleScopeChannel->registerObject("qtBridge", _sampleScopeCommObject);
    _sampleScopeWidget->page()->setWebChannel(_sampleScopeChannel);

    connect(_sampleScopeWidget, &QWidget::destroyed, this, [this]() {
        qDebug() << "SampleScopeWidget destroyed" << getGuiName();
        });


    // Example code for control column width in QTableWidget
    /*auto tableWidget = new QTableWidget(1, 5);

    tableWidget->setItem(0, 0, new QTableWidgetItem("aslkdjaskljdklasjdkljasklgjhklfghjkldfhjklghdfkjhgjkhdfjkghdfjkghkjdsakljaskldjsakljdklajdklsajlhgjkhdsajkhgdjk"));
    tableWidget->setItemDelegateForColumn(0, new ElidedItemDelegate(tableWidget));

    widgetSampleScope->layout()->addWidget(widget);
    widgetSampleScope->layout()->addWidget(tableWidget);*/

    connect(_sampleScopeCommObject, &ChartCommObject::goTermClicked, this, [this](const QString& goTermID) {
        retrieveGOtermGenes(goTermID);
        });

    getSamplerAction().setWidgetViewGeneratorFunction([this](const ViewPluginSamplerAction::SampleContext& toolTipContext) -> QWidget* { //, widgetSampleScope

        QString html = toolTipContext["GeneInfo"].toString();

        if (toolTipContext.contains("EnrichmentTable"))
        {
            QString tableHtml = toolTipContext["EnrichmentTable"].toString();
            html += tableHtml;
        }

        html += "</body></html>";

        qDebug() << "_sampleScopewidget " << _sampleScopeWidget;

        _sampleScopeWidget->setHtml(html);
        return _sampleScopeWidget;
        //return widgetSampleScope;
        });

    getSamplerAction().getEnabledAction().setChecked(false);
    getSamplerAction().setSamplingMode(ViewPluginSamplerAction::SamplingMode::Selection);
}

void DualViewPlugin::init()
{
    getWidget().setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    // Create layout
    auto layout = new QHBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);

    const auto toolbarHeight = 30;

    auto embeddinglayoutA = new QVBoxLayout();

    auto embeddingAToolbarWidget = _embeddingAToolbarAction.createWidget(&getWidget());

    embeddingAToolbarWidget->setFixedHeight(toolbarHeight);

    embeddinglayoutA->addWidget(embeddingAToolbarWidget);
    embeddinglayoutA->addWidget(_embeddingWidgetA, 1);
    embeddinglayoutA->addWidget(_embeddingASecondaryToolbarAction.createWidget(&getWidget()));


    layout->addLayout(embeddinglayoutA, 1);

    auto lineslayout = new QVBoxLayout();

    auto linesToolbarWidget = _linesToolbarAction.createWidget(&getWidget());

    linesToolbarWidget->setFixedHeight(toolbarHeight);

    lineslayout->addWidget(linesToolbarWidget);
    lineslayout->addWidget(_embeddingLinesWidget, 1);

    auto placeholderWidget = _embeddingASecondaryToolbarAction.createWidget(&getWidget());

    auto sizePolicy = placeholderWidget->sizePolicy();

    sizePolicy.setRetainSizeWhenHidden(true);

    placeholderWidget->setSizePolicy(sizePolicy);

    placeholderWidget->hide();

    lineslayout->addWidget(placeholderWidget);


    layout->addLayout(lineslayout, 1);

    auto embeddinglayoutB = new QVBoxLayout();

    auto embeddingBToolbarWidget = _embeddingBToolbarAction.createWidget(&getWidget());

    embeddingBToolbarWidget->setFixedHeight(toolbarHeight);

    embeddinglayoutB->addWidget(embeddingBToolbarWidget);
    embeddinglayoutB->addWidget(_embeddingWidgetB, 1);
    embeddinglayoutB->addWidget(_embeddingBSecondaryToolbarAction.createWidget(&getWidget()));
    layout->addLayout(embeddinglayoutB, 1);

    getWidget().setLayout(layout);

    connect(&_embeddingDatasetA, &Dataset<Points>::changed, this, &DualViewPlugin::embeddingDatasetAChanged);

    connect(&_embeddingDatasetB, &Dataset<Points>::changed, this, &DualViewPlugin::embeddingDatasetBChanged);

    connect(&_embeddingDatasetA, &Dataset<Points>::dataSelectionChanged, this, [this]() {
        if (!_embeddingDatasetA.isValid() || !_embeddingDatasetB.isValid())
            return;

        auto test = _embeddingDatasetA->getSelection<Points>()->indices.size();
        //qDebug() << "embeddingDatasetA dataSelectionChanged" << test;

        _isEmbeddingASelected = true;
        highlightSelectedLines(_embeddingDatasetA);
        highlightSelectedEmbeddings(_embeddingWidgetA, _embeddingDatasetA);

        if (_embeddingDatasetA->getSelection<Points>()->indices.size() != 0)
        {
            updateEmbeddingBSize();//if selected in embedding A and coloring/sizing embedding B by the mean expression of the selected genes     
            sendDataToSampleScope();
        }

        });

    connect(&_embeddingDatasetB, &Dataset<Points>::dataSelectionChanged, this, [this]() {
        if (!_embeddingDatasetA.isValid() || !_embeddingDatasetB.isValid())
            return;

        _isEmbeddingASelected = false;
        highlightSelectedLines(_embeddingDatasetB);
        highlightSelectedEmbeddings(_embeddingWidgetB, _embeddingDatasetB);

        if (_embeddingDatasetB->getSelection<Points>()->indices.size() != 0)
        {
            updateEmbeddingASize();//if selected in embedding B and coloring/sizing embedding A by the number of connected cells
            sendDataToSampleScope();
        }
        });

    // TEMP: hard code the isNotifyDuringSelection to false
    _embeddingWidgetA->getPixelSelectionTool().setNotifyDuringSelection(false);
    _embeddingWidgetB->getPixelSelectionTool().setNotifyDuringSelection(false);
    _embeddingLinesWidget->getPixelSelectionTool().setNotifyDuringSelection(false);

    // Update the selection when the pixel selection tool selected area changed
    connect(&_embeddingWidgetA->getPixelSelectionTool(), &PixelSelectionTool::areaChanged, [this]() {
        if (_embeddingWidgetA->getPixelSelectionTool().isNotifyDuringSelection()) {
            selectPoints(_embeddingWidgetA, _embeddingDatasetA, _embeddingPositionsA);
        }
        });

    // Update the selection when the pixel selection process ended
    connect(&_embeddingWidgetA->getPixelSelectionTool(), &PixelSelectionTool::ended, [this]() {
        if (_embeddingWidgetA->getPixelSelectionTool().isNotifyDuringSelection())
            return;
        selectPoints(_embeddingWidgetA, _embeddingDatasetA, _embeddingPositionsA);
        });

    // Update the selection when the pixel selection tool selected area changed
    connect(&_embeddingWidgetB->getPixelSelectionTool(), &PixelSelectionTool::areaChanged, [this]() {
        if (_embeddingWidgetB->getPixelSelectionTool().isNotifyDuringSelection()) {
            selectPoints(_embeddingWidgetB, _embeddingDatasetB, _embeddingPositionsB);
        }
        });

    // Update the selection when the pixel selection process ended
    connect(&_embeddingWidgetB->getPixelSelectionTool(), &PixelSelectionTool::ended, [this]() {
        if (_embeddingWidgetB->getPixelSelectionTool().isNotifyDuringSelection())
            return;
        selectPoints(_embeddingWidgetB, _embeddingDatasetB, _embeddingPositionsB);
        });

    // selection in embedding lines widget
    connect(&_embeddingLinesWidget->getPixelSelectionTool(), &PixelSelectionTool::areaChanged, [this]() {
        if (_embeddingLinesWidget->getPixelSelectionTool().isNotifyDuringSelection()) {
            std::vector<mv::Vector2f> both1DEmbeddingPositions;
            both1DEmbeddingPositions.reserve(_embedding_src.size() + _embedding_dst.size());
            both1DEmbeddingPositions.insert(both1DEmbeddingPositions.end(), _embedding_src.begin(), _embedding_src.end());
            both1DEmbeddingPositions.insert(both1DEmbeddingPositions.end(), _embedding_dst.begin(), _embedding_dst.end());
            selectPoints(_embeddingLinesWidget, both1DEmbeddingPositions);
        }
        });
    connect(&_embeddingLinesWidget->getPixelSelectionTool(), &PixelSelectionTool::ended, [this]() {
        if (_embeddingLinesWidget->getPixelSelectionTool().isNotifyDuringSelection())
            return;

        std::vector<mv::Vector2f> both1DEmbeddingPositions;
        both1DEmbeddingPositions.reserve(_embedding_src.size() + _embedding_dst.size());
        both1DEmbeddingPositions.insert(both1DEmbeddingPositions.end(), _embedding_src.begin(), _embedding_src.end());
        both1DEmbeddingPositions.insert(both1DEmbeddingPositions.end(), _embedding_dst.begin(), _embedding_dst.end());
        selectPoints(_embeddingLinesWidget, both1DEmbeddingPositions);

        });

    connect(&_embeddingDatasetA, &Dataset<Points>::dataChanged, this, [this]() {
        updateEmbeddingDataA();
        });

    connect(&_embeddingDatasetB, &Dataset<Points>::dataChanged, this, [this]() {
        updateEmbeddingDataB();
        });

    connect(&_oneDEmbeddingDatasetA, &Dataset<Points>::dataChanged, this, [this]() {
        update1DEmbeddingPositions(true);
        });

    connect(&_oneDEmbeddingDatasetB, &Dataset<Points>::dataChanged, this, [this]() {
        update1DEmbeddingPositions(false);
        });

    // update the dropped metadata for coloring 1D embeddings
    connect(&_metaDatasetA, &Dataset<Cluster>::dataChanged, this, [this]() {
        update1DEmbeddingColors(true);

        _settingsAction.getColoringActionA().updateScatterPlotWidgetColors();// update color in 2D embedding A

        qDebug() << "metaDatasetA dataChanged";
        });

    connect(&_metaDatasetA, &Dataset<Cluster>::changed, this, [this]() {
        update1DEmbeddingColors(true);

        if (_metaDatasetA.isValid())
        {
            _settingsAction.getColoringActionA().setCurrentColorDataset(_metaDatasetA);
        }
        else
            qDebug() << "_metaDatasetA changed: metaDatasetA is not valid";

        });

    connect(&_metaDatasetB, &Dataset<Cluster>::dataChanged, this, [this]() {
        update1DEmbeddingColors(false);

        _settingsAction.getColoringActionB().updateScatterPlotWidgetColors();// update color in 2D embedding B

        qDebug() << "metaDatasetB dataChanged";
        });

    connect(&_metaDatasetB, &Dataset<Cluster>::changed, this, [this]() {
        update1DEmbeddingColors(false);

        if (_metaDatasetB.isValid())
        {
            _settingsAction.getColoringActionB().setCurrentColorDataset(_metaDatasetB);
        }
        else
            qDebug() << "_metaDatasetB changed: metaDatasetB is not valid";

        // FIXME: this is a hack to update the top cell types of each gene
        if (!_loadingFromProject)
        {
            computeTopCellForEachGene();
        }

        });

    connect(&getSamplerAction(), &ViewPluginSamplerAction::sampleContextRequested, this, &DualViewPlugin::samplePoints);

    // FIXME: add selection bouderies also for B, switch the bouderies updating in selectPoints
    connect(&_embeddingWidgetA->getPointRendererNavigator().getNavigationAction().getZoomSelectionAction(), &TriggerAction::triggered, this, [this]() -> void {
        if (_selectionBoundariesA.isValid())
            _embeddingWidgetA->getPointRendererNavigator().setZoomRectangleWorld(_selectionBoundariesA);
        });

    // Enrichment analysis
    _client = new EnrichmentAnalysis(this);
    connect(_client, &EnrichmentAnalysis::enrichmentDataReady, this, &DualViewPlugin::updateEnrichmentTable);
    connect(_client, &EnrichmentAnalysis::enrichmentDataNotExists, this, &DualViewPlugin::noDataEnrichmentTable);

    connect(_client, &EnrichmentAnalysis::genesFromGOtermDataReady, this, &DualViewPlugin::highlightGOTermGenesInEmbedding);
}

void DualViewPlugin::update1DEmbeddingPositions(bool isA)
{
    QString embeddingName = isA ? "A" : "B";
    //qDebug() << "<<<<< update1DEmbeddingPositions " << embeddingName;

    if (isA)
    {   // A
        std::vector<mv::Vector2f> embedding_src;
        // FIXME: _oneDEmbeddingDatasetA could be not updated, i.e. the 1D embedding can be from the previous 2D embedding dataset, maybe add check numPoints; 
        // FIXME: or if _oneDEmbeddingDatasetA is cleared in embeddingDatasetChanged, then do not need to check numPoints
        if (_oneDEmbeddingDatasetA.isValid())
        {
            std::vector<float> yValuesA;
            _oneDEmbeddingDatasetA->extractDataForDimension(yValuesA, 0);
            //qDebug() << "yValuesA.size()" << yValuesA.size();

            embedding_src.reserve(yValuesA.size());
            for (const float& yValue : yValuesA) {
                embedding_src.emplace_back(1.0f, yValue);
            }
        }
        else
        {
            qDebug() << "_oneDEmbeddingDatasetA not valid, use projection";
            embedding_src = _embeddingPositionsA;
        }

        normalizeYValues(embedding_src);

        projectToVerticalAxis(embedding_src, 0.0f);
        _embedding_src = embedding_src;
    }
    else
    {   // B
        std::vector<mv::Vector2f> embedding_dst;
        if (_oneDEmbeddingDatasetB.isValid())
        {
            std::vector<float> yValuesB;
            _oneDEmbeddingDatasetB->extractDataForDimension(yValuesB, 0);
            //qDebug() << "yValuesB.size()" << yValuesB.size();

            embedding_dst.reserve(yValuesB.size());
            for (const float& yValue : yValuesB) {
                embedding_dst.emplace_back(1.0f, yValue);
            }
        }
        else
        {
            qDebug() << "_oneDEmbeddingDatasetB not valid, use projection";
            embedding_dst = _embeddingPositionsB;
        }

        normalizeYValues(embedding_dst);
        projectToVerticalAxis(embedding_dst, 1.0f); // TODO : make this value dynamic
        _embedding_dst = embedding_dst;
    }

    _embeddingLinesWidget->setData(_embedding_src, _embedding_dst);

    update1DEmbeddingColors(true);
    update1DEmbeddingColors(false);

}

void DualViewPlugin::update1DEmbeddingColors(bool isA)
{
    QString embeddingName = isA ? "A" : "B";

    auto positionDataset = isA ? _embeddingDatasetA : _embeddingDatasetB;
    auto& metaDataset = isA ? _metaDatasetA : _metaDatasetB;

    if (!metaDataset.isValid() || !positionDataset.isValid())
        return;

    std::vector<std::uint32_t> globalIndices;
    positionDataset->getGlobalIndices(globalIndices);

    int totalNumPoints = positionDataset->isDerivedData()
        ? positionDataset->getSourceDataset<Points>()->getFullDataset<Points>()->getNumPoints()
        : positionDataset->getFullDataset<Points>()->getNumPoints();

    std::vector<Vector3f> globalColors(totalNumPoints, Vector3f(0.0f, 0.0f, 0.0f));
    std::vector<Vector3f> localColors(positionDataset->getNumPoints());

    for (const auto& cluster : metaDataset.get<Clusters>()->getClusters())
    {
        const auto& color = Vector3f(cluster.getColor().redF(), cluster.getColor().greenF(), cluster.getColor().blueF());
        for (const auto& index : cluster.getIndices())
            globalColors[index] = color;
    }

#pragma omp parallel for
    for (std::size_t i = 0; i < globalIndices.size(); i++)
    {
        localColors[i] = globalColors[globalIndices[i]];
    }

    if (isA)
        _embeddingLinesWidget->setPointColorA(localColors);
    else
        _embeddingLinesWidget->setPointColorB(localColors);
}

void DualViewPlugin::updateLineConnections()
{
    if (_embedding_src.empty() || _embedding_dst.empty())
    {
        qDebug() << "Both 1D embedding positions must be assigned before connecting lines";
        return;
    }

    // define lines - assume embedding A is dimension embedding, embedding B is observation embedding
    int numDimensions = _embeddingSourceDatasetB->getNumDimensions(); // Assume the number of points in A is the same as the number of dimensions in B
    int numDimensionsFull = _embeddingSourceDatasetB->getNumDimensions();
    int numPoints = _embeddingSourceDatasetB->getNumPoints(); // num of points in source dataset B
    int numPointsLocal = _embeddingDatasetB->getNumPoints(); // num of points in the embedding B

    std::vector<std::uint32_t> localGlobalIndicesB;
    _embeddingSourceDatasetB->getGlobalIndices(localGlobalIndicesB);

    _lines.clear();

    // Iterate over each row and column in the subset to generate lines
    auto start2 = std::chrono::high_resolution_clock::now();
    for (int cellLocalIndex = 0; cellLocalIndex < numPointsLocal; cellLocalIndex++) {
        for (int dimIdx = 0; dimIdx < numDimensions; dimIdx++) {

            int cellGlobalIdx = static_cast<int>(localGlobalIndicesB[cellLocalIndex]);
            float expression = _embeddingSourceDatasetB->getValueAt(cellGlobalIdx * numDimensionsFull + dimIdx);

            //if (expression != columnMins[dimLocalIdx]) { // only draw lines for cells whose expression are not at the minimum
            if (expression > (_columnMins[dimIdx] + _thresholdLines * _columnRanges[dimIdx])) { // draw lines for cells whose expression are above the thresholdvalue
                _lines.emplace_back(dimIdx, cellLocalIndex);
            }
        }
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2);
    qDebug() << "Iterate over each row and column in the subset to generate lines took " << duration2.count() << "ms";

    //qDebug() << "DualViewPlugin::updateLineConnections() _lines size" << _lines.size();

    _embeddingLinesWidget->setLines(_lines);
}

void DualViewPlugin::updateEmbeddingDataA()
{
    if (!_embeddingDatasetA.isValid())
        return;

    _embeddingDatasetA->extractDataForDimensions(_embeddingPositionsA, 0, 1);
    _embeddingWidgetA->setData(&_embeddingPositionsA);
}

void DualViewPlugin::updateEmbeddingDataB()
{
    if (!_embeddingDatasetB.isValid())
        return;

    _embeddingDatasetB->extractDataForDimensions(_embeddingPositionsB, 0, 1);
    _embeddingWidgetB->setData(&_embeddingPositionsB);
}

void DualViewPlugin::embeddingDatasetAChanged()
{
    _embeddingDropWidgetA->setShowDropIndicator(!_embeddingDatasetA.isValid());

    if (!_embeddingDatasetA.isValid())
        return;

    _embeddingDatasetA->extractDataForDimensions(_embeddingPositionsA, 0, 1);
    qDebug() << "_embeddingPositionsA size" << _embeddingPositionsA.size();

    _embeddingWidgetA->setColorMap(_colorMapAction.getColorMapImage().mirrored(false, true));
    _embeddingWidgetA->setData(&_embeddingPositionsA);

    _embeddingSourceDatasetA = _embeddingDatasetA->getSourceDataset<Points>();

    // TODO: hard code to clear _metaDatasetA, when the embedding A is changed - check if better way to do this
    _metaDatasetA = nullptr;
    qDebug() << "embeddingDatasetAChanged(): metaDatasetA removed";

    // update 1D embedding
    bool oneDEmbeddingExists = false;
    for (const auto& child : _embeddingDatasetA->getChildren({ PointType }, false)) // bool recursively = false
    {
        if (child->getGuiName().contains("1D Embedding")) //child->getGuiName() == "1D Embedding A" // TO DO : change to work with the analysis plugin
        {
            _oneDEmbeddingDatasetA = child;
            oneDEmbeddingExists = true;
            update1DEmbeddingPositions(true);
            updateLineConnections();

            break;
        }
    }

    if (!oneDEmbeddingExists)
    {
        //compute1DEmbedding(_embeddingDatasetA, _oneDEmbeddingDatasetA);
        qDebug() << "1D embedding A does not exist";
        // TODO: clear  _oneDEmbeddingDatasetB and then use projection, and then need to assign the positions to the embedding dataset for selectPoints()
        _oneDEmbeddingDatasetA = nullptr;
        update1DEmbeddingPositions(true);
        updateLineConnections();
    }
}

void DualViewPlugin::embeddingDatasetBChanged()
{
    _embeddingDropWidgetB->setShowDropIndicator(!_embeddingDatasetB.isValid());

    if (!_embeddingDatasetB.isValid())
        return;

    _embeddingDatasetB->extractDataForDimensions(_embeddingPositionsB, 0, 1);
    qDebug() << "_embeddingPositionsB size" << _embeddingPositionsB.size();

    _embeddingWidgetB->setColorMap(_colorMapAction.getColorMapImage().mirrored(false, true));
    _embeddingWidgetB->setData(&_embeddingPositionsB);

    _embeddingSourceDatasetB = _embeddingDatasetB->getSourceDataset<Points>();
    qDebug() << "DualViewPlugin::_embeddingSourceDatasetB: " << _embeddingSourceDatasetB->getGuiName(); // Note: different from that of DualAnalysisPlugin

    // TODO: hard code to clear _metaDatasetB, when the embedding B is changed - check if better way to do this
    _metaDatasetB = nullptr;
    qDebug() << "embeddingDatasetBChanged(): metaDatasetB removed";

    // if HSNE, _embeddingSourceDatasetB is a helper (subset of whole data)
    mv::Dataset<Points> fullDatasetB;
    if (_embeddingSourceDatasetB->isDerivedData())
    {
        qDebug() << "_embeddingSourceDatasetB is derived data";
        fullDatasetB = _embeddingSourceDatasetB->getSourceDataset<Points>()->getFullDataset<Points>();
    }
    else
    {
        qDebug() << "_embeddingSourceDatasetB is not derived data";
        fullDatasetB = _embeddingSourceDatasetB->getFullDataset<Points>();
    }
    qDebug() << "fullDatasetB gui name" << fullDatasetB->getGuiName();

    // precompute the data range
    //computeDataRange(_embeddingSourceDatasetB, _columnMins, _columnRanges);
    computeDataRange(fullDatasetB, _columnMins, _columnRanges);
    qDebug() << "Data range computed" << _columnMins.size() << _columnRanges.size() << _columnMins[0] << _columnRanges[0];

    // precompute the mean expression for each gene // TODO: check if recompute is needed
    //computeMeanExpressionForAllCells(_embeddingSourceDatasetB, _meanExpressionForAllCells);
    computeMeanExpressionForAllCells(fullDatasetB, _meanExpressionForAllCells);
    qDebug() << "Mean expression for all cells computed" << _meanExpressionForAllCells.size();

    // set the background gene names for the enrichment analysis
    if (_embeddingSourceDatasetB->getDimensionNames().size() < 19000) // FIXME: hard code the threshold for the number of genes
    {
        if (_backgroundGeneNames.size() == _embeddingSourceDatasetB->getDimensionNames().size())
        {
            qDebug() << "Background gene names already set";
        }
        else
        {
            _backgroundGeneNames.clear();
            auto dimNames = _embeddingSourceDatasetB->getDimensionNames();
            for (const auto& name : dimNames) {
                _backgroundGeneNames.append(name);
            }
            qDebug() << _backgroundGeneNames.size() << " background gene names set";
        }
    }
    else
        qDebug() << "Background gene names not set: genes more than 19000";

    // update 1D embedding
    bool oneDEmbeddingExists = false;
    for (const auto& child : _embeddingDatasetB->getChildren({ PointType }, false)) // bool recursively = false
    {
        if (child->getGuiName().contains("1D Embedding")) //child->getGuiName() == "1D Embedding B" // TO DO : change to work with the analysis plugin
        {
            _oneDEmbeddingDatasetB = child;
            oneDEmbeddingExists = true;
            update1DEmbeddingPositions(false);
            updateLineConnections();

            break;
        }
    }

    if (!oneDEmbeddingExists)
    {
        //compute1DEmbedding(_embeddingDatasetB, _oneDEmbeddingDatasetB);
        qDebug() << "1D embedding B does not exist";
        // TODO: clear  _oneDEmbeddingDatasetB and then use projection, and then need to assign the positions to the embedding dataset for selectPoints()
        _oneDEmbeddingDatasetB = nullptr;
        update1DEmbeddingPositions(false);
        updateLineConnections();
    }
}

void DualViewPlugin::highlightSelectedLines(mv::Dataset<Points> dataset)
{
    if (!dataset.isValid())
        return;

    auto selection = dataset->getSelection<Points>();

    std::vector<bool> selected; // bool of selected in the current scale
    std::vector<char> highlights;

    dataset->selectedLocalIndices(selection->indices, selected);

    // update embedding lines widget
    std::vector<int> localSelectionIndices; // selected points - local indices
    for (int i = 0; i < selected.size(); i++) {
        if (selected[i] == 1)
        {
            localSelectionIndices.push_back(i);
        }
    }

    if (_isEmbeddingASelected)
    {
        _embeddingLinesWidget->setHighlights(localSelectionIndices, true); //true: A, false: B
    }
    else
    {
        _embeddingLinesWidget->setHighlights(localSelectionIndices, false); //true: A, false: B
    }
}

void DualViewPlugin::highlightInputGenes(const QStringList& dimensionNames)
{
    if (dimensionNames.isEmpty() || !_embeddingSourceDatasetB.isValid())
        return;

    QList<int> indices;
    identifyGeneSymbolsInDataset(_embeddingSourceDatasetB, dimensionNames, indices);

    if (indices.isEmpty())
        return;

    // set the selected indices to 1 in highlights
    std::vector<char> highlights(_embeddingPositionsA.size(), 0);
    for (int i = 0; i < indices.size(); i++)
    {
        highlights[indices[i]] = 1;
    }

    _embeddingWidgetA->setHighlights(highlights, 1);

    // test to set as selection
    //std::vector<std::uint32_t> indicesVec(indices.begin(), indices.end());
    //_embeddingDatasetA->setSelectionIndices(indicesVec);// TODO FIXME, should this publish selection? or just setHighlights? In associatedGenes?
    // FIXME: this will trigger dataSelectionChanged, and then highlightSelectedEmbeddings, and then sendDataToSampleScope
    //qDebug() << "highlightInputGenes - setSelectionIndices 1 ";
    //events().notifyDatasetDataSelectionChanged(_embeddingDatasetA);
    //qDebug() << "highlightInputGenes - setSelectionIndices 2 ";

    // test to create a dataset for the selected genes - for potential density plot
    bool createNewDataset = false;
    if (!_customisedGenes.isValid())
    {
        _customisedGenes = mv::data().createDataset("Points", "CustomisedGenes");
        createNewDataset = true;
        qDebug() << "Create new dataset";
    }

    auto numCustomisedGenes = indices.size();

    QVector<float> customisedGeneCoor(2 * numCustomisedGenes);

    for (int i = 0; i < numCustomisedGenes; i++)
    {
        int geneIndice = indices[i];
        customisedGeneCoor[2 * i] = _embeddingPositionsA[geneIndice].x;
        customisedGeneCoor[2 * i + 1] = _embeddingPositionsA[geneIndice].y;
    }

    // first cancel the selection on this dataset, to avoid selection indices conflict after data changed
    _customisedGenes->setSelectionIndices({});

    _customisedGenes->setData(customisedGeneCoor.data(), numCustomisedGenes, 2);
    events().notifyDatasetDataChanged(_customisedGenes);

    // link the associated genes to the original embedding
    mv::SelectionMap mapping;
    auto& selectionMap = mapping.getMap();

    std::vector<unsigned int> globalIndices;
    _embeddingDatasetA->getGlobalIndices(globalIndices);

    for (size_t i = 0; i < indices.size(); i++) {
        int indexOriginal = indices[i]; // Local index in _embeddingDatasetA
        std::vector<unsigned int> indexOriginalVector = { static_cast<uint32_t>(indexOriginal) };
        selectionMap[i] = indexOriginalVector; // i: index in _associatedGenes, indexOriginal: index in _embeddingDatasetA
    }

    if (createNewDataset)
    {
        qDebug() << "Add linked data";
        _customisedGenes->addLinkedData(_embeddingDatasetA, mapping);
    }
    else
    {
        qDebug() << "Update linked data";
        //qDebug() << "linkedData size = " << _customisedGenes->getLinkedData().size();
        mv::LinkedData& linkedData = _customisedGenes->getLinkedData().back();
        //auto test = linkedData.getSourceDataSet();
        //qDebug() << "highlightInputGenes - source dataset name = " << test->getGuiName();

        linkedData.setMapping(mapping);
        //qDebug() << " finished setMapping";
    }

    // test output the found gene symbols
 //   QStringList foundGeneSymbols;
 //   auto allNames = _embeddingSourceDatasetB->getDimensionNames();
 //   for (int i = 0; i < indices.size(); i++)
    //{
    //	foundGeneSymbols.append(allNames[indices[i]]);
    //}
 //   qDebug() << "Found gene symbols: " << foundGeneSymbols;

}

void DualViewPlugin::highlightSelectedEmbeddings(ScatterplotWidget*& widget, mv::Dataset<Points> dataset)
{
    if (!dataset.isValid())
        return;

    auto selection = dataset->getSelection<Points>();

    std::vector<bool> selected; // bool of selected in the current scale
    std::vector<char> highlights;

    dataset->selectedLocalIndices(selection->indices, selected);

    highlights.resize(dataset->getNumPoints(), 0);

    for (std::size_t i = 0; i < selected.size(); i++)
        highlights[i] = selected[i] ? 1 : 0;

    //update the scatterplot widget
    widget->setHighlights(highlights, static_cast<std::int32_t>(selection->indices.size())); // TODO: repeated when threshold changing
}

void DualViewPlugin::sendDataToSampleScope()
{
    if (getSamplerAction().getEnabledAction().isChecked() == false)
        return;

    QVariantList globalPointIndices;
    QStringList labels;
    QStringList data;
    QStringList backgroundColors;

    // --------- prepare the data for the sampler action -------------------------------
    if (_isEmbeddingASelected)
    {
        // A selected, send gene id and major cell type to samplerAction

        auto selection = _embeddingDatasetA->getSelection<Points>();

        std::vector<std::uint32_t> localGlobalIndices;
        _embeddingDatasetA->getGlobalIndices(localGlobalIndices);

        std::vector<std::uint32_t> sampledPoints;
        sampledPoints.reserve(_embeddingPositionsA.size());

        for (auto selectionIndex : selection->indices)
            sampledPoints.push_back(selectionIndex);

        std::int32_t numberOfPoints = 0;

        const auto numberOfSelectedPoints = selection->indices.size();

        globalPointIndices.reserve(static_cast<std::int32_t>(numberOfSelectedPoints));

        for (const auto& sampledPoint : sampledPoints) {
            if (getSamplerAction().getRestrictNumberOfElementsAction().isChecked() && numberOfPoints >= getSamplerAction().getMaximumNumberOfElementsAction().getValue())
                break;

            const auto& globalPointIndex = localGlobalIndices[sampledPoint];
            globalPointIndices << globalPointIndex;

            numberOfPoints++;
        }

        // if no metadata B avaliable, only send gene ids
        if (_metaDatasetB.isValid())
        {
            // load the meta data categories
            QVector<Cluster> metadata = _metaDatasetB->getClusters();

            int numPointsB = _embeddingSourceDatasetB->getNumPoints();

            int top10 = numPointsB / 10; // top 10% of the cells

            // count the number of cells that have expression more than the lowest value
            int countMin = 0;
            float minExpression = *std::min_element(_columnMins.begin(), _columnMins.end());
            for (int i = 0; i < numPointsB; i++)
            {
                if (_selectedGeneMeanExpression[i] > minExpression)
                    countMin++;
            }
            // if the number of cells with expression more than min is less than 10%, use that number
            int numCellsCounted = (countMin > top10) ? top10 : countMin;

            // first, sort the indices based on the selected gene expression
            std::vector<std::pair<float, int>> rankedCells;
            rankedCells.reserve(numPointsB);

            for (int i = 0; i < numPointsB; i++)
            {
                rankedCells.emplace_back(_selectedGeneMeanExpression[i], i);
            }

            auto nth = rankedCells.begin() + numCellsCounted;
            std::nth_element(rankedCells.begin(), nth, rankedCells.end(), std::greater<std::pair<float, int>>());

            // mapping from local to global indices
            std::vector<std::uint32_t> localGlobalIndicesB;
            _embeddingSourceDatasetB->getGlobalIndices(localGlobalIndicesB);

            std::vector<std::uint32_t> sampledPoints; // global indices of the cells in the top 10%
            for (int i = 0; i < numCellsCounted; ++i)
            {
                int localCellIndex = rankedCells[i].second;
                int globalCellIndex = localGlobalIndicesB[localCellIndex];
                sampledPoints.push_back(globalCellIndex);
            }
            std::tie(labels, data, backgroundColors) = computeMetadataCounts(metadata, sampledPoints);
        }
    }
    else
    {
        // B selected, send cell types to samplerAction

        // TODO: disabled, temporarily updated in updateEmbeddingASize
        // send genes that are connected to selected cells (use _connectedCellsPerGene)
        //for (int i = 0; i < _connectedCellsPerGene.size(); i++)
        //{
        //    if (_connectedCellsPerGene[i] > 0)
        //    {
        //        globalPointIndices << i;// gene index
        //    }
        //}

        if (_metaDatasetB.isValid())
        {
            QVector<Cluster> metadata = _metaDatasetB->getClusters();

            auto selection = _embeddingDatasetB->getSelection<Points>();

            std::vector<std::uint32_t> localGlobalIndices;
            _embeddingDatasetB->getGlobalIndices(localGlobalIndices);

            std::vector<std::uint32_t> sampledPoints;
            sampledPoints.reserve(_embeddingPositionsB.size());

            for (auto selectionIndex : selection->indices)
            {
                sampledPoints.push_back(selectionIndex);// global cell index
                //qDebug() << "selectionIndex" << selectionIndex;
            }

            std::tie(labels, data, backgroundColors) = computeMetadataCounts(metadata, sampledPoints);
        }
    }

    // --------- prepare the data for the sampler action ------------------------------- 
    assert(_embeddingDatasetA->getNumPoints() == _embeddingSourceDatasetB->getNumDimensions());
    std::vector<QString> dimensionNames = _embeddingSourceDatasetB->getDimensionNames(); // TODO: hardcode, assume embedding A is gene map and embedding B stores all the genes in A

    // TODO: disabled, temporarily updated in updateEmbeddingASize
    //_currentGeneSymbols.clear();
    //// assign gene symbols corresponding to globalPointIndices
    //for (int i = 0; i < globalPointIndices.size(); i++)
    //{
    //    int globalIndex = globalPointIndices[i].toInt();
    //    QString geneSymbol = dimensionNames[globalIndex];
    //    _currentGeneSymbols.append(geneSymbol);
    //}

    QString colorDatasetID = _settingsAction.getColoringActionB().getCurrentColorDataset().getDatasetId();
    QString colorDatasetName;
    if (!colorDatasetID.isEmpty())
        colorDatasetName = mv::data().getDataset(colorDatasetID)->getGuiName();

    QString html = buildHtmlForSelection(_isEmbeddingASelected, colorDatasetName, _currentGeneSymbols, labels, data, backgroundColors);


    getSamplerAction().setSampleContext({
        { "GeneInfo", html}
        });

    // cache
    _currentHtmlGeneInfo.clear();
    _currentHtmlGeneInfo = html;
}

void DualViewPlugin::updateEmbeddingBSize()
{
    if (!_embeddingDatasetA.isValid() || !_embeddingDatasetB.isValid())
        return;

    auto start1 = std::chrono::high_resolution_clock::now();
    updateSelectedGeneMeanExpression();
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1);
    qDebug() << "updateSelectedGeneMeanExpression() took " << duration1.count() << "ms";

    if (_selectedGeneMeanExpression.size() != _embeddingDatasetB->getNumPoints())
    {
        qDebug() << "Warning! selectedGeneMeanExpression size " << _selectedGeneMeanExpression.size() << "is not equal to the number of points in embedding B" << _embeddingDatasetB->getNumPoints();
        return;
    }

    std::vector<float> selectedGeneMeanExpression;
    float ptSize = _settingsAction.getEmbeddingBPointPlotAction().getPointPlotActionB().getSizeAction().getMagnitudeAction().getValue();
    scaleDataRange(_selectedGeneMeanExpression, selectedGeneMeanExpression, _reversePointSizeB, ptSize);

    _embeddingWidgetB->setPointSizeScalars(selectedGeneMeanExpression);
}

void DualViewPlugin::reversePointSizeB(bool reversePointSizeB)
{
    _reversePointSizeB = reversePointSizeB;

    if (!_embeddingDatasetB.isValid())
        return;

    if (_selectedGeneMeanExpression.size() == 0)
    {
        qDebug() << "reversePointSizeB canceled, because no gene is selected.";
        _settingsAction.getReversePointSizeBAction().setChecked(false);
        return;
    }

    std::vector<float> selectedGeneMeanExpression;
    float ptSize = _settingsAction.getEmbeddingBPointPlotAction().getPointPlotActionB().getSizeAction().getMagnitudeAction().getValue();
    scaleDataRange(_selectedGeneMeanExpression, selectedGeneMeanExpression, _reversePointSizeB, ptSize);

    _embeddingWidgetB->setPointSizeScalars(selectedGeneMeanExpression);
}

void DualViewPlugin::updateEmbeddingASize()
{
    if (!_embeddingDatasetA.isValid() || !_embeddingDatasetB.isValid())
        return;

    int numPointsA = _embeddingDatasetA->getNumPoints();
    auto selection = _embeddingDatasetB->getSelection<Points>();

    std::vector<bool> selected; // bool of selected in the current scale
    std::vector<char> highlights;
    _embeddingDatasetB->selectedLocalIndices(selection->indices, selected);
    qDebug() << "DualViewPlugin: " << selection->indices.size() << " points in B selected";

    // get how many connected points in B per point in A
//    _connectedCellsPerGene.clear();
//    _connectedCellsPerGene.resize(numPointsA, 0.0f);
//
//#pragma omp parallel for
//    for (int i = 0; i < _lines.size(); i++)
//    {
//        int pointB = _lines[i].second;
//        if (pointB < selected.size() && selected[pointB])
//        {
//#pragma omp atomic
//            _connectedCellsPerGene[_lines[i].first] += 1.0f;
//        }
//    }
//
//    std::vector<float> scaledConnectedCellsPerGene;
//    float ptSize = _settingsAction.getEmbeddingAPointPlotAction().getPointPlotAction().getSizeAction().getMagnitudeAction().getValue();
//    scaleDataRange(_connectedCellsPerGene, scaledConnectedCellsPerGene, false, ptSize * 3); // TODO: 3 is the hard coded factor
//    _embeddingWidgetA->setPointSizeScalars(scaledConnectedCellsPerGene);

    // test2 - use average expression of selected cells in B for the point size in A
//    std::vector<std::uint32_t> localGlobalIndicesB;
//    _embeddingSourceDatasetB->getGlobalIndices(localGlobalIndicesB);
//
//    std::vector<float> selectedMeanExpression(numPointsA, 0.0f);
//    int selectedNumPointsB = selection->indices.size();
//    int numDimensionsB = _embeddingSourceDatasetB->getNumDimensions();
//
//    for (int i = 0; i < selectedNumPointsB; i++)
//    {
//        int globalIndex = selection->indices[i];
//        //qDebug() << "globalIndex" << globalIndex;
//
//        for (int j = 0; j < numPointsA; j++)
//        {
//			selectedMeanExpression[j] += _embeddingSourceDatasetB->getValueAt(globalIndex * numDimensionsB + j);
//		}
//    }
//
//    // average the expression
//    for (int i = 0; i < numPointsA; i++)
//    {
//		selectedMeanExpression[i] /= selectedNumPointsB;
//	}
//
//    qDebug() << "selectedMeanExpression size" << selectedMeanExpression.size();  
//    
//
//    //set size scalars
//    auto min_max = std::minmax_element(selectedMeanExpression.begin(), selectedMeanExpression.end());
//    float min_val = *min_max.first;
//    float max_val = *min_max.second;
//    //qDebug() << "min_val" << min_val << "max_val" << max_val;
//    float ptSize = _settingsAction.getEmbeddingAPointPlotAction().getPointSizeActionA().getValue();
//
//#pragma omp parallel for
//    for (int i = 0; i < selectedMeanExpression.size(); i++)
//    {
//        selectedMeanExpression[i] = ((selectedMeanExpression[i] - min_val) / (max_val - min_val)) * ptSize *3; // TODO: hardcoded factor 
//    }
//    qDebug() << "scalars A computed";
//    _embeddingWidgetA->setPointSizeScalars(selectedMeanExpression);

    // test2 - end

    // test 3 compute genes of cell selection vs all - start
    std::vector<float> selectedCellMeanExpression;
    computeSelectedCellMeanExpression(_embeddingSourceDatasetB, selectedCellMeanExpression);
    qDebug() << "selectedCellMeanExpression size" << selectedCellMeanExpression.size();

    // selection vs all
    std::vector<float> diffSelectionvsAll(selectedCellMeanExpression.size(), 0.0f);
    for (int i = 0; i < selectedCellMeanExpression.size(); i++)
    {
        diffSelectionvsAll[i] = selectedCellMeanExpression[i] - _meanExpressionForAllCells[i];
    }
    qDebug() << "diffSelectionvsAll size" << diffSelectionvsAll.size();

    // test to set connectedcellspergene using diffSelectionvsAll
    _connectedCellsPerGene.clear();
    _connectedCellsPerGene = diffSelectionvsAll;

    std::vector<float> scaledConnectedCellsPerGene;
    float ptSize = _settingsAction.getEmbeddingAPointPlotAction().getPointPlotAction().getSizeAction().getMagnitudeAction().getValue();
    scaleDataRange(_connectedCellsPerGene, scaledConnectedCellsPerGene, false, ptSize * 3); // TODO: 3 is the hard coded factor
    _embeddingWidgetA->setPointSizeScalars(scaledConnectedCellsPerGene);

    // output the gene symbols with highest diffSelectionvsAll
    std::vector<std::pair<float, int>> rankedGenesDiff;
    rankedGenesDiff.reserve(diffSelectionvsAll.size());
    for (int i = 0; i < diffSelectionvsAll.size(); i++)
    {
        rankedGenesDiff.emplace_back(diffSelectionvsAll[i], i);
    }
    int number = 50;
    auto nth = rankedGenesDiff.begin() + number;
    std::nth_element(rankedGenesDiff.begin(), nth, rankedGenesDiff.end(), std::greater<std::pair<float, int>>());
    std::sort(rankedGenesDiff.begin(), nth, std::greater<std::pair<float, int>>()); // sort

    // get the gene and store in _currentGeneSymbols
    _currentGeneSymbols.clear();
    for (int i = 0; i < number; i++)
    {
        int geneIndex = rankedGenesDiff[i].second;
        QString geneSymbol = _embeddingSourceDatasetB->getDimensionNames()[geneIndex];
        _currentGeneSymbols.append(geneSymbol);
    }


   /* qDebug() << "Top genes with highest diffSelectionvsAll";
    for (int i = 0; i < number; i++)
    {
        int geneIndex = rankedGenes[i].second;
        QString geneSymbol = _embeddingSourceDatasetB->getDimensionNames()[geneIndex];
        qDebug() << "gene symbol" << geneSymbol << "diffSelectionvsAll" << rankedGenes[i].first << "mean selection" << selectedCellMeanExpression[geneIndex] << "mean all" << _meanExpressionForAllCells[geneIndex];
    }*/

    // test 3 -end

}

void DualViewPlugin::samplePoints()
{

    // for now only for embedding A
    auto& samplerPixelSelectionTool = _embeddingWidgetA->getSamplerPixelSelectionTool();

    if (!_embeddingDatasetA.isValid() || _embeddingWidgetA->isNavigating() || !samplerPixelSelectionTool.isEnabled())
        return;

    auto selectionAreaImage = samplerPixelSelectionTool.getAreaPixmap().toImage();

    std::vector<std::uint32_t> targetSelectionIndices;

    targetSelectionIndices.reserve(_embeddingDatasetA->getNumPoints());

    std::vector<std::pair<float, std::uint32_t>> sampledPoints;

    std::vector<std::uint32_t> localGlobalIndices;

    _embeddingDatasetA->getGlobalIndices(localGlobalIndices);

    auto& pointRenderer = _embeddingWidgetA->getPointRenderer();
    auto& navigator = pointRenderer.getNavigator();

    const auto zoomRectangleWorld = navigator.getZoomRectangleWorld();
    const auto screenRectangle = QRect(QPoint(), pointRenderer.getRenderSize());
    const auto mousePositionWorld = pointRenderer.getScreenPointToWorldPosition(pointRenderer.getNavigator().getViewMatrix(), _embeddingWidgetA->mapFromGlobal(QCursor::pos()));

    // Go over all points in the dataset to see if they should be sampled
    for (std::uint32_t localPointIndex = 0; localPointIndex < _embeddingPositionsA.size(); localPointIndex++) {
        // Compute the offset of the point in the world space
        const auto pointOffsetWorld = QPointF(_embeddingPositionsA[localPointIndex].x - zoomRectangleWorld.left(), _embeddingPositionsA[localPointIndex].y - zoomRectangleWorld.top());

        // Normalize it 
        const auto pointOffsetWorldNormalized = QPointF(pointOffsetWorld.x() / zoomRectangleWorld.width(), pointOffsetWorld.y() / zoomRectangleWorld.height());

        // Convert it to screen space
        const auto pointOffsetScreen = QPoint(pointOffsetWorldNormalized.x() * screenRectangle.width(), screenRectangle.height() - pointOffsetWorldNormalized.y() * screenRectangle.height());

        // Continue to next point if the point is outside the screen
        if (!screenRectangle.contains(pointOffsetScreen))
            continue;

        // If the corresponding pixel is not transparent, add the point to the selection
        if (selectionAreaImage.pixelColor(pointOffsetScreen).alpha() > 0) {
            const auto sample = std::pair((QVector2D(_embeddingPositionsA[localPointIndex].x, _embeddingPositionsA[localPointIndex].y) - mousePositionWorld.toVector2D()).length(), localPointIndex);

            sampledPoints.emplace_back(sample);

            targetSelectionIndices.push_back(localGlobalIndices[localPointIndex]); // for connecting sampled points as selected points TODO: is this proper?
        }
    }

    QVariantList localPointIndices, globalPointIndices, distances;

    localPointIndices.reserve(static_cast<std::int32_t>(sampledPoints.size()));
    globalPointIndices.reserve(static_cast<std::int32_t>(sampledPoints.size()));
    distances.reserve(static_cast<std::int32_t>(sampledPoints.size()));

    std::int32_t numberOfPoints = 0;

    std::sort(sampledPoints.begin(), sampledPoints.end(), [](const auto& sampleA, const auto& sampleB) -> bool {
        return sampleB.first > sampleA.first;
        });

    std::vector<char> focusHighlights(_embeddingPositionsA.size());

    for (const auto& sampledPoint : sampledPoints) {
        if (getSamplerAction().getRestrictNumberOfElementsAction().isChecked() && numberOfPoints >= getSamplerAction().getMaximumNumberOfElementsAction().getValue())
            break;

        const auto& distance = sampledPoint.first;
        const auto& localPointIndex = sampledPoint.second;
        const auto& globalPointIndex = localGlobalIndices[localPointIndex];

        distances << distance;
        localPointIndices << localPointIndex;
        globalPointIndices << globalPointIndex;

        focusHighlights[localPointIndex] = true;

        numberOfPoints++;
    }

    qDebug() << "DualViewPlugin samplePoints" << numberOfPoints << "points sampled";

    // connect sampled points as selected points
    _embeddingDatasetA->setSelectionIndices(targetSelectionIndices);

    events().notifyDatasetDataSelectionChanged(_embeddingDatasetA->getSourceDataset<Points>());

}

void DualViewPlugin::selectPoints(ScatterplotWidget* widget, mv::Dataset<Points> embeddingDataset, const std::vector<mv::Vector2f>& embeddingPositions)
{
    //if (getSettingsAction().getSelectionAction().getFreezeSelectionAction().isChecked())
    //    return;

    auto& pixelSelectionTool = widget->getPixelSelectionTool();

    // Only proceed with a valid points position dataset and when the pixel selection tool is active
    if (!embeddingDataset.isValid() || !pixelSelectionTool.isActive() || widget->getPointRenderer().getNavigator().isNavigating() || !pixelSelectionTool.isEnabled())
        return;

    auto selectionAreaImage = pixelSelectionTool.getAreaPixmap().toImage();
    auto selectionSet = embeddingDataset->getSelection<Points>();

    std::vector<std::uint32_t> targetSelectionIndices;

    targetSelectionIndices.reserve(embeddingDataset->getNumPoints());

    std::vector<std::uint32_t> localGlobalIndices;

    embeddingDataset->getGlobalIndices(localGlobalIndices);

    auto& pointRenderer = widget->getPointRenderer();
    auto& navigator = pointRenderer.getNavigator();

    const auto zoomRectangleWorld = navigator.getZoomRectangleWorld();
    const auto screenRectangle = QRect(QPoint(), pointRenderer.getRenderSize());

    float boundaries[4]{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::lowest()
    };

    // Go over all points in the dataset to see if they are selected
    for (std::uint32_t localPointIndex = 0; localPointIndex < embeddingPositions.size(); localPointIndex++) {
        const auto& point = embeddingPositions[localPointIndex];

        // Compute the offset of the point in the world space
        const auto pointOffsetWorld = QPointF(point.x - zoomRectangleWorld.left(), point.y - zoomRectangleWorld.top());

        // Normalize it 
        const auto pointOffsetWorldNormalized = QPointF(pointOffsetWorld.x() / zoomRectangleWorld.width(), pointOffsetWorld.y() / zoomRectangleWorld.height());

        // Convert it to screen space
        const auto pointOffsetScreen = QPoint(pointOffsetWorldNormalized.x() * screenRectangle.width(), screenRectangle.height() - pointOffsetWorldNormalized.y() * screenRectangle.height());

        // Continue to next point if the point is outside the screen
        if (!screenRectangle.contains(pointOffsetScreen))
            continue;

        // If the corresponding pixel is not transparent, add the point to the selection
        if (selectionAreaImage.pixelColor(pointOffsetScreen).alpha() > 0) {
            targetSelectionIndices.push_back(localGlobalIndices[localPointIndex]);

            boundaries[0] = std::min(boundaries[0], point.x);
            boundaries[1] = std::max(boundaries[1], point.x);
            boundaries[2] = std::min(boundaries[2], point.y);
            boundaries[3] = std::max(boundaries[3], point.y);
        }
    }

    // FIXME: switch between A and B
    //_selectionBoundariesA = QRectF(boundaries[0], boundaries[2], boundaries[1] - boundaries[0], boundaries[3] - boundaries[2]);

    switch (const auto selectionModifier = pixelSelectionTool.isAborted() ? PixelSelectionModifierType::Subtract : pixelSelectionTool.getModifier())
    {
    case PixelSelectionModifierType::Replace:
        break;

    case PixelSelectionModifierType::Add:
    case PixelSelectionModifierType::Subtract:
    {
        // Get reference to the indices of the selection set
        auto& selectionSetIndices = selectionSet->indices;

        // Create a set from the selection set indices
        QSet<std::uint32_t> set(selectionSetIndices.begin(), selectionSetIndices.end());

        switch (selectionModifier)
        {
            // Add points to the current selection
        case PixelSelectionModifierType::Add:
        {
            // Add indices to the set 
            for (const auto& targetIndex : targetSelectionIndices)
                set.insert(targetIndex);

            break;
        }

        // Remove points from the current selection
        case PixelSelectionModifierType::Subtract:
        {
            // Remove indices from the set 
            for (const auto& targetIndex : targetSelectionIndices)
                set.remove(targetIndex);

            break;
        }

        case PixelSelectionModifierType::Replace:
            break;
        }

        targetSelectionIndices = std::vector<std::uint32_t>(set.begin(), set.end());

        break;
    }
    }

    auto& navigationAction = const_cast<NavigationAction&>(widget->getPointRenderer().getNavigator().getNavigationAction());

    navigationAction.getZoomSelectionAction().setEnabled(!targetSelectionIndices.empty() && !navigationAction.getFreezeNavigation().isChecked());

    embeddingDataset->setSelectionIndices(targetSelectionIndices);

    events().notifyDatasetDataSelectionChanged(embeddingDataset->getSourceDataset<Points>());
}

void DualViewPlugin::selectPoints(EmbeddingLinesWidget* widget, const std::vector<mv::Vector2f>& embeddingPositions)
{
    // Only proceed with a valid points position dataset and when the pixel selection tool is active
    /*if (!_oneDEmbeddingDatasetA.isValid() || !_oneDEmbeddingDatasetB.isValid() || !widget->getPixelSelectionTool().isActive())
    {
        qDebug() << "DualViewPlugin: selectPoints() - invalid 1D dataset or pixel selection tool is not active";
        return;
    }*/

    //if (getSettingsAction().getSelectionAction().getFreezeSelectionAction().isChecked())
   //    return;

    auto& pixelSelectionTool = widget->getPixelSelectionTool();

    // Only proceed with a valid points position dataset and when the pixel selection tool is active
    if (!_oneDEmbeddingDatasetA.isValid() || !_oneDEmbeddingDatasetB.isValid() || !widget->getPixelSelectionTool().isActive())
    {
        qDebug() << "DualViewPlugin: selectPoints() - invalid 1D dataset or pixel selection tool is not active";
        return;
    }

    auto selectionAreaImage = pixelSelectionTool.getAreaPixmap().toImage();

    // Get smart pointer to the position selection dataset
    auto selectionSetA = _oneDEmbeddingDatasetA->getSelection<Points>();
    auto selectionSetB = _oneDEmbeddingDatasetB->getSelection<Points>();

    // Create vector for target selection indices
    std::vector<std::uint32_t> targetSelectionIndicesA;
    std::vector<std::uint32_t> targetSelectionIndicesB;

    // Reserve space for the indices
    targetSelectionIndicesA.reserve(_oneDEmbeddingDatasetA->getNumPoints());
    targetSelectionIndicesB.reserve(_oneDEmbeddingDatasetB->getNumPoints());

    // Mapping from local to global indices
    std::vector<std::uint32_t> localGlobalIndicesA;
    std::vector<std::uint32_t> localGlobalIndicesB;

    // Get global indices from the position dataset
    _oneDEmbeddingDatasetA->getGlobalIndices(localGlobalIndicesA);
    _oneDEmbeddingDatasetB->getGlobalIndices(localGlobalIndicesB);

    auto& pointRenderer = widget->getPointRenderer();
    auto& navigator = pointRenderer.getNavigator();
    const auto zoomRectangleWorld = navigator.getZoomRectangleWorld();
    const auto screenRectangle = QRect(QPoint(), pointRenderer.getRenderSize());

    float boundaries[4]{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::lowest()
    };

    //qDebug() << "Selection area size:" << width << height << size;
    //qDebug() << "Data bounds top" << dataBounds.getTop() << "bottom" << dataBounds.getBottom() << "left" << dataBounds.getLeft() << "right" << dataBounds.getRight();
    //qDebug() << "Data bounds width" << dataBounds.getWidth() << "height" << dataBounds.getHeight();

    std::size_t datasetASize = _oneDEmbeddingDatasetA->getNumPoints();
    std::size_t datasetBSize = _oneDEmbeddingDatasetB->getNumPoints();

    // Loop over all points and establish whether they are selected or not
    for (std::uint32_t localPointIndex = 0; localPointIndex < embeddingPositions.size(); localPointIndex++) {
        const auto& point = embeddingPositions[localPointIndex];
        const auto pointOffsetWorld = QPointF(point.x - zoomRectangleWorld.left(), point.y - zoomRectangleWorld.top());
        const auto pointOffsetWorldNormalized = QPointF(pointOffsetWorld.x() / zoomRectangleWorld.width(), pointOffsetWorld.y() / zoomRectangleWorld.height());
        const auto pointOffsetScreen = QPoint(pointOffsetWorldNormalized.x() * screenRectangle.width(), screenRectangle.height() - pointOffsetWorldNormalized.y() * screenRectangle.height());

        if (!screenRectangle.contains(pointOffsetScreen))
            continue;

        // Add point if the corresponding pixel selection is on
        if (localPointIndex < datasetASize) {
            if (selectionAreaImage.pixelColor(pointOffsetScreen).alpha() > 0)
            {
                targetSelectionIndicesA.push_back(localGlobalIndicesA[localPointIndex]);
                boundaries[0] = std::min(boundaries[0], point.x);
                boundaries[1] = std::max(boundaries[1], point.x);
                boundaries[2] = std::min(boundaries[2], point.y);
                boundaries[3] = std::max(boundaries[3], point.y);
            }

        }
        else {
            std::size_t datasetBIndex = localPointIndex - datasetASize;
            if (selectionAreaImage.pixelColor(pointOffsetScreen).alpha() > 0)
            {
                targetSelectionIndicesB.push_back(localGlobalIndicesB[datasetBIndex]);
                boundaries[0] = std::min(boundaries[0], point.x);
                boundaries[1] = std::max(boundaries[1], point.x);
                boundaries[2] = std::min(boundaries[2], point.y);
                boundaries[3] = std::max(boundaries[3], point.y);
            }
        }
    }

    //qDebug() << "selection done" << "targetSelectionIndicesA.size() = " << targetSelectionIndicesA.size() << "targetSelectionIndicesB.size() = " << targetSelectionIndicesB.size();

    _oneDEmbeddingDatasetA->setSelectionIndices(targetSelectionIndicesA);

    if (targetSelectionIndicesA.size() != 0) // only notify if there are selected points
        events().notifyDatasetDataSelectionChanged(_oneDEmbeddingDatasetA->getSourceDataset<Points>()->getSourceDataset<Points>());

    _oneDEmbeddingDatasetB->setSelectionIndices(targetSelectionIndicesB);

    if (targetSelectionIndicesB.size() != 0) // only notify if there are selected points
        events().notifyDatasetDataSelectionChanged(_oneDEmbeddingDatasetB->getSourceDataset<Points>()->getSourceDataset<Points>());

}

void DualViewPlugin::updateSelectedGeneMeanExpression()
{
    std::vector<float> selectedGeneMeanExpressionFull;
    computeSelectedGeneMeanExpression(_embeddingSourceDatasetB, _embeddingDatasetA, selectedGeneMeanExpressionFull);

    extractSelectedGeneMeanExpression(_embeddingSourceDatasetB, selectedGeneMeanExpressionFull, _selectedGeneMeanExpression);

    if (!_meanExpressionScalars.isValid())
    {
        _meanExpressionScalars = mv::data().createDataset<Points>("Points", "ExprScalars");
        events().notifyDatasetAdded(_meanExpressionScalars);
    }

    _meanExpressionScalars->setData<float>(selectedGeneMeanExpressionFull.data(), selectedGeneMeanExpressionFull.size(), 1);
    events().notifyDatasetDataChanged(_meanExpressionScalars);
}

QString DualViewPlugin::getCurrentEmebeddingDataSetID(mv::Dataset<Points> dataset) const
{
    if (dataset.isValid())
        return dataset->getId();
    else
        return QString{};
}

void DualViewPlugin::updateThresholdLines()
{
    _thresholdLines = _settingsAction.getThresholdLinesAction().getValue();

    updateLineConnections();

    if (_isEmbeddingASelected)
    {
        highlightSelectedLines(_embeddingDatasetA);
        highlightSelectedEmbeddings(_embeddingWidgetA, _embeddingDatasetA);
    }
    else
    {
        highlightSelectedLines(_embeddingDatasetB);
        highlightSelectedEmbeddings(_embeddingWidgetB, _embeddingDatasetB);
    }
}

void DualViewPlugin::computeTopCellForEachGene()
{
    if (!_embeddingDatasetA.isValid() || !_embeddingDatasetB.isValid() || !_metaDatasetB.isValid())
    {
        //qDebug() << "DualViewPlugin: embeddingDatasetA or embeddingDatasetB or metaDatasetB is not valid";
        return;
    }

    qDebug() << "computeTopCellForEachGene(): _metaDatasetB is " << _metaDatasetB->getGuiName();

    int numGene = _embeddingDatasetA->getNumPoints(); // number of genes in the current gene embedding  

    // TEST 2: use the cell type with max avg expression for each gene - START

    auto fullDatasetB = _embeddingDatasetB->getSourceDataset<Points>()->getFullDataset<Points>();

    std::vector<std::vector<float>> avgExpressionForEachGeneForEachCluster; // cluster is stored in the same order as in the meta dataset

    auto start1 = std::chrono::high_resolution_clock::now();
    // FIXME: this would return if embedding B is the overview scale of HSNE, do we want this?
    if (_embeddingDatasetB->getNumPoints() != _embeddingDatasetA->getSourceDataset<Points>()->getNumDimensions())
    {
        //qDebug() << "computeTopCellForEachGene(): num of pt in embeddingDatasetB is different from num of dimensions in embedding A source dataset";
        //qDebug() << "embeddingDatasetB->getNumPoints() = " << _embeddingDatasetB->getNumPoints() << "sourceDatasetA->getNumDimensions() = " << _embeddingDatasetA->getSourceDataset<Points>()->getNumDimensions();
        qDebug() << "WARNING: embedding A and embedding B is not corresponded, use the full expression matrix to compute top cell type";

        // if not correspond, use the full expression matrix + give a warning      

        for (const auto& cluster : _metaDatasetB.get<Clusters>()->getClusters())
        {
            // for this cluster, compute the avg expression for each gene
            std::vector<float> avgExpressionForEachGene(numGene, 0.0f);
            int count = 0;
            for (const auto& index : cluster.getIndices()) // index is the global index of the cell in embedding B
            {

                int offset = index * numGene;
                for (int i = 0; i < numGene; i++)
                {
                    avgExpressionForEachGene[i] += fullDatasetB->getValueAt(offset + i);
                }
                count++;
            }

            if (count == 0)
            {
                qDebug() << "No valid indices in cluster " << cluster.getName();
                //continue;
                for (int i = 0; i < numGene; i++) {
                    avgExpressionForEachGene[i] = -100.0f; // TODO: this cluster is actually invalid, FIXME: Keep a separate boolean vector to indicate invalid clusters when choosing top cluster
                }
            }
            else
            {
                for (int i = 0; i < numGene; i++)
                {
                    avgExpressionForEachGene[i] /= static_cast<float>(count); // better use static_cast<float>(count) ?
                }
            }

            avgExpressionForEachGeneForEachCluster.push_back(avgExpressionForEachGene);
        }
    }
    else // if correspond
    {
        // map global indices to local indices in embedding B
        std::vector<std::uint32_t> localGlobalIndicesB;
        _embeddingDatasetB->getGlobalIndices(localGlobalIndicesB);
        //qDebug() << "localGlobalIndicesB.size() = " << localGlobalIndicesB.size();

        // Build a map from global indices to local indices
        std::unordered_map<std::uint32_t, std::uint32_t> globalToLocalIndexMapB;
        for (std::uint32_t localIndexB = 0; localIndexB < localGlobalIndicesB.size(); ++localIndexB) {
            globalToLocalIndexMapB[localGlobalIndicesB[localIndexB]] = localIndexB;
        }
        //qDebug() << "globalToLocalIndexMapB.size() = " << globalToLocalIndexMapB.size();

        // compute the avg expression of each gene for each cell type

        for (const auto& cluster : _metaDatasetB.get<Clusters>()->getClusters())
        {
            // for this cluster, compute the avg expression for each gene
            std::vector<float> avgExpressionForEachGene(numGene, 0.0f);
            int count = 0;

            for (const auto& globalCellIndex : cluster.getIndices()) // global cell index in embedding B
            {
                // Check if this global index exists in the local embedding B
                auto it = globalToLocalIndexMapB.find(globalCellIndex);
                if (it == globalToLocalIndexMapB.end()) {
                    // Global index not found in local embedding, skip
                    continue;
                }

                // Extract gene expression for this cell from fullDatasetB using global indices
                //std::vector<float> geneExpression(numGene, 0.0f);
                //for (int i = 0; i < numGene; i++)
                //{
                //    // and getValueAt(row * numGene + col) retrieves the value:
                //    geneExpression[i] = fullDatasetB->getValueAt((globalCellIndex * numGene) + i);
                //}

                //qDebug() << "extracted from fullDataA geneExpression.size() = " << geneExpression.size();

                int offset = globalCellIndex * numGene;
                for (int i = 0; i < numGene; i++)
                {
                    //avgExpressionForEachGene[i] += geneExpression[i];
                    avgExpressionForEachGene[i] += fullDatasetB->getValueAt(offset + i);
                }
                count++;
            }

            if (count == 0)
            {
                qDebug() << "No valid indices in cluster " << cluster.getName();
                //continue;
                for (int i = 0; i < numGene; i++) {
                    avgExpressionForEachGene[i] = -100.0f; // TODO: this cluster is actually invalid, FIXME: Keep a separate boolean vector to indicate invalid clusters when choosing top cluster
                }
            }
            else
            {
                for (int i = 0; i < numGene; i++)
                {
                    avgExpressionForEachGene[i] /= static_cast<float>(count);
                }
            }
            //qDebug() << "avgExpressionForEachGene.size() = " << avgExpressionForEachGene.size();

            avgExpressionForEachGeneForEachCluster.push_back(avgExpressionForEachGene);
            qDebug() << "In this subset: " << cluster.getName() << " count = " << count;
        }
    }

    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1);
    qDebug() << "compute avg expression for each gene for each cell type took " << duration1.count() << " ms";


    // find the cell type with max avg expression for each gene
    std::vector<int> topCellTypeForEachLocalGene(numGene, -1);

    for (int i = 0; i < numGene; i++)
    {
        float maxAvgExpression = avgExpressionForEachGeneForEachCluster[0][i];
        int maxAvgExpressionIndex = 0;
        for (int j = 0; j < avgExpressionForEachGeneForEachCluster.size(); j++)
        {
            if (avgExpressionForEachGeneForEachCluster[j][i] > maxAvgExpression)
            {
                maxAvgExpression = avgExpressionForEachGeneForEachCluster[j][i];
                maxAvgExpressionIndex = j;
            }
        }
        topCellTypeForEachLocalGene[i] = maxAvgExpressionIndex;
    }
    //qDebug() << "topCellTypeForEachLocalGene.size() = " << topCellTypeForEachLocalGene.size();


    // extract the colors and cluster names for each cell type
    auto start3 = std::chrono::high_resolution_clock::now();
    std::vector<Vector3f> cellTypeColors(_metaDatasetB.get<Clusters>()->getClusters().size());
    std::vector<QString> cellTypeNames(_metaDatasetB.get<Clusters>()->getClusters().size());

    int i = 0;
    for (const auto& cluster : _metaDatasetB.get<Clusters>()->getClusters())
    {
        cellTypeColors[i] = Vector3f(cluster.getColor().redF(), cluster.getColor().greenF(), cluster.getColor().blueF());
        cellTypeNames[i] = cluster.getName();
        i++;
    }
    qDebug() << "cellTypeColors.size() = " << cellTypeColors.size();


    // create a dataset to store the top cell for each gene, if not exist
    if (!_topCellForEachGeneDataset.isValid())
    {
        _topCellForEachGeneDataset = mv::data().createDataset<Clusters>("Cluster", "TopCellForGene");
        events().notifyDatasetAdded(_topCellForEachGeneDataset);
    }
    else
    {
        _topCellForEachGeneDataset->getClusters().clear();
    }

    // assign the top cell for each gene to a cluster
    for (unsigned int i = 0; i < numGene; i++)
    {
        const QString clusterName = cellTypeNames[topCellTypeForEachLocalGene[i]];
        Vector3f clusterColor = cellTypeColors[topCellTypeForEachLocalGene[i]];
        std::vector<unsigned int> indices = { i };

        Cluster cluster;
        cluster.setName(clusterName);
        cluster.setColor(QColor::fromRgbF(clusterColor.x, clusterColor.y, clusterColor.z));
        cluster.setIndices(indices);

        _topCellForEachGeneDataset->addCluster(cluster);
    }

    events().notifyDatasetDataChanged(_topCellForEachGeneDataset);
    // TEST 2: use the cell type with max avg expression for each gene - END 

    qDebug() << "DualViewPlugin: computeTopCellForEachGene() done";

}

void DualViewPlugin::getEnrichmentAnalysis()
{
    //TODO: some genes end with _dup+number, should deal with this

     //// output _simplifiedToIndexGeneMapping if not empty
     //if (!_simplifiedToIndexGeneMapping.empty()) {
     //    for (auto it = _simplifiedToIndexGeneMapping.constBegin(); it != _simplifiedToIndexGeneMapping.constEnd(); ++it) {
     //        qDebug() << it.key() << ":";
     //        for (const QString& value : it.value()) {
     //            qDebug() << value;
     //        }
     //    }
     //}

    if (!_currentGeneSymbols.isEmpty())
    {
        qDebug() << "DualViewPlugin: getEnrichmentAnalysis()";
        _client->postGeneGprofiler(_currentGeneSymbols, _backgroundGeneNames, _currentEnrichmentSpecies, _currentSignificanceThresholdMethod);
    }
    else
    {
        qDebug() << "DualViewPlugin: getEnrichmentAnalysis() - no gene selected";
    }
}

void DualViewPlugin::updateEnrichmentTable(const QVariantList& data)
{
    QString tableHtml = buildHtmlForEnrichmentResults(data);

    getSamplerAction().setSampleContext({
        { "GeneInfo", _currentHtmlGeneInfo },
        { "EnrichmentTable", tableHtml }
        });

    qDebug() << "updateEnrichmentTable(): Table sent to Sample Scope.";
}

void DualViewPlugin::noDataEnrichmentTable()
{
    QString tableHtml = buildHtmlForEmpty();

    getSamplerAction().setSampleContext({
        { "GeneInfo", _currentHtmlGeneInfo },
        { "EnrichmentTable", tableHtml }
        });
}

void DualViewPlugin::retrieveGOtermGenes(const QString& GOTermId)
{
    qDebug() << "DualViewPlugin::retrieveGOtermGeneSet()" << GOTermId << "species" << _currentEnrichmentSpecies;

    _client->postGOtermGprofiler(GOTermId, _currentEnrichmentSpecies);

}

void DualViewPlugin::highlightGOTermGenesInEmbedding(const QStringList& geneSymbols)
{
    qDebug() << "DualViewPlugin::highlightGOTermGenesInEmbedding()";

    QList<int> indices;
    identifyGeneSymbolsInDataset(_embeddingSourceDatasetB, geneSymbols, indices);

    if (indices.isEmpty())
        return;

    // set the selected indices to 1 in highlights
    std::vector<char> highlights(_embeddingPositionsA.size(), 0);
    for (int i = 0; i < indices.size(); i++)
    {
        highlights[indices[i]] = 1;
    }

    _embeddingWidgetA->setHighlights(highlights, 1);

    // use a seperate dataset for gene subset - Attention: linking to the original embedding is 1-directional, from subset to original embedding
    bool createNewDataset = false;
    if (!_associatedGenes.isValid())
    {
        _associatedGenes = mv::data().createDataset("Points", "AssociatedGenes");
        createNewDataset = true;
        qDebug() << "Create new dataset";
    }

    auto numAssociatedGenes = indices.size();

    QVector<float> associatedGeneCoor(2 * numAssociatedGenes);

    for (int i = 0; i < numAssociatedGenes; i++)
    {
        int geneIndice = indices[i];
        associatedGeneCoor[2 * i] = _embeddingPositionsA[geneIndice].x;
        associatedGeneCoor[2 * i + 1] = _embeddingPositionsA[geneIndice].y;
    }
    //qDebug() << "associatedGeneCoor.size() = " << associatedGeneCoor.size();

    // first cancel the selection on this dataset, to avoid selection indices conflict after data changed
    _associatedGenes->setSelectionIndices({});

    _associatedGenes->setData(associatedGeneCoor.data(), numAssociatedGenes, 2);
    events().notifyDatasetDataChanged(_associatedGenes);

    // link the associated genes to the original embedding
    mv::SelectionMap mapping;
    auto& selectionMap = mapping.getMap();

    std::vector<unsigned int> globalIndices;
    _embeddingDatasetA->getGlobalIndices(globalIndices);
    //qDebug() << "globalIndices.size() = " << globalIndices.size();

    for (size_t i = 0; i < indices.size(); i++) {
        int indexOriginal = indices[i]; // Local index in _embeddingDatasetA
        std::vector<unsigned int> indexOriginalVector = { static_cast<uint32_t>(indexOriginal) };
        selectionMap[i] = indexOriginalVector; // i: index in _associatedGenes, indexOriginal: index in _embeddingDatasetA
    }

    //qDebug() << "selectionMap.size() = " << selectionMap.size();

    if (createNewDataset)
    {
        qDebug() << "Add linked data";
        _associatedGenes->addLinkedData(_embeddingDatasetA, mapping);
    }
    else
    {
        qDebug() << "Update linked data";
        //qDebug() << "linkedData size = " << _associatedGenes->getLinkedData().size();
        mv::LinkedData& linkedData = _associatedGenes->getLinkedData().back();
        //auto test = linkedData.getSourceDataSet();
        //qDebug() << "highlightGOTermGenesInEmbedding - source dataset name = " << test->getGuiName();

        linkedData.setMapping(mapping);
        // qDebug() << " finished setMapping";
    }

}

void DualViewPlugin::updateEnrichmentOrganism()
{
    QString selectedSpecies = _settingsAction.getEnrichmentSettingsAction().getOrganismPickerAction().getCurrentText();

    if (selectedSpecies == "Mus musculus")
    {
        _currentEnrichmentSpecies = "mmusculus";
        qDebug() << "Enrichment species changed to: " << _currentEnrichmentSpecies;
        getEnrichmentAnalysis();
    }
    else if (selectedSpecies == "Homo sapiens")
    {
        _currentEnrichmentSpecies = "hsapiens";
        qDebug() << "Enrichment species changed to: " << _currentEnrichmentSpecies;
        getEnrichmentAnalysis();
    }
}

void DualViewPlugin::updateEnrichmentSignificanceThresholdMethod()
{
    QString selectedMethod = _settingsAction.getEnrichmentSettingsAction().getSignificanceThresholdMethodAction().getCurrentText();

    _currentSignificanceThresholdMethod = selectedMethod;
    qDebug() << "Enrichment method changed to: " << _currentSignificanceThresholdMethod;
    getEnrichmentAnalysis();
}


// =============================================================================
// Serialization
// =============================================================================

void DualViewPlugin::fromVariantMap(const QVariantMap& variantMap)
{
    _loadingFromProject = true;

    ViewPlugin::fromVariantMap(variantMap);

    qDebug() << "DualViewPlugin: fromVariantMap start";

    variantMapMustContain(variantMap, "SettingsAction");

    _settingsAction.fromVariantMap(variantMap["SettingsAction"].toMap());
    qDebug() << "DualViewPlugin: fromVariantMap SettingsAction done";

    _embeddingAToolbarAction.fromVariantMap(variantMap["Toolbar A"].toMap());

    _embeddingBToolbarAction.fromVariantMap(variantMap["Toolbar B"].toMap());

    _linesToolbarAction.fromVariantMap(variantMap["Lines Toolbar"].toMap());

    qDebug() << "DualViewPlugin: fromVariantMap Toolbars done";

    if (variantMap.contains("topCellForEachGeneDataset"))
    {
        QString topCellId = variantMap["topCellForEachGeneDataset"].toString();
        _topCellForEachGeneDataset = mv::data().getDataset<Points>(topCellId);
        qDebug() << "DualViewPlugin: fromVariantMap topCellForEachGeneDataset done";
    }

    if (variantMap.contains("meanExpressionScalars"))
    {
        QString meanExpressionId = variantMap["meanExpressionScalars"].toString();
        _meanExpressionScalars = mv::data().getDataset<Points>(meanExpressionId);
        qDebug() << "DualViewPlugin: fromVariantMap meanExpressionScalars done";
    }

    if (_metaDatasetA.isValid())
        qDebug() << "DualViewPlugin: fromVariantMap _metaDatasetA = " << _metaDatasetA->getGuiName();
    else
        qDebug() << "DualViewPlugin: fromVariantMap _metaDatasetA is not valid";
    if (_metaDatasetB.isValid())
        qDebug() << "DualViewPlugin: fromVariantMap _metaDatasetB = " << _metaDatasetB->getGuiName();
    else
        qDebug() << "DualViewPlugin: fromVariantMap _metaDatasetB is not valid";

    // TODO: T. Kroes
    //if (variantMap.contains("NavigationA")) //FIXME: temp during development for loading old projects, remove later
    //    _embeddingWidgetA->getNavigationAction().fromVariantMap(variantMap["NavigationA"].toMap());
    //if (variantMap.contains("NavigationB")) //FIXME: temp during development for loading old projects, remove later
    //    _embeddingWidgetB->getNavigationAction().fromVariantMap(variantMap["NavigationB"].toMap());

    // FIXME: temp during development, decide whether to keep or not later
    if (variantMap.contains("customisedGenes"))
    {
        QString customisedGenesId = variantMap["customisedGenes"].toString();
        _customisedGenes = mv::data().getDataset<Points>(customisedGenesId);
    }
    if (variantMap.contains("associatedGenes"))
    {
        QString associatedGenesId = variantMap["associatedGenes"].toString();
        _associatedGenes = mv::data().getDataset<Points>(associatedGenesId);
    }

    _loadingFromProject = false;

    getLearningCenterAction().getToolbarVisibleAction().setChecked(false);
    getLearningCenterAction().setAlignment(Qt::AlignmentFlag::AlignBottom | Qt::AlignmentFlag::AlignRight);
}

QVariantMap DualViewPlugin::toVariantMap() const
{
    QVariantMap variantMap = ViewPlugin::toVariantMap();

    _settingsAction.insertIntoVariantMap(variantMap);

    _embeddingAToolbarAction.insertIntoVariantMap(variantMap);

    _embeddingBToolbarAction.insertIntoVariantMap(variantMap);

    _linesToolbarAction.insertIntoVariantMap(variantMap);

    if (_topCellForEachGeneDataset.isValid())
    {
        variantMap.insert("topCellForEachGeneDataset", _topCellForEachGeneDataset.getDatasetId());
    }

    if (_meanExpressionScalars.isValid())
    {
        variantMap.insert("meanExpressionScalars", _meanExpressionScalars.getDatasetId());
    }

    // TODO: T. Kroes
    //insertIntoVariantMap(_embeddingWidgetA->getNavigationAction(), variantMap, "NavigationA");

    //insertIntoVariantMap(_embeddingWidgetB->getNavigationAction(), variantMap, "NavigationB"); //FIXME: is this correct?

    // FIXME: temp during development
    if (_customisedGenes.isValid())
    {
        variantMap.insert("customisedGenes", _customisedGenes.getDatasetId());
    }
    if (_associatedGenes.isValid())
    {
        variantMap.insert("_associatedGenes", _associatedGenes.getDatasetId());
    }


    return variantMap;
}


// =============================================================================
// Plugin Factory 
// =============================================================================



ViewPlugin* DualViewPluginFactory::produce()
{
    return new DualViewPlugin(this);
}

mv::DataTypes DualViewPluginFactory::supportedDataTypes() const
{
    // This example analysis plugin is compatible with points datasets
    DataTypes supportedTypes;
    supportedTypes.append(PointType);
    return supportedTypes;
}

mv::gui::PluginTriggerActions DualViewPluginFactory::getPluginTriggerActions(const mv::Datasets& datasets) const
{
    PluginTriggerActions pluginTriggerActions;

    const auto getPluginInstance = [this]() -> DualViewPlugin* {
        return dynamic_cast<DualViewPlugin*>(Application::core()->getPluginManager().requestViewPlugin(getKind()));
        };

    const auto numberOfDatasets = datasets.count();

    if (numberOfDatasets >= 1 && PluginFactory::areAllDatasetsOfTheSameType(datasets, PointType)) {
        auto pluginTriggerAction = new PluginTriggerAction(const_cast<DualViewPluginFactory*>(this), this, "Dual View", "Dual view visualization", StyledIcon("braille"), [this, getPluginInstance, datasets](PluginTriggerAction& pluginTriggerAction) -> void {
            for (auto dataset : datasets)
                getPluginInstance()->loadData(Datasets({ dataset }));

            });

        pluginTriggerActions << pluginTriggerAction;
    }

    return pluginTriggerActions;
}
