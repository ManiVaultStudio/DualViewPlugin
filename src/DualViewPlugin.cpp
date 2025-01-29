#include "DualViewPlugin.h"

#include "ChartWidget.h"
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

#include <actions/ViewPluginSamplerAction.h>

#include <chrono>

// for pie chart in tooltip
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Q_PLUGIN_METADATA(IID "studio.manivault.DualViewPlugin")

using namespace mv;

DualViewPlugin::DualViewPlugin(const PluginFactory* factory) :
    ViewPlugin(factory),
    _chartWidget(nullptr),
    _embeddingDropWidgetA(nullptr),
    _embeddingDropWidgetB(nullptr),
    _embeddingDatasetA(),
    _embeddingDatasetB(),
    _embeddingWidgetA(new ScatterplotWidget()),
    _embeddingWidgetB(new ScatterplotWidget()),
    _embeddingLinesWidget(new EmbeddingLinesWidget()),
    _colorMapAction(this, "Color map", "RdYlBu"),
    _settingsAction(this, "SettingsAction"),
    _embeddingAToolbarAction(this, "Toolbar A"),
    _embeddingBToolbarAction(this, "Toolbar B"),
    _linesToolbarAction(this, "Lines Toolbar")
{
    _embeddingAToolbarAction.addAction(&_settingsAction.getEmbeddingAPointPlotAction());
    _embeddingAToolbarAction.addAction(&getSamplerAction());
    _embeddingAToolbarAction.addAction(&_settingsAction.getDimensionSelectionAction());

    _embeddingBToolbarAction.addAction(&_settingsAction.getEmbeddingBPointPlotAction());
    _embeddingBToolbarAction.addAction(&_settingsAction.getReversePointSizeBAction());

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

    getSamplerAction().setViewGeneratorFunction([this](const ViewPluginSamplerAction::SampleContext& toolTipContext) -> QString {
        if (!toolTipContext.contains("ASelected"))
            return {};

        auto ASelected = toolTipContext["ASelected"].toBool();  
   
        // Build the HTML string
        QString html = "<html><head><style>"
            "body { font-family: Arial; }"
            ".legend { list-style: none; padding: 0; }"
            ".legend li { display: flex; align-items: center; margin-bottom: 5px; }"
            ".legend-color { width: 16px; height: 16px; display: inline-block; margin-right: 5px; }"
            "</style></head><body>";
      
        // return the gene symbols 
        assert(_embeddingDatasetA->getNumPoints() == _embeddingSourceDatasetB->getNumDimensions());

        QString outputText;
        QString chartTitle;
		if (ASelected)
		{
			std::vector<QString> dimensionNames = _embeddingSourceDatasetB->getDimensionNames(); // TODO: hardcode, assume embedding A is gene map and embedding B stores all the genes in A

			QStringList geneSymbols;
			for (const auto& globalPointIndex : toolTipContext["GlobalPointIndices"].toList())
			{
				QString geneSymbol = dimensionNames[globalPointIndex.toInt()];
				geneSymbols << geneSymbol;
			}

			outputText = QString("<table style='font-size:14px;'> \
                <tr> \
                    <td><b>Gene Symbols: </b></td> \
                    <td>%2</td> \
                </tr> \
               </table>")
				.arg(geneSymbols.join(", "));

            chartTitle = "<b>Cell proportion (10% highest expression of avg. selected genes)</b>"; // </b> bold tag
		}
		else
		{
            outputText = QString("<p style='font-size:14px;'>%1</p>").arg("Cell embedding is selected");
            chartTitle = "<b>Cell proportion</b>";
		}

        // get current color dataset B name
        QString colorDatasetID = toolTipContext["ColorDatasetID"].toString();
        QString colorDatasetName;
        QString colorDatasetIntro;

        if (colorDatasetID.isEmpty())
        {           
            html += "<p>" + outputText + "</p>";
        }
        else
        {
            colorDatasetName = mv::data().getDataset(colorDatasetID)->getGuiName();
            colorDatasetIntro = QString("Cell embedding coloring by %1").arg(colorDatasetName);

            QVariantList labelsVarList = toolTipContext["labels"].toList();
            QVariantList dataVarList = toolTipContext["data"].toList();
            QVariantList backgroundColorsVarList = toolTipContext["backgroundColors"].toList();

            QStringList labels;
            QVector<double> counts;
            QStringList backgroundColors;

            // Convert labels
            for (const QVariant& labelVar : labelsVarList) {
                labels << labelVar.toString();
            }

            // Convert data to numbers and calculate total sum
            double totalCount = 0.0;
            for (const QVariant& dataVar : dataVarList) {
                double value = dataVar.toDouble();
                counts.append(value);
                totalCount += value;
            }

            // Convert background colors
            for (const QVariant& colorVar : backgroundColorsVarList) {
                QString colorStr = colorVar.toString();
                // Remove any extra quotes around the color codes
                if (colorStr.startsWith("'") && colorStr.endsWith("'")) {
                    colorStr = colorStr.mid(1, colorStr.length() - 2);
                }
                backgroundColors << colorStr;
            }

            html += outputText; // font size al set in the table style
            html += QString("<p style='font-size:14px;'>%1</p>").arg(colorDatasetIntro);
            html += QString("<p style='font-size:14px;'>%1</p>").arg(chartTitle);

            // dimensions for the stacked bar
            int barWidth = 50;   
            int barHeight = 200; 

            int svgWidth = barWidth + 300;

            html += QString("<svg width='%1' height='%2' style='border:none;'>")
                .arg(svgWidth)
                .arg(barHeight);

            if (counts.size() == 1) {
                // if there's only one category, fill the entire bar
                html += QString("<rect x='0' y='0' width='%1' height='%2' fill='%3' stroke='none'></rect>")
                    .arg(barWidth)
                    .arg(barHeight)
                    .arg(backgroundColors[0]);

                double labelX = barWidth + 5;
                double labelY = barHeight / 2.0;

                html += QString(
                    "<text x='%1' y='%2' font-family='Arial' font-size='12' "
                    "fill='black' text-anchor='start' alignment-baseline='middle'>"
                    "%3</text>")
                    .arg(labelX)
                    .arg(labelY)
                    .arg(labels[0]);
            }
            else {
                // if multiple categories, stack them vertically
                double yOffset = 0.0; 

                for (int i = 0; i < counts.size(); ++i) {
                    double proportion = counts[i] / totalCount;
                    double sliceHeight = proportion * barHeight;

                    html += QString("<rect x='0' y='%1' width='%2' height='%3' " "fill='%4' stroke='white' stroke-width='1'></rect>")
                        .arg(yOffset)
                        .arg(barWidth)
                        .arg(sliceHeight)
                        .arg(backgroundColors[i]);

                    // whether to show labels
                    double minPixelsForLabel = 10.0;
                    if (sliceHeight >= minPixelsForLabel)
                    {
                        double labelX = barWidth + 5;
                        double labelY = yOffset + (sliceHeight / 2.0);

                        double percentage = (counts[i] / totalCount) * 100.0;

                        QString labelText = QString("%1 (%2%)").arg(labels[i])
                            .arg(QString::number(percentage, 'f', 1));

                        html += QString(
                            "<text x='%1' y='%2' font-family='Arial' font-size='12' "
                            "fill='black' text-anchor='start' alignment-baseline='middle'>"
                            "%3</text>")
                            .arg(labelX)
                            .arg(labelY)
                            .arg(labelText);
                    }

                    yOffset += sliceHeight;
                }
            }

            html += "</svg>";

        }

        html += "</body></html>";
        
        return html;

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
 
    auto embeddinglayoutA = new QVBoxLayout();
    embeddinglayoutA->addWidget(_embeddingAToolbarAction.createWidget(&getWidget()), 1);   
    embeddinglayoutA->addWidget(_embeddingWidgetA, 100);
    layout->addLayout(embeddinglayoutA, 1);

    // TODO: remove if not using d3 vis
    // Create chart widget and set html contents of webpage 
    //_chartWidget = new ChartWidget(this);// TODO remove if not using d3 vis
    //_chartWidget->setPage(":dual_view/dual_chart/test_bipartite.html", "qrc:/dual_view/dual_chart/");
    //layout->addWidget(_chartWidget, 1);

    auto lineslayout = new QVBoxLayout();
    lineslayout->addWidget(_linesToolbarAction.createWidget(&getWidget()), 1);     
    lineslayout->addWidget(_embeddingLinesWidget, 100);
    layout->addLayout(lineslayout, 1);

    auto embeddinglayoutB = new QVBoxLayout();
    embeddinglayoutB->addWidget(_embeddingBToolbarAction.createWidget(&getWidget()), 1);   
    embeddinglayoutB->addWidget(_embeddingWidgetB, 100);
	layout->addLayout(embeddinglayoutB, 1);

    getWidget().setLayout(layout);
      
    // Update the selection (coming from JS) in core
    //connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passSelectionToCore, this, &DualViewPlugin::publishSelection);

    connect(&_embeddingDatasetA, &Dataset<Points>::changed, this, &DualViewPlugin::embeddingDatasetAChanged);

    connect(&_embeddingDatasetB, &Dataset<Points>::changed, this, &DualViewPlugin::embeddingDatasetBChanged);

    connect(&_embeddingDatasetA, &Dataset<Points>::dataSelectionChanged, this, [this]() {
        if (!_embeddingDatasetA.isValid() || !_embeddingDatasetB.isValid())
			return;

        _isEmbeddingASelected = true;
        highlightSelectedLines(_embeddingDatasetA);
        highlightSelectedEmbeddings(_embeddingWidgetA, _embeddingDatasetA);

        if (_embeddingDatasetA->getSelection<Points>()->indices.size() == 0)
            updateEmbeddingPointSizeB();
        else 
        {
            updateEmbeddingBColor();//if selected in embedding A and coloring/sizing embedding B by the mean expression of the selected genes     
            auto start = std::chrono::high_resolution_clock::now();
            sendDataToSampleScope();
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            qDebug() << "sendDataToSampleScope() took " << duration.count() << "ms";
        }
            
    });

    connect(&_embeddingDatasetB, &Dataset<Points>::dataSelectionChanged, this, [this]() {
        if (!_embeddingDatasetA.isValid() || !_embeddingDatasetB.isValid())
            return;

        _isEmbeddingASelected = false;
        highlightSelectedLines(_embeddingDatasetB);// TODO: this step is the slow part of the interface?
        highlightSelectedEmbeddings(_embeddingWidgetB, _embeddingDatasetB);

        if (_embeddingDatasetB->getSelection<Points>()->indices.size() == 0)
			updateEmbeddingPointSizeA();
		else
        { 
			updateEmbeddingAColor();//if selected in embedding B and coloring/sizing embedding A by the number of connected cells
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
    //qDebug() << "<<<<< update1DEmbeddingColors " << embeddingName;

    // test assigning colors to 1D embedding
    if (_metaDatasetB.isValid() && !isA)
    {
        //qDebug() << "update1DEmbeddingColors B";

        // extract the colors - same as in ColoringAction
        // TODO: avoid code duplication
        // Mapping from local to global indices
        std::vector<std::uint32_t> globalIndices;

        auto positionDataset = _embeddingDatasetB; // TODO:hardcoded to assume only for B

        // Get global indices from the position dataset
        int totalNumPoints = 0;
        if (positionDataset->isDerivedData())
            totalNumPoints = positionDataset->getSourceDataset<Points>()->getFullDataset<Points>()->getNumPoints();
        else
            totalNumPoints = positionDataset->getFullDataset<Points>()->getNumPoints();

        positionDataset->getGlobalIndices(globalIndices);

        // Generate color buffer for global and local colors
        std::vector<Vector3f> globalColors(totalNumPoints);
        std::vector<Vector3f> localColors(positionDataset->getNumPoints());

        // Loop over all clusters and populate global colors
        for (const auto& cluster : _metaDatasetB.get<Clusters>()->getClusters())
            for (const auto& index : cluster.getIndices())
                globalColors[index] = Vector3f(cluster.getColor().redF(), cluster.getColor().greenF(), cluster.getColor().blueF());
        std::int32_t localColorIndex = 0;

        // Loop over all global indices and find the corresponding local color
        for (const auto& globalIndex : globalIndices)
            localColors[localColorIndex++] = globalColors[globalIndex];

        _embeddingLinesWidget->setPointColorB(localColors); // 
    }

    if (_metaDatasetA.isValid() && isA)
    {
        //qDebug() << "update1DEmbeddingColors A";

        // extract the colors - same as in ColoringAction
        // TODO: avoid code duplication
        // 
        // Mapping from local to global indices
        std::vector<std::uint32_t> globalIndices;

        auto positionDataset = _embeddingDatasetA; // TODO:hardcoded to assume only for A

        // Get global indices from the position dataset
        int totalNumPoints = 0;
        if (positionDataset->isDerivedData())
            totalNumPoints = positionDataset->getSourceDataset<Points>()->getFullDataset<Points>()->getNumPoints();
        else
            totalNumPoints = positionDataset->getFullDataset<Points>()->getNumPoints();

        positionDataset->getGlobalIndices(globalIndices);

        // Generate color buffer for global and local colors
        std::vector<Vector3f> globalColors(totalNumPoints);
        std::vector<Vector3f> localColors(positionDataset->getNumPoints());

        // Loop over all clusters and populate global colors
        for (const auto& cluster : _metaDatasetA.get<Clusters>()->getClusters())
            for (const auto& index : cluster.getIndices())
                globalColors[index] = Vector3f(cluster.getColor().redF(), cluster.getColor().greenF(), cluster.getColor().blueF());
        std::int32_t localColorIndex = 0;

        // Loop over all global indices and find the corresponding local color
        for (const auto& globalIndex : globalIndices)
            localColors[localColorIndex++] = globalColors[globalIndex];

        _embeddingLinesWidget->setPointColorA(localColors); // 
    }
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

void DualViewPlugin::normalizeYValues(std::vector<Vector2f>& embedding) 
{
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();

    for (const auto& v : embedding) {
        min_y = std::min(min_y, v.y);
        max_y = std::max(max_y, v.y);
    }

    // Normalize y-values to range (0, 1)
    for (auto& v : embedding) {
        v.y = (v.y - min_y) / (max_y - min_y);
    }
}

void DualViewPlugin::projectToVerticalAxis(std::vector<mv::Vector2f>& embeddings, float x_value) {
    for (auto& point : embeddings) {
        point.x = x_value;  // Set x-coordinate to the specified value
    }
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

    // precompute the data range
    computeDataRange();
   
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

void DualViewPlugin::computeDataRange()
{
    if (!_embeddingSourceDatasetB.isValid())
    {
        qDebug() << "Datasets B should be valid to compute data range";
        return;
    }

    // Assume gene embedding is t-SNE
    // FIXME: should check if the number of points in A is the same as the number of dimensions in B

    // define lines - assume embedding A is dimension embedding, embedding B is observation embedding
    int numDimensions = _embeddingSourceDatasetB->getNumDimensions(); // Assume the number of points in A is the same as the number of dimensions in B
    int numDimensionsFull = _embeddingSourceDatasetB->getNumDimensions();
    int numPoints = _embeddingSourceDatasetB->getNumPoints(); // num of points in source dataset B
    int numPointsLocal = _embeddingDatasetB->getNumPoints(); // num of points in the embedding B

    std::vector<std::uint32_t> localGlobalIndicesB;
    _embeddingSourceDatasetB->getGlobalIndices(localGlobalIndicesB);

    _columnMins.resize(numDimensions);
    _columnRanges.resize(numDimensions);

    // Find the minimum value in each column - attention! this is the minimum of the subset (if HSNE)
#pragma omp parallel for
    for (int dimIdx = 0; dimIdx < numDimensions; dimIdx++)
    {

        float minValue = std::numeric_limits<float>::max();
        float maxValue = std::numeric_limits<float>::lowest();

        for (int i = 0; i < numPoints; i++)
        {
            float val = _embeddingSourceDatasetB->getValueAt(i * numDimensionsFull + dimIdx);
            if (val < minValue)
                minValue = val;
            if (val > maxValue)
                maxValue = val;
        }

        _columnMins[dimIdx] = minValue;
        _columnRanges[dimIdx] = maxValue - minValue;
    }

    qDebug() << "DualViewPlugin:: data range computed";

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

void DualViewPlugin::highlightInputGenes()
{
    int dimension = _settingsAction.getDimensionSelectionAction().getDimensionAction().getCurrentDimensionIndex();

    if (dimension < 0)
		return;

    std::vector<char> highlights(_embeddingPositionsA.size(), 0);
    highlights[dimension] = 1;

    _embeddingWidgetA->setHighlights(highlights, 1);

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

void DualViewPlugin::sendDataToSampleScope() {
    // TODO: temp code to deal with the sampler action - currently hard coded to A - generalize this function for B 

	if (getSamplerAction().getEnabledAction().isChecked() == false)
		return;

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

        QVariantList globalPointIndices;

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
        if (!_metaDatasetB.isValid())
        {
            //qDebug() << "metaDatasetB is not valid";

            getSamplerAction().setSampleContext({
                { "ASelected", _isEmbeddingASelected},
                { "ColorDatasetID", _settingsAction.getColoringActionB().getCurrentColorDataset().getDatasetId() },
                { "GlobalPointIndices", globalPointIndices },
                });
            return;
        }

        auto start2 = std::chrono::high_resolution_clock::now();
        // load the meta data categories
        QVector<Cluster> metadata = _metaDatasetB->getClusters();

        int numPointsB = _embeddingSourceDatasetB->getNumPoints();

        //qDebug() << "numPointsB size " << numPointsB;
        //qDebug() << "_selectedGeneMeanExpression size " << _selectedGeneMeanExpression.size();

        // first, sort the indices based on the selected gene expression
        std::vector<std::pair<float, int>> rankedCells;
        rankedCells.reserve(numPointsB);

        for (int i = 0; i < numPointsB; i++)
        {
            rankedCells.push_back(std::make_pair(_selectedGeneMeanExpression[i], i));
        }

        std::sort(rankedCells.begin(), rankedCells.end(), std::greater<std::pair<float, int>>());
        //qDebug() << "rankedCells size " << rankedCells.size(); // local indices

        // count the number of cells in each meta data category for the top 10% cells in rankedCells
        int top10 = numPointsB / 10;

        // count the number of cells that have expression more than 0
        int count0 = 0;
        for (int i = 0; i < numPointsB; i++)
        {
            if (_selectedGeneMeanExpression[i] > 0)
                count0++;
        }


        // if the number of cells with expression more than 0 is less than 10%, use that number
        int numCellsCounted = (count0 > top10) ? top10 : count0;

        // TODO: WORKINPROGRESS here....
        // TEMP: count the number of cells in each meta data category for the top 10% cells in rankedCells
        // FIXME: this does not work for hsne - solved

        std::unordered_map<int, int> cellToClusterMap; // maps global cell index to cluster index
        for (int clusterIndex = 0; clusterIndex < metadata.size(); ++clusterIndex) {
            const auto& ptIndices = metadata[clusterIndex].getIndices();
            for (int cellIndex : ptIndices) {
                cellToClusterMap[cellIndex] = clusterIndex;
            }
        }
        //qDebug() << "cellToClusterMap size " << cellToClusterMap.size(); // global indices
        auto end2 = std::chrono::high_resolution_clock::now();
        auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2);
        qDebug() << "sendDataTosampleScope 2 took " << duration2.count() << "ms";

        // mapping from local to global indices
        std::vector<std::uint32_t> localGlobalIndicesB;
        _embeddingSourceDatasetB->getGlobalIndices(localGlobalIndicesB);

        std::unordered_map<int, int> clusterCount; // maps cluster index to count
        for (int i = 0; i < numCellsCounted; ++i) {
            int localCellIndex = rankedCells[i].second;
            int globalCellIndex = localGlobalIndicesB[localCellIndex];
            //qDebug() << "globalCellIndex " << globalCellIndex << "localCellIndex " << localCellIndex;
            if (cellToClusterMap.find(globalCellIndex) != cellToClusterMap.end()) {
                int clusterIndex = cellToClusterMap[globalCellIndex];
                clusterCount[clusterIndex]++;
            }
        }

        std::vector<std::pair<int, int>> sortedClusterCount(clusterCount.begin(), clusterCount.end());
        std::sort(sortedClusterCount.begin(), sortedClusterCount.end(), [](const auto& a, const auto& b) { return a.second > b.second; });


        // Prepare data arrays for labels, data, and background colors
        QStringList labels;
        QStringList data;
        QStringList backgroundColors;

        for (const auto& [clusterIndex, count] : sortedClusterCount) {
            QString clusterName = metadata[clusterIndex].getName();
            labels << clusterName;
            data << QString::number(count);

            QString color = metadata[clusterIndex].getColor().name();
            backgroundColors << color;
        }


        //qDebug() << "Prepared for setSampleContext " << labels << data << backgroundColors;

        auto start4 = std::chrono::high_resolution_clock::now();
        getSamplerAction().setSampleContext({
            { "ASelected", _isEmbeddingASelected},
            { "ColorDatasetID", _settingsAction.getColoringActionB().getCurrentColorDataset().getDatasetId() },
            { "GlobalPointIndices", globalPointIndices },
            { "labels", labels},// for pie chart
            { "data", data},// for pie chart
            { "backgroundColors", backgroundColors}// for pie chart
            });

        auto end4 = std::chrono::high_resolution_clock::now();
        auto duration4 = std::chrono::duration_cast<std::chrono::milliseconds>(end4 - start4);
        qDebug() << "sendDataTosampleScope 4 took " << duration4.count() << "ms";

    }
    else
    {
        // B selected, send cell types to samplerAction
        // if no metadata B avaliable, only send gene ids
        if (!_metaDatasetB.isValid())
        {
            //qDebug() << "metaDatasetB is not valid";

            getSamplerAction().setSampleContext({
                { "ASelected", _isEmbeddingASelected},
                { "ColorDatasetID", _settingsAction.getColoringActionB().getCurrentColorDataset().getDatasetId() },
                });
            return;
        }

        QVector<Cluster> metadata = _metaDatasetB->getClusters();

        auto selection = _embeddingDatasetB->getSelection<Points>();

        std::vector<std::uint32_t> localGlobalIndices;
        _embeddingDatasetB->getGlobalIndices(localGlobalIndices);

        std::vector<std::uint32_t> sampledPoints;
        sampledPoints.reserve(_embeddingPositionsB.size());

        for (auto selectionIndex : selection->indices)
            sampledPoints.push_back(selectionIndex);

        std::unordered_map<int, int> cellToClusterMap; // maps global cell index to cluster index
        for (int clusterIndex = 0; clusterIndex < metadata.size(); ++clusterIndex) {
            const auto& ptIndices = metadata[clusterIndex].getIndices();
            for (int cellIndex : ptIndices) {
                cellToClusterMap[cellIndex] = clusterIndex;
            }
        }

        std::unordered_map<int, int> clusterCount; // maps cluster index to count
        for (int i = 0; i < sampledPoints.size(); ++i) {
            int globalCellIndex = sampledPoints[i];
            if (cellToClusterMap.find(globalCellIndex) != cellToClusterMap.end()) {
                int clusterIndex = cellToClusterMap[globalCellIndex];
                clusterCount[clusterIndex]++;
            }
        }

        std::vector<std::pair<int, int>> sortedClusterCount(clusterCount.begin(), clusterCount.end());
        std::sort(sortedClusterCount.begin(), sortedClusterCount.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

        // Prepare data arrays for labels, data, and background colors
        QStringList labels;
        QStringList data;
        QStringList backgroundColors;

        for (const auto& [clusterIndex, count] : sortedClusterCount) {
            QString clusterName = metadata[clusterIndex].getName();
            labels << clusterName;
            data << QString::number(count);

            QString color = metadata[clusterIndex].getColor().name();
            backgroundColors << color;
        }


        //qDebug() << "Prepared for setSampleContext " << labels << data << backgroundColors;

        auto start4 = std::chrono::high_resolution_clock::now();
        getSamplerAction().setSampleContext({
            { "ASelected", _isEmbeddingASelected},
            { "ColorDatasetID", _settingsAction.getColoringActionB().getCurrentColorDataset().getDatasetId() },
            { "labels", labels},// for proportion chart
            { "data", data},// for proportion chart
            { "backgroundColors", backgroundColors}// for proportion chart
            });

        auto end4 = std::chrono::high_resolution_clock::now();
        auto duration4 = std::chrono::duration_cast<std::chrono::milliseconds>(end4 - start4);
        qDebug() << "sendDataTosampleScope 4 took " << duration4.count() << "ms";

    }
	
   
}

void DualViewPlugin::updateEmbeddingBColor()
{
    // TODO: this is actually updating size instead of color
    // For now, only update the color of embedding B when embedding A is selected
    if (!_embeddingDatasetA.isValid() || !_embeddingDatasetB.isValid())
        return;
    
    auto start1 = std::chrono::high_resolution_clock::now();
    computeSelectedGeneMeanExpression();
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1);
    qDebug() << "computeSelectedGeneMeanExpression() took " << duration1.count() << "ms";

    if (_selectedGeneMeanExpression.size() != _embeddingDatasetB->getNumPoints())
    {
        qDebug() << "Warning! selectedGeneMeanExpression size " << _selectedGeneMeanExpression.size() << "is not equal to the number of points in embedding B" << _embeddingDatasetB->getNumPoints();
        return;
    }

	// set color scalars
	//_embeddingWidgetB->setScalars(_selectedGeneMeanExpression);
	//_embeddingWidgetB->setScalarEffect(PointEffect::Color);
    
	//set size scalars
	auto min_max = std::minmax_element(_selectedGeneMeanExpression.begin(), _selectedGeneMeanExpression.end());
	float min_val = *min_max.first;
	float max_val = *min_max.second;
    float range = max_val - min_val;
    if (range == 0)
    {
		range = 1;
	}
	float ptSize = _settingsAction.getEmbeddingBPointPlotAction().getPointSizeActionB().getValue();

    std::vector<float> selectedGeneMeanExpression(_selectedGeneMeanExpression.size());

    // In case the user wants to reverse the point size
    if (!_reversePointSizeB)
    {
#pragma omp parallel for
        for (int i = 0; i < _selectedGeneMeanExpression.size(); i++)
        {
            selectedGeneMeanExpression[i] = ((_selectedGeneMeanExpression[i] - min_val) / range) * ptSize; // TODO: hardcoded factor?
        }
    }
    else
    {
#pragma omp parallel for
        for (int i = 0; i < _selectedGeneMeanExpression.size(); i++)
        {
			selectedGeneMeanExpression[i] = (1 - (_selectedGeneMeanExpression[i] - min_val) / range) * ptSize;
		}
	}

	_embeddingWidgetB->setPointSizeScalars(selectedGeneMeanExpression);
}

void DualViewPlugin::reversePointSizeB(bool reversePointSizeB)
{

    _reversePointSizeB = reversePointSizeB;

    if (!_embeddingDatasetB.isValid())
		return;

    // TODO: if this function to be kept
    // this is same code snippet as in updateEmbeddingBColor(), put in one function
    auto min_max = std::minmax_element(_selectedGeneMeanExpression.begin(), _selectedGeneMeanExpression.end());
    float min_val = *min_max.first;
    float max_val = *min_max.second;
    float range = max_val - min_val;
    if (range == 0)
    {
		range = 1;
	}
    float ptSize = _settingsAction.getEmbeddingBPointPlotAction().getPointSizeActionB().getValue();

    std::vector<float> selectedGeneMeanExpression(_selectedGeneMeanExpression.size());
    // In case the user wants to reverse the point size
    if (!_reversePointSizeB)
    {
#pragma omp parallel for
        for (int i = 0; i < _selectedGeneMeanExpression.size(); i++)
        {
            selectedGeneMeanExpression[i] = ((_selectedGeneMeanExpression[i] - min_val) / range) * ptSize; // TODO: hardcoded factor?

        }
    }
    else
    {
#pragma omp parallel for
        for (int i = 0; i < _selectedGeneMeanExpression.size(); i++)
        {
            selectedGeneMeanExpression[i] = (1 - (_selectedGeneMeanExpression[i] - min_val) / range) * ptSize;
        }
    }

    _embeddingWidgetB->setPointSizeScalars(selectedGeneMeanExpression);
}

void DualViewPlugin::updateEmbeddingAColor()
{
    // TODO: this is actually updating size instead of color
    // update the color of embedding A when embedding B is selected
    if (!_embeddingDatasetA.isValid() || !_embeddingDatasetB.isValid())
        return;

    int numPointsA = _embeddingDatasetA->getNumPoints();

    auto selection = _embeddingDatasetB->getSelection<Points>();

    if (selection->indices.size() == 0)
    {
        std::vector<float> connectedCellsPerGene(numPointsA, 1.0f);
        _embeddingWidgetA->setScalars(connectedCellsPerGene);
		return;
	}

    std::vector<bool> selected; // bool of selected in the current scale
    std::vector<char> highlights;
    _embeddingDatasetB->selectedLocalIndices(selection->indices, selected);
    qDebug() << "DualViewPlugin: " << selection->indices.size() << " points in B selected";

    // get how many connected points in B per point in A
    std::vector<float> connectedCellsPerGene(numPointsA, 0.0f);
#pragma omp parallel for
    for (int i = 0; i < _lines.size(); i++)
    {
        int pointB = _lines[i].second;
        if (pointB < selected.size() && selected[pointB])
        {
#pragma omp atomic
            connectedCellsPerGene[_lines[i].first] += 1.0f;
        }
    }
    //qDebug() << "connectedCellsPerGene.size() = " << connectedCellsPerGene.size();

        // set color scalars
    //_embeddingWidgetA->setScalars(connectedCellsPerGene);
    //_embeddingWidgetA->setScalarEffect(PointEffect::Color);

    //set size scalars
    auto min_max = std::minmax_element(connectedCellsPerGene.begin(), connectedCellsPerGene.end());
    float min_val = *min_max.first;
    float max_val = *min_max.second;
    //qDebug() << "min_val" << min_val << "max_val" << max_val;
    float ptSize = _settingsAction.getEmbeddingAPointPlotAction().getPointSizeActionA().getValue();

#pragma omp parallel for
    for (int i = 0; i < connectedCellsPerGene.size(); i++)
    {
        connectedCellsPerGene[i] = ((connectedCellsPerGene[i] - min_val) / (max_val - min_val)) * ptSize * 3; // TODO: hardcoded factor 10
    }
    //qDebug() << "scalars A computed";
    _embeddingWidgetA->setPointSizeScalars(connectedCellsPerGene);


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

}

void DualViewPlugin::samplePoints() 
{
    // for now only for embedding A
    auto& samplerPixelSelectionTool = _embeddingWidgetA->getSamplerPixelSelectionTool();

    if (!_embeddingDatasetA.isValid())
        return;

    auto selectionAreaImage = samplerPixelSelectionTool.getAreaPixmap().toImage();

    auto selectionSet = _embeddingDatasetA->getSelection<Points>();

    std::vector<std::uint32_t> targetSelectionIndices;

    targetSelectionIndices.reserve(_embeddingDatasetA->getNumPoints());

    std::vector<std::uint32_t> localGlobalIndices;

    _embeddingDatasetA->getGlobalIndices(localGlobalIndices);

    const auto dataBounds = _embeddingWidgetA->getBounds();
    const auto width = selectionAreaImage.width();
    const auto height = selectionAreaImage.height();
    const auto size = width < height ? width : height;
    const auto uvOffset = QPoint((selectionAreaImage.width() - size) / 2.0f, (selectionAreaImage.height() - size) / 2.0f);

    QPointF pointUvNormalized;

    QPoint  pointUv, mouseUv = _embeddingWidgetA->mapFromGlobal(QCursor::pos());

    std::vector<char> focusHighlights(_embeddingPositionsA.size());

    std::vector<std::pair<float, std::uint32_t>> sampledPoints;

    for (std::uint32_t localPointIndex = 0; localPointIndex < _embeddingPositionsA.size(); localPointIndex++) {
        pointUvNormalized = QPointF((_embeddingPositionsA[localPointIndex].x - dataBounds.getLeft()) / dataBounds.getWidth(), (dataBounds.getTop() - _embeddingPositionsA[localPointIndex].y) / dataBounds.getHeight());
        pointUv = uvOffset + QPoint(pointUvNormalized.x() * size, pointUvNormalized.y() * size);

        if (pointUv.x() >= selectionAreaImage.width() || pointUv.x() < 0 || pointUv.y() >= selectionAreaImage.height() || pointUv.y() < 0)
            continue;

        if (selectionAreaImage.pixelColor(pointUv).alpha() > 0) {
            const auto sample = std::pair<float, std::uint32_t>((QVector2D(mouseUv) - QVector2D(pointUv)).length(), localPointIndex);

            sampledPoints.emplace_back(sample);

            targetSelectionIndices.push_back(localGlobalIndices[localPointIndex]); // test for connecting sampled points as selected points
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

    //if (getSamplerAction().getHighlightFocusedElementsAction().isChecked())
        //const_cast<PointRenderer&>(_embeddingWidgetA->getPointRenderer()).setFocusHighlights(focusHighlights, static_cast<std::int32_t>(focusHighlights.size()));

    //_embeddingWidgetA->update(); // repeated when dataselection changed

    //auto& coloringAction = _settingsAction.getColoringAction();

    getSamplerAction().setSampleContext({
        { "ColorDatasetID", _settingsAction.getColoringActionB().getCurrentColorDataset().getDatasetId() },
        { "LocalPointIndices", localPointIndices },
        { "GlobalPointIndices", globalPointIndices },
        });

    // connect sampled points as selected points
    _embeddingDatasetA->setSelectionIndices(targetSelectionIndices);

    events().notifyDatasetDataSelectionChanged(_embeddingDatasetA->getSourceDataset<Points>());

}

void DualViewPlugin::selectPoints(ScatterplotWidget* widget, mv::Dataset<Points> embeddingDataset, const std::vector<mv::Vector2f>& embeddingPositions)
{
    // Only proceed with a valid points position dataset and when the pixel selection tool is active
    if (!embeddingDataset.isValid() || !widget->getPixelSelectionTool().isActive() )
        return;

    //qDebug() << _positionDataset->getGuiName() << "selectPoints";

    // Get binary selection area image from the pixel selection tool
    auto selectionAreaImage = widget->getPixelSelectionTool().getAreaPixmap().toImage();

    // Get smart pointer to the position selection dataset
    auto selectionSet = embeddingDataset->getSelection<Points>();

    // Create vector for target selection indices
    std::vector<std::uint32_t> targetSelectionIndices;

    // Reserve space for the indices
    targetSelectionIndices.reserve(embeddingDataset->getNumPoints());

    // Mapping from local to global indices
    std::vector<std::uint32_t> localGlobalIndices;

    // Get global indices from the position dataset
    embeddingDataset->getGlobalIndices(localGlobalIndices);

    const auto dataBounds = widget->getBounds();
    const auto width = selectionAreaImage.width();
    const auto height = selectionAreaImage.height();
    const auto size = width < height ? width : height;

    //qDebug() << "Selection area size:" << width << height << size;
    //qDebug() << "Data bounds top" << dataBounds.getTop() << "bottom" << dataBounds.getBottom() << "left" << dataBounds.getLeft() << "right" << dataBounds.getRight();
    //qDebug() << "Data bounds width" << dataBounds.getWidth() << "height" << dataBounds.getHeight();


    // Loop over all points and establish whether they are selected or not
    for (std::uint32_t i = 0; i < embeddingPositions.size(); i++) {
        const auto uvNormalized = QPointF((embeddingPositions[i].x - dataBounds.getLeft()) / dataBounds.getWidth(), (dataBounds.getTop() - embeddingPositions[i].y) / dataBounds.getHeight());
        const auto uvOffset = QPoint((selectionAreaImage.width() - size) / 2.0f, (selectionAreaImage.height() - size) / 2.0f);
        const auto uv = uvOffset + QPoint(uvNormalized.x() * size, uvNormalized.y() * size);

        // Add point if the corresponding pixel selection is on
        if (selectionAreaImage.pixelColor(uv).alpha() > 0)
            targetSelectionIndices.push_back(localGlobalIndices[i]);


        /*if (i / 110 == 1)
        {
            qDebug() << "embeddingPositions:" << i << "x:" << embeddingPositions[i].x << "y:" << embeddingPositions[i].y << "uvNormalized:" << uvNormalized << "uvOffset" << uvOffset <<"uv:" << uv;
        }*/
    }

    // Selection should be subtracted when the selection process was aborted by the user (e.g. by pressing the escape key)
    const auto selectionModifier = widget->getPixelSelectionTool().isAborted() ? PixelSelectionModifierType::Subtract : widget->getPixelSelectionTool().getModifier();

    switch (selectionModifier)
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

        default:
            break;
        }

        // Convert set back to vector
        targetSelectionIndices = std::vector<std::uint32_t>(set.begin(), set.end());

        break;
    }

    default:
        break;
    }

    embeddingDataset->setSelectionIndices(targetSelectionIndices);

    events().notifyDatasetDataSelectionChanged(embeddingDataset->getSourceDataset<Points>());
}

void DualViewPlugin::selectPoints(EmbeddingLinesWidget* widget, const std::vector<mv::Vector2f>& embeddingPositions)
{
    // Only proceed with a valid points position dataset and when the pixel selection tool is active
    if (!_oneDEmbeddingDatasetA.isValid() || !_oneDEmbeddingDatasetB.isValid() || !widget->getPixelSelectionTool().isActive())
    {
        qDebug() << "DualViewPlugin: selectPoints() - invalid 1D dataset or pixel selection tool is not active";
        return;
    }
        

    // Get binary selection area image from the pixel selection tool
    auto selectionAreaImage = widget->getPixelSelectionTool().getAreaPixmap().toImage();

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

    const auto dataBounds = widget->getBounds();
    const auto width = selectionAreaImage.width();
    const auto height = selectionAreaImage.height();
    const auto size = width < height ? width : height;

    //qDebug() << "Selection area size:" << width << height << size;
    //qDebug() << "Data bounds top" << dataBounds.getTop() << "bottom" << dataBounds.getBottom() << "left" << dataBounds.getLeft() << "right" << dataBounds.getRight();
    //qDebug() << "Data bounds width" << dataBounds.getWidth() << "height" << dataBounds.getHeight();

    std::size_t datasetASize = _oneDEmbeddingDatasetA->getNumPoints();
    std::size_t datasetBSize = _oneDEmbeddingDatasetB->getNumPoints();

    // Loop over all points and establish whether they are selected or not
    for (std::uint32_t i = 0; i < embeddingPositions.size(); i++) {
        const auto uvNormalized = QPointF((embeddingPositions[i].x - dataBounds.getLeft()) / dataBounds.getWidth(), (dataBounds.getTop() - embeddingPositions[i].y) / dataBounds.getHeight());
        const auto uvOffset = QPoint((selectionAreaImage.width() - size) / 2.0f, (selectionAreaImage.height() - size) / 2.0f);
        const auto uv = uvOffset + QPoint(uvNormalized.x() * size, uvNormalized.y() * size);

        // Add point if the corresponding pixel selection is on
        if (i < datasetASize) {
            if (selectionAreaImage.pixelColor(uv).alpha() > 0)
                targetSelectionIndicesA.push_back(localGlobalIndicesA[i]);
        }
        else {
            std::size_t datasetBIndex = i - datasetASize;
            if (selectionAreaImage.pixelColor(uv).alpha() > 0)
                targetSelectionIndicesB.push_back(localGlobalIndicesB[datasetBIndex]);
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

void DualViewPlugin::computeSelectedGeneMeanExpression()
{
    //qDebug() << "DualViewPlugin: computeSelectedGeneMeanExpression()";

    auto geneSelection = _embeddingDatasetA->getSelection<Points>();

    if (geneSelection->indices.size() == 0)
    {
        qDebug() << "DualViewPlugin: selected 0 genes";
        return;
    }
		
    if (!_embeddingSourceDatasetB.isValid())
    {
        qDebug() << "DualViewPlugin: cell embedding source dataset is not valid";
        return;
    }		

    int numPointsB = _embeddingSourceDatasetB->getNumPoints();
    int numDimensionsB = _embeddingSourceDatasetB->getNumDimensions();
    int numSelectedGenes = geneSelection->indices.size();
    int numPointsEmbeddingB = _embeddingDatasetB->getNumPoints();
    qDebug() << "DualViewPlugin: " << numSelectedGenes << " points in A selected";

    // Output a dataset to color the spatial map by the selected gene avg. expression - always the same size as the full dataset
    mv::Dataset<Points> fullDatasetB;
    if (_embeddingSourceDatasetB->isDerivedData())
    {
        fullDatasetB = _embeddingSourceDatasetB->getSourceDataset<Points>()->getFullDataset<Points>();
    }     
    else
    {   
        fullDatasetB = _embeddingSourceDatasetB->getFullDataset<Points>();
    }
    int totalNumPoints = fullDatasetB->getNumPoints();
    //qDebug() << "totalNumPoints" << totalNumPoints << "fullDatasetB" << fullDatasetB->getGuiName();
        
    std::vector<float> selectedGeneMeanExpressionFull(totalNumPoints, 0.0f);
    //qDebug() << "avgGeneExpressionScalars size: " << selectedGeneMeanExpressionFull.size();

#pragma omp parallel for  
    for (int j = 0; j < totalNumPoints; j++)
    {
        float sum = 0.0f;
        for (int i = 0; i < numSelectedGenes; i++)
        {
            int geneIndex = geneSelection->indices[i];
            sum += fullDatasetB->getValueAt(j * numDimensionsB + geneIndex); 
        }
        selectedGeneMeanExpressionFull[j] = sum / numSelectedGenes;
    }

    // extract _selectedGeneMeanExpression from selectedGeneMeanExpressionFull
    _selectedGeneMeanExpression.clear();
    _selectedGeneMeanExpression.resize(numPointsB, 0.0f);

    std::vector<std::uint32_t> localGlobalIndicesB;
    _embeddingSourceDatasetB->getGlobalIndices(localGlobalIndicesB);
    
    for (int i = 0; i < numPointsB; i++)
    {
		_selectedGeneMeanExpression[i] = selectedGeneMeanExpressionFull[localGlobalIndicesB[i]];
	}

    if (!_meanExpressionScalars.isValid())
    {
        _meanExpressionScalars = mv::data().createDataset<Points>("Points", "ExprScalars");
        events().notifyDatasetAdded(_meanExpressionScalars);
    }

    _meanExpressionScalars->setData<float>(selectedGeneMeanExpressionFull.data(), selectedGeneMeanExpressionFull.size(), 1);
    events().notifyDatasetDataChanged(_meanExpressionScalars);

    //qDebug() << "_meanExpressionScalars->getNumDimensions" << _meanExpressionScalars->getNumDimensions();
    //qDebug() << "_meanExpressionScalars->getNumPoints" << _meanExpressionScalars->getNumPoints();
}

QString DualViewPlugin::getCurrentEmebeddingDataSetID(mv::Dataset<Points> dataset) const
{
    if (dataset.isValid())
        return dataset->getId();
    else
        return QString{};
}

void DualViewPlugin::updateEmbeddingPointSizeA()
{
    float size = _settingsAction.getEmbeddingAPointPlotAction().getPointSizeActionA().getValue();
    std::vector<float> ptSizeScalars(_embeddingPositionsA.size(), size);
    _embeddingWidgetA->setPointSizeScalars(ptSizeScalars);
}

void DualViewPlugin::updateEmbeddingOpacityA()
{
    float opacity = _settingsAction.getEmbeddingAPointPlotAction().getPointOpacityActionA().getValue();
    std::vector<float> ptOpacityScalars(_embeddingPositionsA.size(), opacity);
    _embeddingWidgetA->setPointOpacityScalars(ptOpacityScalars);
}

void DualViewPlugin::updateEmbeddingPointSizeB()
{
    float size = _settingsAction.getEmbeddingBPointPlotAction().getPointSizeActionB().getValue();
    std::vector<float> ptSizeScalars(_embeddingPositionsB.size(), size);
    _embeddingWidgetB->setPointSizeScalars(ptSizeScalars);
}

void DualViewPlugin::updateEmbeddingOpacityB()
{
    float opacity = _settingsAction.getEmbeddingBPointPlotAction().getPointOpacityActionB().getValue();
    std::vector<float> ptOpacityScalars(_embeddingPositionsB.size(), opacity);
    _embeddingWidgetB->setPointOpacityScalars(ptOpacityScalars);
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


    _loadingFromProject = false;

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

	return variantMap;
}


// =============================================================================
// Plugin Factory 
// =============================================================================

QIcon DualViewPluginFactory::getIcon(const QColor& color /*= Qt::black*/) const
{
    return mv::Application::getIconFont("FontAwesome").getIcon("bullseye", color);
}

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
        return dynamic_cast<DualViewPlugin*>(plugins().requestViewPlugin(getKind()));
    };

    const auto numberOfDatasets = datasets.count();

    if (numberOfDatasets >= 1 && PluginFactory::areAllDatasetsOfTheSameType(datasets, PointType)) {
        auto pluginTriggerAction = new PluginTriggerAction(const_cast<DualViewPluginFactory*>(this), this, "Dual View", "Dual view visualization", getIcon(), [this, getPluginInstance, datasets](PluginTriggerAction& pluginTriggerAction) -> void {
            for (auto dataset : datasets)
                getPluginInstance()->loadData(Datasets({ dataset }));

        });

        pluginTriggerActions << pluginTriggerAction;
    }

    return pluginTriggerActions;
}
