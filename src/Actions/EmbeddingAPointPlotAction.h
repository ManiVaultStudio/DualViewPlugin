#pragma once
#include <actions/DecimalAction.h>
#include <actions/VerticalGroupAction.h>

#include "PointPlotAction.h"


using namespace mv::gui;

class DualViewPlugin;

/**
 * Point plot action class
 *
 * Action class for point plot settings
 */
class EmbeddingAPointPlotAction : public VerticalGroupAction
{
    Q_OBJECT


public:

    /**
     * Construct with \p parent and \p title
     * @param parent Pointer to parent object
     * @param title Title of the action
     */
    Q_INVOKABLE EmbeddingAPointPlotAction(QObject* parent, const QString& title);



public: // Serialization

    /**
     * Load widget action from variant map
     * @param Variant map representation of the widget action
     */
    void fromVariantMap(const QVariantMap& variantMap) override;

    /**
     * Save widget action to variant map
     * @return Variant map representation of the widget action
     */

    QVariantMap toVariantMap() const override;

public: // Action getters

    //DecimalAction& getPointSizeActionA() { return _pointSizeActionA; }
    //DecimalAction& getPointSizeActionB() { return _pointSizeActionB; }
    //DecimalAction& getPointOpacityActionA() { return _pointOpacityActionA; }
    //DecimalAction& getPointOpacityActionB() { return _pointOpacityActionB; }

    PointPlotAction& getPointPlotAction() { return _pointPlotAction; }


private:
    //DecimalAction           _pointSizeActionA;           /** point size action for embedding A*/
    //DecimalAction           _pointSizeActionB;           /** point size action for embedding B*/
    //DecimalAction           _pointOpacityActionA;        /** point opacity action for embedding A*/
    //DecimalAction           _pointOpacityActionB;        /** point opacity action  for embedding B*/

    PointPlotAction         _pointPlotAction;            /** point plot action*/

};

Q_DECLARE_METATYPE(EmbeddingAPointPlotAction)

inline const auto embeddingAPointPlotActionMetaTypeId = qRegisterMetaType<EmbeddingAPointPlotAction*>("EmbeddingAPointPlotAction");