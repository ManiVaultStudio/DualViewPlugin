#pragma once
#include <actions/GroupAction.h>
#include <actions/TriggerAction.h>

using namespace mv::gui;

class DualViewPlugin;

/**
 * position action class
 *
 * Action class for position dimensions
 */
class EnrichmentAction : public GroupAction
{
    Q_OBJECT

public:

    /**
     * Construct with \p parent and \p title
     * @param parent Pointer to parent object
     * @param title Title of the action
     */
    Q_INVOKABLE EnrichmentAction(QObject* parent, const QString& title);

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

    TriggerAction& getEnrichmentAction() { return _enrichmentAction; }

private:
    TriggerAction                     _enrichmentAction;          /** Action for triggering enrichment analysis */

};

Q_DECLARE_METATYPE(EnrichmentAction)

inline const auto enrichmentActionMetaTypeId = qRegisterMetaType<EnrichmentAction*>("EnrichmentAction");