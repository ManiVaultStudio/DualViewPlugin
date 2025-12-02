#pragma once
#include <actions/GroupAction.h>
#include <actions/DecimalAction.h>

using namespace mv::gui;

class DualViewPlugin;

/**
 * 
 *
 * Action class for line settings
 */
class LineSettingsAction : public GroupAction
{
    Q_OBJECT

public:

    /**
     * Construct with \p parent and \p title
     * @param parent Pointer to parent object
     * @param title Title of the action
     */
    Q_INVOKABLE LineSettingsAction(QObject* parent, const QString& title);

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

    DecimalAction& getThresholdLinesAction() { return _thresholdLinesAction; }
    DecimalAction& getlog2FCThresholdAction() { return _log2FCThreshold; };

private:
    DecimalAction                     _thresholdLinesAction;      /** Action for expression value threshold for lines */
    DecimalAction                     _log2FCThreshold;              /** Action for log2FC threshold for lines */
};

Q_DECLARE_METATYPE(LineSettingsAction)

inline const auto lineSettingsActionMetaTypeId = qRegisterMetaType<LineSettingsAction*>("LineSettingsAction");