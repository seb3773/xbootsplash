#include "tqtcollapsiblegroup.h"
#include <ntqlayout.h>
#include <ntqpainter.h>
#include <ntqpixmap.h>
#include <ntqsizepolicy.h>
#include <ntqpushbutton.h>

TQtCollapsibleGroup::TQtCollapsibleGroup(const TQString& title, TQWidget* parent)
    : TQWidget(parent),
      m_isExpanded(true)
{
    TQVBoxLayout* mainLayout = new TQVBoxLayout(this, 0, 0);

    // Create the header button
    // Create the header button
    m_headerButton = new TQPushButton(title, this);
    m_headerButton->setSizePolicy(TQSizePolicy(TQSizePolicy::Expanding, TQSizePolicy::Fixed));
    
    // Style the button
    // To make it look a bit flatter:
    // m_headerButton->setFlat(true); // Qt3 might not have setFlat on TQPushButton, but we can leave it as a regular button for clear visibility.
    m_headerButton->setFont(TQFont("sans", 10, TQFont::Bold));

    connect(m_headerButton, SIGNAL(clicked()), this, SLOT(onHeaderClicked()));
    
    mainLayout->addWidget(m_headerButton);

    // Create the content area
    m_contentArea = new TQWidget(this);
    mainLayout->addWidget(m_contentArea);

    // Initial setup
    updateHeaderIcon();
}

TQtCollapsibleGroup::~TQtCollapsibleGroup()
{
}

TQWidget* TQtCollapsibleGroup::contentWidget() const
{
    return m_contentArea;
}

void TQtCollapsibleGroup::setTitle(const TQString& title)
{
    m_headerButton->setText(title);
}

TQString TQtCollapsibleGroup::title() const
{
    return m_headerButton->text();
}

void TQtCollapsibleGroup::setExpanded(bool expanded)
{
    if (m_isExpanded == expanded)
        return;

    m_isExpanded = expanded;
    
    // Toggle content visibility
    if (m_isExpanded) {
        m_contentArea->show();
    } else {
        m_contentArea->hide();
    }
    
    updateHeaderIcon();
    emit toggled(m_isExpanded);
}

bool TQtCollapsibleGroup::isExpanded() const
{
    return m_isExpanded;
}

void TQtCollapsibleGroup::onHeaderClicked()
{
    setExpanded(!m_isExpanded);
}

void TQtCollapsibleGroup::updateHeaderIcon()
{
    // Draw a small triangle for the icon depending on state
    TQPixmap pm(16, 16);
    pm.fill(m_headerButton->paletteBackgroundColor());
    
    TQPainter p(&pm);
    p.setPen(TQt::black);
    p.setBrush(TQt::black);
    
    TQPointArray pts(3);
    if (m_isExpanded) {
        // Triangle pointing down
        pts.setPoint(0, 3, 5);
        pts.setPoint(1, 13, 5);
        pts.setPoint(2, 8, 11);
    } else {
        // Triangle pointing right
        pts.setPoint(0, 6, 3);
        pts.setPoint(1, 6, 13);
        pts.setPoint(2, 12, 8);
    }
    
    p.drawPolygon(pts);
    p.end();
    
    m_headerButton->setIconSet(TQIconSet(pm));
}

#include "tqtcollapsiblegroup.moc"
