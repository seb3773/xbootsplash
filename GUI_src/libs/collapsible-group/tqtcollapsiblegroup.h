#ifndef TQTCOLLAPSIBLEGROUP_H
#define TQTCOLLAPSIBLEGROUP_H

#include <ntqwidget.h>
#include <ntqpushbutton.h>

/**
 * @brief An accordion-style expandable/collapsible container widget.
 *
 * This widget provides a header button that can be clicked to show or hide
 * the underlying content area. It is designed to work smoothly with TQt3
 * layouts.
 */
class TQtCollapsibleGroup : public TQWidget
{
    TQ_OBJECT

public:
    /**
     * @brief Constructs a new collapsible group.
     * @param title The text to display in the header.
     * @param parent The parent widget.
     */
    TQtCollapsibleGroup(const TQString& title, TQWidget* parent = 0);
    ~TQtCollapsibleGroup();

    /**
     * @brief Returns the container widget where user UI elements should be added.
     * 
     * You should set a layout (e.g., TQVBoxLayout) on this returned widget
     * and add your child widgets to that layout.
     * 
     * @return TQWidget* The content area.
     */
    TQWidget* contentWidget() const;

    /**
     * @brief Sets the title text in the header.
     */
    void setTitle(const TQString& title);
    TQString title() const;

    /**
     * @brief Expands or collapses the group.
     */
    void setExpanded(bool expanded);
    
    /**
     * @brief Returns true if the group is currently expanded.
     */
    bool isExpanded() const;

signals:
    /**
     * @brief Emitted when the expanded state changes.
     * @param expanded True if the group is now expanded, false if collapsed.
     */
    void toggled(bool expanded);

private slots:
    void onHeaderClicked();

private:
    void updateHeaderIcon();

    TQPushButton* m_headerButton;
    TQWidget* m_contentArea;
    bool m_isExpanded;
};

#endif // TQTCOLLAPSIBLEGROUP_H
