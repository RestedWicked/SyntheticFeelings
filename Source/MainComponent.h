#pragma once

//==============================================================================
struct SineWaveSound   : public juce::SynthesiserSound
{
    SineWaveSound() {}

    bool appliesToNote    (int) override        { return true; }
    bool appliesToChannel (int) override        { return true; }
};

//==============================================================================
struct SineWaveVoice   : public juce::SynthesiserVoice
{
    SineWaveVoice() {}

    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SineWaveSound*> (sound) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int /*currentPitchWheelPosition*/) override
    {
        currentAngle = 0.0;
        level = velocity * 0.15;
        tailOff = 0.0;

        auto cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        auto cyclesPerSample = cyclesPerSecond / getSampleRate();

        angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;
    }

    void stopNote (float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            if (tailOff == 0.0)
                tailOff = 1.0;
        }
        else
        {
            clearCurrentNote();
            angleDelta = 0.0;
        }
    }

    void pitchWheelMoved (int) override      {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioSampleBuffer& outputBuffer, int startSample, int numSamples) override
    {
        if (angleDelta != 0.0)
        {
            if (tailOff > 0.0) // [7]
            {
                while (--numSamples >= 0)
                {
                    auto currentSample = (float) (std::sin (currentAngle) * level * tailOff);

                    for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                        outputBuffer.addSample (i, startSample, currentSample);

                    currentAngle += angleDelta;
                    ++startSample;

                    tailOff *= 0.99; // [8]

                    if (tailOff <= 0.005)
                    {
                        clearCurrentNote(); // [9]

                        angleDelta = 0.0;
                        break;
                    }
                }
            }
            else
            {
                while (--numSamples >= 0) // [6]
                {
                    auto currentSample = (float) (std::sin (currentAngle) * level);

                    for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                        outputBuffer.addSample (i, startSample, currentSample);

                    currentAngle += angleDelta;
                    ++startSample;
                }
            }
        }
    }

private:
    double currentAngle = 0.0, angleDelta = 0.0, level = 0.0, tailOff = 0.0;
};

//==============================================================================
class SynthAudioSource   : public juce::AudioSource
{
public:
    SynthAudioSource (juce::MidiKeyboardState& keyState)
        : keyboardState (keyState)
    {
        for (auto i = 0; i < 4; ++i)
            synth.addVoice (new SineWaveVoice());

        synth.addSound (new SineWaveSound());
    }

    void setUsingSineWaveSound()
    {
        synth.clearSounds();
    }

    void prepareToPlay (int /*samplesPerBlockExpected*/, double sampleRate) override
    {
        synth.setCurrentPlaybackSampleRate (sampleRate);
        midiCollector.reset (sampleRate); // [10]
    }

    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        bufferToFill.clearActiveBufferRegion();

        juce::MidiBuffer incomingMidi;
        midiCollector.removeNextBlockOfMessages (incomingMidi, bufferToFill.numSamples); // [11]

        keyboardState.processNextMidiBuffer (incomingMidi, bufferToFill.startSample,
                                             bufferToFill.numSamples, true);

        synth.renderNextBlock (*bufferToFill.buffer, incomingMidi,
                               bufferToFill.startSample, bufferToFill.numSamples);
    }

    juce::MidiMessageCollector* getMidiCollector()
    {
        return &midiCollector;
    }

private:
    juce::MidiKeyboardState& keyboardState;
    juce::Synthesiser synth;
    juce::MidiMessageCollector midiCollector;
};

//==============================================================================
class MainComponent   : public juce::AudioAppComponent,
                               private juce::Timer
{
public:
    MainComponent()
        : synthAudioSource  (keyboardState),
          keyboardComponent (keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        addAndMakeVisible (midiInputListLabel);
        midiInputListLabel.setText ("MIDI Input:", juce::dontSendNotification);
        midiInputListLabel.attachToComponent (&midiInputList, true);

        auto midiInputs = juce::MidiInput::getAvailableDevices();
        addAndMakeVisible (midiInputList);
        midiInputList.setTextWhenNoChoicesAvailable ("No MIDI Inputs Enabled");

        juce::StringArray midiInputNames;
        for (auto input : midiInputs)
            midiInputNames.add (input.name);

        midiInputList.addItemList (midiInputNames, 1);
        midiInputList.onChange = [this] { setMidiInput (midiInputList.getSelectedItemIndex()); };

        for (auto input : midiInputs)
        {
            if (deviceManager.isMidiInputDeviceEnabled (input.identifier))
            {
                setMidiInput (midiInputs.indexOf (input));
                break;
            }
        }

        if (midiInputList.getSelectedId() == 0)
            setMidiInput (0);

        addAndMakeVisible (keyboardComponent);
        setAudioChannels (0, 2);

        setSize (600, 190);
        startTimer (400);
    }

    ~MainComponent() override
    {
        shutdownAudio();
    }

    void resized() override
    {
        midiInputList    .setBounds (200, 10, getWidth() - 210, 20);
        keyboardComponent.setBounds (10,  40, getWidth() - 20, getHeight() - 50);
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        synthAudioSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        synthAudioSource.getNextAudioBlock (bufferToFill);
    }

    void releaseResources() override
    {
        synthAudioSource.releaseResources();
    }

private:
    void timerCallback() override
    {
        keyboardComponent.grabKeyboardFocus();
        stopTimer();
    }

    void setMidiInput (int index)
    {
        auto list = juce::MidiInput::getAvailableDevices();

        deviceManager.removeMidiInputDeviceCallback (list[lastInputIndex].identifier, synthAudioSource.getMidiCollector());

        auto newInput = list[index];

        if (! deviceManager.isMidiInputDeviceEnabled (newInput.identifier))
            deviceManager.setMidiInputDeviceEnabled (newInput.identifier, true);

        deviceManager.addMidiInputDeviceCallback (newInput.identifier, synthAudioSource.getMidiCollector());
        midiInputList.setSelectedId (index + 1, juce::dontSendNotification);

        lastInputIndex = index;
    }

    //==========================================================================
    juce::MidiKeyboardState keyboardState;
    SynthAudioSource synthAudioSource;
    juce::MidiKeyboardComponent keyboardComponent;

    juce::ComboBox midiInputList;
    juce::Label midiInputListLabel;
    int lastInputIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
