#pragma once

#include <ViewPlugin.h>
#include <AnalysisPlugin.h> // for request tsne plugin

#include <Dataset.h>
#include <PointData/PointData.h>
#include <ClusterData/ClusterData.h>
#include <widgets/DropWidget.h>
#include <actions/ColorMap1DAction.h>
#include <actions/HorizontalToolbarAction.h>

#include <QWidget>

#include "Actions/SettingsAction.h"

/** All plugin related classes are in the ManiVault plugin namespace */
using namespace mv::plugin;

/** Drop widget used in this plugin is located in the ManiVault gui namespace */
using namespace mv::gui;

/** Dataset reference used in this plugin is located in the ManiVault util namespace */
using namespace mv::util;

class ChartWidget;
class ScatterplotWidget;
class EmbeddingLinesWidget;


class DualViewPlugin : public ViewPlugin
{
    Q_OBJECT

public:
    /**
     * Constructor
     * @param factory Pointer to the plugin factory
     */
    DualViewPlugin(const PluginFactory* factory);

    /** Destructor */
    ~DualViewPlugin() override = default;
    
    /** This function is called by the core after the view plugin has been created */
    void init() override;

    void updateThresholdLines();

    // experimental actions
    void reversePointSizeB(bool reversePointSizeB);

    void highlightInputGenes(const QStringList& dimensionNames);


private:
    QString getCurrentEmebeddingDataSetID(mv::Dataset<Points> dataset) const;

    void updateEmbeddingDataA();

    void updateEmbeddingDataB();

    void highlightSelectedEmbeddings(ScatterplotWidget*& widget, mv::Dataset<Points> dataset);

    void updateEmbeddingBColor(); // for now only for embedding B - TODO: rename the function to updateColorAndPointSize 

    void updateEmbeddingAColor(); // for now only for embedding A - TODO: rename the function to updateColorAndPointSize

    void selectPoints(ScatterplotWidget* widget, mv::Dataset<Points> embeddingDataset, const std::vector<mv::Vector2f>& embeddingPositions); // for selection on scatterplot

    void selectPoints(EmbeddingLinesWidget* widget, const std::vector<mv::Vector2f>& embeddingPositions); // for selection on embedding lines

    /** Use the sampler pixel selection tool to sample data points */
    void samplePoints();


    // for embedding lines widget

    void update1DEmbeddingPositions(bool isA); // bool isA: true for embedding A, false for embedding B

    void update1DEmbeddingColors(bool isA); // bool isA: true for embedding A, false for embedding B

    void updateLineConnections();

    void highlightSelectedLines(mv::Dataset<Points> dataset);

    void projectToVerticalAxis(std::vector<mv::Vector2f>& embeddings, float x_value);

    void normalizeYValues(std::vector<Vector2f>& embedding);

    void computeDataRange(); // precompute the range of each column every time the dataset changes, for computing line connections


    
    // for embedding A
    void computeSelectedGeneMeanExpression();

    void sendDataToSampleScope();

    void computeTopCellForEachGene();

protected:

    void embeddingDatasetAChanged(); // if use generic function, many input needed

    void embeddingDatasetBChanged();


public: // Serialization
    /**
    * Load plugin from variant map
    * @param Variant map representation of the plugin
    */
    void fromVariantMap(const QVariantMap& variantMap) override;

    /**
    * Save plugin to variant map
    * @return Variant map representation of the plugin
    */
    QVariantMap toVariantMap() const override;


private:
    ChartWidget*               _chartWidget;                    // WebWidget that sets up the HTML page
    DropWidget*                _embeddingDropWidgetA;        // Widget for drag and drop behavior
    DropWidget*                _embeddingDropWidgetB;        // Widget for drag and drop behavior
   
    mv::Dataset<Points>        _embeddingDatasetA; // 2D embedding
    mv::Dataset<Points>        _embeddingDatasetB; // 2D embedding
    mv::Dataset<Points>        _embeddingSourceDatasetA;
    mv::Dataset<Points>        _embeddingSourceDatasetB;

    mv::Dataset<Points>        _oneDEmbeddingDatasetA; // 1D embedding
    mv::Dataset<Points>        _oneDEmbeddingDatasetB; // 2D embedding

