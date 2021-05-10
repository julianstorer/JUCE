/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 6 End-User License
   Agreement and JUCE Privacy Policy (both effective as of the 16th June 2020).

   End User License Agreement: www.juce.com/juce-6-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

namespace juce
{

class ComponentAnimator::AnimationTask
{
public:
    AnimationTask (Component* c) noexcept  : component (c) {}

    ~AnimationTask()
    {
        proxy.deleteAndZero();
    }

    void reset (const Rectangle<int>& finalBounds,
                float finalAlpha,
                int millisecondsBeforeStartMoving,
                int millisecondsToSpendMoving,
                bool useProxyComponent,
                double startSpd, double endSpd)
    {
        msElapsed = -jmax (0, millisecondsBeforeStartMoving);
        msTotal = jmax (1, millisecondsToSpendMoving);
        lastProgress = 0;
        destination = finalBounds;
        destAlpha = finalAlpha;

        isMoving = (finalBounds != component->getBounds());
        isChangingAlpha = (finalAlpha != component->getAlpha());

        left    = component->getX();
        top     = component->getY();
        right   = component->getRight();
        bottom  = component->getBottom();
        alpha   = component->getAlpha();

        const double invTotalDistance = 4.0 / (startSpd + endSpd + 2.0);
        startSpeed = jmax (0.0, startSpd * invTotalDistance);
        midSpeed = invTotalDistance;
        endSpeed = jmax (0.0, endSpd * invTotalDistance);

        proxy.deleteAndZero();

        if (useProxyComponent)
            proxy = new ProxyComponent (*component);

        component->setVisible (! useProxyComponent);
    }

    bool useTimeslice (const int elapsed)
    {
        if (auto* c = proxy != nullptr ? proxy.getComponent()
                                       : component.get())
        {
            msElapsed += elapsed;
            if (msElapsed < 0)
                return true;
            double newProgress = msElapsed / (double) msTotal;

            if (newProgress >= 0 && newProgress < 1.0)
            {
                const WeakReference<AnimationTask> weakRef (this);
                newProgress = timeToDistance (newProgress);
                const double delta = (newProgress - lastProgress) / (1.0 - lastProgress);
                jassert (newProgress >= lastProgress);
                lastProgress = newProgress;

                if (delta < 1.0)
                {
                    bool stillBusy = false;

                    if (isMoving)
                    {
                        left   += (destination.getX()      - left)   * delta;
                        top    += (destination.getY()      - top)    * delta;
                        right  += (destination.getRight()  - right)  * delta;
                        bottom += (destination.getBottom() - bottom) * delta;

                        const Rectangle<int> newBounds (roundToInt (left),
                                                        roundToInt (top),
                                                        roundToInt (right - left),
                                                        roundToInt (bottom - top));

                        if (newBounds != destination)
                        {
                            c->setBounds (newBounds);
                            stillBusy = true;
                        }
                    }

                    // Check whether the animation was cancelled/deleted during
                    // a callback during the setBounds method
                    if (weakRef.wasObjectDeleted())
                        return false;

                    if (isChangingAlpha)
                    {
                        alpha += (destAlpha - alpha) * delta;
                        c->setAlpha ((float) alpha);
                        stillBusy = true;
                    }

                    if (stillBusy)
                        return true;
                }
            }
        }

        moveToFinalDestination();
        return false;
    }

    void moveToFinalDestination()
    {
        if (component != nullptr)
        {
            const WeakReference<AnimationTask> weakRef (this);
            component->setAlpha ((float) destAlpha);
            component->setBounds (destination);

            if (! weakRef.wasObjectDeleted())
                if (proxy != nullptr)
                    component->setVisible (destAlpha > 0);
        }
    }

    //==============================================================================
    struct ProxyComponent  : public Component
    {
        ProxyComponent (Component& c)
        {
            setWantsKeyboardFocus (false);
            setBounds (c.getBounds());
            setTransform (c.getTransform());
            setAlpha (c.getAlpha());
            setInterceptsMouseClicks (false, false);

            if (auto* parent = c.getParentComponent())
                parent->addAndMakeVisible (this);
            else if (c.isOnDesktop() && c.getPeer() != nullptr)
                addToDesktop (c.getPeer()->getStyleFlags() | ComponentPeer::windowIgnoresKeyPresses);
            else
                jassertfalse; // seem to be trying to animate a component that's not visible..

            auto scale = (float) Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds())->scale
                           * Component::getApproximateScaleFactorForComponent (&c);

            image = c.createComponentSnapshot (c.getLocalBounds(), false, scale);

            setVisible (true);
            toBehind (&c);
        }

        void paint (Graphics& g) override
        {
            g.setOpacity (1.0f);
            g.drawImageTransformed (image, AffineTransform::scale ((float) getWidth()  / (float) jmax (1, image.getWidth()),
                                                                   (float) getHeight() / (float) jmax (1, image.getHeight())), false);
        }

