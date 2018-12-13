#include "ARASampleProjectAudioProcessorEditor.h"
#include "ARASampleProjectDocumentController.h"
#include "RegionSequenceView.h"

constexpr int kTrackHeaderWidth = 120;
constexpr int kTrackHeight = 80;
constexpr int kStatusBarHeight = 20;
constexpr int kMinWidth = 500;
constexpr int kWidth = 1000;
constexpr int kMinHeight = 1 * kTrackHeight;
constexpr int kHeight = 5 * kTrackHeight + kStatusBarHeight;

//==============================================================================
ARASampleProjectAudioProcessorEditor::ARASampleProjectAudioProcessorEditor (ARASampleProjectAudioProcessor& p)
    : AudioProcessorEditor (&p),
      AudioProcessorEditorARAExtension (&p),
      playheadView (*this),
      horizontalScrollBar (false),
      araSampleProcessor (p)
{
    tracksViewPort.setScrollBarsShown (false, false, false, false);
    regionSequencesViewPort.setScrollBarsShown (true, true, false, false);
    regionSequenceListView.setBounds (0, 0, kWidth, kHeight);
    regionSequenceListView.addAndMakeVisible (playheadView);
    playheadView.setAlwaysOnTop (true);
    tracksViewPort.setViewedComponent (&regionSequenceListView, false);
    tracksView.setBounds (0, 0, kWidth, kHeight);
    tracksView.addAndMakeVisible (tracksViewPort);

    regionSequencesViewPort.setViewedComponent (&tracksView, false);
    addAndMakeVisible (regionSequencesViewPort);
    addAndMakeVisible (horizontalScrollBar);
    horizontalScrollBar.setColour (ScrollBar::ColourIds::backgroundColourId, Colours::yellow);
    horizontalScrollBar.addListener (this);

    zoomInButton.setButtonText("+");
    zoomOutButton.setButtonText("-");
    zoomInButton.onClick = [this]
    {
        storeRelativePosition();
        pixelsPerSecond = pixelsPerSecond * 2.0;
        resized();
    };
    zoomOutButton.onClick = [this]
    {
        storeRelativePosition();
        pixelsPerSecond =  pixelsPerSecond * 0.5;
        resized();
    };
    addAndMakeVisible (zoomInButton);
    addAndMakeVisible (zoomOutButton);

    followPlayheadToggleButton.setButtonText ("Viewport follows playhead");
    addAndMakeVisible (followPlayheadToggleButton);

    setSize (kWidth, kHeight);
    setResizeLimits (kMinWidth, kMinHeight, 32768, 32768);
    setResizable (true, false);

    if (isARAEditorView())
    {
        getARAEditorView()->addListener (this);

        static_cast<ARADocument*> (getARADocumentController()->getDocument())->addListener (this);

        rebuildView();
        startTimerHz (60);
    }
}

ARASampleProjectAudioProcessorEditor::~ARASampleProjectAudioProcessorEditor()
{
    if (isARAEditorView())
    {
        clearView();

        static_cast<ARADocument*> (getARADocumentController()->getDocument())->removeListener (this);

        getARAEditorView()->removeListener (this);
    }
    horizontalScrollBar.removeListener (this);
}

void ARASampleProjectAudioProcessorEditor::storeRelativePosition()
{
    pixelsUntilPlayhead = roundToInt (pixelsPerSecond * playheadPositionInSeconds - tracksViewPort.getViewArea().getX());
}

//==============================================================================
void ARASampleProjectAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));

    if (! isARAEditorView())
    {
        g.setColour (Colours::white);
        g.setFont (20.0f);
        g.drawFittedText ("Non ARA Instance. Please re-open as ARA2!", getLocalBounds(), Justification::centred, 1);
    }
}

void ARASampleProjectAudioProcessorEditor::scrollBarMoved (ScrollBar *scrollBarThatHasMoved, double newRangeStart)
{
    auto newRangeStartInt = roundToInt (newRangeStart);
    if (scrollBarThatHasMoved == &horizontalScrollBar)
        tracksViewPort.setViewPosition (newRangeStartInt, tracksViewPort.getViewPositionY());
}

