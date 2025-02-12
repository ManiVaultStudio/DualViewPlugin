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

    PointPlotAction& getPointPlotAction() { return _pointPlotActionA; }


private:
    PointPlotAction         _pointPlotActionA;            /** point plot action*/

};

Q_DECLARE_METATYPE(EmbeddingAPointPlotAction)

inline const auto embeddingAPointPlotActionMetaTypeId = qRegisterMetaType<EmbeddingAPointPlotAction*>("EmbeddingAPointPlotAction");