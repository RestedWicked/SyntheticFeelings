#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::AudioAppComponent
{
public:
    //==============================================================================
    MainComponent()
    {
        setSize (800, 600);
        setAudioChannels (0, 2); // no inputs, two outputs
        addAndMakeVisible (frequencySlider);
        frequencySlider.setRange (50.0, 5000.0);
        frequencySlider.setSkewFactorFromMidPoint (500.0); // [4]
        frequencySlider.setValue (currentFrequency, juce::dontSendNotification);  // [6]
    };
    ~MainComponent() override
    {
        shutdownAudio();
    }

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        currentSampleRate = sampleRate;
        updateAngleDelta();
        frequencySlider.onValueChange = [this]
                {
                    if (currentSampleRate > 0.0)
                        updateAngleDelta();
                };
    }
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        auto level = 0.125f;
                auto* leftBuffer  = bufferToFill.buffer->getWritePointer (0, bufferToFill.startSample);
                auto* rightBuffer = bufferToFill.buffer->getWritePointer (1, bufferToFill.startSample);
         
                auto localTargetFrequency = targetFrequency;
         
                if (localTargetFrequency != currentFrequency)                                                         // [7]
                {
                    auto frequencyIncrement = (localTargetFrequency - currentFrequency) / bufferToFill.numSamples;    // [8]
         
                    for (auto sample = 0; sample < bufferToFill.numSamples; ++sample)
                    {
                        auto currentSample = (float) std::sin (currentAngle);
                        currentFrequency += frequencyIncrement;                                                       // [9]
                        updateAngleDelta();                                                                           // [10]
                        currentAngle += angleDelta;
                        leftBuffer[sample]  = currentSample * level;
                        rightBuffer[sample] = currentSample * level;
                    }
         
                    currentFrequency = localTargetFrequency;
                }
                else                                                                                                  // [11]
                {
                    for (auto sample = 0; sample < bufferToFill.numSamples; ++sample)
                    {
                        auto currentSample = (float) std::sin (currentAngle);
                        currentAngle += angleDelta;
                        leftBuffer[sample]  = currentSample * level;
                        rightBuffer[sample] = currentSample * level;
                    }
                }
    }
    void releaseResources() override
    {
        juce::Logger::getCurrentLogger()->writeToLog ("Releasing audio resources");
    }
    void updateAngleDelta()
        {
            auto cyclesPerSample = frequencySlider.getValue() / currentSampleRate;         // [2]
            angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;          // [3]
        }


    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    //==============================================================================
    // Your private member variables go here...
    juce::Random random;
    juce::Slider frequencySlider;
    double currentSampleRate = 0.0, currentAngle = 0.0, angleDelta = 0.0; // [1]
    double currentFrequency = 500.0, targetFrequency = 500.0; // [5]
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
