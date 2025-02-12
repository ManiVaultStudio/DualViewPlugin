#pragma once
#include <actions/DecimalAction.h>
#include <actions/VerticalGroupAction.h>

#include "PointPlotActionB.h"

using namespace mv::gui;

class DualViewPlugin;

/**
 * Point plot action class
 *
 * Action class for point plot settings
 */
class EmbeddingBPointPlotAction : public VerticalGroupAction
{
    Q_OBJECT


public:

    /**
     * Construct with \p parent and \p title
     * @param parent Pointer to parent object
     * @param title Title of the action
     */
    Q_INVOKABLE EmbeddingBPointPlotAction(QObject* parent, const QString& title);



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

    PointPlotActionB& getPointPlotActionB() { return _pointPlotActionB; }

private:

    PointPlotActionB        _pointPlotActionB;           /** point plot action for embedding B*/
};

Q_DECLARE_METATYPE(EmbeddingBPointPlotAction)

inline const auto embeddingBPointPlotActionMetaTypeId = qRegisterMetaType<EmbeddingBPointPlotAction*>("EmbeddingBPointPlotAction");