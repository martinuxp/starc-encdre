#include "toggle.h"

#include <ui/design_system/design_system.h>
#include <ui/widgets/animations/click_animation.h>
#include <utils/helpers/color_helper.h>
#include <utils/helpers/image_helper.h>

#include <QMouseEvent>
#include <QPainter>
#include <QVariantAnimation>


class Toggle::Implementation
{
public:
    explicit Implementation(Toggle* _q);

    /**
     * @brief Размер контента
     */
    QSizeF contentSize() const;

    /**
     * @brief Анимировать переключение
     */
    void animateToggle();

    /**
     * @brief Прекратить аниацию, если виджет невидим
     */
    void finishAnimationIfInvisible();


    Toggle* q = nullptr;
    bool isChecked = true;
    QVariantAnimation tumblerAnimation;

    /**
     * @brief  Декорации переключателя при клике
     */
    ClickAnimation decorationAnimation;
};

Toggle::Implementation::Implementation(Toggle* _q)
    : q(_q)
{
    tumblerAnimation.setDuration(160);
    tumblerAnimation.setEasingCurve(QEasingCurve::OutQuad);
}

QSizeF Toggle::Implementation::contentSize() const
{
    return QSizeF(q->contentsMargins().left() + Ui::DesignSystem::layout().px12()
                      + Ui::DesignSystem::toggle().tumblerOverflow()
                      + Ui::DesignSystem::toggle().trackSize().width()
                      + Ui::DesignSystem::toggle().tumblerOverflow()
                      + Ui::DesignSystem::layout().px12() + q->contentsMargins().right(),
                  q->contentsMargins().top() + Ui::DesignSystem::layout().px12()
                      + Ui::DesignSystem::toggle().tumblerSize().height()
                      + Ui::DesignSystem::layout().px12() + q->contentsMargins().bottom());
}

void Toggle::Implementation::animateToggle()
{
    //
    // Определим крайние положения переключателя
    //

    const QRectF toggleLeftRect({ q->contentsMargins().left() + Ui::DesignSystem::layout().px12(),
                                  q->contentsMargins().top() + Ui::DesignSystem::layout().px12() },
                                Ui::DesignSystem::toggle().tumblerSize());
    const QRectF toggleRightRect({ q->contentsMargins().left() + Ui::DesignSystem::layout().px12()
                                       + Ui::DesignSystem::toggle().tumblerOverflow() * 2
                                       + Ui::DesignSystem::toggle().trackSize().width()
                                       - Ui::DesignSystem::toggle().tumblerSize().width(),
                                   toggleLeftRect.top() },
                                 toggleLeftRect.size());
    if ((isChecked && q->isLeftToRight()) || (!isChecked && q->isRightToLeft())) {
        tumblerAnimation.setStartValue(toggleLeftRect);
        tumblerAnimation.setEndValue(toggleRightRect);
    } else {
        tumblerAnimation.setStartValue(toggleRightRect);
        tumblerAnimation.setEndValue(toggleLeftRect);
    }
    tumblerAnimation.start();

    //
    // Если переключатель скрыт, то выполнять всю анимацию ни к чему
    //
    if (!q->isVisible()) {
        tumblerAnimation.setCurrentTime(tumblerAnimation.duration());
    }
}


// ****


Toggle::Toggle(QWidget* _parent)
    : Widget(_parent)
    , d(new Implementation(this))
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    d->animateToggle();

    connect(&d->tumblerAnimation, &QVariantAnimation::valueChanged, this,
            qOverload<>(&Toggle::update));
    connect(&d->decorationAnimation, &ClickAnimation::valueChanged, this,
            qOverload<>(&Toggle::update));
}

bool Toggle::isChecked() const
{
    return d->isChecked;
}

Toggle::~Toggle() = default;

void Toggle::setChecked(bool _checked)
{
    if (d->isChecked == _checked) {
        return;
    }

    d->isChecked = _checked;
    d->animateToggle();
    emit checkedChanged(d->isChecked);
}

void Toggle::toggle()
{
    setChecked(!isChecked());
}

QSize Toggle::minimumSizeHint() const
{
    return d->contentSize().toSize();
}

bool Toggle::event(QEvent* _event)
{
    if (_event->type() == QEvent::ContentsRectChange
        || _event->type() == QEvent::LayoutDirectionChange) {
        d->animateToggle();
    }

    return Widget::event(_event);
}

