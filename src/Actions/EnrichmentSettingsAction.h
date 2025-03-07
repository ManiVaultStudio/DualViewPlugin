#pragma once
#include <actions/GroupAction.h>
#include <actions/OptionAction.h>

using namespace mv::gui;

class DualViewPlugin;

/**
 * position action class
 *
 * Action class for position dimensions
 */
class EnrichmentSettingsAction : public GroupAction
{
    Q_OBJECT

public:

    /**
     * Construct with \p parent and \p title
     * @param parent Pointer to parent object
     * @param title Title of the action
     */
    Q_INVOKABLE EnrichmentSettingsAction(QObject* parent, const QString& title);

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

    OptionAction& getOrganismPickerAction() { return _organismPickerAction; }

private:
    OptionAction                     _organismPickerAction;          /** Action for choose organism */

};

Q_DECLARE_METATYPE(EnrichmentSettingsAction)

inline const auto enrichmentSettingsActionMetaTypeId = qRegisterMetaType<EnrichmentSettingsAction*>("EnrichmentSettingsAction");