void ARASampleProjectAudioProcessorEditor::resized()
{
    // max zoom 1px : 1sample (this is naive assumption as audio can be in different samplerate)
    maxPixelsPerSecond = jmax (processor.getSampleRate(), 300.0);

    // calculate visible time range
    if (regionSequenceViews.isEmpty())
    {
        startTime = 0.0;
        endTime = 0.0;
    }
    else
    {
        startTime = std::numeric_limits<double>::max();
        endTime = std::numeric_limits<double>::lowest();
        for (auto v : regionSequenceViews)
        {
            double sequenceStartTime, sequenceEndTime;
            v->getTimeRange (sequenceStartTime, sequenceEndTime);

            startTime = jmin (startTime, sequenceStartTime);
            endTime = jmax (endTime, sequenceEndTime);
        }
    }

    // enforce zoom in/out limits
    minPixelsPerSecond = (tracksView.getWidth()  - kTrackHeaderWidth + regionSequencesViewPort.getScrollBarThickness()) / (endTime - startTime);
    pixelsPerSecond =  jmax (minPixelsPerSecond, jmin (pixelsPerSecond, maxPixelsPerSecond));
    zoomOutButton.setEnabled (pixelsPerSecond > minPixelsPerSecond);
    zoomInButton.setEnabled (pixelsPerSecond < maxPixelsPerSecond);

    // set new bounds for all region sequence views
    int width = (int) ((endTime - startTime) * pixelsPerSecond + 0.5);
    int y = 0;
    for (auto v : regionSequenceViews)
    {
        // child of tracksView
        v->getTrackHeaderView().setBounds (0, y, kTrackHeaderWidth, kTrackHeight);
        // child of regionSequenceListView
        v->setBounds (0, y, width - regionSequencesViewPort.getScrollBarThickness(), kTrackHeight);
        y += kTrackHeight;
    }

    regionSequenceListView.setBounds (0, 0, width, y);
    tracksView.setBounds (0, 0, getWidth() - tracksViewPort.getScrollBarThickness(), y);
    tracksViewPort.setBounds (kTrackHeaderWidth, 0, getWidth() - kTrackHeaderWidth, y);
    regionSequencesViewPort.setBounds (0, 0, getWidth(), getHeight() - tracksViewPort.getScrollBarThickness() - kStatusBarHeight);
    playheadView.setBounds (regionSequenceListView.getBounds());

    // keeps viewport position relative to playhead
    const auto newPixelBasedPositionInSeconds = pixelsUntilPlayhead / pixelsPerSecond;
    regionSequencesViewPort.setViewPosition (regionSequencesViewPort.getViewPosition());
    auto relativeViewportPosition = tracksViewPort.getViewPosition();
    relativeViewportPosition.setX (roundToInt ((playheadPositionInSeconds - newPixelBasedPositionInSeconds) * pixelsPerSecond));
    tracksViewPort.setViewPosition (relativeViewportPosition);

    horizontalScrollBar.setRangeLimits (tracksViewPort.getHorizontalScrollBar().getRangeLimit());
    horizontalScrollBar.setBounds (kTrackHeaderWidth, regionSequencesViewPort.getBottom(), tracksViewPort.getWidth() - regionSequencesViewPort.getScrollBarThickness(), regionSequencesViewPort.getScrollBarThickness());
    horizontalScrollBar.setCurrentRange (tracksViewPort.getHorizontalScrollBar().getCurrentRange());

    zoomInButton.setBounds (getWidth() - kStatusBarHeight, getHeight() - kStatusBarHeight, kStatusBarHeight, kStatusBarHeight);
    zoomOutButton.setBounds (zoomInButton.getBounds().translated (-kStatusBarHeight, 0));
    followPlayheadToggleButton.setBounds (0, zoomInButton.getY(), 200, kStatusBarHeight);
}

void ARASampleProjectAudioProcessorEditor::rebuildView()
{
    clearView();

    for (auto regionSequence : getARADocumentController()->getDocument()->getRegionSequences())
    {
        if (ARA::contains (getARAEditorView()->getHiddenRegionSequences(), regionSequence))
            continue;

        auto sequenceView = new RegionSequenceView (this, static_cast<ARARegionSequence*> (regionSequence));
        regionSequenceViews.add (sequenceView);
        regionSequenceListView.addAndMakeVisible (sequenceView);
        tracksView.addAndMakeVisible (sequenceView->getTrackHeaderView());
    }

    // for demo purposes each rebuild resets zoom to show all document
    pixelsPerSecond = (endTime - startTime) / getWidth();

    resized();
}

void ARASampleProjectAudioProcessorEditor::clearView()
{
    regionSequenceViews.clear();
}

//==============================================================================
void ARASampleProjectAudioProcessorEditor::onNewSelection (const ARA::PlugIn::ViewSelection& /*currentSelection*/)
{
    rebuildView();
}

void ARASampleProjectAudioProcessorEditor::onHideRegionSequences (std::vector<ARARegionSequence*> const& /*regionSequences*/)
{
    rebuildView();
}

void ARASampleProjectAudioProcessorEditor::didEndEditing (ARADocument* document)
{
    jassert (document == getARADocumentController()->getDocument());

    if (isViewDirty)
    {
        rebuildView();
        isViewDirty = false;
    }
}

void ARASampleProjectAudioProcessorEditor::didReorderRegionSequencesInDocument (ARADocument* document)
{
    jassert (document == getARADocumentController()->getDocument());

    setDirty();
}

void ARASampleProjectAudioProcessorEditor::getVisibleTimeRange(double &start, double &end)
{
    start = tracksViewPort.getViewArea().getX() / pixelsPerSecond;
    end = tracksViewPort.getViewArea().getRight() / pixelsPerSecond;
}

ARASampleProjectAudioProcessorEditor::PlayheadView::PlayheadView(ARASampleProjectAudioProcessorEditor &owner)
    : owner(owner)
{}

void ARASampleProjectAudioProcessorEditor::PlayheadView::paint(juce::Graphics &g)
{
    int playheadX = roundToInt (owner.getPlayheadPositionInSeconds() * owner.getPixelsPerSeconds());
    g.setColour (findColour (ScrollBar::ColourIds::thumbColourId));
    g.fillRect(playheadX - kPlayheadWidth, 0, kPlayheadWidth, getHeight());
}

void ARASampleProjectAudioProcessorEditor::timerCallback()
{
    auto position = araSampleProcessor.getLastKnownPositionInfo();
    if (position.isPlaying)
    {
        playheadPositionInSeconds = position.timeInSeconds;
        if (followPlayheadToggleButton.getToggleState())
        {
            double visibleStart, visibleEnd;
            getVisibleTimeRange (visibleStart, visibleEnd);
            if (playheadPositionInSeconds < visibleStart || playheadPositionInSeconds > visibleEnd)
                tracksViewPort.setViewPosition(tracksViewPort.getViewPosition().withX (playheadPositionInSeconds * pixelsPerSecond));
        };
        playheadView.repaint();
    }
}