    private:
        Image image;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProxyComponent)
    };

    WeakReference<Component> component;
    Component::SafePointer<Component> proxy;

    Rectangle<int> destination;
    double destAlpha;

    int msElapsed, msTotal;
    double startSpeed, midSpeed, endSpeed, lastProgress;
    double left, top, right, bottom, alpha;
    bool isMoving, isChangingAlpha;

private:
    double timeToDistance (const double time) const noexcept
    {
        return (time < 0.5) ? time * (startSpeed + time * (midSpeed - startSpeed))
                            : 0.5 * (startSpeed + 0.5 * (midSpeed - startSpeed))
                                + (time - 0.5) * (midSpeed + (time - 0.5) * (endSpeed - midSpeed));
    }

    JUCE_DECLARE_WEAK_REFERENCEABLE (AnimationTask)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnimationTask)
};

//==============================================================================
ComponentAnimator::ComponentAnimator()  : lastTime (0) {}
ComponentAnimator::~ComponentAnimator() {}

//==============================================================================
ComponentAnimator::AnimationTask* ComponentAnimator::findTaskFor (Component* const component) const noexcept
{
    for (int i = tasks.size(); --i >= 0;)
        if (component == tasks.getUnchecked(i)->component.get())
            return tasks.getUnchecked(i);

    return nullptr;
}

void ComponentAnimator::animateComponent (Component* const component,
                                          const Rectangle<int>& finalBounds,
                                          const float finalAlpha,
                                          const int millisecondsToSpendMoving,
                                          const bool useProxyComponent,
                                          const double startSpeed,
                                          const double endSpeed)
{
    animateComponent(component, finalBounds, finalAlpha, 0, millisecondsToSpendMoving, useProxyComponent, startSpeed, endSpeed);
}

void ComponentAnimator::animateComponent (Component* const component,
                                          const Rectangle<int>& finalBounds,
                                          const float finalAlpha,
                                          const int millisecondsBeforeStartMoving,
                                          const int millisecondsToSpendMoving,
                                          const bool useProxyComponent,
                                          const double startSpeed,
                                          const double endSpeed)
{
    // the speeds must be 0 or greater!
    jassert (startSpeed >= 0 && endSpeed >= 0);

    if (component != nullptr)
    {
        auto* at = findTaskFor (component);

        if (at == nullptr)
        {
            at = new AnimationTask (component);
            tasks.add (at);
            sendChangeMessage();
        }

        at->reset (finalBounds, finalAlpha,
                   millisecondsBeforeStartMoving, millisecondsToSpendMoving,
                   useProxyComponent, startSpeed, endSpeed);

        if (! isTimerRunning())
        {
            lastTime = Time::getMillisecondCounter();
            startTimerHz (50);
        }
    }
}

void ComponentAnimator::fadeOut (Component* component, int millisecondsToTake)
{
    if (component != nullptr)
    {
        if (component->isShowing() && millisecondsToTake > 0)
            animateComponent (component, component->getBounds(), 0.0f, millisecondsToTake, true, 1.0, 1.0);

        component->setVisible (false);
    }
}

void ComponentAnimator::fadeIn (Component* component, int millisecondsToTake)
{
    if (component != nullptr && ! (component->isVisible() && component->getAlpha() == 1.0f))
    {
        component->setAlpha (0.0f);
        component->setVisible (true);
        animateComponent (component, component->getBounds(), 1.0f, millisecondsToTake, false, 1.0, 1.0);
    }
}

void ComponentAnimator::cancelAllAnimations (const bool moveComponentsToTheirFinalPositions)
{
    if (tasks.size() > 0)
    {
        if (moveComponentsToTheirFinalPositions)
            for (int i = tasks.size(); --i >= 0;)
                tasks.getUnchecked(i)->moveToFinalDestination();

        tasks.clear();
        sendChangeMessage();
    }
}

void ComponentAnimator::cancelAnimation (Component* const component,
                                         const bool moveComponentToItsFinalPosition)
{
    if (auto* at = findTaskFor (component))
    {
        if (moveComponentToItsFinalPosition)
            at->moveToFinalDestination();

        tasks.removeObject (at);
        sendChangeMessage();
    }
}

Rectangle<int> ComponentAnimator::getComponentDestination (Component* const component)
{
    jassert (component != nullptr);

    if (auto* at = findTaskFor (component))
        return at->destination;

    return component->getBounds();
}

bool ComponentAnimator::isAnimating (Component* component) const noexcept
{
    return findTaskFor (component) != nullptr;
}

bool ComponentAnimator::isAnimating() const noexcept
{
    return tasks.size() != 0;
}

void ComponentAnimator::timerCallback()
{
    auto timeNow = Time::getMillisecondCounter();

    if (lastTime == 0)
        lastTime = timeNow;

    auto elapsed = (int) (timeNow - lastTime);

    for (auto* task : Array<AnimationTask*> (tasks.begin(), tasks.size()))
    {
        if (tasks.contains (task) && ! task->useTimeslice (elapsed))
        {
            tasks.removeObject (task);
            sendChangeMessage();
        }
    }

    lastTime = timeNow;

    if (tasks.size() == 0)
        stopTimer();
}

} // namespace juce