void Toggle::paintEvent(QPaintEvent* _event)
{
    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(_event->rect(), backgroundColor());

    //
    // Material Design 3 switch: filled track with a light handle when checked,
    // outlined track with a small outline-colored handle when unchecked
    //
    const auto accentColor = Ui::DesignSystem::color().accent();
    const auto outlineColor = ColorHelper::flattenAlpha(
        ColorHelper::transparent(textColor(), 0.6), backgroundColor());
    const auto trackColor = d->isChecked
        ? accentColor
        : ColorHelper::flattenAlpha(ColorHelper::transparent(textColor(), 0.12),
                                    backgroundColor());
    const auto handleColor
        = d->isChecked ? Ui::DesignSystem::color().onAccent() : outlineColor;
    const qreal opacity = isEnabled() ? 1.0 : Ui::DesignSystem::disabledTextOpacity();
    painter.setOpacity(opacity);

    //
    // Трэк
    //
    const QRectF trackRect({ contentsMargins().left() + Ui::DesignSystem::layout().px12()
                                 + Ui::DesignSystem::toggle().tumblerOverflow(),
                             contentsMargins().top() + Ui::DesignSystem::layout().px12()
                                 + Ui::DesignSystem::toggle().tumblerOverflow() },
                           Ui::DesignSystem::toggle().trackSize());
    painter.setBrush(trackColor);
    if (!d->isChecked) {
        const qreal outlineWidth = 2.0 * Ui::DesignSystem::scaleFactor();
        painter.setPen(QPen(outlineColor, outlineWidth));
        const QRectF outlineRect
            = trackRect.adjusted(outlineWidth / 2.0, outlineWidth / 2.0, -outlineWidth / 2.0,
                                 -outlineWidth / 2.0);
        painter.drawRoundedRect(outlineRect, outlineRect.height() / 2.0,
                                outlineRect.height() / 2.0);
        painter.setPen(Qt::NoPen);
    } else {
        painter.drawRoundedRect(trackRect, trackRect.height() / 2.0, trackRect.height() / 2.0);
    }

    //
    // Состояния наведения, фокуса и нажатия
    //
    const QRectF slotRect = d->tumblerAnimation.currentValue().toRectF();
    const auto stateColor = d->isChecked ? accentColor : textColor();
    if (underMouse() || hasFocus()) {
        painter.setBrush(stateColor);
        painter.setOpacity(opacity
                           * (hasFocus() ? Ui::DesignSystem::focusBackgroundOpacity()
                                         : Ui::DesignSystem::hoverBackgroundOpacity()));
        const auto radius = d->decorationAnimation.maximumRadius() * 0.84;
        painter.drawEllipse(slotRect.center(), radius, radius);
        painter.setOpacity(opacity);
    }
    if (d->decorationAnimation.state() == ClickAnimation::Running) {
        painter.setBrush(stateColor);
        painter.setOpacity(opacity * d->decorationAnimation.opacity());
        const auto radius = d->decorationAnimation.radius() * 0.84;
        painter.drawEllipse(slotRect.center(), radius, radius);
        painter.setOpacity(opacity);
    }

    //
    // Переключатель: 24dp when checked or pressed, 16dp when unchecked
    //
    const bool isPressed = d->decorationAnimation.state() == ClickAnimation::Running;
    const qreal handleSize = (d->isChecked || isPressed ? slotRect.width()
                                                        : slotRect.width() * 2.0 / 3.0);
    const QRectF handleRect(slotRect.center() - QPointF(handleSize, handleSize) / 2.0,
                            QSizeF(handleSize, handleSize));
    painter.setBrush(handleColor);
    painter.drawEllipse(handleRect);
}

void Toggle::mousePressEvent(QMouseEvent* _event)
{
    Q_UNUSED(_event);
    d->decorationAnimation.start();
}

void Toggle::mouseReleaseEvent(QMouseEvent* _event)
{
    if (!rect().contains(_event->pos())) {
        return;
    }

    setChecked(!d->isChecked);
}

void Toggle::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    Q_UNUSED(_event)

    d->decorationAnimation.setRadiusInterval(Ui::DesignSystem::toggle().tumblerSize().height()
                                                 / 2.0,
                                             Ui::DesignSystem::toggle().tumblerSize().height());

    updateGeometry();
    update();
}