    mv::Dataset<Clusters>       _metaDatasetA; // Dragged in to color embedding A
    mv::Dataset<Clusters>       _metaDatasetB; // Dragged in to color embedding B
    
    // 2D embedding positions
    std::vector<mv::Vector2f>  _embeddingPositionsA;
    std::vector<mv::Vector2f>  _embeddingPositionsB;

    // 1D embedding positions
    std::vector<mv::Vector2f>  _embedding_src;
    std::vector<mv::Vector2f>  _embedding_dst;

    mv::Dataset<Points>        _meanExpressionScalars; 
    std::vector<float>         _selectedGeneMeanExpression; // TODO: remove dataset or vector, only keep one

    std::vector<float>         _connectedCellsPerGene; // number of connected cells for each gene

    mv::Dataset<Clusters>      _topCellForEachGeneDataset; // Dragged in to color embedding A

    float				       _thresholdLines = 0.f;

    std::vector<std::pair<uint32_t, uint32_t>>   _lines; // first: embedding A feature map, second: embedding B point map - both local indices?

    std::vector<float>         _columnMins; // cached for updateLineConnections when threshold changes
    std::vector<float>         _columnRanges; // cached for updateLineConnections when threshold changes


    bool                       _isEmbeddingASelected = false; // if the latest selection is on embedding A

    bool                       _loadingFromProject = false;

    bool                       _reversePointSizeB = false;

protected:

    ScatterplotWidget*        _embeddingWidgetA;
    ScatterplotWidget*        _embeddingWidgetB;
    ColorMap1DAction          _colorMapAction;
    EmbeddingLinesWidget*     _embeddingLinesWidget;

    SettingsAction            _settingsAction;
    HorizontalToolbarAction   _embeddingAToolbarAction;   // Horizontal toolbar for embedding A
    HorizontalToolbarAction   _embeddingBToolbarAction;   // Horizontal toolbar for embedding B

    HorizontalToolbarAction   _embeddingASecondaryToolbarAction;   
    HorizontalToolbarAction   _embeddingBSecondaryToolbarAction;   

    HorizontalToolbarAction   _linesToolbarAction;   // Horizontal toolbar for lines


public:
    /** Get smart pointer to points dataset for point position */
    mv::Dataset<Points>& getEmbeddingDatasetA() { return _embeddingDatasetA; }
    mv::Dataset<Points>& getEmbeddingDatasetB() { return _embeddingDatasetB; }

    mv::Dataset<Clusters>& getMetaDatasetA() { return _metaDatasetA; }
    mv::Dataset<Clusters>& getMetaDatasetB() { return _metaDatasetB; }

public:
    ScatterplotWidget& getEmbeddingWidgetB() { return *_embeddingWidgetB; }
    ScatterplotWidget& getEmbeddingWidgetA() { return *_embeddingWidgetA; }

    SettingsAction& getSettingsAction() { return _settingsAction; }
};

/**
 * Example view plugin factory class
 *
 * Note: Factory does not need to be altered (merely responsible for generating new plugins when requested)
 */
class DualViewPluginFactory : public ViewPluginFactory
{
    Q_INTERFACES(mv::plugin::ViewPluginFactory mv::plugin::PluginFactory)
    Q_OBJECT
    Q_PLUGIN_METADATA(IID   "studio.manivault.DualViewPlugin"
                      FILE  "DualViewPlugin.json")

public:

    /** Default constructor */
    DualViewPluginFactory() {}

    /** Destructor */
    ~DualViewPluginFactory() override {}
    
    /** Get plugin icon */
    QIcon getIcon(const QColor& color = Qt::black) const override;

    /** Creates an instance of the example view plugin */
    ViewPlugin* produce() override;

    /** Returns the data types that are supported by the example view plugin */
    mv::DataTypes supportedDataTypes() const override;

    /**
     * Get plugin trigger actions given \p datasets
     * @param datasets Vector of input datasets
     * @return Vector of plugin trigger actions
     */
    PluginTriggerActions getPluginTriggerActions(const mv::Datasets& datasets) const override;
};
