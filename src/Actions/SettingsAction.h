#pragma once

#include <actions/GroupAction.h>
#include <actions/DecimalAction.h>
#include <actions/ToggleAction.h>

#include "EmbeddingAPointPlotAction.h"

//#include "PointPlotAction.h"


#include "EmbeddingBPointPlotAction.h"
#include "ColoringActionB.h"
#include "ColoringActionA.h" // TODO: combine coloring actions for embedding A and B
#include "SelectionAction.h"
#include "LoadedDatasetsAction.h"

#include "DimensionSelectionAction.h"

//#include "ClusteringAction.h"

//#include "ExportAction.h"
//#include "MiscellaneousAction.h"
//#include "PositionAction.h"
//#include "RenderModeAction.h"
//#include "SelectionAction.h"
//#include "SubsetAction.h"

using namespace mv::gui;

class DualViewPlugin;

/**
 * Settings action class
 *
 * Action class for configuring settings
 *
 * @author Thomas Kroes
 */
class SettingsAction : public GroupAction
{
public:
    
    /**
     * Construct with \p parent object and \p title
     * @param parent Pointer to parent object
     * @param title Title
     */
    Q_INVOKABLE SettingsAction(QObject* parent, const QString& title);

    /**
     * Get action context menu
     * @return Pointer to menu
     */
    QMenu* getContextMenu();

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

public: // Action getters
    
    EmbeddingAPointPlotAction& getEmbeddingAPointPlotAction() { return _embeddingAPointPlotAction; }
    //PointPlotAction& getPointPlotAction() { return _pointPlotAction; }

    EmbeddingBPointPlotAction& getEmbeddingBPointPlotAction() { return _embeddingBPointPlotAction; }
    DecimalAction& getThresholdLinesAction() { return _thresholdLinesAction; }
    ColoringActionB& getColoringActionB() { return _coloringActionB; }
    ColoringActionA& getColoringActionA() { return _coloringActionA; }
    SelectionAction& getSelectionAction() { return _selectionAction; }

    // experimental actions
    ToggleAction& getReversePointSizeBAction() { return _reversePointSizeBAction; }
    DimensionSelectionAction& getDimensionSelectionAction() { return _dimensionSelectionAction; }
    DecimalAction& getThresholdLinesActionVariance() { return _thresholdLinesActionVariance; }
    

protected:
    DualViewPlugin*                   _dualViewPlugin;         /** Pointer to dual view plugin */

    LoadedDatasetsAction			  _currentDatasetsAction;    /** Action for managing loaded datasets */
    EmbeddingAPointPlotAction         _embeddingAPointPlotAction;           /** Action for configuring point plots */
    //PointPlotAction 				  _pointPlotAction;           /** Action for configuring point plots */

    EmbeddingBPointPlotAction         _embeddingBPointPlotAction;           /** Action for configuring point plots */
    DecimalAction                     _thresholdLinesAction;      
    ColoringActionB                   _coloringActionB;            /** Action for configuring point coloring - for embedding B*/
    ColoringActionA                   _coloringActionA;           /** Action for configuring point coloring - for embedding A*/
    SelectionAction                   _selectionAction;           /** Action for configuring selection */

    // experimental actions
    ToggleAction                      _reversePointSizeBAction;
    DimensionSelectionAction          _dimensionSelectionAction;
    DecimalAction					  _thresholdLinesActionVariance; // experimental, filter by variance
};